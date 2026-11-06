#include "Csv.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ql::csv {

std::string toString(const Candles& c) {
    std::string out = "time,open,high,low,close,volume\n";
    char buf[256];
    for (const auto& x : c) {
        std::snprintf(buf, sizeof buf, "%lld,%.10g,%.10g,%.10g,%.10g,%.10g\n",
                      static_cast<long long>(x.time), x.open, x.high, x.low, x.close, x.volume);
        out += buf;
    }
    return out;
}

Candles fromString(const std::string& text) {
    Candles out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] < '0' || line[0] > '9') continue; // header / junk
        long long t = 0;
        double o, h, l, c, v = 0;
        const int n = std::sscanf(line.c_str(), "%lld,%lf,%lf,%lf,%lf,%lf", &t, &o, &h, &l, &c, &v);
        if (n < 5) continue;
        out.push_back({t, o, h, l, c, n >= 6 ? v : 0.0});
    }
    return out;
}

} // namespace ql::csv
