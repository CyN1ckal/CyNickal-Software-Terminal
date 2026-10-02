# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc analyses, added after the pre-registered run. Nothing here feeds the verdict.

Reads daily.csv and trades.csv only; does not open the store. Run from the repo root:

    python research/qqq-15m-turtle-overnight/research/posthoc.py
"""

from __future__ import annotations

import csv
import json
import math
from collections import Counter
from datetime import date
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252
WINDOW = 252


def sharpe(r: np.ndarray) -> float:
    sd = r.std(ddof=1)
    return float(r.mean() / sd * math.sqrt(ANNUAL)) if sd > 0 else float("nan")


def main() -> None:
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    days = [date.fromisoformat(r["session"]) for r in rows]
    oos = np.array([r["sample"] == "OOS" for r in rows])
    out: dict = {"label": "post hoc: computed after the pre-registered run; never used for the verdict"}

    for col in ("qqq_ret", "spy_ret", "igv_ret"):
        r = np.array([float(x[col]) for x in rows])
        roll = np.array([sharpe(r[k - WINDOW:k]) for k in range(WINDOW, len(r) + 1)])
        out[col] = {
            "rolling_252_sharpe": {"share_positive": float((roll > 0).mean()), "min": float(roll.min()),
                                   "min_end": days[WINDOW - 1 + int(roll.argmin())].isoformat(),
                                   "max": float(roll.max()), "last": float(roll[-1]),
                                   "series_end_dates": [d.isoformat() for d in days[WINDOW - 1:]],
                                   "series": [float(x) for x in roll]},
            "trailing_252": {"sharpe": sharpe(r[-WINDOW:]), "return": float(np.prod(1 + r[-WINDOW:]) - 1)},
        }
        best = np.argsort(r)[::-1]
        drop = {}
        for n in (5, 10, 20):
            z = r.copy()
            z[best[:n]] = 0.0
            drop[str(n)] = {"full_sharpe": sharpe(z), "full_return": float(np.prod(1 + z) - 1)}
        out[col]["without_best_days"] = drop

    r = np.array([float(x["qqq_ret"]) for x in rows])
    apr25 = np.array([d.year == 2025 and d.month == 4 for d in days])
    out["qqq_oos_without_april_2025"] = {"sharpe": sharpe(r[oos & ~apr25]),
                                         "return": float(np.prod(1 + r[oos & ~apr25]) - 1),
                                         "april_2025_return": float(np.prod(1 + r[apr25]) - 1)}
    out["qqq_oos_from_2025_05"] = {"sharpe": sharpe(r[np.array([d >= date(2025, 5, 1) for d in days])]),
                                   "return": float(np.prod(1 + r[np.array([d >= date(2025, 5, 1) for d in days])]) - 1)}

    trades = [t for t in csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")) if t["symbol"] == "QQQ"]
    stops_per_day = Counter(t["exit_time"][:10] for t in trades if t["reason"] == "stop")
    whipsaw = sorted(d for d, n in stops_per_day.items() if n >= 2)
    net_on = sum(float(t["net_ret"]) for t in trades if t["exit_time"][:10] in set(whipsaw))
    out["qqq_whipsaw_days"] = {"sessions_with_2plus_stops": len(whipsaw), "dates": whipsaw,
                               "net_trade_return_sum_exiting_those_days": net_on}
    for sample in ("IS", "OOS"):
        tr = [t for t in trades if t["sample"] == sample]
        out[f"qqq_{sample}_by_side"] = {
            side: {"trades": sum(1 for t in tr if t["side"] == side),
                   "avg_gross_bp": float(np.mean([float(t["gross_ret"]) for t in tr if t["side"] == side]) * 1e4)}
            for side in ("long", "short")}

    nets = np.array([float(t["net_ret"]) for t in trades])
    out["qqq_trade_shape"] = {"avg_winner_bp": float(nets[nets > 0].mean() * 1e4),
                              "avg_loser_bp": float(nets[nets < 0].mean() * 1e4),
                              "sessions_with_entry": len({t["entry_time"][:10] for t in trades}),
                              "sessions_evaluated": len(rows)}

    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    cells = results["grid"]["cells"]
    is_s = np.array([c["is_sharpe"] for c in cells])
    oos_s = np.array([c["oos_sharpe"] for c in cells])
    rank = lambda x: np.argsort(np.argsort(x)).astype(float)  # noqa: E731
    best = cells[int(is_s.argmax())]
    out["grid_selection_bias"] = {"spearman_is_oos": float(np.corrcoef(rank(is_s), rank(oos_s))[0, 1]),
                                  "is_winner": {"n_in": best["n_in"], "n_out": best["n_out"],
                                                "is_sharpe": best["is_sharpe"], "oos_sharpe": best["oos_sharpe"]},
                                  "primary_is_rank_of_15": int(15 - rank(is_s)[[c["n_in"] == 55 and c["n_out"] == 20
                                                                                for c in cells].index(True)]),
                                  "oos_mean_sharpe": float(oos_s.mean())}

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2))
    summary = {k: v for k, v in out.items() if not isinstance(v, dict) or "rolling_252_sharpe" not in v}
    print(json.dumps(summary, indent=1)[:3000])
    for col in ("qqq_ret", "spy_ret", "igv_ret"):
        v = out[col]
        print(col, {k: x for k, x in v["rolling_252_sharpe"].items() if not k.startswith("series")},
              v["trailing_252"], v["without_best_days"])


if __name__ == "__main__":
    main()
