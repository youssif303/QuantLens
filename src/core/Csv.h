#pragma once

#include "Types.h"

#include <string>

namespace ql::csv {

// Format: header line, then "time,open,high,low,close,volume" with time in Unix seconds.
std::string toString(const Candles& c);
Candles fromString(const std::string& text);

} // namespace ql::csv
