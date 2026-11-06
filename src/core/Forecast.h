#pragma once

#include "Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ql {

struct ForecastParams {
    std::size_t horizon{5};         // bars ahead
    double periodsPerYear{252};
    std::size_t paths{2000};
    std::uint32_t seed{42};
};

struct ConeStep {
    double p10{0}, p25{0}, p50{0}, p75{0}, p90{0}; // price quantiles
};

enum class Direction { Down, Neutral, Up };
enum class Confidence { Low, Medium, High };

struct Forecast {
    bool valid{false};
    std::size_t horizon{0};
    double lastPrice{0};
    double probUp{0.5};
    double expectedReturn{0}; // simple return over the horizon (median path)
    double drift{0};          // per-bar log drift used
    double volatility{0};     // per-bar volatility used
    Direction direction{Direction::Neutral};
    Confidence confidence{Confidence::Low};
    std::vector<ConeStep> cone; // size == horizon, step i = i+1 bars ahead
    std::vector<std::string> reasons;
    double trendT{0};
    double rsi{50};
};

// Statistical forecast: shrunk drift (mean + trend + momentum), EWMA volatility,
// filtered-historical-bootstrap Monte Carlo. Deterministic for a given seed.
// Requires at least 60 bars; otherwise returns valid == false.
Forecast forecast(const Series& close, const ForecastParams& p);

const char* toString(Direction d);
const char* toString(Confidence c);

} // namespace ql
