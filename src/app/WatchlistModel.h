#pragma once

#include <QAbstractListModel>
#include <QVariantList>

namespace ql {

class WatchlistModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { SymbolRole = Qt::UserRole + 1, NameRole, PriceRole, ChangeRole, StatusRole, SparkRole, TickSeqRole, TickDirRole, CryptoRole };

    struct Item {
        QString symbol, name, status{"..."};
        double price{0}, reference{0};
        QVariantList spark;
        int tickSeq{0}, tickDir{0};
        bool crypto{false};
    };

    explicit WatchlistModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& = {}) const override { return items_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSymbols(const QStringList& symbols);
    QStringList symbols() const;
    bool contains(const QString& symbol) const { return indexOf(symbol) >= 0; }
    int indexOf(const QString& symbol) const;
    const Item* item(const QString& symbol) const;

    void setHistory(const QString& symbol, const QVariantList& spark, double lastClose, double prevClose, const QString& status);
    void setTick(const QString& symbol, double price, double reference, const QString& status);
    void setStatus(const QString& symbol, const QString& status);

private:
    QList<Item> items_;
};

} // namespace ql
