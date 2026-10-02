# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent re-implementation of backtest.py, in pure Python with no NumPy.

Rebuilds the sleeve series from the source CSVs, re-runs the in-sample grid search
for A and B with plain loops, rescales, and recomputes every daily portfolio return.
Every weight and every daily return in daily.csv must match within 1e-10.

    python research/qqq-strategy-portfolio/research/verify.py
"""
from __future__ import annotations

import csv
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
RS = Path(__file__).resolve().parents[2]


def rows(rel):
    with open(RS / rel, newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


turtle = rows("qqq-15m-turtle-overnight/research/daily.csv")
days = [r["session"] for r in turtle]
pos = {d: i for i, d in enumerate(days)}
is_day = [d <= "2024-06-28" for d in days]


def column(rel, key, col):
    out = [0.0] * len(days)
    for r in rows(rel):
        if r[key] in pos:
            out[pos[r[key]]] = float(r[col])
    return out


r1 = {
    "Q": [float(r["bench_qqq_ret"]) for r in turtle],
    "P1": column("qqq-intraday-trend/research/daily.csv", "session", "p1"),
    "T": [float(r["qqq_ret"]) for r in turtle],
    "C": column("intraday-channel-trend/research/daily.csv", "session", "r_book"),
}
cost = {"Q": [0.0] * len(days), "P1": [0.0] * len(days), "C": [0.0] * len(days),
        "T": [float(r["qqq_ret"]) - float(r["qqq_2bp_ret"]) for r in turtle]}
for r in rows("qqq-intraday-trend/research/trades_p1.csv"):
    if r["session"] in pos:
        cost["P1"][pos[r["session"]]] += 0.0002
for r in rows("intraday-channel-trend/research/trades.csv"):
    if r["session"] in pos:
        cost["C"][pos[r["session"]]] += 0.0002 / 3


def mean(x):
    return sum(x) / len(x)


def sd(x):
    m = mean(x)
    return math.sqrt(sum((v - m) ** 2 for v in x) / (len(x) - 1))


def search(names):
    """Brute force over the 0.01 simplex, using IS means and covariances."""
    cols = [[r1[n][i] for i in range(len(days)) if is_day[i]] for n in names]
    mu = [mean(c) for c in cols]
    k = len(names)
    cov = [[sum((cols[a][t] - mu[a]) * (cols[b][t] - mu[b]) for t in range(len(cols[0]))) / (len(cols[0]) - 1)
            for b in range(k)] for a in range(k)]
    best, best_s = None, -1e300

    def walk(prefix, left):
        nonlocal best, best_s
        if len(prefix) == k - 1:
            w = [p / 100 for p in prefix + [left]]
            num = sum(w[a] * mu[a] for a in range(k))
            var = sum(w[a] * w[b] * cov[a][b] for a in range(k) for b in range(k))
            if var > 0:
                s = num / math.sqrt(var) * math.sqrt(252)
                if s > best_s + 1e-12:
                    best, best_s = w, s
            return
        for a in range(left + 1):
            walk(prefix + [a], left - a)

    walk([], 100)
    return best


def daily(names, w, mult=1.0):
    out = []
    for i in range(len(days)):
        rs = [r1[n][i] - (mult - 1) * cost[n][i] for n in names]
        g = sum(wi * ri for wi, ri in zip(w, rs))
        reb = sum(wi * abs(ri - g) for n, wi, ri in zip(names, w, rs) if n in ("Q", "T"))
        out.append(g - mult * 1e-4 * reb)
    return out


vol_q = sd([r1["Q"][i] for i in range(len(days)) if is_day[i]]) * math.sqrt(252)
res = json.loads((HERE / "results.json").read_text())
with open(HERE / "daily.csv", newline="", encoding="utf-8") as fh:
    published = list(csv.DictReader(fh))

bad = []
for label, names in (("A", ["Q", "P1", "T", "C"]), ("B", ["Q", "P1"])):
    d = search(names)
    overnight = sum(di for n, di in zip(names, d) if n in ("Q", "T"))
    vol = sd([sum(di * r1[n][i] for n, di in zip(names, d)) for i in range(len(days)) if is_day[i]]) * math.sqrt(252)
    s = min(1.0 / overnight if overnight else math.inf, 2.0 / sum(d), vol_q / vol)
    w = [s * di for di in d]
    if any(abs(a - b) > 1e-10 for a, b in zip(w, res["portfolios"][label]["w"])):
        bad.append(f"{label} weights {w} vs {res['portfolios'][label]['w']}")
    series = daily(names, w)
    diffs = [abs(series[i] - float(published[i][label])) for i in range(len(days))]
    if max(diffs) > 1e-10:
        bad.append(f"{label} daily max diff {max(diffs)}")
    oos = daily(names, w, 2.0)[sum(is_day):]
    s2 = mean(oos) / sd(oos) * math.sqrt(252)
    if abs(s2 - res["cost_sweep"][label]["2.0"]["oos_sharpe"]) > 1e-9:
        bad.append(f"{label} 2x OOS Sharpe {s2}")
    print(f"{label}: d={[round(x, 2) for x in d]} w={[round(x, 4) for x in w]} daily max diff {max(diffs):.2e}, 2x OOS Sharpe {s2:.4f}")

n_series = daily(["Q", "P1"], [1.0, 1.0])
if max(abs(n_series[i] - float(published[i]["N"])) for i in range(len(days))) > 1e-10:
    bad.append("N daily mismatch")

if bad:
    sys.exit("MISMATCH: " + "; ".join(bad))
print(f"verify: A, B, and N match on all {len(days)} sessions; weights and 2x-cost OOS Sharpe match")
