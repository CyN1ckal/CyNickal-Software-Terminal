# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. None of these change the verdict.

    python research/low-liq-high-vol-mean-reversion/research/posthoc.py
"""

from __future__ import annotations

import csv
import json
import math
from collections import defaultdict
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(r: np.ndarray) -> float:
    if len(r) < 2:
        return float("nan")
    sd = float(r.std(ddof=1))
    return float(r.mean() / sd * math.sqrt(ANNUAL)) if sd > 0 else float("nan")


def total(r: np.ndarray) -> float:
    return float(np.prod(1.0 + r) - 1.0) if len(r) else float("nan")


def main() -> None:
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    trades = list(csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")))
    ret = np.array([float(r["ret"]) for r in rows])
    ew = np.array([float(r["bench_ew_ret"]) for r in rows])
    spy = np.array([float(r["bench_spy_ret"]) for r in rows])
    roll = []
    for i in range(len(ret)):
        if i + 1 < ANNUAL:
            roll.append(None)
        else:
            roll.append(sharpe(ret[i + 1 - ANNUAL:i + 1]))
    order = np.argsort(ret)
    best = order[-20:]
    worst = order[:20]
    drop_best = ret.copy()
    drop_best[best] = 0.0
    drop_worst = ret.copy()
    drop_worst[worst] = 0.0
    by_symbol = defaultdict(float)
    for t in trades:
        by_symbol[t["symbol"]] += float(t["net_pnl"])
    ranked = sorted(by_symbol.items(), key=lambda kv: kv[1])
    trail = ret[-ANNUAL:] if len(ret) >= ANNUAL else ret
    by_year = []
    for year in sorted({r["session"][:4] for r in rows}):
        idx = [i for i, r in enumerate(rows) if r["session"].startswith(year)]
        sl = ret[idx]
        by_year.append({
            "year": int(year),
            "strategy_return": total(sl),
            "equal_weight_return": total(ew[idx]),
            "spy_return": total(spy[idx]),
            "sessions": len(idx),
        })
    overlap = [i for i, r in enumerate(rows) if r["session"] >= "2021-09-16"]
    ov_ret, ov_spy = ret[overlap], spy[overlap]
    is_days = [r["session"] for r in rows if r["sample"] == "IS"]
    oos_days = [r["session"] for r in rows if r["sample"] == "OOS"]
    out = {
        "label": "post hoc",
        "rolling_sharpe_252": roll,
        "rolling_sharpe_last": roll[-1] if roll else None,
        "correlation_spy": float(np.corrcoef(ret, spy)[0, 1]) if len(ret) > 2 else None,
        "correlation_equal_weight": float(np.corrcoef(ret, ew)[0, 1]) if len(ret) > 2 else None,
        "drop_20_best_days": {"sharpe": sharpe(drop_best), "total_return": total(drop_best)},
        "drop_20_worst_days": {"sharpe": sharpe(drop_worst), "total_return": total(drop_worst)},
        "sum_of_20_best_daily_returns": float(ret[best].sum()),
        "sum_of_20_worst_daily_returns": float(ret[worst].sum()),
        "trailing_252": {"sessions": int(len(trail)), "sharpe": sharpe(trail), "total_return": total(trail)},
        "symbol_net_pnl_worst5": [{"symbol": s, "net_pnl": v} for s, v in ranked[:5]],
        "symbol_net_pnl_best5": [{"symbol": s, "net_pnl": v} for s, v in ranked[-5:]],
        "by_year": by_year,
        "is_first": is_days[0] if is_days else None,
        "is_last": is_days[-1] if is_days else None,
        "oos_first": oos_days[0] if oos_days else None,
        "oos_last": oos_days[-1] if oos_days else None,
        "spy_overlap_from_2021_09_16": {
            "sessions": len(overlap),
            "strategy_sharpe": sharpe(ov_ret),
            "strategy_return": total(ov_ret),
            "spy_sharpe": sharpe(ov_spy),
            "spy_return": total(ov_spy),
            "note": "SPY daily bars in the store start 2021-09-16. Earlier benchmark days are zeros, as RULES.md specifies for a missing print.",
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(out) + "\n", encoding="utf-8")
    print(f"posthoc wrote correlation_spy {out['correlation_spy']:.3f} "
          f"trailing_sharpe {out['trailing_252']['sharpe']:.3f}")


if __name__ == "__main__":
    main()
