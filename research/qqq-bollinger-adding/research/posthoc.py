# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. None of these enter the verdict.

Reads this study's daily.csv and trades.csv, and the published daily.csv of
qqq-intraday-trend and qqq-atr-scale-in. Writes posthoc.json.

    python research/qqq-bollinger-adding/research/posthoc.py
"""
from __future__ import annotations

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]


def sharpe(r) -> float:
    r = np.asarray(r, dtype=float)
    sd = r.std(ddof=1) if len(r) > 1 else 0.0
    return float(r.mean() / sd * math.sqrt(252)) if sd > 0 else float("nan")


def total(r) -> float:
    return float(np.prod(1.0 + np.asarray(r)) - 1.0)


def read(path: Path) -> list[dict]:
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def main() -> None:
    daily = read(HERE / "daily.csv")
    trades = read(HERE / "trades.csv")
    days = [d["session"] for d in daily]
    r = np.array([float(d["qqq_net"]) for d in daily])
    o2c = {d["session"]: float(d["open_to_close"]) for d in daily}
    out: dict = {}

    # 1. Side against the day's direction.
    groups = {"against_day": [], "with_day": [], "flat_day": []}
    for t in trades:
        m = o2c[t["session"]]
        s = 1 if t["side"] == "long" else -1
        key = "flat_day" if m == 0 else ("against_day" if s * m < 0 else "with_day")
        groups[key].append(t)
    out["side_vs_day_direction"] = {
        k: {"campaigns": len(v),
            "share": len(v) / len(trades),
            "mean_gross_bp_unit": float(np.mean([float(t["gross_bp_unit"]) for t in v])) if v else None,
            "net_dollars": float(sum(float(t["net_dollars"]) for t in v)),
            "session_exits": sum(1 for t in v if t["reason"] == "session"),
            "mean_units": float(np.mean([int(t["units"]) for t in v])) if v else None}
        for k, v in groups.items()}

    # 2. Middle-band exits that were still losses (the middle moved to the price).
    mid = [t for t in trades if t["reason"] == "middle"]
    out["middle_exits"] = {
        "count": len(mid),
        "gross_negative": sum(1 for t in mid if float(t["gross_bp_unit"]) < 0),
        "share_gross_negative": sum(1 for t in mid if float(t["gross_bp_unit"]) < 0) / len(mid),
        "net_negative": sum(1 for t in mid if float(t["net_bp_unit"]) < 0),
    }

    # 3. Dependence on the extreme days.
    order = np.argsort(r)
    out["zero_worst_20"] = {"sharpe": sharpe(np.where(np.isin(np.arange(len(r)), order[:20]), 0, r)),
                            "total_return": total(np.where(np.isin(np.arange(len(r)), order[:20]), 0, r))}
    out["zero_best_20"] = {"sharpe": sharpe(np.where(np.isin(np.arange(len(r)), order[-20:]), 0, r)),
                           "total_return": total(np.where(np.isin(np.arange(len(r)), order[-20:]), 0, r))}
    out["worst_20_days"] = [{"session": days[i], "return": float(r[i]),
                             "open_to_close": o2c[days[i]]} for i in order[:20]]

    # 4. Rolling 252-session Sharpe.
    roll = [sharpe(r[i - 252:i]) for i in range(252, len(r) + 1)]
    out["rolling_252"] = {"min": float(np.nanmin(roll)), "max": float(np.nanmax(roll)),
                          "share_positive": float(np.mean(np.array(roll) > 0)),
                          "trailing_sharpe": roll[-1], "trailing_return": total(r[-252:]),
                          "trailing_from": days[-252]}
    with open(HERE / "rolling_sharpe.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["session", "rolling_252_sharpe"])
        for i, v in enumerate(roll):
            w.writerow([days[251 + i], f"{v:.6f}"])

    # 5. Correlation with earlier QQQ intraday studies, on shared dates.
    def corr_with(path: Path, col: str) -> dict:
        other = {d["session"]: d[col] for d in read(path) if d.get(col) not in ("", None)}
        idx = [i for i, d in enumerate(days) if d in other]
        a = r[idx]
        b = np.array([float(other[days[i]]) for i in idx])
        return {"sessions": len(idx), "corr": float(np.corrcoef(a, b)[0, 1])}

    out["corr_qqq_intraday_trend_p1"] = corr_with(ROOT / "research/qqq-intraday-trend/research/daily.csv", "p1")
    out["corr_qqq_atr_scale_in"] = corr_with(ROOT / "research/qqq-atr-scale-in/research/daily.csv", "qqq_ret")

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in out.items() if k != "worst_20_days"}, indent=2))


if __name__ == "__main__":
    main()
