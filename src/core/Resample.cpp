#include "Resample.h"

#include <algorithm>

namespace ql {

Candles resample(const Candles& in, std::int64_t bucketSecs) {
    Candles out;
    if (bucketSecs <= 0) return in;
    for (const Candle& c : in) {
        const std::int64_t t = c.time - c.time % bucketSecs;
        if (out.empty() || out.back().time != t) {
            out.push_back({t, c.open, c.high, c.low, c.close, c.volume});
        } else {
            Candle& o = out.back();
            o.high = std::max(o.high, c.high);
            o.low = std::min(o.low, c.low);
            o.close = c.close;
            o.volume += c.volume;
        }
    }
    return out;
}

} // namespace ql
