#include "Indicators.h"

#include <algorithm>
#include <cmath>

namespace ql::ind {

Series sma(const Series& x, std::size_t period) {
    Series out(x.size(), kNaN);
    if (period == 0 || x.size() < period) return out;
    double sum = 0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        sum += x[i];
        if (i >= period) sum -= x[i - period];
        if (i + 1 >= period) out[i] = sum / static_cast<double>(period);
    }
    return out;
}

Series ema(const Series& x, std::size_t period) {
    Series out(x.size(), kNaN);
    if (period == 0 || x.size() < period) return out;
    const double k = 2.0 / (static_cast<double>(period) + 1.0);
    double seed = 0;
    for (std::size_t i = 0; i < period; ++i) seed += x[i];
    out[period - 1] = seed / static_cast<double>(period);
    for (std::size_t i = period; i < x.size(); ++i) out[i] = x[i] * k + out[i - 1] * (1.0 - k);
    return out;
}

Series rsi(const Series& close, std::size_t period) {
    Series out(close.size(), kNaN);
    if (period == 0 || close.size() <= period) return out;
    double gain = 0, loss = 0;
    for (std::size_t i = 1; i <= period; ++i) {
        const double d = close[i] - close[i - 1];
        (d >= 0 ? gain : loss) += std::fabs(d);
    }
    gain /= static_cast<double>(period);
    loss /= static_cast<double>(period);
    auto toRsi = [](double g, double l) { return l == 0 ? 100.0 : 100.0 - 100.0 / (1.0 + g / l); };
    out[period] = toRsi(gain, loss);
    const double p = static_cast<double>(period);
    for (std::size_t i = period + 1; i < close.size(); ++i) {
        const double d = close[i] - close[i - 1];
        gain = (gain * (p - 1) + std::max(d, 0.0)) / p;
        loss = (loss * (p - 1) + std::max(-d, 0.0)) / p;
        out[i] = toRsi(gain, loss);
    }
    return out;
}

Macd macd(const Series& close, std::size_t fast, std::size_t slow, std::size_t signal) {
    Macd m;
    m.line.assign(close.size(), kNaN);
    m.signal.assign(close.size(), kNaN);
    m.hist.assign(close.size(), kNaN);
    const Series f = ema(close, fast);
    const Series s = ema(close, slow);
    std::size_t first = close.size();
    for (std::size_t i = 0; i < close.size(); ++i) {
        if (!std::isnan(f[i]) && !std::isnan(s[i])) {
            m.line[i] = f[i] - s[i];
            first = std::min(first, i);
        }
    }
    if (first >= close.size()) return m;
    const Series compact(m.line.begin() + static_cast<std::ptrdiff_t>(first), m.line.end());
    const Series sig = ema(compact, signal);
    for (std::size_t i = 0; i < sig.size(); ++i) {
        if (std::isnan(sig[i])) continue;
        m.signal[first + i] = sig[i];
        m.hist[first + i] = m.line[first + i] - sig[i];
    }
    return m;
}

Bands bollinger(const Series& close, std::size_t period, double k) {
    Bands b;
    b.mid = sma(close, period);
    b.upper.assign(close.size(), kNaN);
    b.lower.assign(close.size(), kNaN);
    if (period == 0 || close.size() < period) return b;
    for (std::size_t i = period - 1; i < close.size(); ++i) {
        double var = 0;
        for (std::size_t j = i + 1 - period; j <= i; ++j) {
            const double d = close[j] - b.mid[i];
            var += d * d;
        }
        const double sd = std::sqrt(var / static_cast<double>(period));
        b.upper[i] = b.mid[i] + k * sd;
        b.lower[i] = b.mid[i] - k * sd;
    }
    return b;
}

Series closes(const Candles& c) {
    Series out;
    out.reserve(c.size());
    for (const auto& x : c) out.push_back(x.close);
    return out;
}

Series logReturns(const Series& close) {
    Series r;
    if (close.size() < 2) return r;
    r.reserve(close.size() - 1);
    for (std::size_t i = 1; i < close.size(); ++i) {
        r.push_back(close[i - 1] > 0 && close[i] > 0 ? std::log(close[i] / close[i - 1]) : 0.0);
    }
    return r;
}

Series ewmaVolatility(const Series& returns, double lambda) {
    Series out(returns.size() + 1, kNaN);
    if (returns.empty()) return out;
    const std::size_t warm = std::min<std::size_t>(returns.size(), 20);
    double var = 0;
    for (std::size_t i = 0; i < warm; ++i) var += returns[i] * returns[i];
    var /= static_cast<double>(warm);
    out[0] = std::sqrt(var);
    for (std::size_t i = 0; i < returns.size(); ++i) {
        var = lambda * var + (1.0 - lambda) * returns[i] * returns[i];
        out[i + 1] = std::sqrt(var);
    }
    return out;
}

double mean(const Series& x, std::size_t from, std::size_t to) {
    to = std::min(to, x.size());
    if (from >= to) return 0;
    double s = 0;
    for (std::size_t i = from; i < to; ++i) s += x[i];
    return s / static_cast<double>(to - from);
}

double stdev(const Series& x, std::size_t from, std::size_t to) {
    to = std::min(to, x.size());
    if (to <= from + 1) return 0;
    const double m = mean(x, from, to);
    double s = 0;
    for (std::size_t i = from; i < to; ++i) s += (x[i] - m) * (x[i] - m);
    return std::sqrt(s / static_cast<double>(to - from - 1));
}

LinearFit linearFit(const Series& y) {
    LinearFit f;
    const std::size_t n = y.size();
    if (n < 3) return f;
    const double dn = static_cast<double>(n);
    const double xm = (dn - 1.0) / 2.0;
    const double ym = mean(y, 0, n);
    double sxx = 0, sxy = 0, syy = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = static_cast<double>(i) - xm;
        sxx += dx * dx;
        sxy += dx * (y[i] - ym);
        syy += (y[i] - ym) * (y[i] - ym);
    }
    f.slope = sxy / sxx;
    f.intercept = ym - f.slope * xm;
    const double ssRes = std::max(syy - f.slope * sxy, 0.0);
    f.r2 = syy > 0 ? 1.0 - ssRes / syy : 0.0;
    const double se = std::sqrt(ssRes / (dn - 2.0) / sxx);
    f.tStat = se > 0 ? f.slope / se : 0.0;
    return f;
}

} // namespace ql::ind
