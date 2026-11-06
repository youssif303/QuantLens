#pragma once

#include "Asset.h"
#include "core/Types.h"

#include <QNetworkAccessManager>
#include <QObject>

namespace ql {

// Loads candles for any interval. Daily data: bundled sample / disk cache first (instant UI),
// then the network (Binance for crypto, Yahoo chart endpoint for stocks). Other intervals come
// from the network only. Older history for crypto can be paged in on demand.
class MarketData : public QObject {
    Q_OBJECT
public:
    explicit MarketData(QObject* parent = nullptr);
    void setOffline(bool off) { offline_ = off; }
    void load(const Asset& asset, const QString& interval = "1d");
    void loadOlder(const Asset& asset, const QString& interval, qint64 beforeSec);

    static Candles parseBinance(const QByteArray& json);
    static Candles parseYahoo(const QByteArray& json);

signals:
    // source: "network", "cache", "sample", "none" or "error"
    void historyReady(const QString& symbol, const QString& interval, const ql::Candles& candles, const QString& source);
    void olderReady(const QString& symbol, const QString& interval, const ql::Candles& candles);

private:
    bool loadLocal(const Asset& a);
    void fetchBinance(const Asset& a, const QString& interval);
    void fetchYahoo(const Asset& a, const QString& interval);
    QNetworkRequest binanceRequest(const Asset& a, const QString& interval, qint64 endTimeMs) const;
    void saveCache(const Asset& a, const Candles& c) const;
    QString cachePath(const Asset& a) const;

    QNetworkAccessManager net_;
    bool offline_{false};
};

} // namespace ql
