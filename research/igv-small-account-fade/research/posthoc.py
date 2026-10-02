# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. They do not change the verdict.

Reads daily.csv and trades.csv only. Does not open the store.

    python research/igv-small-account-fade/research/posthoc.py
"""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(r: np.ndarray) -> float:
    sd = float(r.std(ddof=1))
    if sd <= 0 or len(r) < 2:
        return float("nan")
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total_return(r: np.ndarray) -> float:
    return float(np.prod(1.0 + r) - 1.0)


def ready(value):
    if isinstance(value, float) and (math.isnan(value) or math.isinf(value)):
        return None
    return value


def main() -> None:
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    sessions = [row["session"] for row in rows]
    r = np.array([float(row["strategy"]) for row in rows])
    rolling = []
    for end in range(ANNUAL - 1, len(r)):
        rolling.append({
            "session": sessions[end],
            "sharpe": sharpe(r[end - ANNUAL + 1:end + 1]),
        })
    values = np.array([point["sharpe"] for point in rolling])
    best = np.argsort(r)[-20:]
    worst = np.argsort(r)[:20]
    zeroed = r.copy()
    zeroed[best] = 0.0
    trade_rows = list(csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")))
    payload = {
        "label": "post hoc",
        "rolling_252": rolling,
        "rolling_252_positive_share": float(np.mean(values > 0)) if len(values) else None,
        "rolling_252_min": ready(float(np.min(values))) if len(values) else None,
        "rolling_252_max": ready(float(np.max(values))) if len(values) else None,
        "trailing_252_sharpe": ready(float(values[-1])) if len(values) else None,
        "drop_20_best_days": {
            "sharpe": ready(sharpe(zeroed)),
            "total_return": total_return(zeroed),
            "sessions_zeroed": [sessions[i] for i in sorted(best)],
        },
        "worst_20_day_sum": float(r[worst].sum()),
        "all_day_sum": float(r.sum()),
        "trades_per_252_sessions": len(trade_rows) * ANNUAL / len(r),
    }
    (HERE / "posthoc.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(
        f"rolling positive share {payload['rolling_252_positive_share']:.3f} "
        f"min {payload['rolling_252_min']:.2f} trailing {payload['trailing_252_sharpe']:.2f}"
    )
    print(
        f"drop 20 best sharpe {payload['drop_20_best_days']['sharpe']:.2f} "
        f"return {payload['drop_20_best_days']['total_return']:.2%}"
    )


if __name__ == "__main__":
    main()
