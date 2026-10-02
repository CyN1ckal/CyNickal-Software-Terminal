# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the primary run. Reads daily.csv only. Not a verdict input."""

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252
WINDOW = 252


def sharpe(values) -> float | None:
    r = np.asarray(values, dtype=float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0:
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total_return(values) -> float:
    return float(np.prod(1.0 + np.asarray(values, dtype=float)) - 1.0)


def main() -> None:
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        trades = list(csv.DictReader(handle))
    dates = [row["date"] for row in rows]
    strat = np.array([float(row["strategy_net"]) for row in rows])
    bench = np.array([float(row["benchmark"]) for row in rows])
    gross = np.array([float(row["strategy_gross"]) for row in rows])

    roll_s, roll_b = [], []
    for i in range(WINDOW, len(strat) + 1):
        roll_s.append(sharpe(strat[i - WINDOW:i]))
        roll_b.append(sharpe(bench[i - WINDOW:i]))
    roll_s = np.array(roll_s, dtype=float)
    roll_b = np.array(roll_b, dtype=float)
    finite = roll_s[np.isfinite(roll_s)]

    order = np.argsort(strat)
    worst = order[:10]
    best = order[-10:][::-1]
    drop_best = strat.copy()
    drop_worst = strat.copy()
    drop_best[best] = 0.0
    drop_worst[worst] = 0.0

    out = {
        "label": "post hoc",
        "rolling_sharpe": {
            "window": WINDOW,
            "dates": dates[WINDOW - 1:],
            "strategy": roll_s.tolist(),
            "benchmark": roll_b.tolist(),
            "fraction_positive": float(np.mean(finite > 0.0)) if len(finite) else None,
            "min": float(np.min(finite)) if len(finite) else None,
            "max": float(np.max(finite)) if len(finite) else None,
        },
        "concentration": {
            "full_return": total_return(strat),
            "return_without_10_best_days": total_return(drop_best),
            "return_without_10_worst_days": total_return(drop_worst),
            "sharpe_without_10_best_days": sharpe(drop_best),
            "sharpe_without_10_worst_days": sharpe(drop_worst),
            "best_days": [
                {"date": dates[int(i)], "strategy_net": float(strat[int(i)])} for i in best
            ],
            "worst_days": [
                {"date": dates[int(i)], "strategy_net": float(strat[int(i)])} for i in worst
            ],
        },
        "correlation_with_benchmark": float(np.corrcoef(strat, bench)[0, 1]),
        "correlation_gross_with_benchmark": float(np.corrcoef(gross, bench)[0, 1]),
        "last_252": {
            "start": dates[-WINDOW],
            "end": dates[-1],
            "return_": total_return(strat[-WINDOW:]),
            "sharpe": sharpe(strat[-WINDOW:]),
            "benchmark_return": total_return(bench[-WINDOW:]),
            "benchmark_sharpe": sharpe(bench[-WINDOW:]),
        },
        "oos_trades_by_name": {},
        "end_trades": [],
    }
    for row in trades:
        if row["sample"] == "OOS":
            slot = out["oos_trades_by_name"].setdefault(
                row["symbol"], {"trades": 0, "net": 0.0, "gross": 0.0})
            slot["trades"] += 1
            slot["net"] += float(row["net_pnl"])
            slot["gross"] += float(row["gross_pnl"])
        if row["exit_reason"] == "end":
            out["end_trades"].append({
                "symbol": row["symbol"],
                "side": row["side"],
                "entry_date": row["entry_date"],
                "sample": row["sample"],
                "gross_pnl": float(row["gross_pnl"]),
                "net_pnl": float(row["net_pnl"]),
            })
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(
        f"posthoc rolling positive {out['rolling_sharpe']['fraction_positive']:.3f} "
        f"corr {out['correlation_with_benchmark']:.3f}"
    )


if __name__ == "__main__":
    main()
