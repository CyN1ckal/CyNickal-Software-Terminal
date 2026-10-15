# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Trailing Sharpe from daily.csv. Post hoc. Does not read the store or change the verdict."""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
WINDOW = 252


def _sharpe(values: list[float]) -> float | None:
    if len(values) < 2:
        return None
    mean = sum(values) / len(values)
    var = sum((value - mean) ** 2 for value in values) / (len(values) - 1)
    if var <= 0:
        return None
    return mean / math.sqrt(var) * math.sqrt(252)


def main() -> None:
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    series = []
    nets = [float(row["strategy_net"]) for row in rows]
    for index, row in enumerate(rows):
        if index + 1 < WINDOW:
            sharpe = None
        else:
            sharpe = _sharpe(nets[index + 1 - WINDOW:index + 1])
        series.append({"session": row["session"], "sharpe": sharpe})
    defined = [row["sharpe"] for row in series if row["sharpe"] is not None]
    payload = {
        "rolling_sharpe": series,
        "window": WINDOW,
        "label": "post hoc",
        "rolling_sharpe_summary": {
            "defined": len(defined),
            "min": min(defined) if defined else None,
            "max": max(defined) if defined else None,
            "positive": sum(1 for value in defined if value > 0),
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(payload) + "\n", encoding="utf-8")
    print(f"posthoc sessions: {len(series)}")


if __name__ == "__main__":
    main()
