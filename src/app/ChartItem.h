#pragma once

#include <QPointF>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

namespace ql {

class AppController;

class ChartItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(int bars READ bars WRITE setBars NOTIFY optionsChanged)
    Q_PROPERTY(bool candles READ candles WRITE setCandles NOTIFY optionsChanged)
    Q_PROPERTY(bool showSma READ showSma WRITE setShowSma NOTIFY optionsChanged)
    Q_PROPERTY(bool showBands READ showBands WRITE setShowBands NOTIFY optionsChanged)
    Q_PROPERTY(bool showRsi READ showRsi WRITE setShowRsi NOTIFY optionsChanged)
    Q_PROPERTY(bool showCone READ showCone WRITE setShowCone NOTIFY optionsChanged)
    Q_PROPERTY(bool longCone READ longCone WRITE setLongCone NOTIFY optionsChanged)
public:
    explicit ChartItem(QQuickItem* parent = nullptr);
    void paint(QPainter* p) override;

    QObject* controller() const;
    void setController(QObject* c);
    int bars() const { return bars_; }
    void setBars(int v) { set(bars_, v); }
    bool candles() const { return candles_; }
    void setCandles(bool v) { set(candles_, v); }
    bool showSma() const { return sma_; }
    void setShowSma(bool v) { set(sma_, v); }
    bool showBands() const { return bands_; }
    void setShowBands(bool v) { set(bands_, v); }
    bool showRsi() const { return rsi_; }
    void setShowRsi(bool v) { set(rsi_, v); }
    bool showCone() const { return cone_; }
    void setShowCone(bool v) { set(cone_, v); }
    bool longCone() const { return longCone_; }
    void setLongCone(bool v) { set(longCone_, v); }

signals:
    void controllerChanged();
    void optionsChanged();

protected:
    void hoverMoveEvent(QHoverEvent* e) override;
    void hoverLeaveEvent(QHoverEvent* e) override;

private:
    template <typename T> void set(T& field, T v) {
        if (field == v) return;
        field = v;
        emit optionsChanged();
        update();
    }

    AppController* ctrl_{nullptr};
    int bars_{180};
    bool candles_{true}, sma_{true}, bands_{false}, rsi_{true}, cone_{true}, longCone_{false};
    QPointF hover_{-1, -1};
};

} // namespace ql
