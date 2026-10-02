# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc analyses for the opening-pop fade (added after seeing results).

Everything here explains the result or exposes risk. None of it changes the
verdict. Reads daily.csv and trades.csv only; writes posthoc.json.

    python research/index-opening-pop-fade/research/posthoc.py
"""
from __future__ import annotations

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
WIN = 252


def sharpe(r) -> float | None:
    r = np.asarray(r, dtype=float)
    sd = r.std(ddof=1) if len(r) > 1 else 0
    return float(r.mean() / sd * math.sqrt(252)) if sd > 0 else None


def main() -> None:
    daily = list(csv.DictReader((HERE / "daily.csv").open()))
    trades = list(csv.DictReader((HERE / "trades.csv").open()))
    days = [r["session"] for r in daily]
    out: dict = {}
    for col in ("primary_net", "s1_net", "qqq_net"):
        r = np.array([float(x[col]) for x in daily])
        roll = [sharpe(r[i - WIN + 1:i + 1]) for i in range(WIN - 1, len(r))]
        vals = [v for v in roll if v is not None]
        order = np.argsort(r)
        drop_best = r.copy()
        drop_best[order[-20:]] = 0.0
        drop_worst = r.copy()
        drop_worst[order[:20]] = 0.0
        out[col] = {
            "rolling_252": {"sessions": days[WIN - 1:], "sharpe": roll,
                            "share_positive": float(np.mean([v > 0 for v in vals])),
                            "min": float(min(vals)), "max": float(max(vals)), "last": vals[-1]},
            "trailing_252": {"from": days[-WIN], "to": days[-1], "sharpe": sharpe(r[-WIN:]),
                             "return": float(np.prod(1 + r[-WIN:]) - 1)},
            "zero_best_20": {"sharpe": sharpe(drop_best), "return": float(np.prod(1 + drop_best) - 1)},
            "zero_worst_20": {"sharpe": sharpe(drop_worst), "return": float(np.prod(1 + drop_worst) - 1)},
        }
    for book in ("primary", "S1"):
        tr = [t for t in trades if t["book"] == book and t["symbol"] == "SPY"]
        tr.sort(key=lambda t: float(t["net"]))
        nets = np.array([float(t["net"]) for t in tr])
        out[f"{book}_spy_trades"] = {
            "worst_10": [{k: t[k] for k in ("session", "z", "gap_up", "net_bp")} for t in tr[:10]],
            "best_10": [{k: t[k] for k in ("session", "z", "gap_up", "net_bp")} for t in tr[-10:][::-1]],
            "sum_net_worst_10": float(nets[:10].sum()), "sum_net_all": float(nets.sum()),
            "sum_net_best_10": float(nets[-10:].sum()),
        }
    # Overlap of the two SPY books: sessions traded by both, and by only one.
    p = {t["session"] for t in trades if t["book"] == "primary" and t["symbol"] == "SPY"}
    s = {t["session"] for t in trades if t["book"] == "S1" and t["symbol"] == "SPY"}
    pn = {t["session"]: float(t["net"]) for t in trades if t["book"] == "primary" and t["symbol"] == "SPY"}
    sn = {t["session"]: float(t["net"]) for t in trades if t["book"] == "S1" and t["symbol"] == "SPY"}
    out["overlap_spy"] = {
        "both": len(p & s), "primary_only": len(p - s), "s1_only": len(s - p),
        "sum_net_both": float(sum(pn[d] for d in p & s)),
        "sum_net_primary_only": float(sum(pn[d] for d in p - s)),
        "sum_net_s1_only": float(sum(sn[d] for d in s - p)),
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=1))
    for col in ("primary_net", "s1_net", "qqq_net"):
        o = out[col]
        print(col, {k: o[k] for k in ("trailing_252", "zero_best_20", "zero_worst_20")},
              {k: v for k, v in o["rolling_252"].items() if k not in ("sessions", "sharpe")})
    for book in ("primary", "S1"):
        o = out[f"{book}_spy_trades"]
        print(book, "worst", o["worst_10"][:6], o["sum_net_worst_10"], o["sum_net_all"])
        print(book, "best", o["best_10"][:6], o["sum_net_best_10"])
    print(out["overlap_spy"])


if __name__ == "__main__":
    main()
