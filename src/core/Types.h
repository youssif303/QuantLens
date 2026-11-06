#pragma once

#include <cstdint>
#include <vector>

namespace ql {

// One OHLCV bar. `time` is the bar open time in Unix seconds (UTC).
struct Candle {
    std::int64_t time{0};
    double open{0};
    double high{0};
    double low{0};
    double close{0};
    double volume{0};
};

using Series = std::vector<double>;
using Candles = std::vector<Candle>;

} // namespace ql
