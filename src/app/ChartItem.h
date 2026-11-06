#pragma once

#include <QPointF>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

#include <algorithm>

namespace ql {

class AppController;

// Custom-painted market chart with its own view model: the visible window is a (possibly
// fractional) start index plus a bar count, so zooming and panning are plain arithmetic.
class ChartItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(bool candles READ candles WRITE setCandles NOTIFY optionsChanged)
    Q_PROPERTY(bool showSma READ showSma WRITE setShowSma NOTIFY optionsChanged)
    Q_PROPERTY(bool showBands READ showBands WRITE setShowBands NOTIFY optionsChanged)
    Q_PROPERTY(bool showRsi READ showRsi WRITE setShowRsi NOTIFY optionsChanged)
    Q_PROPERTY(bool showMacd READ showMacd WRITE setShowMacd NOTIFY optionsChanged)
    Q_PROPERTY(bool showVolume READ showVolume WRITE setShowVolume NOTIFY optionsChanged)
    Q_PROPERTY(bool logScale READ logScale WRITE setLogScale NOTIFY optionsChanged)
    Q_PROPERTY(bool showCone READ showCone WRITE setShowCone NOTIFY optionsChanged)
    Q_PROPERTY(bool longCone READ longCone WRITE setLongCone NOTIFY optionsChanged)
public:
    explicit ChartItem(QQuickItem* parent = nullptr);
    void paint(QPainter* p) override;

    QObject* controller() const;
    void setController(QObject* c);
    bool candles() const { return candles_; }
    void setCandles(bool v) { set(candles_, v); }
    bool showSma() const { return sma_; }
    void setShowSma(bool v) { set(sma_, v); }
    bool showBands() const { return bands_; }
    void setShowBands(bool v) { set(bands_, v); }
    bool showRsi() const { return rsi_; }
    void setShowRsi(bool v) { set(rsi_, v); }
    bool showMacd() const { return macd_; }
    void setShowMacd(bool v) { set(macd_, v); }
    bool showVolume() const { return volume_; }
    void setShowVolume(bool v) { set(volume_, v); }
    bool logScale() const { return log_; }
    void setLogScale(bool v) { set(log_, v); }
    bool showCone() const { return cone_; }
    void setShowCone(bool v) {
        if (set(cone_, v)) resetView();
    }
    bool longCone() const { return longCone_; }
    void setLongCone(bool v) {
        if (set(longCone_, v)) resetView();
    }

    Q_INVOKABLE void zoom(double factor);       // factor > 1 zooms in
    Q_INVOKABLE void resetView();               // default window for the current interval
    Q_INVOKABLE void showRange(double seconds); // last N seconds; 0 = everything loaded

signals:
    void controllerChanged();
    void optionsChanged();

protected:
    void hoverMoveEvent(QHoverEvent* e) override;
    void hoverLeaveEvent(QHoverEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    template <typename T> bool set(T& field, T v) {
        if (field == v) return false;
        field = v;
        emit optionsChanged();
        update();
        return true;
    }
    int total() const;
    double rightPad() const;
    bool coneActive() const;
    void clampView();
    void onDataChanged();
    void zoomAt(double factor, double anchorFrac);
    double plotWidth() const { return std::max(1.0, width() - 8 - 74); }

    AppController* ctrl_{nullptr};
    bool candles_{true}, sma_{true}, bands_{false}, rsi_{true}, macd_{false}, volume_{true}, log_{false},
        cone_{true}, longCone_{false};
    QPointF hover_{-1, -1};

    double viewStart_{0}, vis_{100};
    bool pendingReset_{true};
    long long firstTime_{0};
    int lastCount_{0};
    double lastPad_{3};
    double wantRange_{0};
    bool dragging_{false};
    QPointF dragLast_;
};

} // namespace ql
