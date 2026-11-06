"""Download daily candles for the bundled offline demo data (data/sample/*.csv).

Binance public API for crypto, Yahoo chart endpoint for stocks/ETFs (unofficial).
Output format: time,open,high,low,close,volume (Unix seconds).
"""
import json
import pathlib
import urllib.request

OUT = pathlib.Path(__file__).resolve().parent.parent / "data" / "sample"
UA = {"User-Agent": "Mozilla/5.0"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=30) as r:
        return json.load(r)


def binance(symbol, pages=3):
    rows, end = [], None
    for _ in range(pages):
        url = f"https://data-api.binance.vision/api/v3/klines?symbol={symbol}&interval=1d&limit=1000"
        if end:
            url += f"&endTime={end}"
        data = get(url)
        if not data:
            break
        rows = data + rows
        end = data[0][0] - 1
    seen, out = set(), []
    for k in rows:
        t = k[0] // 1000
        if t not in seen:
            seen.add(t)
            out.append((t, k[1], k[2], k[3], k[4], k[5]))
    return sorted(out)


def yahoo(symbol):
    d = get(f"https://query1.finance.yahoo.com/v8/finance/chart/{symbol}?range=5y&interval=1d")
    r = d["chart"]["result"][0]
    q = r["indicators"]["quote"][0]
    out = []
    for i, t in enumerate(r["timestamp"]):
        vals = [q[k][i] for k in ("open", "high", "low", "close")]
        if any(v is None for v in vals):
            continue
        out.append((t, *vals, q["volume"][i] or 0))
    return out


def write(name, rows):
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / f"{name}.csv", "w", newline="\n") as f:
        f.write("time,open,high,low,close,volume\n")
        for r in rows:
            f.write(",".join(str(x) for x in r) + "\n")
    print(name, len(rows))


if __name__ == "__main__":
    write("BTC", binance("BTCUSDT"))
    write("ETH", binance("ETHUSDT"))
    write("SOL", binance("SOLUSDT"))
    for s, n in (("SPY", "SPY"), ("QQQ", "QQQ"), ("^GSPC", "GSPC"), ("AAPL", "AAPL"), ("NVDA", "NVDA")):
        write(n, yahoo(s))
