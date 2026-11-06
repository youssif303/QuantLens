#include "MarketData.h"

#include "core/Csv.h"

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
    emit historyReady(a.symbol, c, src);
    return true;
}

void MarketData::load(const Asset& a) {
    const bool haveLocal = loadLocal(a);
    if (offline_) {
        if (!haveLocal) emit historyReady(a.symbol, {}, "none");
        return;
    }
    if (a.crypto) fetchBinance(a);
    else fetchYahoo(a);
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

void MarketData::fetchBinance(const Asset& a) {
    // Up to ~3000 daily bars via two pages (newest first, then older).
    auto base = [&](qint64 endTime) {
        QUrl u("https://data-api.binance.vision/api/v3/klines");
        QUrlQuery q;
        q.addQueryItem("symbol", a.binance);
        q.addQueryItem("interval", "1d");
        q.addQueryItem("limit", "1000");
        if (endTime > 0) q.addQueryItem("endTime", QString::number(endTime));
        u.setQuery(q);
        return QNetworkRequest(u);
    };
    auto* first = net_.get(base(0));
    connect(first, &QNetworkReply::finished, this, [this, a, first, base]() {
        first->deleteLater();
        if (first->error() != QNetworkReply::NoError) return;
        Candles newest = parseBinance(first->readAll());
        if (newest.size() < 60) return;
        const qint64 endTime = static_cast<qint64>(newest.front().time) * 1000 - 1;
        auto* second = net_.get(base(endTime));
        connect(second, &QNetworkReply::finished, this, [this, a, second, newest]() mutable {
            second->deleteLater();
            if (second->error() == QNetworkReply::NoError) {
                Candles older = parseBinance(second->readAll());
                older.insert(older.end(), newest.begin(), newest.end());
                newest = std::move(older);
            }
            QFile f(cachePath(a));
            if (f.open(QIODevice::WriteOnly)) f.write(QByteArray::fromStdString(csv::toString(newest)));
            emit historyReady(a.symbol, newest, "network");
        });
    });
}

void MarketData::fetchYahoo(const Asset& a) {
    QUrl u("https://query1.finance.yahoo.com/v8/finance/chart/" + QString::fromUtf8(QUrl::toPercentEncoding(a.yahoo)));
    QUrlQuery q;
    q.addQueryItem("range", "5y");
    q.addQueryItem("interval", "1d");
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    auto* reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, a, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        const Candles c = parseYahoo(reply->readAll());
        if (c.size() < 60) return;
        QFile f(cachePath(a));
        if (f.open(QIODevice::WriteOnly)) f.write(QByteArray::fromStdString(csv::toString(c)));
        emit historyReady(a.symbol, c, "network");
    });
}

} // namespace ql
