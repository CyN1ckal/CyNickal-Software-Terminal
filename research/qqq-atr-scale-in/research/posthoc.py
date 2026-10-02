# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. None of these enter the verdict.

    python research/qqq-atr-scale-in/research/posthoc.py
"""
from __future__ import annotations

import csv
import json
import math
from datetime import date
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(r: np.ndarray) -> float:
    if len(r) < 2:
        return float("nan")
    sd = float(r.std(ddof=1))
    if sd <= 0:
        return float("nan")
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total(r: np.ndarray) -> float:
    return float(np.prod(1.0 + r) - 1.0)


def zeroed(r: np.ndarray, idx: np.ndarray) -> dict:
    x = r.copy()
    x[idx] = 0.0
    return {"sharpe": sharpe(x), "total_return": total(x)}


def main() -> None:
    daily = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    days = [date.fromisoformat(row["session"]) for row in daily]
    r = np.array([float(row["qqq_ret"]) for row in daily])
    trades = [row for row in csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")) if row["symbol"] == "QQQ"]
    dollars = np.array([float(row["net_dollars"]) for row in trades]) if trades else np.zeros(0)

    roll_dates = []
    roll_values = []
    if len(r) >= ANNUAL:
        for i in range(ANNUAL - 1, len(r)):
            roll_dates.append(days[i].isoformat())
            roll_values.append(sharpe(r[i - ANNUAL + 1: i + 1]))
    roll = np.array(roll_values) if roll_values else np.zeros(0)

    order = np.argsort(-r, kind="mergesort")  # best day first
    worst = np.argsort(r, kind="mergesort")
    drop_best = {str(k): zeroed(r, order[:k]) for k in (5, 10, 20)}
    drop_worst = {str(k): zeroed(r, worst[:k]) for k in (5, 10, 20)}

    trail = r[-ANNUAL:] if len(r) >= ANNUAL else r
    may = np.array([days[i] >= date(2025, 5, 1) for i in range(len(days))])
    april = np.array([date(2025, 4, 1) <= days[i] <= date(2025, 4, 30) for i in range(len(days))])

    gross_sum = float(dollars.sum()) if len(dollars) else 0.0
    worst_camps = np.argsort(dollars, kind="mergesort")[:5] if len(dollars) else []
    worst_share = float(dollars[worst_camps].sum() / gross_sum) if gross_sum != 0 and len(dollars) else None

    # Longest run of negative sessions, and of non-positive sessions.
    longest_neg = run = 0
    for value in r:
        if value < 0:
            run += 1
            longest_neg = max(longest_neg, run)
        else:
            run = 0

    out = {
        "label": "post hoc",
        "rolling_252": {
            "dates": roll_dates,
            "sharpe": roll_values,
            "share_positive": float(np.mean(roll > 0)) if len(roll) else None,
            "min": float(roll.min()) if len(roll) else None,
            "max": float(roll.max()) if len(roll) else None,
            "last": float(roll[-1]) if len(roll) else None,
            "last_date": roll_dates[-1] if roll_dates else None,
        },
        "zero_best_days": drop_best,
        "zero_worst_days": drop_worst,
        "trailing_252": {
            "sessions": int(len(trail)),
            "sharpe": sharpe(trail),
            "total_return": total(trail),
            "end": days[-1].isoformat(),
        },
        "from_2025_05_01": {
            "sessions": int(may.sum()),
            "sharpe": sharpe(r[may]),
            "total_return": total(r[may]),
        },
        "april_2025": {
            "sessions": int(april.sum()),
            "total_return": total(r[april]) if april.any() else None,
            "sharpe": sharpe(r[april]) if int(april.sum()) >= 2 else None,
        },
        "worst_5_campaigns_share_of_net_dollars": worst_share,
        "longest_negative_session_run": longest_neg,
        "full_sharpe_for_reference": sharpe(r),
        "full_return_for_reference": total(r),
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"trailing 252 sharpe {out['trailing_252']['sharpe']:.4f}  "
          f"from May 2025 {out['from_2025_05_01']['sharpe']:.4f}")


if __name__ == "__main__":
    main()
