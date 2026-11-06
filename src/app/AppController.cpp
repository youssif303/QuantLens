#include "AppController.h"

#include "core/Indicators.h"

#include <QDateTime>
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

    connect(&data_, &MarketData::historyReady, this, &AppController::onHistory);
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
const Candles& AppController::candles() const {
    auto it = history_.constFind(selected_);
    return it == history_.constEnd() ? kEmpty : it.value();
}
bool AppController::crypto() const {
    const Asset* a = findAsset(selected_);
    return a && a->crypto;
}

void AppController::select(const QString& symbol) {
    if (symbol.isEmpty() || symbol == selected_) return;
    selected_ = symbol;
    result_ = AnalysisResult{};
    shortOut_ = longOut_ = shortBt_ = longBt_ = QVariantMap();
    emit selectedChanged();
    emit priceChanged();
    emit analysisChanged();
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
        if (history_.contains(s)) onHistory(s, history_[s], source_.value(s));
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
        if (history_.contains(s)) onHistory(s, history_[s], source_.value(s));
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

void AppController::onHistory(const QString& symbol, const Candles& c, const QString& source) {
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
    if (it != history_.end() && !it->empty()) {
        Candles& c = it.value();
        const std::int64_t now = QDateTime::currentSecsSinceEpoch();
        const std::int64_t day = now - now % 86400;
        if (c.back().time < day && realtime) {
            c.push_back({day, price, price, price, price, 0});
        } else {
            Candle& l = c.back();
            l.close = price;
            l.high = std::max(l.high, price);
            l.low = std::min(l.low, price);
        }
    }
    if (symbol == selected_) {
        emit priceChanged();
        chartDirty_ = true;
    }
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
