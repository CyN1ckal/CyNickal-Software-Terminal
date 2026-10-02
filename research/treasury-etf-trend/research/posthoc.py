# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. Reads daily.csv and trades.csv. Does not open the store.

Nothing written here enters the acceptance table.
"""

import csv
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(r):
    r = np.asarray(r, float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0:
        return None
    return float(r.mean() / sd * np.sqrt(ANNUAL))


def compound(r):
    return float(np.prod(1.0 + np.asarray(r, float)) - 1.0)


def main():
    with (HERE / "daily.csv").open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    dates = [r["date"] for r in rows]
    strat = np.array([float(r["strategy_net"]) for r in rows])
    gross = np.array([float(r["strategy_gross"]) for r in rows])
    bench = np.array([float(r["benchmark"]) for r in rows])
    spy = np.array([float(r["spy_c2c"]) for r in rows])
    oos = np.array([d >= "2024-07-01" for d in dates])

    roll_dates = []
    roll_s = []
    for i in range(ANNUAL - 1, len(strat)):
        roll_dates.append(dates[i])
        roll_s.append(sharpe(strat[i + 1 - ANNUAL:i + 1]))
    roll_a = np.array(roll_s, float)

    order = np.argsort(strat)
    best = order[-20:]
    zero_best = strat.copy()
    zero_best[best] = 0.0
    zero_2022 = strat.copy()
    for i, d in enumerate(dates):
        if d.startswith("2022-"):
            zero_2022[i] = 0.0
    oos_idx = np.flatnonzero(oos)
    oos_r = strat[oos]
    worst_cut = oos_r.copy()
    worst_cut[np.argsort(oos_r)[-10:]] = 0.0

    def corr(a, b):
        if a.std(ddof=1) == 0.0 or b.std(ddof=1) == 0.0:
            return None
        return float(np.corrcoef(a, b)[0, 1])

    pos = strat[strat > 0.0].sum()
    with (HERE / "trades.csv").open(newline="", encoding="utf-8") as f:
        trips = list(csv.DictReader(f))
    oos_side = {}
    for side in ("long", "short"):
        sub = [t for t in trips if t["side"] == side and t["entry_date"] >= "2024-07-01"]
        nets = np.array([float(t["net_dollars"]) for t in sub], float)
        oos_side[side] = {
            "trades": len(sub),
            "net_dollars": float(nets.sum()) if len(nets) else 0.0,
            "win_rate": float(np.mean(nets > 0.0)) if len(nets) else None,
        }
    out = {
        "label": "post hoc",
        "rolling_252": {
            "n": int(len(roll_a)),
            "min": float(roll_a.min()),
            "max": float(roll_a.max()),
            "last": float(roll_a[-1]),
            "frac_positive": float(np.mean(roll_a > 0.0)),
            "dates": roll_dates[::],
            "sharpe": [float(x) for x in roll_a],
        },
        "zero_20_best_days": {
            "total_return": compound(zero_best),
            "sharpe": sharpe(zero_best),
            "sum_of_those_days": float(strat[best].sum()),
            "share_of_positive_day_sum": float(strat[best].sum() / pos) if pos else None,
        },
        "zero_2022": {
            "total_return": compound(zero_2022),
            "sharpe": sharpe(zero_2022),
        },
        "oos_zero_10_best_days": {
            "total_return": compound(worst_cut),
            "sharpe": sharpe(worst_cut),
        },
        "correlation": {
            "full_spy": corr(strat, spy),
            "full_benchmark": corr(strat, bench),
            "oos_spy": corr(strat[oos], spy[oos]),
            "oos_benchmark": corr(strat[oos], bench[oos]),
            "full_gross_vs_net": corr(strat, gross),
        },
        "oos_by_side": oos_side,
    }
    # The rolling path is long. Keep it; charts.py reads it. Also write a short
    # summary block that the report quotes, which is the same object.
    (HERE / "posthoc.json").write_text(json.dumps(out) + "\n", encoding="utf-8", newline="\n")
    print(
        f"rolling last {out['rolling_252']['last']:.3f} "
        f"frac>0 {out['rolling_252']['frac_positive']:.3f} "
        f"zero20 sharpe {out['zero_20_best_days']['sharpe']:.3f} "
        f"zero2022 return {out['zero_2022']['total_return']:.3f}"
    )


if __name__ == "__main__":
    main()
