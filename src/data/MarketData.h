#pragma once

#include "Asset.h"
#include "core/Types.h"

#include <QNetworkAccessManager>
#include <QObject>

namespace ql {

// Loads daily candles. Order: bundled sample / disk cache first (instant UI),
// then the network (Binance for crypto, Yahoo chart endpoint for stocks); a successful
// download replaces the data and refreshes the cache.
class MarketData : public QObject {
    Q_OBJECT
public:
    explicit MarketData(QObject* parent = nullptr);
    void setOffline(bool off) { offline_ = off; }
    void load(const Asset& asset);

    static Candles parseBinance(const QByteArray& json);
    static Candles parseYahoo(const QByteArray& json);

signals:
    // source: "network", "cache" or "sample"
    void historyReady(const QString& symbol, const ql::Candles& candles, const QString& source);

private:
    bool loadLocal(const Asset& a);
    void fetchBinance(const Asset& a);
    void fetchYahoo(const Asset& a);
    QString cachePath(const Asset& a) const;

    QNetworkAccessManager net_;
    bool offline_{false};
};

} // namespace ql
