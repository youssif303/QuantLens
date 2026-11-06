#pragma once

#include "Asset.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QWebSocket>

namespace ql {

// Live prices. Crypto: Binance public WebSocket (real-time).
// Stocks/ETFs/indices: free sources have no tick stream, so the Yahoo chart endpoint
// is polled and the UI labels it DELAYED.
class LiveFeed : public QObject {
    Q_OBJECT
public:
    explicit LiveFeed(QObject* parent = nullptr);
    void setSymbols(const QStringList& symbols);
    void setEnabled(bool on);
    bool cryptoConnected() const { return wsConnected_; }

signals:
    // reference = 24h-ago open (crypto) or previous close (stocks)
    void tick(const QString& symbol, double price, double reference, bool realtime);
    void connectionChanged();

private slots:
    void onText(const QString& text);
    void pollNext();

private:
    void reconnect();
    void scheduleReconnect();

    QWebSocket ws_;
    QNetworkAccessManager net_;
    QTimer pollTimer_;
    QTimer retryTimer_;
    QStringList crypto_, stocks_;
    int pollIndex_{0};
    int backoffMs_{1000};
    bool enabled_{true};
    bool wsConnected_{false};
    bool intentionalClose_{false};
};

} // namespace ql
