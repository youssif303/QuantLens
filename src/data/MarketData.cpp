#include "MarketData.h"

#include "core/Csv.h"
#include "core/Resample.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace ql {

MarketData::MarketData(QObject* parent) : QObject(parent) {}

QString MarketData::cachePath(const Asset& a) const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/history";
    QDir().mkpath(dir);
    QString name = a.symbol;
    name.replace('^', '_');
    return dir + "/" + name + ".csv";
}

void MarketData::saveCache(const Asset& a, const Candles& c) const {
    QFile f(cachePath(a));
    if (f.open(QIODevice::WriteOnly)) f.write(QByteArray::fromStdString(csv::toString(c)));
}

bool MarketData::loadLocal(const Asset& a) {
    auto readCsv = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return Candles{};
        return csv::fromString(f.readAll().toStdString());
    };
    Candles c = readCsv(cachePath(a));
    QString src = "cache";
    if (c.size() < 60 && !a.sampleFile.isEmpty()) {
        c = readCsv(":/qt/qml/QuantLens/data/" + a.sampleFile + ".csv");
        src = "sample";
    }
    if (c.size() < 60) return false;
    emit historyReady(a.symbol, "1d", c, src);
    return true;
}

void MarketData::load(const Asset& a, const QString& interval) {
    const bool daily = interval == "1d";
    const bool haveLocal = daily && loadLocal(a);
    if (offline_) {
        if (!haveLocal) emit historyReady(a.symbol, interval, {}, "none");
        return;
    }
    if (a.crypto) fetchBinance(a, interval);
    else fetchYahoo(a, interval);
}

Candles MarketData::parseBinance(const QByteArray& json) {
    Candles out;
    const QJsonArray arr = QJsonDocument::fromJson(json).array();
    out.reserve(static_cast<size_t>(arr.size()));
    for (const auto& v : arr) {
        const QJsonArray k = v.toArray();
        if (k.size() < 6) continue;
        out.push_back({static_cast<std::int64_t>(k[0].toDouble() / 1000.0), k[1].toString().toDouble(),
                       k[2].toString().toDouble(), k[3].toString().toDouble(), k[4].toString().toDouble(),
                       k[5].toString().toDouble()});
    }
    return out;
}

Candles MarketData::parseYahoo(const QByteArray& json) {
    Candles out;
    const QJsonArray results = QJsonDocument::fromJson(json).object()["chart"].toObject()["result"].toArray();
    if (results.isEmpty()) return out;
    const QJsonObject r = results[0].toObject();
    const QJsonArray ts = r["timestamp"].toArray();
    const QJsonObject q = r["indicators"].toObject()["quote"].toArray()[0].toObject();
    const QJsonArray o = q["open"].toArray(), h = q["high"].toArray(), l = q["low"].toArray(),
                     c = q["close"].toArray(), v = q["volume"].toArray();
    for (int i = 0; i < ts.size(); ++i) {
        if (c[i].isNull() || o[i].isNull() || h[i].isNull() || l[i].isNull()) continue; // skip holes
        out.push_back({static_cast<std::int64_t>(ts[i].toDouble()), o[i].toDouble(), h[i].toDouble(),
                       l[i].toDouble(), c[i].toDouble(), v[i].toDouble()});
    }
    return out;
}

QNetworkRequest MarketData::binanceRequest(const Asset& a, const QString& interval, qint64 endTimeMs) const {
    QUrl u("https://data-api.binance.vision/api/v3/klines");
    QUrlQuery q;
    q.addQueryItem("symbol", a.binance);
    q.addQueryItem("interval", interval);
    q.addQueryItem("limit", "1000");
    if (endTimeMs > 0) q.addQueryItem("endTime", QString::number(endTimeMs));
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setTransferTimeout(15000);
    return req;
}

void MarketData::loadOlder(const Asset& a, const QString& interval, qint64 beforeSec) {
    if (offline_ || !a.crypto) {
        emit olderReady(a.symbol, interval, {});
        return;
    }
    auto* reply = net_.get(binanceRequest(a, interval, beforeSec * 1000 - 1));
    connect(reply, &QNetworkReply::finished, this, [this, a, interval, reply]() {
        reply->deleteLater();
        Candles c;
        if (reply->error() == QNetworkReply::NoError) c = parseBinance(reply->readAll());
        emit olderReady(a.symbol, interval, c);
    });
}

void MarketData::fetchBinance(const Asset& a, const QString& interval) {
    const bool daily = interval == "1d";
    auto* first = net_.get(binanceRequest(a, interval, 0));
    connect(first, &QNetworkReply::finished, this, [this, a, interval, daily, first]() {
        first->deleteLater();
        if (first->error() != QNetworkReply::NoError) {
            emit historyReady(a.symbol, interval, {}, "error");
            return;
        }
        Candles newest = parseBinance(first->readAll());
        if (newest.size() < 2) {
            emit historyReady(a.symbol, interval, {}, "error");
            return;
        }
        if (!daily || newest.size() < 900) { // other intervals: one page, older pages on demand
            emit historyReady(a.symbol, interval, newest, "network");
            return;
        }
        // Daily: a second page for ~2000 bars, needed by the forecast/backtest.
        auto* second = net_.get(binanceRequest(a, interval, static_cast<qint64>(newest.front().time) * 1000 - 1));
        connect(second, &QNetworkReply::finished, this, [this, a, interval, second, newest]() mutable {
            second->deleteLater();
            if (second->error() == QNetworkReply::NoError) {
                Candles older = parseBinance(second->readAll());
                older.insert(older.end(), newest.begin(), newest.end());
                newest = std::move(older);
            }
            saveCache(a, newest);
            emit historyReady(a.symbol, interval, newest, "network");
        });
    });
}

void MarketData::fetchYahoo(const Asset& a, const QString& interval) {
    struct Spec {
        const char *range, *iv;
    };
    Spec s{"10y", "1d"};
    if (interval == "1m") s = {"7d", "1m"};
    else if (interval == "5m") s = {"60d", "5m"};
    else if (interval == "15m") s = {"60d", "15m"};
    else if (interval == "1h" || interval == "4h") s = {"730d", "60m"};
    else if (interval == "1w") s = {"10y", "1wk"};
    else if (interval == "1M") s = {"max", "1mo"};

    QUrl u("https://query1.finance.yahoo.com/v8/finance/chart/" + QString::fromUtf8(QUrl::toPercentEncoding(a.yahoo)));
    QUrlQuery q;
    q.addQueryItem("range", s.range);
    q.addQueryItem("interval", s.iv);
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    req.setTransferTimeout(20000);
    auto* reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, a, interval, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit historyReady(a.symbol, interval, {}, "error");
            return;
        }
        Candles c = parseYahoo(reply->readAll());
        if (interval == "4h") c = resample(c, 4 * 3600);
        if (c.size() < 2) {
            emit historyReady(a.symbol, interval, {}, "error");
            return;
        }
        if (interval == "1d" && c.size() >= 60) saveCache(a, c);
        emit historyReady(a.symbol, interval, c, "network");
    });
}

} // namespace ql
