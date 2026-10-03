# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc trailing Sharpe. Not an input to the verdict."""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path

HERE = Path(__file__).resolve().parent
WINDOW = 126
ANNUAL = 252


def sharpe(values) -> float | None:
    if len(values) < 2:
        return None
    mean = sum(values) / len(values)
    var = sum((value - mean) ** 2 for value in values) / (len(values) - 1)
    if var <= 0.0 or not math.isfinite(var):
        return None
    return mean / math.sqrt(var) * math.sqrt(ANNUAL)


def compound(values) -> float:
    total = 1.0
    for value in values:
        total *= 1.0 + value
    return total - 1.0


def main() -> None:
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    net = [float(row["strategy_net"]) for row in rows]
    series = []
    for end in range(WINDOW - 1, len(rows)):
        value = sharpe(net[end - WINDOW + 1: end + 1])
        series.append({"session": rows[end]["session"], "sharpe": value})
    finite = [row["sharpe"] for row in series if row["sharpe"] is not None]
    order = sorted(range(len(net)), key=lambda index: net[index])
    worst = order[:10]
    best = order[-10:][::-1]

    def dropped(indexes) -> list[float]:
        edited = list(net)
        for index in indexes:
            edited[index] = 0.0
        return edited

    without_best = dropped(best)
    without_worst = dropped(worst)
    payload = {
        "label": "post hoc",
        "note": "post hoc. Not an input to the verdict.",
        "window": WINDOW,
        "rolling_sharpe": series,
        "rolling_summary": {
            "windows": len(series),
            "fraction_positive": (sum(value > 0.0 for value in finite) / len(finite)) if finite else None,
            "min": min(finite) if finite else None,
            "max": max(finite) if finite else None,
        },
        "concentration": {
            "full_return": compound(net),
            "return_without_10_best_days": compound(without_best),
            "return_without_10_worst_days": compound(without_worst),
            "sharpe_without_10_best_days": sharpe(without_best),
            "sharpe_without_10_worst_days": sharpe(without_worst),
            "best_days": [
                {"session": rows[index]["session"], "strategy_net": net[index]} for index in best
            ],
            "worst_days": [
                {"session": rows[index]["session"], "strategy_net": net[index]} for index in worst
            ],
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"post hoc rolling windows {len(series)} finite {len(finite)}")


if __name__ == "__main__":
    main()
