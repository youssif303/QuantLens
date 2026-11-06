#pragma once

#include "Forecast.h"

#include <array>

namespace ql {

struct CalibrationBin {
    double predicted{0}; // mean predicted probability in bin
    double observed{0};  // fraction that actually went up
    std::size_t count{0};
};

struct BacktestResult {
    bool valid{false};
    std::size_t samples{0};
    double hitRate{0};          // direction correct (probUp>0.5 vs. actual)
    double baselineHitRate{0};  // always predicting the more frequent outcome ("always up")
    double brier{0};            // mean (p - outcome)^2
    double brierBaseline{0};    // constant base-rate forecast
    double skill{0};            // 1 - brier / brierBaseline  (>0 means better than baseline)
    std::array<CalibrationBin, 5> bins{};
};

// Walk-forward: at every `stride`-th bar, forecast using only past data and compare
// with what really happened `horizon` bars later.
BacktestResult backtest(const Series& close, std::size_t horizon, double periodsPerYear,
                        std::size_t stride = 5, std::size_t minHistory = 250);

} // namespace ql
