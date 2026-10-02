# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc checks on backtest.py output. Not in RULES.md; reported as post hoc.

Run after backtest.py: python research/qqq-intraday-trend/research/posthoc.py
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
    sd = np.std(r, ddof=1)
    return float(np.mean(r) / sd * math.sqrt(ANNUAL)) if sd > 0 else 0.0


def main() -> None:
    rows = list(csv.DictReader(open(HERE / "daily.csv", encoding="utf-8")))
    days = [r["session"] for r in rows]
    p1 = np.array([float(r["p1"]) for r in rows])
    hold = np.array([float(r["hold"]) for r in rows])
    lev = np.array([float(r["p2_leverage"]) for r in rows])
    out: dict = {}

    # Dependence on the best days.
    order = np.argsort(p1)[::-1]
    conc = []
    for drop in (0, 5, 10, 20):
        keep = np.ones(len(p1), bool)
        keep[order[:drop]] = False
        r = np.where(keep, p1, 0.0)
        conc.append({"best_days_removed": drop, "sharpe": sharpe(r), "total_return": float(np.prod(1 + r) - 1)})
    out["best_day_dependence"] = conc
    out["best_days"] = [{"session": days[i], "p1_bps": float(p1[i] * 1e4), "hold_bps": float(hold[i] * 1e4)}
                        for i in order[:10]]
    out["worst_days"] = [{"session": days[i], "p1_bps": float(p1[i] * 1e4), "hold_bps": float(hold[i] * 1e4)}
                         for i in np.argsort(p1)[:5]]

    # Volatility regime: sample std of the previous 14 close-to-close returns (what P2 sizes on).
    sigma = np.full(len(hold), np.nan)
    for i in range(14, len(hold)):
        sigma[i] = np.std(hold[i - 14:i], ddof=1)
    first = 14
    sigma, p1_v = sigma[first:], p1[first:]
    edges = np.percentile(sigma, [33.33, 66.67])
    terc = np.searchsorted(edges, sigma, side="right")
    out["vol_terciles"] = []
    for k, name in enumerate(("low", "mid", "high")):
        m = terc == k
        out["vol_terciles"].append({
            "regime": name, "sessions": int(m.sum()),
            "trailing_vol_daily_pct_max": float(sigma[m].max() * 100),
            "p1_sharpe": sharpe(p1_v[m]), "p1_avg_bps": float(p1_v[m].mean() * 1e4),
            "p1_total": float(np.prod(1 + p1_v[m]) - 1),
        })

    # Rolling 252-session Sharpe.
    roll = []
    for i in range(ANNUAL, len(p1) + 1):
        roll.append({"session": days[i - 1], "p1": sharpe(p1[i - ANNUAL:i]), "hold": sharpe(hold[i - ANNUAL:i])})
    out["rolling_252_min"] = min(roll, key=lambda x: x["p1"])
    out["rolling_252_share_positive"] = float(np.mean([x["p1"] > 0 for x in roll]))
    out["rolling_252"] = roll

    # Windows that include or exclude the April 2025 tariff sessions.
    def window(lo: str, hi: str, skip: tuple[str, str] | None = None) -> dict:
        m = np.array([lo <= d <= hi and not (skip and skip[0] <= d <= skip[1]) for d in days])
        return {"sessions": int(m.sum()), "sharpe": sharpe(p1[m]), "total_return": float(np.prod(1 + p1[m]) - 1),
                "hold_sharpe": sharpe(hold[m]), "hold_total": float(np.prod(1 + hold[m]) - 1)}
    out["windows"] = {
        "oos": window("2024-07-01", "2026-09-25"),
        "oos_without_april_2025": window("2024-07-01", "2026-09-25", ("2025-04-01", "2025-04-30")),
        "may_2025_to_apr_2026": window("2025-05-01", "2026-04-30"),
        "last_6_months": window("2026-03-26", "2026-09-25"),
        "since_may_2025": window("2025-05-01", "2026-09-25"),
    }

    # Down days for the market: what P1 did on QQQ's worst close-to-close days.
    worst_mkt = np.argsort(hold)[:20]
    out["qqq_worst_20_days"] = {"avg_hold_bps": float(hold[worst_mkt].mean() * 1e4),
                                "avg_p1_bps": float(p1[worst_mkt].mean() * 1e4),
                                "p1_positive_share": float((p1[worst_mkt] > 0).mean())}

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in out.items() if k != "rolling_252"}, indent=1))


if __name__ == "__main__":
    main()
