# QuantLens interview cheat sheet

Read this before an interview. Every point maps to code in this repo, so open the file and look at it.

## 1. 30-second pitch

> QuantLens is a desktop app in C++17 and Qt 6/QML. It shows live prices and interactive charts for stocks, ETFs and crypto. It also estimates the probability of up or down moves for a short and a long horizon, using a statistical model, and it backtests that model honestly. It is an analysis tool, not a trading platform and not financial advice.

## 2. Architecture (draw this on a whiteboard)

```
qml/ (UI)  <--  src/app (AppController, ChartItem, WatchlistModel)
                    |
                src/data (MarketData, LiveFeed, Asset)      <- Qt: network, cache, WebSocket
                    |
                src/core (Indicators, Forecast, Backtest, Csv, Resample)   <- pure C++17, no Qt
```

- **Why the core has no Qt:** it is unit-testable (`tests/core_tests.cpp`, GoogleTest), fast to build and reusable in a CLI or server.
- **AppController** is the single QML-facing object (`app` context property). It exposes Q_PROPERTYs, Q_INVOKABLEs and signals. QML only binds and calls.
- **Data flow:** `MarketData` loads candles, `AppController` stores them and runs the analysis, and the UI redraws on signals (`chartChanged`, `analysisChanged`).

## 3. Qt topics you must be able to explain

| Topic | Where | What to say |
|---|---|---|
| Signals/slots | `AppController`, `MarketData` | Loose coupling: the data layer emits `historyReady`, the controller reacts. |
| Q_PROPERTY + NOTIFY | `AppController.h` | QML bindings update automatically when the NOTIFY signal fires. |
| Context property | `main.cpp` | `app` is injected into QML. A simple alternative is a registered QML singleton. |
| Custom painted item | `ChartItem` (QQuickPaintedItem) | Draws with QPainter into an FBO. Fine for a chart. For very large data I would use the scene graph. |
| Async networking | `MarketData`, `LiveFeed` | QNetworkAccessManager and QWebSocket are event-driven, so the UI never blocks. |
| Worker thread | `AppController::analyze` | `QtConcurrent::run` plus `QFutureWatcher`. The forecast and backtest run off the GUI thread. A rerun flag handles changes while a job is running. |
| Throttling | `chartTimer_` (250 ms) | Live ticks arrive often, but repaints are rate limited. |
| QAbstractListModel | `WatchlistModel` | Model/view for the sidebar. |
| QSettings | `AppController` | Persists the watchlist, currency and FX rate. |
| Resources | CMake `qt_add_qml_module` | QML and sample CSVs are embedded in the exe. |

## 4. How the chart works (a common question)

- The view is two numbers: `viewStart_` (index of the left edge, fractional) and `vis_` (number of visible bars).
- The x position of bar `i` is `left + (i - viewStart_ + 0.5) / vis_ * plotW`. Zoom and pan are only arithmetic on these two values.
- **Wheel zoom** keeps the bar under the cursor fixed: `anchorIdx = viewStart_ + frac*vis_`, change `vis_`, then recompute `viewStart_`.
- **Drag** shifts `viewStart_` by `dx / plotW * vis_`. **Double click** resets to a default range per interval.
- The Y axis auto-fits to the visible candles only. Candles are drawn when a bar is at least 3 px wide, otherwise a line.
- Indicators are computed on the full history so the warm-up is valid inside the window.
- Older crypto history loads lazily when you pan near the left edge (`requestOlder`, Binance paging with `endTime`). The view shifts by the number of prepended bars so it does not jump.

## 5. Data and intervals

- **Crypto:** Binance public klines for 1s to 1M, plus a WebSocket mini-ticker for live prices. A flag `intentionalClose_` prevents a reconnect loop.
- **Stocks, ETFs, indices:** the Yahoo chart endpoint (unofficial) is polled, so the status shows DELAYED. It has no seconds data, and intraday history is limited (1m: 7 days, 5m/15m: 60 days, 1h: about 2 years).
- **4h for stocks** is Yahoo 60m data resampled (`core/Resample.cpp`, buckets aligned to the Unix epoch).
- **Offline:** bundled sample CSVs and a disk cache, so the app still works without a network.
- **EUR display:** the price is multiplied by 1/EURUSDT from Binance. Indices stay in points. Only display values change, the stored data stays in USD.
- Be ready to say: Yahoo is unofficial and can rate-limit. A production app would use a licensed data provider.

## 6. Finance basics

- **Candle (OHLCV):** open, high, low, close, volume for one interval.
- **SMA(n):** the average of the last n closes. **Bollinger:** SMA(20) plus/minus 2 standard deviations.
- **RSI(14):** a momentum oscillator from 0 to 100, using Wilder smoothing. Above 70 is often called overbought, below 30 oversold.
- **MACD(12,26,9):** EMA12 minus EMA26, a signal line (EMA9 of it) and a histogram.
- **Log return:** `ln(P_t / P_{t-1})`. It is additive over time, which is why it is used.
- **Volatility:** the standard deviation of returns. **EWMA volatility** weights recent returns more (lambda = 0.94, the RiskMetrics value).

## 7. The forecast model (`core/Forecast.cpp`)

1. Compute log returns and the EWMA volatility.
2. **Drift** is a shrunk blend: `0.35 * (0.5 * historical mean + 0.5 * trend drift) + momentum drift`. The shrinkage keeps the drift small, because noisy drift estimates are the main error source.
3. **Trend significance:** a linear regression on the last 60 bars gives a t-statistic. `|t| > 2` is called significant in the reasons text.
4. **Monte Carlo:** 2000 paths (deterministic seed 42). Shocks are bootstrapped from the standardised historical residuals ("filtered historical simulation"), so fat tails are kept.
5. From the simulated paths: `probUp` is the share of paths ending above the last price, and the cone shows the 10/25/50/75/90 percentiles per step.
6. **Confidence** depends on the size of the edge (`|probUp - 0.5|`) and on whether trend, momentum and drift agree.
7. **Horizons:** stocks 5 and 63 bars, crypto 7 and 90 bars.

What it is not: a claim to predict prices. The cone mostly measures uncertainty (volatility), and the direction edge is small.

## 8. Backtest and honesty (`core/Backtest.cpp`)

- **Walk-forward:** at each step only data up to that day is used, then the model forecasts, and the result is compared with what happened. This avoids lookahead bias.
- **Hit rate:** the share of correct up/down calls. Compare it with the baseline "always up".
- **Brier score:** the mean of `(p - outcome)^2`. Lower is better, and a constant 50% forecast scores 0.25. The baseline Brier uses the historical up-rate as a constant forecast.
- **Skill** is `1 - Brier / BrierBaseline`. Positive means better than the baseline.
- **Result:** skill is around 0% and sometimes slightly negative (BTC short term about -3%). That is the expected result for liquid markets (weak-form efficiency). The honest answer in an interview is: "The value of the project is the engineering and the honest evaluation, not alpha."
- **Calibration bins:** predicted probability against the observed share of ups. A calibrated model lies on the diagonal.

## 9. Testing, build and CI

- 12 GoogleTest tests for indicators, CSV, resampling, forecast and backtest. Run `build\core_tests.exe`.
- CMake with Ninja. Qt 6.8 (MinGW) on Windows. `windeployqt` produces `dist\`.
- GitHub Actions workflow in `.github/workflows` builds and tests. Check that it is green before you show the repo.

## 10. Bugs you fixed (good stories)

- **WebSocket flapping:** `abort()` emits `disconnected` synchronously, which triggered a reconnect. The fix is an `intentionalClose_` flag.
- **Name collisions:** `y0` is a libm function and `slots` is a Qt macro, so both are reserved in this context.
- **Cut-off window:** the fixed 1440x880 window was larger than a scaled laptop screen. It now sizes itself from `Screen.desktopAvailableWidth/Height` and uses a wrapping toolbar.
- **Stale data:** a sample or cache load must not overwrite fresher network data (checked in `onHistory`).

## 11. Likely questions with short answers

- **Why Qt Quick and not Widgets?** Declarative, GPU-accelerated UI, easy theming and responsive layouts. A custom painted item covers the chart.
- **Why QQuickPaintedItem?** It is simple, and QPainter code is easy to read. The limit is CPU rasterisation, which is fine for thousands of bars.
- **How is the UI kept responsive?** Network is event-driven, analysis runs in QtConcurrent, and repaints from ticks are throttled.
- **How would you scale to many symbols?** Share one WebSocket stream, cache per symbol and interval, and decimate candles when zoomed out.
- **Does it make money?** No, the backtest shows no edge. It is a framework to test ideas honestly.
- **What would you add?** GARCH volatility, intraday forecasts, compare and overlay assets, alerts, export, an installer, and UI tests.
- **Thread safety?** The controller data is only touched on the GUI thread. The worker gets a copy of the close series and returns a result by value.

## 12. Things to say carefully

- Do not say "predicts" or "AI". Say "statistical estimate".
- Do not claim accuracy numbers that are not in the backtest card.
- Mention the data sources and that Yahoo is unofficial.
