#include "ChartItem.h"

#include "AppController.h"
#include "core/Indicators.h"

#include <QCursor>
#include <QDateTime>
#include <QFontMetricsF>
#include <QHoverEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace ql {

namespace {
const QColor kBg("#0E1621"), kGrid("#1D2A3B"), kText("#8B9BB0"), kUp("#26C281"), kDown("#F0555A"),
    kAccent("#4DA3FF"), kSma20("#F5B84D"), kSma50("#B388FF");
constexpr double kLeft = 8, kRight = 74, kTop = 10, kAxisH = 20;

double g_k = 1.0; // display-currency factor, set at the start of each paint

QString fmtPrice(double v) {
    v *= g_k;
    const double a = std::abs(v);
    if (a >= 1000) return QString::number(v, 'f', 0);
    if (a >= 10) return QString::number(v, 'f', 2);
    return QString::number(v, 'f', 4);
}

QString fmtVolume(double v) {
    if (v >= 1e9) return QString::number(v / 1e9, 'f', 2) + "B";
    if (v >= 1e6) return QString::number(v / 1e6, 'f', 2) + "M";
    if (v >= 1e3) return QString::number(v / 1e3, 'f', 1) + "K";
    return QString::number(v, 'f', 0);
}

// Default visible window per interval, in seconds (0 = everything loaded).
double defaultRange(qint64 secs) {
    switch (secs) {
    case 1: return 150;
    case 60: return 2 * 3600;
    case 300: return 12 * 3600;
    case 900: return 36 * 3600;
    case 3600: return 6 * 86400;
    case 14400: return 24 * 86400;
    case 86400: return 120 * 86400;
    case 604800: return 2 * 365 * 86400.0;
    default: return 0;
    }
}
} // namespace

ChartItem::ChartItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAntialiasing(true);
    setCursor(Qt::CrossCursor);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

QObject* ChartItem::controller() const { return ctrl_; }

void ChartItem::setController(QObject* c) {
    auto* ac = qobject_cast<AppController*>(c);
    if (ac == ctrl_) return;
    ctrl_ = ac;
    if (ctrl_) {
        connect(ctrl_, &AppController::chartChanged, this, &ChartItem::onDataChanged);
        connect(ctrl_, &AppController::chartReset, this, [this]() { pendingReset_ = true; });
    }
    emit controllerChanged();
    update();
}

int ChartItem::total() const { return ctrl_ ? static_cast<int>(ctrl_->candles().size()) : 0; }

bool ChartItem::coneActive() const {
    if (!ctrl_ || !cone_ || ctrl_->interval() != "1d") return false;
    return (longCone_ ? ctrl_->longForecast() : ctrl_->shortForecast()).valid;
}

double ChartItem::rightPad() const {
    if (!coneActive()) return 3;
    const Forecast& fc = longCone_ ? ctrl_->longForecast() : ctrl_->shortForecast();
    return static_cast<double>(fc.cone.size()) + 2;
}

void ChartItem::clampView() {
    const double n = total(), pad = rightPad();
    vis_ = std::clamp(vis_, 8.0, n * 1.3 + pad + 5);
    const double lo = -vis_ * 0.5;
    const double hi = std::max(lo, n + pad - vis_ * 0.5);
    viewStart_ = std::clamp(viewStart_, lo, hi);
}

void ChartItem::showRange(double seconds) {
    if (!ctrl_) return;
    const Candles& c = ctrl_->candles();
    const int n = static_cast<int>(c.size());
    if (n == 0) return;
    const double pad = rightPad();
    wantRange_ = seconds;
    firstTime_ = c.front().time;
    lastCount_ = n;
    lastPad_ = pad;
    if (seconds <= 0) {
        vis_ = n + pad;
        viewStart_ = 0;
    } else {
        const long long cutoff = c.back().time - static_cast<long long>(seconds);
        const auto it = std::lower_bound(c.begin(), c.end(), cutoff, [](const Candle& k, long long t) { return k.time < t; });
        const int cnt = n - static_cast<int>(it - c.begin());
        vis_ = std::max(8.0, cnt + pad);
        viewStart_ = n + pad - vis_;
        if (c.front().time > cutoff) ctrl_->requestOlder();
    }
    clampView();
    update();
}

void ChartItem::resetView() {
    if (ctrl_) showRange(defaultRange(ctrl_->intervalSecs()));
}

void ChartItem::onDataChanged() {
    if (!ctrl_) return;
    const Candles& c = ctrl_->candles();
    const int n = static_cast<int>(c.size());
    if (n == 0) {
        update();
        return;
    }
    if (pendingReset_) {
        pendingReset_ = false;
        resetView();
        return;
    }
    const double pad = rightPad();
    const bool following = viewStart_ + vis_ >= lastCount_ + lastPad_ - 1.5;
    if (c.front().time != firstTime_) {
        const auto it = std::lower_bound(c.begin(), c.end(), firstTime_, [](const Candle& k, long long t) { return k.time < t; });
        viewStart_ += static_cast<double>(it - c.begin());
        firstTime_ = c.front().time;
        lastCount_ = n;
        if (wantRange_ > 0) {
            showRange(wantRange_);
            return;
        }
    } else if (following && n > lastCount_) {
        viewStart_ += n - lastCount_;
    }
    if (pad != lastPad_) {
        vis_ += pad - lastPad_;
        lastPad_ = pad;
    }
    lastCount_ = n;
    clampView();
    if (viewStart_ < 20) ctrl_->requestOlder();
    update();
}

void ChartItem::zoomAt(double factor, double anchorFrac) {
    const double anchorIdx = viewStart_ + anchorFrac * vis_;
    vis_ /= factor;
    viewStart_ = anchorIdx - anchorFrac * vis_;
    wantRange_ = 0;
    clampView();
    if (viewStart_ < 20 && ctrl_) ctrl_->requestOlder();
    update();
}

void ChartItem::zoom(double factor) { zoomAt(factor, 0.5); }

void ChartItem::wheelEvent(QWheelEvent* e) {
    const double frac = std::clamp((e->position().x() - kLeft) / plotWidth(), 0.0, 1.0);
    zoomAt(std::pow(1.0015, e->angleDelta().y()), frac);
    e->accept();
}

void ChartItem::mousePressEvent(QMouseEvent* e) {
    dragging_ = true;
    dragLast_ = e->position();
    setCursor(Qt::ClosedHandCursor);
    e->accept();
}

void ChartItem::mouseMoveEvent(QMouseEvent* e) {
    hover_ = e->position();
    if (dragging_) {
        viewStart_ -= (e->position().x() - dragLast_.x()) / plotWidth() * vis_;
        dragLast_ = e->position();
        wantRange_ = 0;
        clampView();
        if (viewStart_ < 20 && ctrl_) ctrl_->requestOlder();
    }
    update();
}

void ChartItem::mouseReleaseEvent(QMouseEvent*) {
    dragging_ = false;
    setCursor(Qt::CrossCursor);
}

void ChartItem::mouseDoubleClickEvent(QMouseEvent*) { resetView(); }

void ChartItem::hoverMoveEvent(QHoverEvent* e) {
    hover_ = e->position();
    update();
}

void ChartItem::hoverLeaveEvent(QHoverEvent*) {
    hover_ = {-1, -1};
    update();
}

void ChartItem::paint(QPainter* p) {
    p->fillRect(boundingRect(), kBg);
    if (!ctrl_) return;
    g_k = ctrl_->displayFactor();
    const Candles& all = ctrl_->candles();
    const int n = static_cast<int>(all.size());
    if (n < 2) {
        p->setPen(kText);
        p->drawText(boundingRect(), Qt::AlignCenter, ctrl_->chartLoading() ? "Loading market data..." : "No data for this interval");
        return;
    }
    p->setRenderHint(QPainter::Antialiasing, true);

    const qint64 secs = ctrl_->intervalSecs();
    const bool intraday = secs < 86400;
    const bool drawCone = coneActive();
    const Forecast& fc = longCone_ ? ctrl_->longForecast() : ctrl_->shortForecast();
    const int horizon = drawCone ? static_cast<int>(fc.cone.size()) : 0;

    const double W = width(), H = height();
    const double plotW = W - kLeft - kRight;
    const int panes = (rsi_ ? 1 : 0) + (macd_ ? 1 : 0);
    double paneH = std::min(80.0, H * 0.16);
    double priceH = H - kTop - kAxisH - panes * (paneH + 6);
    if (priceH < 90 && panes > 0) {
        paneH = std::max(30.0, (H - kTop - kAxisH - 90) / panes - 6);
        priceH = H - kTop - kAxisH - panes * (paneH + 6);
    }
    if (plotW < 50 || priceH < 40) return;

    const int i0 = std::max(0, static_cast<int>(std::floor(viewStart_)));
    const int i1 = std::min(n - 1, static_cast<int>(std::ceil(viewStart_ + vis_)));
    if (i0 > i1) return;
    const double step = plotW / vis_;
    auto X = [&](double idx) { return kLeft + (idx - viewStart_ + 0.5) / vis_ * plotW; };

    // Indicators are computed on the full history so warm-up values are valid inside the window.
    const Series close = ind::closes(all);
    Series sma20, sma50, rsi;
    ind::Bands bb;
    ind::Macd mc;
    if (sma_) {
        sma20 = ind::sma(close, 20);
        sma50 = ind::sma(close, 50);
    }
    if (bands_) bb = ind::bollinger(close, 20, 2.0);
    if (rsi_) rsi = ind::rsi(close, 14);
    if (macd_) mc = ind::macd(close);

    auto T = [&](double v) { return log_ ? std::log(std::max(v, 1e-9)) : v; };
    auto Tinv = [&](double v) { return log_ ? std::exp(v) : v; };

    double lo = 1e300, hi = -1e300;
    for (int i = i0; i <= i1; ++i) {
        lo = std::min(lo, T(all[i].low));
        hi = std::max(hi, T(all[i].high));
        if (bands_ && !std::isnan(bb.lower[i])) {
            lo = std::min(lo, T(bb.lower[i]));
            hi = std::max(hi, T(bb.upper[i]));
        }
    }
    if (drawCone && viewStart_ + vis_ > n)
        for (const auto& c : fc.cone) {
            lo = std::min(lo, T(c.p10));
            hi = std::max(hi, T(c.p90));
        }
    if (hi <= lo) hi = lo + 1e-6;
    const double pad = (hi - lo) * 0.06;
    lo -= pad;
    hi += pad;
    auto Y = [&](double v) { return kTop + (hi - T(v)) / (hi - lo) * priceH; };

    QFont f = p->font();
    f.setPixelSize(11);
    p->setFont(f);

    // Price grid + axis.
    for (int g = 0; g <= 5; ++g) {
        const double tv = lo + (hi - lo) * g / 5.0;
        const double y = kTop + (hi - tv) / (hi - lo) * priceH;
        p->setPen(QPen(kGrid, 1));
        p->drawLine(QPointF(kLeft, y), QPointF(kLeft + plotW, y));
        p->setPen(kText);
        p->drawText(QRectF(kLeft + plotW + 6, y - 8, kRight - 6, 16), Qt::AlignLeft | Qt::AlignVCenter, fmtPrice(Tinv(tv)));
    }

    // Time axis with labels that adapt to the visible span.
    {
        const qint64 span = all[i1].time - all[i0].time;
        QString fmt;
        if (intraday) fmt = secs < 60 ? "HH:mm:ss" : (span <= 2 * 86400 ? "HH:mm" : "d MMM HH:mm");
        else fmt = span <= 400LL * 86400 ? "d MMM" : (span <= 5 * 366LL * 86400 ? "MMM yyyy" : "yyyy");
        const int ticks = std::max(2, static_cast<int>(plotW / 110));
        const double axisY = H - kAxisH + 3;
        for (int t = 0; t < ticks; ++t) {
            const double x = kLeft + plotW * (t + 0.5) / ticks;
            const int idx = static_cast<int>(std::lround(viewStart_ + (x - kLeft) / plotW * vis_ - 0.5));
            if (idx < 0 || idx >= n) continue;
            const QDateTime dt = intraday ? QDateTime::fromSecsSinceEpoch(all[idx].time)
                                          : QDateTime::fromSecsSinceEpoch(all[idx].time, QTimeZone::UTC);
            p->setPen(QPen(kGrid, 1, Qt::DotLine));
            p->drawLine(QPointF(x, kTop), QPointF(x, H - kAxisH));
            p->setPen(kText);
            p->drawText(QRectF(x - 45, axisY, 90, 16), Qt::AlignCenter, dt.toString(fmt));
        }
    }

    p->save();
    p->setClipRect(QRectF(kLeft, kTop, plotW, priceH));

    // Volume overlay (bottom fifth of the price pane).
    if (volume_) {
        double vmax = 0;
        for (int i = i0; i <= i1; ++i) vmax = std::max(vmax, all[i].volume);
        if (vmax > 0) {
            const double bw = std::max(1.0, std::min(24.0, step * 0.7));
            const double base = kTop + priceH, maxH = priceH * 0.2;
            p->setPen(Qt::NoPen);
            for (int i = i0; i <= i1; ++i) {
                QColor c = all[i].close >= all[i].open ? kUp : kDown;
                c.setAlpha(70);
                p->setBrush(c);
                const double h = all[i].volume / vmax * maxH;
                p->drawRect(QRectF(X(i) - bw / 2, base - h, bw, h));
            }
            p->setBrush(Qt::NoBrush);
        }
    }

    auto seriesPath = [&](const Series& s) {
        QPainterPath path;
        bool started = false;
        for (int i = i0; i <= i1; ++i) {
            if (std::isnan(s[i])) continue;
            const QPointF pt(X(i), Y(s[i]));
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        return path;
    };

    if (bands_) {
        QPainterPath path;
        bool started = false;
        for (int i = i0; i <= i1; ++i) {
            if (std::isnan(bb.upper[i])) continue;
            const QPointF pt(X(i), Y(bb.upper[i]));
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        for (int i = i1; i >= i0; --i)
            if (!std::isnan(bb.lower[i])) path.lineTo(X(i), Y(bb.lower[i]));
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(77, 163, 255, 28));
        p->drawPath(path);
        p->setBrush(Qt::NoBrush);
    }

    if (drawCone) {
        const double cx = X(n - 1), cy = Y(fc.lastPrice);
        auto band = [&](auto get1, auto get2, const QColor& col) {
            QPainterPath path;
            path.moveTo(cx, cy);
            for (int h = 0; h < horizon; ++h) path.lineTo(X(n + h), Y(get1(fc.cone[h])));
            for (int h = horizon - 1; h >= 0; --h) path.lineTo(X(n + h), Y(get2(fc.cone[h])));
            path.closeSubpath();
            p->setPen(Qt::NoPen);
            p->setBrush(col);
            p->drawPath(path);
        };
        band([](const ConeStep& c) { return c.p90; }, [](const ConeStep& c) { return c.p10; }, QColor(77, 163, 255, 36));
        band([](const ConeStep& c) { return c.p75; }, [](const ConeStep& c) { return c.p25; }, QColor(77, 163, 255, 64));
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kAccent, 1.6, Qt::DashLine));
        QPainterPath mid;
        mid.moveTo(cx, cy);
        for (int h = 0; h < horizon; ++h) mid.lineTo(X(n + h), Y(fc.cone[h].p50));
        p->drawPath(mid);
        p->setPen(QPen(QColor(77, 163, 255, 120), 1, Qt::DotLine));
        p->drawLine(QPointF(cx, kTop), QPointF(cx, kTop + priceH));
    }

    // Price series: candles when there is room, otherwise an area line.
    if (candles_ && step >= 3.0) {
        const double cw = std::max(1.0, std::min(24.0, step * 0.7));
        for (int i = i0; i <= i1; ++i) {
            const Candle& c = all[i];
            const double x = X(i);
            const QColor col = c.close >= c.open ? kUp : kDown;
            p->setPen(QPen(col, 1));
            p->drawLine(QPointF(x, Y(c.high)), QPointF(x, Y(c.low)));
            const double yo = Y(c.open), yc = Y(c.close);
            p->setBrush(col);
            p->drawRect(QRectF(x - cw / 2, std::min(yo, yc), cw, std::max(1.0, std::abs(yo - yc))));
        }
        p->setBrush(Qt::NoBrush);
    } else {
        QPainterPath path;
        for (int i = i0; i <= i1; ++i) {
            const QPointF pt(X(i), Y(all[i].close));
            i == i0 ? path.moveTo(pt) : path.lineTo(pt);
        }
        const bool up = all[i1].close >= all[i0].close;
        p->setPen(QPen(up ? kUp : kDown, 1.8));
        p->drawPath(path);
        QPainterPath fill = path;
        fill.lineTo(X(i1), kTop + priceH);
        fill.lineTo(X(i0), kTop + priceH);
        fill.closeSubpath();
        QLinearGradient g(0, kTop, 0, kTop + priceH);
        QColor c0 = up ? kUp : kDown;
        c0.setAlpha(60);
        g.setColorAt(0, c0);
        c0.setAlpha(0);
        g.setColorAt(1, c0);
        p->setPen(Qt::NoPen);
        p->setBrush(g);
        p->drawPath(fill);
        p->setBrush(Qt::NoBrush);
    }

    if (sma_) {
        p->setPen(QPen(kSma20, 1.3));
        p->drawPath(seriesPath(sma20));
        p->setPen(QPen(kSma50, 1.3));
        p->drawPath(seriesPath(sma50));
    }
    if (bands_) {
        p->setPen(QPen(QColor(77, 163, 255, 160), 1.2));
        p->drawPath(seriesPath(bb.upper));
        p->drawPath(seriesPath(bb.lower));
    }
    p->restore();

    // Live price tag.
    {
        const double last = all.back().close;
        const double y = Y(last);
        if (y > kTop && y < kTop + priceH) {
            const QColor col = last >= all[n - 2].close ? kUp : kDown;
            p->setPen(QPen(col, 1, Qt::DashLine));
            p->drawLine(QPointF(kLeft, y), QPointF(kLeft + plotW, y));
            p->setPen(Qt::NoPen);
            p->setBrush(col);
            p->drawRoundedRect(QRectF(kLeft + plotW + 2, y - 9, kRight - 2, 18), 3, 3);
            p->setPen(Qt::white);
            p->drawText(QRectF(kLeft + plotW + 2, y - 9, kRight - 2, 18), Qt::AlignCenter, fmtPrice(last));
            p->setBrush(Qt::NoBrush);
        }
    }

    // Legend.
    {
        double lx = kLeft + 6;
        const double ly = kTop + 14;
        auto legend = [&](const QString& t, const QColor& c) {
            p->setPen(c);
            p->drawText(QPointF(lx, ly), t);
            lx += QFontMetricsF(p->font()).horizontalAdvance(t) + 12;
        };
        if (sma_) {
            legend("SMA20", kSma20);
            legend("SMA50", kSma50);
        }
        if (bands_) legend("Bollinger(20,2)", QColor(77, 163, 255));
        if (drawCone) legend(QString("Forecast %1 bars").arg(horizon), kAccent);
        if (log_) legend("log", kText);
    }

    // Indicator panes.
    double paneTop = kTop + priceH + 6;
    auto paneFrame = [&](double top) {
        p->setPen(QPen(kGrid, 1));
        p->setBrush(Qt::NoBrush);
        p->drawRect(QRectF(kLeft, top, plotW, paneH));
    };
    if (rsi_) {
        const double rtop = paneTop;
        auto RY = [&](double v) { return rtop + (100 - v) / 100.0 * paneH; };
        paneFrame(rtop);
        p->save();
        p->setClipRect(QRectF(kLeft, rtop, plotW, paneH));
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 10));
        p->drawRect(QRectF(kLeft, RY(70), plotW, RY(30) - RY(70)));
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kGrid, 1, Qt::DashLine));
        p->drawLine(QPointF(kLeft, RY(70)), QPointF(kLeft + plotW, RY(70)));
        p->drawLine(QPointF(kLeft, RY(30)), QPointF(kLeft + plotW, RY(30)));
        QPainterPath path;
        bool started = false;
        for (int i = i0; i <= i1; ++i) {
            if (std::isnan(rsi[i])) continue;
            const QPointF pt(X(i), RY(rsi[i]));
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        p->setPen(QPen(QColor("#C792EA"), 1.4));
        p->drawPath(path);
        p->restore();
        p->setPen(kText);
        p->drawText(QRectF(kLeft + plotW + 6, RY(70) - 8, kRight, 16), Qt::AlignLeft, "70");
        p->drawText(QRectF(kLeft + plotW + 6, RY(30) - 8, kRight, 16), Qt::AlignLeft, "30");
        const double lastRsi = std::isnan(rsi.back()) ? 0 : rsi.back();
        p->drawText(QPointF(kLeft + 6, rtop + 13), QString("RSI(14)  %1").arg(lastRsi, 0, 'f', 1));
        paneTop += paneH + 6;
    }
    if (macd_) {
        const double mtop = paneTop;
        double mlo = 1e300, mhi = -1e300;
        auto upd = [&](double v) {
            if (std::isnan(v)) return;
            mlo = std::min(mlo, v);
            mhi = std::max(mhi, v);
        };
        for (int i = i0; i <= i1; ++i) {
            upd(mc.line[i]);
            upd(mc.signal[i]);
            upd(mc.hist[i]);
        }
        if (mhi <= mlo) {
            mlo = -1;
            mhi = 1;
        }
        mlo = std::min(mlo, 0.0);
        mhi = std::max(mhi, 0.0);
        const double mp = (mhi - mlo) * 0.08;
        mlo -= mp;
        mhi += mp;
        auto MY = [&](double v) { return mtop + (mhi - v) / (mhi - mlo) * paneH; };
        paneFrame(mtop);
        p->save();
        p->setClipRect(QRectF(kLeft, mtop, plotW, paneH));
        p->setPen(QPen(kGrid, 1, Qt::DashLine));
        p->drawLine(QPointF(kLeft, MY(0)), QPointF(kLeft + plotW, MY(0)));
        const double bw = std::max(1.0, std::min(24.0, step * 0.7));
        for (int i = i0; i <= i1; ++i) {
            if (std::isnan(mc.hist[i])) continue;
            QColor c = mc.hist[i] >= 0 ? kUp : kDown;
            c.setAlpha(150);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            const double y0v = MY(0), y1v = MY(mc.hist[i]);
            p->drawRect(QRectF(X(i) - bw / 2, std::min(y0v, y1v), bw, std::max(1.0, std::abs(y1v - y0v))));
        }
        auto macdPath = [&](const Series& s) {
            QPainterPath path;
            bool started = false;
            for (int i = i0; i <= i1; ++i) {
                if (std::isnan(s[i])) continue;
                const QPointF pt(X(i), MY(s[i]));
                started ? path.lineTo(pt) : path.moveTo(pt);
                started = true;
            }
            return path;
        };
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kAccent, 1.3));
        p->drawPath(macdPath(mc.line));
        p->setPen(QPen(kSma20, 1.3));
        p->drawPath(macdPath(mc.signal));
        p->restore();
        p->setPen(kText);
        p->drawText(QPointF(kLeft + 6, mtop + 13), "MACD(12,26,9)");
    }

    // Crosshair + tooltip.
    if (hover_.x() >= kLeft && hover_.x() <= kLeft + plotW && hover_.y() >= kTop && hover_.y() <= H - kAxisH) {
        const int idx = static_cast<int>(std::lround(viewStart_ + (hover_.x() - kLeft) / plotW * vis_ - 0.5));
        if (idx >= 0 && idx < n) {
            const Candle& c = all[idx];
            const double x = X(idx);
            p->setPen(QPen(QColor(255, 255, 255, 70), 1, Qt::DashLine));
            p->drawLine(QPointF(x, kTop), QPointF(x, H - kAxisH));
            if (hover_.y() <= kTop + priceH) {
                p->drawLine(QPointF(kLeft, hover_.y()), QPointF(kLeft + plotW, hover_.y()));
                const double tv = hi - (hover_.y() - kTop) / priceH * (hi - lo);
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(60, 75, 95));
                p->drawRoundedRect(QRectF(kLeft + plotW + 2, hover_.y() - 9, kRight - 2, 18), 3, 3);
                p->setBrush(Qt::NoBrush);
                p->setPen(Qt::white);
                p->drawText(QRectF(kLeft + plotW + 2, hover_.y() - 9, kRight - 2, 18), Qt::AlignCenter, fmtPrice(Tinv(tv)));
            }
            const QDateTime dt = intraday ? QDateTime::fromSecsSinceEpoch(c.time) : QDateTime::fromSecsSinceEpoch(c.time, QTimeZone::UTC);
            QString txt = QString("%1\nO %2   H %3\nL %4   C %5")
                              .arg(dt.toString(intraday ? "ddd d MMM HH:mm:ss" : "ddd d MMM yyyy"), fmtPrice(c.open),
                                   fmtPrice(c.high), fmtPrice(c.low), fmtPrice(c.close));
            if (c.volume > 0) txt += "\nVol " + fmtVolume(c.volume);
            const double bh = c.volume > 0 ? 70 : 54;
            const QRectF box(x > plotW * 0.6 ? x - 200 : x + 14, kTop + 24, 186, bh);
            p->setPen(QPen(kGrid, 1));
            p->setBrush(QColor(21, 31, 45, 235));
            p->drawRoundedRect(box, 6, 6);
            p->setPen(QColor("#E6EDF5"));
            p->drawText(box.adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, txt);
            p->setBrush(Qt::NoBrush);
        }
    }
}

} // namespace ql
