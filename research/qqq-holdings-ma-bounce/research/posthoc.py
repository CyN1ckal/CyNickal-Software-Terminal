# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc descriptions. Does not open the store and does not change the verdict.

Run from the repo root:

    python research/qqq-holdings-ma-bounce/research/posthoc.py
"""

from __future__ import annotations

import csv
import json
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent


def sharpe(values: np.ndarray):
    if len(values) < 2:
        return None
    deviation = float(values.std(ddof=1))
    if deviation == 0.0:
        return None
    return float(values.mean() / deviation * np.sqrt(252.0))


def total_return(values: np.ndarray) -> float:
    return float(np.prod(1.0 + values) - 1.0)


def spearman(left: np.ndarray, right: np.ndarray) -> float:
    left_rank = np.argsort(np.argsort(left)).astype(float)
    right_rank = np.argsort(np.argsort(right)).astype(float)
    if len(left) < 2 or float(left_rank.std()) == 0.0 or float(right_rank.std()) == 0.0:
        return float("nan")
    return float(np.corrcoef(left_rank, right_rank)[0, 1])


def main() -> None:
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        daily = list(csv.DictReader(handle))
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    net = np.array([float(row["strategy_net"]) for row in daily])
    dates = [row["session"] for row in daily]
    rolling = []
    for end in range(125, len(net)):
        rolling.append({
            "session": dates[end],
            "sharpe": sharpe(net[end - 125:end + 1]),
        })
    order = np.argsort(-net, kind="mergesort")
    dropped = net.copy()
    dropped[order[:10]] = 0.0
    positive = net[net > 0]
    top10 = net[order[:10]]
    grid = results["grid"]
    is_sharpe = np.array([np.nan if row["is_sharpe"] is None else row["is_sharpe"] for row in grid], dtype=float)
    oos_sharpe = np.array([np.nan if row["oos_sharpe"] is None else row["oos_sharpe"] for row in grid], dtype=float)
    finite = [row for row in rolling if row["sharpe"] is not None]
    worst_window = min(finite, key=lambda row: row["sharpe"]) if finite else None
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        trade_net = [float(row["net"]) for row in csv.DictReader(handle)]
    payload = {
        "label": "post hoc",
        "rolling_sharpe": rolling,
        "rolling_window": 126,
        "rolling_windows": len(rolling),
        "rolling_windows_positive": sum(1 for row in finite if row["sharpe"] > 0),
        "rolling_sharpe_min": None if worst_window is None else worst_window["sharpe"],
        "rolling_sharpe_min_session": None if worst_window is None else worst_window["session"],
        "rolling_sharpe_max": None if not finite else max(row["sharpe"] for row in finite),
        "rolling_sharpe_last": None if not rolling else rolling[-1]["sharpe"],
        "rolling_sharpe_last_session": None if not rolling else rolling[-1]["session"],
        "best_10_sessions": [
            {"session": dates[int(index)], "strategy_net": float(net[int(index)])} for index in order[:10]
        ],
        "full_return_without_best_10": total_return(dropped),
        "full_sharpe_without_best_10": sharpe(dropped),
        "best_10_share_of_positive_days": None if positive.sum() == 0 else float(top10[top10 > 0].sum() / positive.sum()),
        "worst_trade_net": None if not trade_net else min(trade_net),
        "best_trade_net": None if not trade_net else max(trade_net),
        "grid_is_oos_spearman": spearman(is_sharpe, oos_sharpe),
    }
    (HERE / "posthoc.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"rolling points {len(rolling)}")


if __name__ == "__main__":
    main()
