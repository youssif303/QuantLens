#include "WatchlistModel.h"

#include "data/Asset.h"

namespace ql {

QHash<int, QByteArray> WatchlistModel::roleNames() const {
    return {{SymbolRole, "symbol"}, {NameRole, "name"},     {PriceRole, "price"},     {ChangeRole, "changePct"},
            {StatusRole, "status"}, {SparkRole, "spark"},   {TickSeqRole, "tickSeq"}, {TickDirRole, "tickDir"},
            {CryptoRole, "crypto"}};
}

QVariant WatchlistModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= items_.size()) return {};
    const Item& it = items_[idx.row()];
    switch (role) {
    case SymbolRole: return it.symbol;
    case NameRole: return it.name;
    case PriceRole: return it.price;
    case ChangeRole: return it.reference > 0 ? (it.price / it.reference - 1.0) * 100.0 : 0.0;
    case StatusRole: return it.status;
    case SparkRole: return it.spark;
    case TickSeqRole: return it.tickSeq;
    case TickDirRole: return it.tickDir;
    case CryptoRole: return it.crypto;
    default: return {};
    }
}

int WatchlistModel::indexOf(const QString& symbol) const {
    for (int i = 0; i < items_.size(); ++i)
        if (items_[i].symbol == symbol) return i;
    return -1;
}

const WatchlistModel::Item* WatchlistModel::item(const QString& symbol) const {
    const int i = indexOf(symbol);
    return i < 0 ? nullptr : &items_[i];
}

void WatchlistModel::setSymbols(const QStringList& symbols) {
    beginResetModel();
    items_.clear();
    for (const auto& s : symbols) {
        const Asset* a = findAsset(s);
        if (!a) continue;
        Item it;
        it.symbol = a->symbol;
        it.name = a->name;
        it.crypto = a->crypto;
        items_.append(it);
    }
    endResetModel();
}

QStringList WatchlistModel::symbols() const {
    QStringList l;
    for (const auto& i : items_) l << i.symbol;
    return l;
}

void WatchlistModel::setHistory(const QString& symbol, const QVariantList& spark, double lastClose, double prevClose, const QString& status) {
    const int i = indexOf(symbol);
    if (i < 0) return;
    Item& it = items_[i];
    it.spark = spark;
    // A live tick is fresher than the daily history; keep it.
    if (it.status != "LIVE" && it.status != "DELAYED") {
        it.price = lastClose;
        it.reference = prevClose;
        it.status = status;
    }
    emit dataChanged(index(i), index(i));
}

void WatchlistModel::setTick(const QString& symbol, double price, double reference, const QString& status) {
    const int i = indexOf(symbol);
    if (i < 0) return;
    Item& it = items_[i];
    it.tickDir = price > it.price ? 1 : (price < it.price ? -1 : 0);
    if (it.tickDir != 0) ++it.tickSeq;
    it.price = price;
    if (reference > 0) it.reference = reference;
    it.status = status;
    emit dataChanged(index(i), index(i));
}

void WatchlistModel::setStatus(const QString& symbol, const QString& status) {
    const int i = indexOf(symbol);
    if (i < 0 || items_[i].status == status) return;
    items_[i].status = status;
    emit dataChanged(index(i), index(i), {StatusRole});
}

} // namespace ql
