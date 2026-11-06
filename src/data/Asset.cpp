#include "Asset.h"

namespace ql {

const QList<Asset>& catalog() {
    static const QList<Asset> c = {
        {"BTC", "Bitcoin", true, "BTCUSDT", "BTC-USD", "BTC"},
        {"ETH", "Ethereum", true, "ETHUSDT", "ETH-USD", "ETH"},
        {"SOL", "Solana", true, "SOLUSDT", "SOL-USD", "SOL"},
        {"BNB", "BNB", true, "BNBUSDT", "BNB-USD", ""},
        {"XRP", "XRP", true, "XRPUSDT", "XRP-USD", ""},
        {"ADA", "Cardano", true, "ADAUSDT", "ADA-USD", ""},
        {"DOGE", "Dogecoin", true, "DOGEUSDT", "DOGE-USD", ""},
        {"SPY", "SPDR S&P 500 ETF", false, "", "SPY", "SPY"},
        {"QQQ", "Invesco QQQ (Nasdaq 100)", false, "", "QQQ", "QQQ"},
        {"^GSPC", "S&P 500 Index", false, "", "^GSPC", "GSPC"},
        {"^IXIC", "Nasdaq Composite", false, "", "^IXIC", ""},
        {"^GDAXI", "DAX Performance Index", false, "", "^GDAXI", ""},
        {"VOO", "Vanguard S&P 500 ETF", false, "", "VOO", ""},
        {"VTI", "Vanguard Total Stock Market ETF", false, "", "VTI", ""},
        {"AAPL", "Apple", false, "", "AAPL", "AAPL"},
        {"MSFT", "Microsoft", false, "", "MSFT", ""},
        {"NVDA", "NVIDIA", false, "", "NVDA", "NVDA"},
        {"TSLA", "Tesla", false, "", "TSLA", ""},
        {"AMZN", "Amazon", false, "", "AMZN", ""},
        {"GOOGL", "Alphabet", false, "", "GOOGL", ""},
        {"META", "Meta Platforms", false, "", "META", ""},
        {"BMW.DE", "BMW", false, "", "BMW.DE", ""},
        {"VOW3.DE", "Volkswagen", false, "", "VOW3.DE", ""},
        {"SAP", "SAP", false, "", "SAP", ""},
    };
    return c;
}

const QList<IntervalInfo>& intervals() {
    static const QList<IntervalInfo> l = {
        {"1s", "1s", 1},    {"1m", "1m", 60},      {"5m", "5m", 300},   {"15m", "15m", 900},
        {"1h", "1h", 3600}, {"4h", "4h", 14400},   {"1d", "1D", 86400}, {"1w", "1W", 604800},
        {"1M", "1M", 2592000},
    };
    return l;
}

qint64 intervalSecs(const QString& id) {
    for (const auto& i : intervals())
        if (i.id == id) return i.secs;
    return 86400;
}

const Asset* findAsset(const QString& symbol) {
    for (const auto& a : catalog())
        if (a.symbol.compare(symbol, Qt::CaseInsensitive) == 0) return &a;
    return nullptr;
}

QList<Asset> searchAssets(const QString& query) {
    QList<Asset> out;
    const QString q = query.trimmed();
    if (q.isEmpty()) return out;
    for (const auto& a : catalog())
        if (a.symbol.contains(q, Qt::CaseInsensitive) || a.name.contains(q, Qt::CaseInsensitive)) out.append(a);
    return out;
}

QStringList defaultWatchlist() {
    return {"BTC", "ETH", "SOL", "SPY", "QQQ", "^GSPC", "AAPL", "NVDA"};
}

} // namespace ql
