#pragma once

#include "WatchlistModel.h"
#include "core/Backtest.h"
#include "core/Forecast.h"
#include "data/LiveFeed.h"
#include "data/MarketData.h"

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace ql {

struct AnalysisResult {
    QString symbol;
    Forecast shortF, longF;
    BacktestResult shortB, longB;
};

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* watchlist READ watchlist CONSTANT)
    Q_PROPERTY(QString selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(QString name READ name NOTIFY selectedChanged)
    Q_PROPERTY(double price READ price NOTIFY priceChanged)
    Q_PROPERTY(double changePct READ changePct NOTIFY priceChanged)
    Q_PROPERTY(QString status READ status NOTIFY priceChanged)
    Q_PROPERTY(QString lastUpdate READ lastUpdate NOTIFY priceChanged)
    Q_PROPERTY(QString dataSource READ dataSource NOTIFY selectedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY analysisChanged)
    Q_PROPERTY(QVariantMap shortOutlook READ shortOutlook NOTIFY analysisChanged)
    Q_PROPERTY(QVariantMap longOutlook READ longOutlook NOTIFY analysisChanged)
    Q_PROPERTY(QVariantMap shortBacktest READ shortBacktest NOTIFY analysisChanged)
    Q_PROPERTY(QVariantMap longBacktest READ longBacktest NOTIFY analysisChanged)
public:
    AppController(bool offline, QObject* parent = nullptr);

    QObject* watchlist() { return &model_; }
    QString selected() const { return selected_; }
    QString name() const;
    double price() const;
    double changePct() const;
    QString status() const;
    QString lastUpdate() const { return lastUpdate_; }
    QString dataSource() const { return source_.value(selected_); }
    bool busy() const { return busy_; }
    QVariantMap shortOutlook() const { return shortOut_; }
    QVariantMap longOutlook() const { return longOut_; }
    QVariantMap shortBacktest() const { return shortBt_; }
    QVariantMap longBacktest() const { return longBt_; }

    // Chart access (read by ChartItem).
    const Candles& candles() const;
    const Forecast& shortForecast() const { return result_.shortF; }
    const Forecast& longForecast() const { return result_.longF; }
    bool crypto() const;

    Q_INVOKABLE void select(const QString& symbol);
    Q_INVOKABLE void addAsset(const QString& symbol);
    Q_INVOKABLE void removeAsset(const QString& symbol);
    Q_INVOKABLE QVariantList search(const QString& query) const;

signals:
    void selectedChanged();
    void priceChanged();
    void analysisChanged();
    void chartChanged();

private:
    void onHistory(const QString& symbol, const Candles& c, const QString& source);
    void onTick(const QString& symbol, double price, double reference, bool realtime);
    void analyze();
    void saveWatchlist();
    void syncFeed();
    static QVariantMap toMap(const Forecast& f, const QString& label);
    static QVariantMap toMap(const BacktestResult& b);

    WatchlistModel model_;
    MarketData data_;
    LiveFeed feed_;
    QHash<QString, Candles> history_;
    QHash<QString, QString> source_;
    QString selected_;
    QString lastUpdate_{"-"};
    AnalysisResult result_;
    QFutureWatcher<AnalysisResult> watcher_;
    bool busy_{false};
    bool rerun_{false};
    QTimer chartTimer_;
    bool chartDirty_{false};
    QVariantMap shortOut_, longOut_, shortBt_, longBt_;
};

} // namespace ql
