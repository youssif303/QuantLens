#pragma once

#include <QList>
#include <QString>

namespace ql {

struct Asset {
    QString symbol;     // display symbol, e.g. "BTC", "^GSPC"
    QString name;
    bool crypto{false};
    QString binance;    // e.g. "BTCUSDT" (crypto only)
    QString yahoo;      // e.g. "SPY", "^GSPC"
    QString sampleFile; // bundled demo CSV name (without extension)

    double periodsPerYear() const { return crypto ? 365.0 : 252.0; }
    int shortHorizon() const { return crypto ? 7 : 5; }
    int longHorizon() const { return crypto ? 90 : 63; }
    QString shortLabel() const { return crypto ? "Next 7 days" : "Next 5 trading days"; }
    QString longLabel() const { return crypto ? "Next 3 months" : "Next 3 months"; }
};

const QList<Asset>& catalog();
const Asset* findAsset(const QString& symbol);
QList<Asset> searchAssets(const QString& query);
QStringList defaultWatchlist();

} // namespace ql
