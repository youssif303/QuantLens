#include "ChartItem.h"

#include "AppController.h"
#include "core/Indicators.h"

#include <QDateTime>
#include <QFontMetricsF>
#include <QHoverEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace ql {

namespace {
const QColor kBg("#0E1621"), kGrid("#1D2A3B"), kText("#8B9BB0"), kUp("#26C281"), kDown("#F0555A"),
    kAccent("#4DA3FF"), kSma20("#F5B84D"), kSma50("#B388FF");

QString fmtPrice(double v) {
    if (v >= 1000) return QString::number(v, 'f', 0);
    if (v >= 10) return QString::number(v, 'f', 2);
    return QString::number(v, 'f', 4);
}
} // namespace

ChartItem::ChartItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAcceptHoverEvents(true);
    setAntialiasing(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

QObject* ChartItem::controller() const { return ctrl_; }

void ChartItem::setController(QObject* c) {
    auto* ac = qobject_cast<AppController*>(c);
    if (ac == ctrl_) return;
    ctrl_ = ac;
    if (ctrl_) connect(ctrl_, &AppController::chartChanged, this, [this]() { update(); });
    emit controllerChanged();
    update();
}

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
    const Candles& all = ctrl_->candles();
    if (all.size() < 5) {
        p->setPen(kText);
        p->drawText(boundingRect(), Qt::AlignCenter, "Loading market data...");
        return;
    }
    p->setRenderHint(QPainter::Antialiasing, true);

    // Indicators run on full history so warm-up values are valid inside the visible window.
    const Series close = ind::closes(all);
    const Series sma20 = ind::sma(close, 20), sma50 = ind::sma(close, 50);
    const ind::Bands bb = ind::bollinger(close, 20, 2.0);
    const Series rsi = ind::rsi(close, 14);

    const Forecast& fc = longCone_ ? ctrl_->longForecast() : ctrl_->shortForecast();
    const bool drawCone = cone_ && fc.valid;
    const int horizon = drawCone ? static_cast<int>(fc.cone.size()) : 0;

    const int total = static_cast<int>(all.size());
    const int vis = std::clamp(bars_, 20, total);
    const int first = total - vis;
    const int nslots = vis + horizon + 1;

    const double W = width(), H = height();
    const double left = 8, right = 74, top = 10, bottom = 8;
    const double plotW = W - left - right;
    const double rsiH = rsi_ ? std::min(90.0, H * 0.2) : 0;
    const double gap = rsi_ ? 24 : 0;
    const double priceH = H - top - bottom - rsiH - gap - (rsi_ ? 0 : 20);
    if (plotW < 50 || priceH < 50) return;

    // Y range from visible candles, bands and cone.
    double lo = 1e300, hi = -1e300;
    for (int i = first; i < total; ++i) {
        lo = std::min(lo, all[i].low);
        hi = std::max(hi, all[i].high);
        if (bands_ && !std::isnan(bb.lower[i])) {
            lo = std::min(lo, bb.lower[i]);
            hi = std::max(hi, bb.upper[i]);
        }
    }
    if (drawCone)
        for (const auto& c : fc.cone) {
            lo = std::min(lo, c.p10);
            hi = std::max(hi, c.p90);
        }
    const double pad = (hi - lo) * 0.06 + 1e-9;
    lo -= pad;
    hi += pad;

    auto X = [&](double idx) { return left + (idx + 0.5) / nslots * plotW; };
    auto Y = [&](double v) { return top + (hi - v) / (hi - lo) * priceH; };
    const double step = plotW / nslots;

    // Grid + price axis.
    QFont f = p->font();
    f.setPixelSize(11);
    p->setFont(f);
    for (int g = 0; g <= 5; ++g) {
        const double v = lo + (hi - lo) * g / 5.0;
        const double y = Y(v);
        p->setPen(QPen(kGrid, 1));
        p->drawLine(QPointF(left, y), QPointF(left + plotW, y));
        p->setPen(kText);
        p->drawText(QRectF(left + plotW + 6, y - 8, right - 6, 16), Qt::AlignLeft | Qt::AlignVCenter, fmtPrice(v));
    }
    // Date axis.
    const int ticks = std::max(2, static_cast<int>(plotW / 110));
    for (int t = 0; t < ticks; ++t) {
        const int i = first + t * (vis - 1) / std::max(1, ticks - 1);
        const QString d = QDateTime::fromSecsSinceEpoch(all[i].time, QTimeZone::UTC).toString(vis > 400 ? "MMM yyyy" : "d MMM");
        p->setPen(kText);
        p->drawText(QRectF(std::clamp(X(i - first) - 40, 0.0, W - 80), top + priceH + 4, 80, 16), Qt::AlignCenter, d);
    }

    // Bollinger band fill.
    if (bands_) {
        QPainterPath path;
        bool started = false;
        for (int i = first; i < total; ++i) {
            if (std::isnan(bb.upper[i])) continue;
            const QPointF pt(X(i - first), Y(bb.upper[i]));
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        for (int i = total - 1; i >= first; --i)
            if (!std::isnan(bb.lower[i])) path.lineTo(X(i - first), Y(bb.lower[i]));
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(77, 163, 255, 28));
        p->drawPath(path);
        p->setBrush(Qt::NoBrush);
    }

    // Forecast cone.
    if (drawCone) {
        const double cx = X(vis - 1), cy = Y(fc.lastPrice);
        auto band = [&](auto get1, auto get2, const QColor& col) {
            QPainterPath path;
            path.moveTo(cx, cy);
            for (int h = 0; h < horizon; ++h) path.lineTo(X(vis + h), Y(get1(fc.cone[h])));
            for (int h = horizon - 1; h >= 0; --h) path.lineTo(X(vis + h), Y(get2(fc.cone[h])));
            path.closeSubpath();
            p->setPen(Qt::NoPen);
            p->setBrush(col);
            p->drawPath(path);
        };
        band([](const ConeStep& c) { return c.p90; }, [](const ConeStep& c) { return c.p10; }, QColor(77, 163, 255, 36));
        band([](const ConeStep& c) { return c.p75; }, [](const ConeStep& c) { return c.p25; }, QColor(77, 163, 255, 64));
        p->setBrush(Qt::NoBrush);
        QPen dash(kAccent, 1.6, Qt::DashLine);
        p->setPen(dash);
        QPainterPath mid;
        mid.moveTo(cx, cy);
        for (int h = 0; h < horizon; ++h) mid.lineTo(X(vis + h), Y(fc.cone[h].p50));
        p->drawPath(mid);
        p->setPen(QPen(QColor(77, 163, 255, 120), 1, Qt::DotLine));
        p->drawLine(QPointF(cx, top), QPointF(cx, top + priceH));
        p->setPen(kAccent);
        p->drawText(QRectF(cx - 166, top + 2, 160, 14), Qt::AlignRight, QString("Forecast: %1 bars").arg(horizon));
    }

    // Price series.
    const double cw = std::max(1.0, std::min(14.0, step * 0.7));
    if (candles_ && step >= 3.0) {
        for (int i = first; i < total; ++i) {
            const Candle& c = all[i];
            const double x = X(i - first);
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
        for (int i = first; i < total; ++i) {
            const QPointF pt(X(i - first), Y(all[i].close));
            i == first ? path.moveTo(pt) : path.lineTo(pt);
        }
        const bool up = all.back().close >= all[first].close;
        p->setPen(QPen(up ? kUp : kDown, 1.8));
        p->drawPath(path);
        QPainterPath fill = path;
        fill.lineTo(X(vis - 1), top + priceH);
        fill.lineTo(X(0), top + priceH);
        fill.closeSubpath();
        QLinearGradient g(0, top, 0, top + priceH);
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

    auto line = [&](const Series& s, const QColor& col) {
        QPainterPath path;
        bool started = false;
        for (int i = first; i < total; ++i) {
            if (std::isnan(s[i])) continue;
            const QPointF pt(X(i - first), Y(s[i]));
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        p->setPen(QPen(col, 1.3));
        p->drawPath(path);
    };
    if (sma_) {
        line(sma20, kSma20);
        line(sma50, kSma50);
    }
    if (bands_) {
        line(bb.upper, QColor(77, 163, 255, 160));
        line(bb.lower, QColor(77, 163, 255, 160));
    }

    // Live price tag + line.
    {
        const double last = all.back().close;
        const double y = Y(last);
        const QColor col = last >= all[std::max(0, total - 2)].close ? kUp : kDown;
        p->setPen(QPen(col, 1, Qt::DashLine));
        p->drawLine(QPointF(left, y), QPointF(left + plotW, y));
        p->setPen(Qt::NoPen);
        p->setBrush(col);
        p->drawRoundedRect(QRectF(left + plotW + 2, y - 9, right - 2, 18), 3, 3);
        p->setPen(Qt::white);
        p->drawText(QRectF(left + plotW + 2, y - 9, right - 2, 18), Qt::AlignCenter, fmtPrice(last));
        p->setBrush(Qt::NoBrush);
    }

    // Legend.
    {
        double lx = left + 6;
        auto legend = [&](const QString& t, const QColor& c) {
            p->setPen(c);
            p->drawText(QPointF(lx, top + 14 + (drawCone ? 14 : 0)), t);
            lx += QFontMetricsF(p->font()).horizontalAdvance(t) + 12;
        };
        if (sma_) {
            legend("SMA20", kSma20);
            legend("SMA50", kSma50);
        }
        if (bands_) legend("Bollinger(20,2)", QColor(77, 163, 255));
    }

    // RSI pane.
    if (rsi_) {
        const double ry = top + priceH + gap + 0;
        const double rtop = ry + 6, rh = rsiH - 6;
        auto RY = [&](double v) { return rtop + (100 - v) / 100.0 * rh; };
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 10));
        p->drawRect(QRectF(left, RY(70), plotW, RY(30) - RY(70)));
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kGrid, 1, Qt::DashLine));
        p->drawLine(QPointF(left, RY(70)), QPointF(left + plotW, RY(70)));
        p->drawLine(QPointF(left, RY(30)), QPointF(left + plotW, RY(30)));
        p->setPen(kText);
        p->drawText(QRectF(left + plotW + 6, RY(70) - 8, right, 16), Qt::AlignLeft, "70");
        p->drawText(QRectF(left + plotW + 6, RY(30) - 8, right, 16), Qt::AlignLeft, "30");
        QPainterPath path;
        bool started = false;
        for (int i = first; i < total; ++i) {
            if (std::isnan(rsi[i])) continue;
            const QPointF pt(X(i - first), RY(rsi[i]));
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        p->setPen(QPen(QColor("#C792EA"), 1.4));
        p->drawPath(path);
        p->setPen(kText);
        const double lastRsi = std::isnan(rsi.back()) ? 0 : rsi.back();
        p->drawText(QPointF(left + 6, rtop + 12), QString("RSI(14)  %1").arg(lastRsi, 0, 'f', 1));
    }

    // Crosshair + tooltip.
    if (hover_.x() >= left && hover_.x() <= left + plotW && hover_.y() >= top && hover_.y() <= top + priceH) {
        int idx = static_cast<int>(std::round((hover_.x() - left) / plotW * nslots - 0.5));
        if (idx >= 0 && idx < vis) {
            const Candle& c = all[first + idx];
            const double x = X(idx);
            p->setPen(QPen(QColor(255, 255, 255, 70), 1, Qt::DashLine));
            p->drawLine(QPointF(x, top), QPointF(x, top + priceH));
            p->drawLine(QPointF(left, hover_.y()), QPointF(left + plotW, hover_.y()));
            const QString txt = QString("%1\nO %2   H %3\nL %4   C %5")
                                    .arg(QDateTime::fromSecsSinceEpoch(c.time, QTimeZone::UTC).toString("ddd d MMM yyyy"),
                                         fmtPrice(c.open), fmtPrice(c.high), fmtPrice(c.low), fmtPrice(c.close));
            const QRectF box(x > plotW * 0.6 ? x - 190 : x + 14, top + 24, 176, 54);
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
