#include "Backtest.h"

#include <algorithm>
#include <cmath>

namespace ql {

BacktestResult backtest(const Series& close, std::size_t horizon, double periodsPerYear,
                        std::size_t stride, std::size_t minHistory) {
    BacktestResult res;
    if (close.size() < minHistory + horizon + 1 || stride == 0) return res;

    std::size_t hits = 0, ups = 0;
    double brier = 0;
    std::array<double, 5> sumP{}, sumO{};
    std::array<std::size_t, 5> cnt{};

    ForecastParams fp;
    fp.horizon = horizon;
    fp.periodsPerYear = periodsPerYear;
    fp.paths = 300;

    for (std::size_t t = minHistory; t + horizon < close.size(); t += stride) {
        const Series past(close.begin(), close.begin() + static_cast<std::ptrdiff_t>(t + 1));
        fp.seed = static_cast<std::uint32_t>(1000 + t);
        const Forecast f = forecast(past, fp);
        if (!f.valid) continue;
        const bool wentUp = close[t + horizon] > close[t];
        const double o = wentUp ? 1.0 : 0.0;
        ++res.samples;
        if ((f.probUp > 0.5) == wentUp) ++hits;
        if (wentUp) ++ups;
        brier += (f.probUp - o) * (f.probUp - o);
        const std::size_t b = std::min<std::size_t>(4, static_cast<std::size_t>(f.probUp * 5.0));
        sumP[b] += f.probUp;
        sumO[b] += o;
        ++cnt[b];
    }
    if (res.samples == 0) return res;

    const double n = static_cast<double>(res.samples);
    const double base = static_cast<double>(ups) / n;
    res.hitRate = static_cast<double>(hits) / n;
    res.baselineHitRate = std::max(base, 1.0 - base);
    res.brier = brier / n;
    res.brierBaseline = base * (1 - base);
    res.skill = res.brierBaseline > 0 ? 1.0 - res.brier / res.brierBaseline : 0.0;
    for (std::size_t i = 0; i < 5; ++i) {
        if (cnt[i] == 0) continue;
        res.bins[i] = {sumP[i] / static_cast<double>(cnt[i]), sumO[i] / static_cast<double>(cnt[i]), cnt[i]};
    }
    res.valid = true;
    return res;
}

} // namespace ql
