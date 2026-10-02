# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. None of these enter the verdict.

    python research/qqq-atr-martingale/research/posthoc.py
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
    if not sd:
        return float("nan")
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total(r: np.ndarray) -> float:
    return float(r.sum())


def zeroed(r: np.ndarray, idx: np.ndarray) -> dict:
    x = r.copy()
    x[idx] = 0.0
    return {"sharpe": sharpe(x), "pnl": total(x)}


def main() -> None:
    daily = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    days = [date.fromisoformat(row["session"]) for row in daily]
    r = np.array([float(row["pnl"]) for row in daily])
    trades = [row for row in csv.DictReader((HERE / "trades.csv").open(encoding="utf-8"))
              if row["symbol"] == "QQQ" and row["exit_reason"] == "breakeven"]
    nets = np.array([float(row["realized_net"]) for row in trades])
    units = np.array([int(row["n_units"]) for row in trades])
    big = units >= 4
    roll_dates, roll_values = [], []
    if len(r) >= ANNUAL:
        for i in range(ANNUAL - 1, len(r)):
            roll_dates.append(days[i].isoformat())
            roll_values.append(sharpe(r[i - ANNUAL + 1: i + 1]))
    roll = np.array(roll_values) if roll_values else np.zeros(0)
    order = np.argsort(-r, kind="mergesort")
    worst = np.argsort(r, kind="mergesort")
    may = np.array([d >= date(2025, 5, 1) for d in days])
    trail = r[-ANNUAL:]
    out = {
        "label": "post hoc",
        "rolling_252": {
            "dates": roll_dates, "sharpe": roll_values,
            "share_positive": float(np.mean(roll > 0)) if len(roll) else None,
            "min": float(roll.min()) if len(roll) else None,
            "max": float(roll.max()) if len(roll) else None,
            "last": float(roll[-1]) if len(roll) else None,
            "last_date": roll_dates[-1] if roll_dates else None,
        },
        "zero_best_days": {str(k): zeroed(r, order[:k]) for k in (5, 10, 20)},
        "zero_worst_days": {str(k): zeroed(r, worst[:k]) for k in (5, 10, 20)},
        "trailing_252": {"sessions": int(len(trail)), "sharpe": sharpe(trail), "pnl": total(trail),
                         "end": days[-1].isoformat()},
        "from_2025_05_01": {"sessions": int(may.sum()), "sharpe": sharpe(r[may]), "pnl": total(r[may])},
        "campaigns_with_4_or_more_units": {
            "n": int(big.sum()),
            "realized": float(nets[big].sum()) if big.any() else 0.0,
            "share_of_realized": float(nets[big].sum() / nets.sum()) if nets.sum() else None,
        },
        "one_unit_share_of_campaigns": float(np.mean(units == 1)) if len(units) else None,
        "full_pnl_for_reference": total(r),
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"trailing {out['trailing_252']['sharpe']:.3f}  "
          f"4+ unit share {out['campaigns_with_4_or_more_units']['share_of_realized']:.3f}")


if __name__ == "__main__":
    main()
