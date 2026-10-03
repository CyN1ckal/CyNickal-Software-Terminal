# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc trailing Sharpe. Not an input to the verdict.

Run from the repo root:
    python research/qqq-holdings-obv-divergence/research/posthoc.py
"""

from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from research.kit.metrics import sharpe  # noqa: E402

WINDOW = 126


def _rank(values: list[float]) -> list[float]:
    order = sorted(range(len(values)), key=lambda i: values[i])
    ranks = [0.0] * len(values)
    start = 0
    while start < len(values):
        stop = start
        while stop + 1 < len(values) and values[order[stop + 1]] == values[order[start]]:
            stop += 1
        average = 0.5 * (start + stop) + 1.0
        for index in range(start, stop + 1):
            ranks[order[index]] = average
        start = stop + 1
    return ranks


def _spearman(xs: list[float], ys: list[float]) -> float | None:
    if len(xs) != len(ys) or len(xs) < 2:
        return None
    rx, ry = _rank(xs), _rank(ys)
    n = len(xs)
    mx = sum(rx) / n
    my = sum(ry) / n
    num = sum((a - mx) * (b - my) for a, b in zip(rx, ry))
    dx = sum((a - mx) ** 2 for a in rx) ** 0.5
    dy = sum((b - my) ** 2 for b in ry) ** 0.5
    if dx == 0.0 or dy == 0.0:
        return None
    return num / (dx * dy)


def main() -> None:
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    nets = [float(row["strategy_net"]) for row in rows]
    series = []
    for end in range(WINDOW - 1, len(nets)):
        value = sharpe(nets[end - WINDOW + 1:end + 1])
        series.append({"session": rows[end]["session"], "sharpe": value})
    finite = [row for row in series if row["sharpe"] is not None]
    lowest = min(finite, key=lambda row: row["sharpe"])
    highest = max(finite, key=lambda row: row["sharpe"])
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        trades = list(csv.DictReader(handle))
    years: dict[str, int] = {}
    for trade in trades:
        year = trade["session"][:4]
        years[year] = years.get(year, 0) + 1
    doc = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    grid = doc["grid"]
    is_sharpe = [float(row["is_sharpe"]) for row in grid]
    oos_sharpe = [float(row["oos_sharpe"]) for row in grid]
    primary = next(row for row in grid if row.get("primary") is True)
    winner = max(grid, key=lambda row: float(row["is_sharpe"]))
    payload = {
        "label": "post hoc",
        "note": "Trailing 126-session Sharpe of the primary net path, plus counts read from the saved grid and trades. Not an acceptance input.",
        "rolling_window": WINDOW,
        "rolling_sharpe": series,
        "rolling_sharpe_min": {"session": lowest["session"], "sharpe": lowest["sharpe"]},
        "rolling_sharpe_max": {"session": highest["session"], "sharpe": highest["sharpe"]},
        "rolling_sharpe_positive_share": sum(1 for row in finite if row["sharpe"] > 0.0) / len(finite),
        "entry_year_trades": [{"year": int(year), "trades": years[year]} for year in sorted(years)],
        "grid_is_positive_share": sum(1 for value in is_sharpe if value > 0.0) / len(is_sharpe),
        "primary_is_rank": 1 + sum(1 for value in is_sharpe if value > float(primary["is_sharpe"])),
        "grid_is_oos_spearman": _spearman(is_sharpe, oos_sharpe),
        "is_winner": {
            "params": winner["params"],
            "is_sharpe": winner["is_sharpe"],
            "oos_sharpe": winner["oos_sharpe"],
            "full_return": winner["full_return"],
        },
        "oos_positive_cells": [
            {"params": row["params"], "is_sharpe": row["is_sharpe"], "oos_sharpe": row["oos_sharpe"]}
            for row in grid
            if float(row["oos_sharpe"]) > 0.0
        ],
    }
    (HERE / "posthoc.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"rolling points {len(series)}")


if __name__ == "__main__":
    main()
