# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the store run. Reads output files only. Not the verdict."""

from __future__ import annotations

import csv
import json
import math
from datetime import date
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(values):
    arr = np.asarray(values, dtype=float)
    if arr.size < 2:
        return None
    sd = float(arr.std(ddof=1))
    if sd == 0.0 or not math.isfinite(sd):
        return None
    return float(arr.mean() / sd * math.sqrt(ANNUAL))


def total_return(values):
    arr = np.asarray(values, dtype=float)
    if arr.size == 0:
        return 0.0
    return float(np.prod(1.0 + arr) - 1.0)


def py(value):
    if value is None:
        return None
    if isinstance(value, float) and not math.isfinite(value):
        return None
    return value


def main():
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        daily = list(csv.DictReader(handle))
    dates = [date.fromisoformat(row["date"]) for row in daily]
    net = np.array([float(row["strategy_net"]) for row in daily])
    gld = np.array([float(row["gld_bh"]) for row in daily])
    slv = np.array([float(row["slv_bh"]) for row in daily])
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        trades = list(csv.DictReader(handle))
    nets = np.array([float(row["net_pnl"]) for row in trades])
    winners = nets[nets > 0]
    losers = nets[nets < 0]
    fill_dates = {row["entry_date"] for row in trades} | {row["exit_date"] for row in trades}
    years = len(net) / ANNUAL

    roll_end = []
    roll_s = []
    roll_g = []
    roll_v = []
    for i in range(ANNUAL - 1, len(net)):
        window = slice(i - ANNUAL + 1, i + 1)
        roll_end.append(dates[i].isoformat())
        roll_s.append(sharpe(net[window]))
        roll_g.append(sharpe(gld[window]))
        roll_v.append(sharpe(slv[window]))
    with (HERE / "rolling_sharpe.csv").open("w", encoding="utf-8", newline="\n") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["date", "strategy", "gld", "slv"])
        for row in zip(roll_end, roll_s, roll_g, roll_v):
            writer.writerow([row[0], "" if row[1] is None else format(row[1], ".17g"),
                             "" if row[2] is None else format(row[2], ".17g"),
                             "" if row[3] is None else format(row[3], ".17g")])
    finite = [value for value in roll_s if value is not None]

    order = np.argsort(net, kind="mergesort")
    worst = order[:10]
    best = order[-10:][::-1]

    def zeroed(index):
        altered = net.copy()
        altered[index] = 0.0
        return {"total_return": total_return(altered), "sharpe": sharpe(altered)}

    last = slice(-ANNUAL, None)
    doc = {
        "rolling_252": {
            "windows": len(roll_s),
            "first_end": roll_end[0] if roll_end else None,
            "last_end": roll_end[-1] if roll_end else None,
            "fraction_positive": (sum(1 for value in finite if value > 0.0) / len(finite)) if finite else None,
            "min": min(finite) if finite else None,
            "max": max(finite) if finite else None,
        },
        "concentration": {
            "full_return": total_return(net),
            "without_10_best": zeroed(best),
            "without_10_worst": zeroed(worst),
            "best_days": [{"date": dates[i].isoformat(), "strategy_net": float(net[i])} for i in best],
            "worst_days": [{"date": dates[i].isoformat(), "strategy_net": float(net[i])} for i in worst],
        },
        "correlation": {
            "strategy_gld": float(np.corrcoef(net, gld)[0, 1]),
            "strategy_slv": float(np.corrcoef(net, slv)[0, 1]),
            "gld_slv": float(np.corrcoef(gld, slv)[0, 1]),
        },
        "last_252": {
            "start": dates[-ANNUAL].isoformat(),
            "end": dates[-1].isoformat(),
            "strategy_return": total_return(net[last]),
            "strategy_sharpe": sharpe(net[last]),
            "gld_return": total_return(gld[last]),
            "gld_sharpe": sharpe(gld[last]),
            "slv_return": total_return(slv[last]),
            "slv_sharpe": sharpe(slv[last]),
        },
        "descriptive": {
            "trades_per_year": len(trades) / years if years else None,
            "sessions_with_a_fill": len(fill_dates),
            "avg_winner": float(winners.mean()) if winners.size else None,
            "avg_loser": float(losers.mean()) if losers.size else None,
            "winners": int(winners.size),
            "losers": int(losers.size),
            "zeros": int(np.sum(nets == 0.0)),
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(
        f"rolling {doc['rolling_252']['fraction_positive']:.3f} positive, "
        f"without 10 best {doc['concentration']['without_10_best']['total_return']:.3f}"
    )


if __name__ == "__main__":
    main()
