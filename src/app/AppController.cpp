#include "AppController.h"

#include "core/Indicators.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QtConcurrent>

namespace ql {

namespace {
const Candles kEmpty;
}

AppController::AppController(bool offline, QObject* parent) : QObject(parent), data_(this), feed_(this) {
    data_.setOffline(offline);
    QSettings s;
    QStringList list = s.value("watchlist", defaultWatchlist()).toStringList();
    if (list.isEmpty()) list = defaultWatchlist();
    model_.setSymbols(list);
    currency_ = s.value("currency", "USD").toString();
    eurUsd_ = s.value("eurUsd", 1.08).toDouble();
    if (!offline) {
        refreshFx();
        auto* fxTimer = new QTimer(this);
        connect(fxTimer, &QTimer::timeout, this, &AppController::refreshFx);
        fxTimer->start(5 * 60 * 1000);
    }

    connect(&data_, &MarketData::historyReady, this, &AppController::onHistory);
    connect(&data_, &MarketData::olderReady, this, &AppController::onOlder);
    connect(&feed_, &LiveFeed::tick, this, &AppController::onTick);
    connect(&feed_, &LiveFeed::connectionChanged, this, [this]() {
        if (feed_.cryptoConnected()) return;
        for (const auto& sym : model_.symbols()) {
            const Asset* a = findAsset(sym);
            const auto* it = model_.item(sym);
            if (a && a->crypto && it && it->status == "LIVE") model_.setStatus(sym, "OFFLINE");
        }
        emit priceChanged();
    });
    connect(&watcher_, &QFutureWatcher<AnalysisResult>::finished, this, [this]() {
        AnalysisResult r = watcher_.result();
        busy_ = false;
        if (r.symbol != selected_ || rerun_) { // selection or data changed while computing
            rerun_ = false;
            emit analysisChanged();
            analyze();
            if (r.symbol != selected_) return;
        }
        result_ = r;
        const Asset* a = findAsset(r.symbol);
        shortOut_ = toMap(r.shortF, a ? a->shortLabel() : "");
        longOut_ = toMap(r.longF, a ? a->longLabel() : "");
        shortBt_ = toMap(r.shortB);
        longBt_ = toMap(r.longB);
        emit analysisChanged();
        emit chartChanged();
    });

    // Throttle chart repaints coming from live ticks.
    chartTimer_.setInterval(250);
    connect(&chartTimer_, &QTimer::timeout, this, [this]() {
        if (chartDirty_) {
            chartDirty_ = false;
            emit chartChanged();
        }
    });
    chartTimer_.start();

    for (const auto& sym : model_.symbols())
        if (const Asset* a = findAsset(sym)) data_.load(*a);
    syncFeed();
    feed_.setEnabled(!offline);
    select(model_.symbols().value(0));
}

double AppController::factor(const QString& symbol) const {
    if (currency_ != "EUR" || symbol.startsWith('^')) return 1.0;
    return 1.0 / eurUsd_;
}

void AppController::setCurrency(const QString& c) {
    if ((c != "USD" && c != "EUR") || c == currency_) return;
    currency_ = c;
    QSettings().setValue("currency", c);
    emit currencyChanged();
    emit chartChanged();
}

void AppController::refreshFx() {
    QNetworkRequest req(QUrl("https://data-api.binance.vision/api/v3/ticker/price?symbol=EURUSDT"));
    req.setTransferTimeout(10000);
    auto* r = fxNet_.get(req);
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) return;
        const double v = QJsonDocument::fromJson(r->readAll()).object()["price"].toString().toDouble();
        if (v < 0.5 || v > 2.0) return;
        eurUsd_ = v;
        QSettings().setValue("eurUsd", v);
        emit currencyChanged();
        emit chartChanged();
    });
}

void AppController::syncFeed() { feed_.setSymbols(model_.symbols()); }

void AppController::saveWatchlist() { QSettings().setValue("watchlist", model_.symbols()); }

QString AppController::name() const {
    const Asset* a = findAsset(selected_);
    return a ? a->name : QString();
}
double AppController::price() const {
    const auto* it = model_.item(selected_);
    return it ? it->price : 0.0;
}
double AppController::changePct() const {
    const auto* it = model_.item(selected_);
    return it && it->reference > 0 ? (it->price / it->reference - 1.0) * 100.0 : 0.0;
}
QString AppController::status() const {
    const auto* it = model_.item(selected_);
    return it ? it->status : QString("...");
}
const Candles& AppController::dailyCandles() const {
    auto it = history_.constFind(selected_);
    return it == history_.constEnd() ? kEmpty : it.value();
}
const Candles& AppController::candles() const {
    if (interval_ == "1d") return dailyCandles();
    auto it = chart_.constFind(chartKey(selected_, interval_));
    return it == chart_.constEnd() ? kEmpty : it.value();
}
bool AppController::crypto() const {
    const Asset* a = findAsset(selected_);
    return a && a->crypto;
}
QStringList AppController::intervals() const {
    QStringList out;
    const Asset* a = findAsset(selected_);
    for (const auto& i : ql::intervals())
        if (!a || a->supports(i.id)) out << i.id;
    return out;
}
int AppController::coneBars(bool longHorizon) const {
    const Asset* a = findAsset(selected_);
    if (!a) return 0;
    return longHorizon ? a->longHorizon() : a->shortHorizon();
}

void AppController::loadChart() {
    if (interval_ == "1d") {
        chartLoading_ = false;
        return;
    }
    const Asset* a = findAsset(selected_);
    if (!a) return;
    chartLoading_ = !chart_.contains(chartKey(selected_, interval_));
    data_.load(*a, interval_); // refresh in the background even when cached in memory
}

void AppController::setInterval(const QString& id) {
    const Asset* a = findAsset(selected_);
    if (id == interval_ || !a || !a->supports(id)) return;
    interval_ = id;
    loadChart();
    emit intervalChanged();
    emit chartReset();
    emit chartChanged();
}

void AppController::requestOlder() {
    const Asset* a = findAsset(selected_);
    const Candles& c = candles();
    if (!a || !a->crypto || c.empty() || loadingOlder_ || exhausted_.contains(chartKey(selected_, interval_))) return;
    loadingOlder_ = true;
    data_.loadOlder(*a, interval_, c.front().time);
}

void AppController::onOlder(const QString& symbol, const QString& interval, const Candles& older) {
    loadingOlder_ = false;
    const QString key = chartKey(symbol, interval);
    Candles* target = interval == "1d" ? (history_.contains(symbol) ? &history_[symbol] : nullptr)
                                       : (chart_.contains(key) ? &chart_[key] : nullptr);
    if (!target || target->empty()) return;
    Candles add;
    for (const auto& c : older)
        if (c.time < target->front().time) add.push_back(c);
    if (add.empty()) {
        exhausted_.insert(key);
        return;
    }
    target->insert(target->begin(), add.begin(), add.end());
    if (symbol == selected_ && interval == interval_) {
        emit candlesPrepended(static_cast<int>(add.size()));
        emit chartChanged();
    }
}

void AppController::select(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_) return;
    selected_ = symbol;
    result_ = AnalysisResult{};
    shortOut_ = longOut_ = shortBt_ = longBt_ = QVariantMap();
    const Asset* a = findAsset(symbol);
    if (a && !a->supports(interval_)) interval_ = "1d";
    loadingOlder_ = false;
    loadChart();
    emit selectedChanged();
    emit intervalChanged();
    emit priceChanged();
    emit analysisChanged();
    emit chartReset();
    emit chartChanged();
    if (history_.contains(symbol)) analyze();
}

void AppController::addAsset(const QString& symbol) {
    const Asset* a = findAsset(symbol);
    if (!a || model_.contains(a->symbol)) {
        if (a) select(a->symbol);
        return;
    }
    QStringList l = model_.symbols();
    l << a->symbol;
    model_.setSymbols(l);
    for (const auto& s : l) // rebuild keeps cached histories; refresh row data
        if (history_.contains(s)) onHistory(s, "1d", history_[s], source_.value(s));
    data_.load(*a);
    saveWatchlist();
    syncFeed();
    select(a->symbol);
}

void AppController::removeAsset(const QString& symbol) {
    QStringList l = model_.symbols();
    if (l.size() <= 1) return;
    l.removeAll(symbol);
    model_.setSymbols(l);
    for (const auto& s : l)
        if (history_.contains(s)) onHistory(s, "1d", history_[s], source_.value(s));
    saveWatchlist();
    syncFeed();
    if (selected_ == symbol) select(l.first());
}

QVariantList AppController::search(const QString& query) const {
    QVariantList out;
    for (const auto& a : searchAssets(query)) {
        if (model_.contains(a.symbol)) continue;
        out.append(QVariantMap{{"symbol", a.symbol}, {"name", a.name}, {"crypto", a.crypto}});
    }
    return out;
}

void AppController::onHistory(const QString& symbol, const QString& interval, const Candles& c, const QString& source) {
    if (interval != "1d") {
        if (symbol == selected_ && interval == interval_) {
            chartLoading_ = false;
            emit intervalChanged();
        }
        if (c.size() < 2) return;
        chart_[chartKey(symbol, interval)] = c;
        exhausted_.remove(chartKey(symbol, interval));
        if (symbol == selected_ && interval == interval_) emit chartChanged();
        return;
    }
    if (c.size() < 2) return;
    // Don't let an older sample/cache snapshot overwrite fresher network data.
    if (source != "network" && source_.value(symbol) == "network" && history_.contains(symbol)) return;
    history_[symbol] = c;
    source_[symbol] = source;

    QVariantList spark;
    const std::size_t n = std::min<std::size_t>(c.size(), 40);
    for (std::size_t i = c.size() - n; i < c.size(); ++i) spark.append(c[i].close);
    const QString st = source == "network" ? "DAILY" : (source == "cache" ? "CACHED" : "OFFLINE");
    model_.setHistory(symbol, spark, c.back().close, c[c.size() - 2].close, st);

    if (symbol == selected_) {
        emit priceChanged();
        emit selectedChanged();
        emit chartChanged();
        analyze();
    }
}

void AppController::onTick(const QString& symbol, double price, double reference, bool realtime) {
    model_.setTick(symbol, price, reference, realtime ? "LIVE" : "DELAYED");
    lastUpdate_ = QDateTime::currentDateTime().toString("HH:mm:ss");

    auto it = history_.find(symbol);
    const bool isCrypto = realtime;
    if (it != history_.end() && !it->empty()) applyTick(it.value(), 86400, isCrypto, price, false);
    if (interval_ != "1d" && symbol == selected_) {
        auto ct = chart_.find(chartKey(symbol, interval_));
        if (ct != chart_.end() && !ct->empty()) applyTick(ct.value(), intervalSecs(), isCrypto, price, true);
    }
    if (symbol == selected_) {
        emit priceChanged();
        chartDirty_ = true;
    }
}

void AppController::applyTick(Candles& c, qint64 secs, bool realtime, double price, bool) {
    const std::int64_t now = QDateTime::currentSecsSinceEpoch();
    const Candle last = c.back();
    if (realtime && secs < 604800 && now >= last.time + secs) {
        const std::int64_t t = last.time + ((now - last.time) / secs) * secs;
        c.push_back({t, price, price, price, price, 0});
        return;
    }
    Candle& l = c.back();
    l.close = price;
    l.high = std::max(l.high, price);
    l.low = std::min(l.low, price);
}

void AppController::analyze() {
    if (watcher_.isRunning()) {
        rerun_ = true; // re-run once the current job finishes
        return;
    }
    const Asset* a = findAsset(selected_);
    auto it = history_.constFind(selected_);
    if (!a || it == history_.constEnd()) return;
    busy_ = true;
    emit analysisChanged();
    const Series close = ind::closes(it.value());
    const Asset asset = *a;
    watcher_.setFuture(QtConcurrent::run([close, asset]() {
        AnalysisResult r;
        r.symbol = asset.symbol;
        ForecastParams p;
        p.periodsPerYear = asset.periodsPerYear();
        p.horizon = static_cast<std::size_t>(asset.shortHorizon());
        r.shortF = forecast(close, p);
        p.horizon = static_cast<std::size_t>(asset.longHorizon());
        r.longF = forecast(close, p);
        r.shortB = backtest(close, static_cast<std::size_t>(asset.shortHorizon()), asset.periodsPerYear(), 5);
        r.longB = backtest(close, static_cast<std::size_t>(asset.longHorizon()), asset.periodsPerYear(), 10);
        return r;
    }));
}

QVariantMap AppController::toMap(const Forecast& f, const QString& label) {
    QVariantMap m;
    m["valid"] = f.valid;
    m["label"] = label;
    if (!f.valid) return m;
    m["direction"] = QString::fromLatin1(toString(f.direction));
    m["probUp"] = f.probUp;
    m["expectedReturn"] = f.expectedReturn;
    m["low"] = f.cone.back().p10 / f.lastPrice - 1.0;
    m["high"] = f.cone.back().p90 / f.lastPrice - 1.0;
    m["confidence"] = QString::fromLatin1(toString(f.confidence));
    QVariantList reasons;
    for (const auto& r : f.reasons) reasons << QString::fromStdString(r);
    m["reasons"] = reasons;
    return m;
}

QVariantMap AppController::toMap(const BacktestResult& b) {
    QVariantMap m;
    m["valid"] = b.valid;
    if (!b.valid) return m;
    m["samples"] = static_cast<int>(b.samples);
    m["hitRate"] = b.hitRate;
    m["baseline"] = b.baselineHitRate;
    m["brier"] = b.brier;
    m["brierBase"] = b.brierBaseline;
    m["skill"] = b.skill;
    QVariantList bins;
    for (const auto& x : b.bins) bins << QVariantMap{{"p", x.predicted}, {"o", x.observed}, {"n", static_cast<int>(x.count)}};
    m["bins"] = bins;
    return m;
}

} // namespace ql
