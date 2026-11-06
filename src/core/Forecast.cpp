#include "Forecast.h"

#include "Indicators.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

namespace ql {

const char* toString(Direction d) {
    switch (d) {
    case Direction::Up: return "Up";
    case Direction::Down: return "Down";
    default: return "Neutral";
    }
}

const char* toString(Confidence c) {
    switch (c) {
    case Confidence::High: return "High";
    case Confidence::Medium: return "Medium";
    default: return "Low";
    }
}

namespace {
std::string fmt(const char* f, double a) {
    char b[160];
    std::snprintf(b, sizeof b, f, a);
    return b;
}
std::string fmt2(const char* f, double a, double b2) {
    char b[200];
    std::snprintf(b, sizeof b, f, a, b2);
    return b;
}
double quantile(std::vector<double>& v, double q) {
    const std::size_t k = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}
} // namespace

Forecast forecast(const Series& close, const ForecastParams& p) {
    Forecast f;
    f.horizon = p.horizon;
    if (close.size() < 60 || p.horizon == 0) return f;

    const Series r = ind::logReturns(close);
    const std::size_t n = r.size();
    const Series vol = ind::ewmaVolatility(r);
    const double sigmaNow = std::max(vol[n], 1e-6);

    // Long-run per-bar volatility and mean over the last ~2 years.
    const std::size_t look = std::min<std::size_t>(n, 504);
    const double sigmaLong = std::max(ind::stdev(r, n - look, n), 1e-6);
    const double muHist = ind::mean(r, n - look, n);

    // Trend: log-price regression over the last 60 bars.
    const std::size_t tw = std::min<std::size_t>(close.size(), 60);
    Series lp;
    for (std::size_t i = close.size() - tw; i < close.size(); ++i) lp.push_back(std::log(close[i]));
    const ind::LinearFit fit = ind::linearFit(lp);

    // Momentum: 20-bar return normalised by volatility.
    const std::size_t mw = std::min<std::size_t>(n, 20);
    const double mom = ind::mean(r, n - mw, n) * static_cast<double>(mw);
    const double momZ = mom / (sigmaNow * std::sqrt(static_cast<double>(mw)));

    // Drift: heavily shrunk blend. Expected returns are tiny relative to noise.
    const double trendDrift = std::clamp(fit.slope, -2.0 * sigmaLong, 2.0 * sigmaLong);
    const double momDrift = 0.05 * std::clamp(momZ, -2.0, 2.0) * sigmaNow;
    f.drift = 0.35 * (0.5 * muHist + 0.5 * trendDrift) + momDrift;
    f.trendT = fit.tStat;

    const Series cl = ind::rsi(close);
    f.rsi = std::isnan(cl.back()) ? 50.0 : cl.back();
    f.lastPrice = close.back();

    // Standardised residuals for the bootstrap.
    Series z;
    z.reserve(n);
    for (std::size_t i = 0; i < n; ++i) z.push_back((r[i] - muHist) / std::max(vol[i], 1e-6));
    const double zs = std::max(ind::stdev(z, 0, z.size()), 1e-6);
    for (auto& x : z) x = (x - ind::mean(z, 0, z.size())) / zs;

    std::mt19937 rng(p.seed);
    const std::size_t H = p.horizon;
    std::vector<std::vector<double>> prices(H, std::vector<double>(p.paths));
    std::size_t up = 0;
    std::vector<double> finals(p.paths);
    for (std::size_t k = 0; k < p.paths; ++k) {
        double lnP = std::log(f.lastPrice);
        double v = sigmaNow;
        for (std::size_t h = 0; h < H; ++h) {
            const double shock = z[rng() % z.size()];
            const double ret = f.drift + v * shock;
            lnP += ret;
            prices[h][k] = std::exp(lnP);
            const double var = 0.94 * v * v + 0.06 * ret * ret;
            // mean revert towards the long-run volatility
            v = std::sqrt(0.9 * var + 0.1 * sigmaLong * sigmaLong);
        }
        finals[k] = prices[H - 1][k];
        if (finals[k] > f.lastPrice) ++up;
    }
    f.probUp = static_cast<double>(up) / static_cast<double>(p.paths);
    f.cone.resize(H);
    for (std::size_t h = 0; h < H; ++h) {
        auto& v = prices[h];
        f.cone[h] = {quantile(v, 0.10), quantile(v, 0.25), quantile(v, 0.50), quantile(v, 0.75), quantile(v, 0.90)};
    }
    f.expectedReturn = f.cone[H - 1].p50 / f.lastPrice - 1.0;
    f.volatility = sigmaNow;

    f.direction = f.probUp >= 0.55 ? Direction::Up : (f.probUp <= 0.45 ? Direction::Down : Direction::Neutral);

    // Confidence: edge size and agreement between trend, momentum and drift.
    const double edge = std::fabs(f.probUp - 0.5);
    const int sTrend = fit.tStat > 2 ? 1 : (fit.tStat < -2 ? -1 : 0);
    const int sMom = momZ > 0.5 ? 1 : (momZ < -0.5 ? -1 : 0);
    const int sDir = f.direction == Direction::Up ? 1 : (f.direction == Direction::Down ? -1 : 0);
    const int agree = (sTrend == sDir && sDir != 0) + (sMom == sDir && sDir != 0);
    if (edge > 0.12 && agree == 2) f.confidence = Confidence::High;
    else if (edge > 0.05 && agree >= 1) f.confidence = Confidence::Medium;
    else f.confidence = Confidence::Low;

    f.reasons.push_back(fit.tStat > 2 ? fmt("Uptrend: 60-bar trend is statistically significant (t = %.1f)", fit.tStat)
                        : fit.tStat < -2 ? fmt("Downtrend: 60-bar trend is statistically significant (t = %.1f)", fit.tStat)
                                         : fmt("No clear trend over the last 60 bars (t = %.1f)", fit.tStat));
    f.reasons.push_back(momZ > 0.5 ? fmt("Positive momentum over 20 bars (%.1f sigma)", momZ)
                        : momZ < -0.5 ? fmt("Negative momentum over 20 bars (%.1f sigma)", momZ)
                                      : fmt("Neutral momentum over 20 bars (%.1f sigma)", momZ));
    f.reasons.push_back(fmt2("Volatility %.1f%% per bar (long-run %.1f%%)", sigmaNow * 100, sigmaLong * 100));
    if (f.rsi > 70) f.reasons.push_back(fmt("RSI %.0f: overbought zone", f.rsi));
    else if (f.rsi < 30) f.reasons.push_back(fmt("RSI %.0f: oversold zone", f.rsi));

    f.valid = true;
    return f;
}

} // namespace ql
