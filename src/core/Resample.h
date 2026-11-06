#pragma once

#include "Types.h"

namespace ql {

// Aggregates candles into buckets of `bucketSecs` (aligned to the Unix epoch, UTC).
// Input must be sorted by time.
Candles resample(const Candles& in, std::int64_t bucketSecs);

} // namespace ql
