#pragma once

#include "WatchlistModel.h"
#include "core/Backtest.h"
#include "core/Forecast.h"
#include "data/LiveFeed.h"
#include "data/MarketData.h"

#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
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
    Q_PROPERTY(QString interval READ interval NOTIFY intervalChanged)
    Q_PROPERTY(QStringList intervals READ intervals NOTIFY selectedChanged)
    Q_PROPERTY(bool chartLoading READ chartLoading NOTIFY intervalChanged)
    Q_PROPERTY(QString currency READ currency NOTIFY currencyChanged)
    Q_PROPERTY(double displayFactor READ displayFactor NOTIFY currencyChanged)
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
    QString interval() const { return interval_; }
    QStringList intervals() const;
    bool chartLoading() const { return chartLoading_; }
    qint64 intervalSecs() const { return ql::intervalSecs(interval_); }
    const Candles& dailyCandles() const;
    int coneBars(bool longHorizon) const;

    QString currency() const { return currency_; }
    double displayFactor() const { return factor(selected_); }
    // Multiplier from the asset's quote currency (USD) to the display currency; indices stay in points.
    Q_INVOKABLE double factor(const QString& symbol) const;
    Q_INVOKABLE void setCurrency(const QString& c);
    Q_INVOKABLE void setInterval(const QString& id);
    Q_INVOKABLE void requestOlder();
    Q_INVOKABLE void select(const QString& symbol);
    Q_INVOKABLE void addAsset(const QString& symbol);
    Q_INVOKABLE void removeAsset(const QString& symbol);
    Q_INVOKABLE QVariantList search(const QString& query) const;

signals:
    void selectedChanged();
    void priceChanged();
    void analysisChanged();
    void chartChanged();
    void intervalChanged();
    void currencyChanged();
    void candlesPrepended(int n);
    void chartReset();

private:
    void onHistory(const QString& symbol, const QString& interval, const Candles& c, const QString& source);
    void onOlder(const QString& symbol, const QString& interval, const Candles& c);
    void applyTick(Candles& c, qint64 secs, bool realtime, double price, bool intraday);
    QString chartKey(const QString& symbol, const QString& interval) const { return symbol + "|" + interval; }
    void loadChart();
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
    QHash<QString, Candles> chart_;
    QHash<QString, QString> source_;
    QString interval_{"1d"};
    QString currency_{"USD"};
    double eurUsd_{1.08}; // USD per EUR; refreshed from Binance EURUSDT
    QNetworkAccessManager fxNet_;
    void refreshFx();
    bool chartLoading_{false};
    bool loadingOlder_{false};
    QSet<QString> exhausted_;
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
