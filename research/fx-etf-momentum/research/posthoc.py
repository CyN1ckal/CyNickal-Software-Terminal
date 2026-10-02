# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. Not part of the verdict.

Reads daily.csv and results.json only. Does not open the store.
"""

import ast
import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(r):
    r = np.asarray(r, float)
    if len(r) < 2 or r.std(ddof=1) == 0.0:
        return None
    return float(r.mean() / r.std(ddof=1) * math.sqrt(ANNUAL))


def compound(r):
    return float(np.prod(1.0 + np.asarray(r, float)) - 1.0)


def main():
    with open(HERE / "daily.csv", newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    dates = [r["date"] for r in rows]
    net = np.array([ast.literal_eval(r["strategy_net"]) for r in rows])
    bench = np.array([ast.literal_eval(r["benchmark"]) for r in rows])
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    years = res["breakdown"]["by_year"]
    n_neg = sum(1 for y in years.values() if y["return"] < 0.0)
    n_pos = sum(1 for y in years.values() if y["return"] > 0.0)

    roll = []
    for i in range(ANNUAL - 1, len(net)):
        roll.append({"date": dates[i], "sharpe": sharpe(net[i - ANNUAL + 1:i + 1])})
    roll_s = np.array([x["sharpe"] for x in roll], float)
    trail = net[-ANNUAL:]
    order = np.argsort(net, kind="mergesort")
    best = order[-10:][::-1]
    worst = order[:10]

    def without(idx):
        x = net.copy()
        x[idx] = 0.0
        return compound(x)

    out = {
        "label": "post hoc",
        "verdict_input": False,
        "negative_calendar_years": n_neg,
        "positive_calendar_years": n_pos,
        "calendar_years": len(years),
        "rolling_252": {
            "n": len(roll),
            "min": float(roll_s.min()),
            "max": float(roll_s.max()),
            "min_date": roll[int(roll_s.argmin())]["date"],
            "max_date": roll[int(roll_s.argmax())]["date"],
            "share_le_0": float(np.mean(roll_s <= 0.0)),
            "last": roll[-1]["sharpe"],
            "last_date": roll[-1]["date"],
            "series": roll,
        },
        "trailing_252": {
            "start": dates[-ANNUAL],
            "end": dates[-1],
            "return": compound(trail),
            "sharpe": sharpe(trail),
        },
        "concentration": {
            "full_return": compound(net),
            "return_without_best_10_days": without(best),
            "return_without_worst_10_days": without(worst),
            "best_10": [{"date": dates[i], "strategy_net": float(net[i]), "benchmark": float(bench[i])} for i in best],
            "worst_10": [{"date": dates[i], "strategy_net": float(net[i]), "benchmark": float(bench[i])} for i in worst],
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=1), encoding="utf-8", newline="\n")
    print(json.dumps({k: out[k] for k in ("negative_calendar_years", "positive_calendar_years", "trailing_252")}, indent=1))
    c = out["concentration"]
    print(json.dumps({"full": c["full_return"], "without_best_10": c["return_without_best_10_days"],
                      "without_worst_10": c["return_without_worst_10_days"],
                      "roll_min": out["rolling_252"]["min"], "roll_max": out["rolling_252"]["max"],
                      "roll_share_le_0": out["rolling_252"]["share_le_0"],
                      "roll_last": out["rolling_252"]["last"]}, indent=1))


if __name__ == "__main__":
    main()
