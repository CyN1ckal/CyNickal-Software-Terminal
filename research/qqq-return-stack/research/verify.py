# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent pure-Python replay of qqq-return-stack. No NumPy, no shared code.

Rebuilds each sleeve's return series, IS volatility, weight, overnight exposure,
and 1x cost from the source files, then recomputes the daily returns of S, K, and
ALL and the 2x-cost OOS Sharpe of S and K. Compares with daily.csv and
results.json.

    python research/qqq-return-stack/research/verify.py
"""
import csv
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
R = Path(__file__).resolve().parents[3] / "research"
TOL = 1e-10


def rows(rel):
    with open(R / rel, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


turtle = rows("qqq-15m-turtle-overnight/research/daily.csv")
days = [t["session"] for t in turtle]
pos = {s: i for i, s in enumerate(days)}
N = len(days)
IS = [s < "2024-07-01" for s in days]


def series(rel, dcol, col, keep=lambda row: True):
    ret, live = [0.0] * N, [False] * N
    for row in rows(rel):
        i = pos.get(row[dcol])
        if i is not None and keep(row):
            ret[i], live[i] = float(row[col]), True
    return ret, live


spec = {
    "P1": ("qqq-intraday-trend/research/daily.csv", "session", "p1"),
    "T": ("qqq-15m-turtle-overnight/research/daily.csv", "session", "qqq_ret"),
    "C": ("intraday-channel-trend/research/daily.csv", "session", "r_book"),
    "F": ("igv-small-account-fade/research/daily.csv", "session", "strategy"),
    "M": ("micro-futures-trend/research/daily.csv", "date", "ret"),
    "POP": ("index-opening-pop-fade/research/daily.csv", "session", "primary_net"),
    "SCALE": ("qqq-atr-scale-in/research/daily.csv", "session", "qqq_ret"),
    "MART": ("qqq-atr-martingale/research/daily.csv", "session", "pnl"),
    "BOLL": ("qqq-bollinger-adding/research/daily.csv", "session", "qqq_net"),
    "GAP": ("small-cap-gap-up-fade/research/daily.csv", "session", "ret"),
    "REV": ("low-liq-high-vol-mean-reversion/research/daily.csv", "session", "ret"),
    "RSI2": ("spy-rsi2-dip-buy/research/daily.csv", "date", "strategy_net"),
}
q = [float(t["bench_qqq_ret"]) for t in turtle]
ret, live = {}, {}
for k, (rel, dc, col) in spec.items():
    keep = (lambda row: row["in_window"] == "1") if k == "M" else (lambda row: True)
    ret[k], live[k] = series(rel, dc, col, keep)


def sd(v):
    m = sum(v) / len(v)
    return math.sqrt(sum((a - m) ** 2 for a in v) / (len(v) - 1))


def sharpe(v):
    return sum(v) / len(v) / sd(v) * math.sqrt(252)


w = {}
for k in spec:
    v = [ret[k][i] for i in range(N) if IS[i] and live[k][i]]
    w[k] = 0.05 / (sd(v) * math.sqrt(252))

# overnight exposure
e = {k: [0.0] * N for k in spec}
for t in rows("qqq-15m-turtle-overnight/research/trades.csv"):
    if t["symbol"] == "QQQ":
        sg = 1.0 if t["side"] == "long" else -1.0
        for i, s in enumerate(days):
            if t["entry_time"][:10] <= s < t["exit_time"][:10]:
                e["T"][i] += sg
for row in rows("qqq-atr-martingale/research/daily.csv"):
    if row["session"] in pos:
        e["MART"][pos[row["session"]]] = int(row["side"]) * float(row["gross_notional"])
for row in rows("spy-rsi2-dip-buy/research/daily.csv"):
    e["RSI2"][pos[row["date"]]] = float(row["held_at_close"])

# 1x costs
x = {k: [0.0] * N for k in spec}


def bump(k, s, v):
    if s in pos:
        x[k][pos[s]] += v


for t in rows("qqq-intraday-trend/research/trades_p1.csv"):
    bump("P1", t["session"], 0.0002)
for i, t in enumerate(turtle):
    x["T"][i] = float(t["qqq_ret"]) - float(t["qqq_2bp_ret"])
for t in rows("intraday-channel-trend/research/trades.csv"):
    bump("C", t["session"], 0.0002 / 3)
eq_prev, last = {}, 1.0
for row in rows("igv-small-account-fade/research/daily.csv"):
    eq_prev[row["session"]], last = last, float(row["equity"])
for t in rows("igv-small-account-fade/research/trades.csv"):
    if t["session"] in pos:
        bump("F", t["session"], (float(t["gross_dollars"]) - float(t["net_dollars"])) / 25000.0 / eq_prev[t["session"]])
for row in rows("micro-futures-trend/research/daily.csv"):
    if row["in_window"] == "1":
        bump("M", row["date"], float(row["gross_ret"]) - float(row["ret"]))
for row in rows("index-opening-pop-fade/research/daily.csv"):
    bump("POP", row["session"], float(row["primary_gross"]) - float(row["primary_net"]))
for t in rows("qqq-atr-scale-in/research/trades.csv"):
    bump("SCALE", t["session"], 0.0002 * int(t["n_units"]) / 3)
for t in rows("qqq-atr-martingale/research/trades.csv"):
    bump("MART", t["exit_session"], 0.0002 * sum(map(float, t["leg_notionals"].split(";"))))
for row in rows("qqq-bollinger-adding/research/daily.csv"):
    bump("BOLL", row["session"], float(row["qqq_gross"]) - float(row["qqq_net"]))
for t in rows("small-cap-gap-up-fade/research/trades.csv"):
    bump("GAP", t["entry_session"], float(t["weight"]) * (float(t["gross_ret"]) - float(t["net_ret"])))
eq_prev, last = {}, 1.0
for row in rows("low-liq-high-vol-mean-reversion/research/daily.csv"):
    eq_prev[row["session"]], last = last, float(row["equity"])
for t in rows("low-liq-high-vol-mean-reversion/research/trades.csv"):
    if t["exit_session"] in pos:
        bump("REV", t["exit_session"], (float(t["cost"]) + float(t["borrow"])) / eq_prev[t["exit_session"]])
for row in rows("spy-rsi2-dip-buy/research/daily.csv"):
    bump("RSI2", row["date"], float(row["strategy_gross"]) - float(row["strategy_net"]))


def book(keys, k=1.0):
    out = []
    for i in range(N):
        g = q[i] + sum(w[s] * (ret[s][i] - (k - 1) * x[s][i]) for s in keys)
        lev = 1.0 + sum(w[s] * e[s][i] for s in keys)
        out.append(g - k * 1e-4 * abs(q[i] - g) - 0.05 / 252 * max(0.0, lev - 1.0))
    return out


S = ("P1", "T", "C", "MART", "GAP", "REV", "RSI2")
K = ("P1", "MART", "GAP", "RSI2")
ALL = tuple(spec)
res = json.load(open(HERE / "results.json", encoding="utf-8"))
published = rows("qqq-return-stack/research/daily.csv")
bad = []
for k in spec:
    if abs(w[k] - res["scales"][k]["w"]) > TOL:
        bad.append(f"w {k}: {w[k]} vs {res['scales'][k]['w']}")
worst = 0.0
for name, keys in (("S", S), ("K", K), ("ALL", ALL)):
    mine = book(keys)
    for i, row in enumerate(published):
        diff = abs(mine[i] - float(row[name]))
        worst = max(worst, diff)
        if diff > TOL:
            bad.append(f"{name} {row['session']}: {mine[i]} vs {row[name]}")
            break
oos = [i for i in range(N) if not IS[i]]
for name, keys in (("S", S), ("K", K)):
    v = book(keys, k=2.0)
    s2 = sharpe([v[i] for i in oos])
    if abs(s2 - res["cost_sweep"][name]["2.0"]["oos_sharpe"]) > 1e-9:
        bad.append(f"2x cost {name}: {s2} vs {res['cost_sweep'][name]['2.0']['oos_sharpe']}")
    print(f"{name} 2x-cost OOS Sharpe {s2:.6f}")
if bad:
    sys.exit("VERIFY FAILED:\n  " + "\n  ".join(bad))
print(f"verify: 12 weights, {3 * N} daily returns (max abs diff {worst:.2e}) and 2 cost-stressed Sharpes match")
