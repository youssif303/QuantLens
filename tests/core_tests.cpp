#include "core/Backtest.h"
#include "core/Csv.h"
#include "core/Forecast.h"
#include "core/Indicators.h"
#include "core/Resample.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

using namespace ql;

namespace {
Series makeWalk(std::size_t n, double drift, double vol, unsigned seed) {
    std::mt19937 g(seed);
    std::normal_distribution<double> nd(0, 1);
    Series s{100};
    for (std::size_t i = 1; i < n; ++i) s.push_back(s.back() * std::exp(drift + vol * nd(g)));
    return s;
}
} // namespace

TEST(Indicators, SmaBasic) {
    const Series s = ind::sma({1, 2, 3, 4, 5}, 3);
    EXPECT_TRUE(std::isnan(s[1]));
    EXPECT_DOUBLE_EQ(s[2], 2);
    EXPECT_DOUBLE_EQ(s[4], 4);
}

TEST(Indicators, RsiOfRisingSeriesIs100) {
    Series s;
    for (int i = 0; i < 40; ++i) s.push_back(100 + i);
    EXPECT_DOUBLE_EQ(ind::rsi(s).back(), 100.0);
}

TEST(Indicators, RsiOfFallingSeriesIs0) {
    Series s;
    for (int i = 0; i < 40; ++i) s.push_back(200 - i);
    EXPECT_NEAR(ind::rsi(s).back(), 0.0, 1e-9);
}

TEST(Indicators, LinearFitRecoversLine) {
    Series y;
    for (int i = 0; i < 50; ++i) y.push_back(3.0 + 0.5 * i);
    const auto f = ind::linearFit(y);
    EXPECT_NEAR(f.slope, 0.5, 1e-9);
    EXPECT_NEAR(f.intercept, 3.0, 1e-9);
    EXPECT_NEAR(f.r2, 1.0, 1e-9);
}

TEST(Indicators, BollingerBandsOrdered) {
    const Series s = makeWalk(100, 0, 0.01, 1);
    const auto b = ind::bollinger(s);
    for (std::size_t i = 19; i < s.size(); ++i) {
        EXPECT_LE(b.lower[i], b.mid[i]);
        EXPECT_LE(b.mid[i], b.upper[i]);
    }
}

TEST(Indicators, LogReturnsSize) {
    EXPECT_EQ(ind::logReturns({1, 2, 4}).size(), 2u);
    EXPECT_NEAR(ind::logReturns({1, 2})[0], std::log(2.0), 1e-12);
}

TEST(Csv, RoundTrip) {
    Candles c{{1700000000, 1.5, 2.5, 1.0, 2.0, 100}, {1700086400, 2.0, 3.0, 1.5, 2.5, 50}};
    const Candles d = csv::fromString(csv::toString(c));
    ASSERT_EQ(d.size(), 2u);
    EXPECT_EQ(d[1].time, 1700086400);
    EXPECT_DOUBLE_EQ(d[0].high, 2.5);
}

TEST(Resample, AggregatesBuckets) {
    Candles c{{0, 1, 2, 0.5, 1.5, 10}, {3600, 1.5, 3, 1, 2, 5}, {7200, 2, 2.5, 1.8, 2.2, 7}, {10800, 2.2, 4, 2, 3, 1}};
    const Candles r = resample(c, 7200);
    ASSERT_EQ(r.size(), 2u);
    EXPECT_DOUBLE_EQ(r[0].open, 1);
    EXPECT_DOUBLE_EQ(r[0].high, 3);
    EXPECT_DOUBLE_EQ(r[0].low, 0.5);
    EXPECT_DOUBLE_EQ(r[0].close, 2);
    EXPECT_DOUBLE_EQ(r[0].volume, 15);
    EXPECT_EQ(r[1].time, 7200);
}

TEST(Forecast, TooShortIsInvalid) {
    EXPECT_FALSE(forecast(Series(30, 100.0), {}).valid);
}

TEST(Forecast, DeterministicAndOrderedCone) {
    const Series s = makeWalk(400, 0.0005, 0.01, 7);
    ForecastParams p;
    p.horizon = 10;
    const Forecast a = forecast(s, p), b = forecast(s, p);
    ASSERT_TRUE(a.valid);
    EXPECT_DOUBLE_EQ(a.probUp, b.probUp);
    ASSERT_EQ(a.cone.size(), 10u);
    for (const auto& c : a.cone) {
        EXPECT_LE(c.p10, c.p25);
        EXPECT_LE(c.p25, c.p50);
        EXPECT_LE(c.p50, c.p75);
        EXPECT_LE(c.p75, c.p90);
    }
}

TEST(Forecast, StrongUptrendLeansUp) {
    const Series s = makeWalk(300, 0.01, 0.003, 3);
    ForecastParams p;
    p.horizon = 5;
    EXPECT_GT(forecast(s, p).probUp, 0.5);
}

TEST(Backtest, SanityOnRandomWalk) {
    const Series s = makeWalk(700, 0.0003, 0.012, 11);
    const auto r = backtest(s, 5, 252, 10);
    ASSERT_TRUE(r.valid);
    EXPECT_GT(r.samples, 10u);
    EXPECT_GE(r.brier, 0.0);
    EXPECT_LE(r.brier, 1.0);
    EXPECT_GE(r.hitRate, 0.0);
    EXPECT_LE(r.hitRate, 1.0);
}
