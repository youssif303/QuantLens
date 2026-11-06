#include "LiveFeed.h"

#include <QJsonArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace ql {

LiveFeed::LiveFeed(QObject* parent) : QObject(parent) {
    connect(&ws_, &QWebSocket::textMessageReceived, this, &LiveFeed::onText);
    connect(&ws_, &QWebSocket::connected, this, [this]() {
        wsConnected_ = true;
        backoffMs_ = 1000;
        emit connectionChanged();
    });
    connect(&ws_, &QWebSocket::disconnected, this, [this]() {
        if (intentionalClose_) return;
        qWarning() << "Binance WS disconnected:" << ws_.closeReason() << ws_.errorString();
        wsConnected_ = false;
        emit connectionChanged();
        scheduleReconnect();
    });
    retryTimer_.setSingleShot(true);
    connect(&ws_, &QWebSocket::errorOccurred, this, [this]() {
        if (!intentionalClose_ && !wsConnected_ && !retryTimer_.isActive()) scheduleReconnect();
    });
    connect(&retryTimer_, &QTimer::timeout, this, &LiveFeed::reconnect);

    pollTimer_.setInterval(4000); // one stock per tick, staggered: 8 stocks ~ 32 s per full cycle
    connect(&pollTimer_, &QTimer::timeout, this, &LiveFeed::pollNext);
}

void LiveFeed::setEnabled(bool on) {
    enabled_ = on;
    if (!on) {
        retryTimer_.stop();
        pollTimer_.stop();
        intentionalClose_ = true;
        ws_.close();
        intentionalClose_ = false;
    } else {
        reconnect();
        pollTimer_.start();
    }
}

void LiveFeed::setSymbols(const QStringList& symbols) {
    QStringList c, s;
    for (const auto& sym : symbols) {
        const Asset* a = findAsset(sym);
        if (!a) continue;
        (a->crypto ? c : s).append(a->symbol);
    }
    const bool changed = (c != crypto_);
    crypto_ = c;
    stocks_ = s;
    if (enabled_) {
        if (changed) reconnect();
        if (!stocks_.isEmpty() && !pollTimer_.isActive()) pollTimer_.start();
    }
}

void LiveFeed::reconnect() {
    if (!enabled_) return;
    retryTimer_.stop();
    intentionalClose_ = true; // abort() emits disconnected synchronously; don't treat it as a failure
    ws_.abort();
    intentionalClose_ = false;
    wsConnected_ = false;
    if (crypto_.isEmpty()) return;
    QStringList streams;
    for (const auto& s : crypto_) streams << findAsset(s)->binance.toLower() + "@miniTicker";
    ws_.open(QUrl("wss://data-stream.binance.vision/stream?streams=" + streams.join('/')));
}

void LiveFeed::scheduleReconnect() {
    if (!enabled_ || crypto_.isEmpty()) return;
    retryTimer_.start(backoffMs_);
    backoffMs_ = qMin(backoffMs_ * 2, 30000);
}

void LiveFeed::onText(const QString& text) {
    const QJsonObject d = QJsonDocument::fromJson(text.toUtf8()).object()["data"].toObject();
    const QString bsym = d["s"].toString();
    if (bsym.isEmpty()) return;
    for (const auto& s : crypto_) {
        const Asset* a = findAsset(s);
        if (a && a->binance == bsym) {
            emit tick(a->symbol, d["c"].toString().toDouble(), d["o"].toString().toDouble(), true);
            return;
        }
    }
}

void LiveFeed::pollNext() {
    if (stocks_.isEmpty()) return;
    const Asset* a = findAsset(stocks_[pollIndex_++ % stocks_.size()]);
    if (!a) return;
    QUrl u("https://query1.finance.yahoo.com/v8/finance/chart/" + QString::fromUtf8(QUrl::toPercentEncoding(a->yahoo)));
    QUrlQuery q;
    q.addQueryItem("range", "1d");
    q.addQueryItem("interval", "1m");
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    req.setTransferTimeout(8000);
    auto* reply = net_.get(req);
    const QString sym = a->symbol;
    connect(reply, &QNetworkReply::finished, this, [this, reply, sym]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const QJsonArray res = QJsonDocument::fromJson(reply->readAll()).object()["chart"].toObject()["result"].toArray();
        if (res.isEmpty()) return;
        const QJsonObject meta = res[0].toObject()["meta"].toObject();
        const double price = meta["regularMarketPrice"].toDouble();
        const double prev = meta["chartPreviousClose"].toDouble();
        if (price > 0) emit tick(sym, price, prev, false);
    });
}

} // namespace ql
