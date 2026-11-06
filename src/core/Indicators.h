#pragma once

#include "Types.h"

#include <cstddef>
#include <limits>

namespace ql::ind {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// All moving-window indicators return a series of the same length as the input,
// with NaN where there is not enough history yet.
Series sma(const Series& x, std::size_t period);
Series ema(const Series& x, std::size_t period);
Series rsi(const Series& close, std::size_t period = 14); // Wilder smoothing

struct Macd {
    Series line, signal, hist;
};
Macd macd(const Series& close, std::size_t fast = 12, std::size_t slow = 26, std::size_t signal = 9);

struct Bands {
    Series mid, upper, lower;
};
Bands bollinger(const Series& close, std::size_t period = 20, double k = 2.0);

Series closes(const Candles& c);

// r[i] = ln(close[i+1] / close[i]); result has size n-1.
Series logReturns(const Series& close);

// EWMA (RiskMetrics) volatility. out[i] is the volatility forecast for r[i]
// using returns up to i-1; out has size n+1 so out[n] is the forecast for the next return.
Series ewmaVolatility(const Series& returns, double lambda = 0.94);

double mean(const Series& x, std::size_t from, std::size_t to);  // [from, to)
double stdev(const Series& x, std::size_t from, std::size_t to); // sample standard deviation

struct LinearFit {
    double slope{0}, intercept{0}, r2{0}, tStat{0};
};
// Least squares fit of y against x = 0..n-1.
LinearFit linearFit(const Series& y);

} // namespace ql::ind
