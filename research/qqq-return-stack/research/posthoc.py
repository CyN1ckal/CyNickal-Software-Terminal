# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc diagnostics for qqq-return-stack. Written after the first run.

Nothing here feeds the verdict. Reads daily.csv and results.json, writes posthoc.json.

    python research/qqq-return-stack/research/posthoc.py
"""
from __future__ import annotations

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANN = 252
SLEEVES = ("P1", "T", "C", "F", "M", "POP", "SCALE", "MART", "BOLL", "GAP", "REV", "RSI2")
S_SET = ("P1", "T", "C", "MART", "GAP", "REV", "RSI2")


def sharpe(v):
    sd = np.std(v, ddof=1)
    return float(np.mean(v) / sd * math.sqrt(ANN)) if sd > 0 else float("nan")


def main() -> None:
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    rows = list(csv.DictReader((HERE / "daily.csv").open(newline="", encoding="utf-8")))
    ses = [r["session"] for r in rows]
    oos = np.array([r["sample"] == "OOS" for r in rows])
    col = {k: np.array([float(r[k]) for r in rows]) for k in ("Q", "S", "K", "ALL", "L_S") + SLEEVES}
    w = {k: res["scales"][k]["w"] for k in SLEEVES}
    out: dict = {}

    # 1. dependence on the best days
    out["best_days"] = {}
    for name in ("Q", "S", "K"):
        v = col[name]
        d = {}
        for nz in (10, 20):
            z = v.copy()
            z[np.argsort(v)[::-1][:nz]] = 0.0
            zo = v[oos].copy()
            zo[np.argsort(v[oos])[::-1][:nz]] = 0.0
            d[str(nz)] = {"full_sharpe": sharpe(z), "full_return": float(np.prod(1 + z) - 1),
                          "oos_sharpe": sharpe(zo), "oos_return": float(np.prod(1 + zo) - 1)}
        out["best_days"][name] = d

    # 2. rolling 252-session Sharpe
    roll = {}
    for name in ("Q", "S", "K", "ALL"):
        v = col[name]
        rs = [sharpe(v[i - 251:i + 1]) for i in range(251, len(v))]
        roll[name] = {"share_positive": float(np.mean(np.array(rs) > 0)), "latest": rs[-1], "min": float(min(rs)),
                      "series": rs}
    out["rolling_252"] = {"first_session": ses[251], **roll}

    # 3. since 2025-05-01
    m = np.array([s >= "2025-05-01" for s in ses])
    out["since_2025_05"] = {"sessions": int(m.sum()),
                            **{n: {"return": float(np.prod(1 + col[n][m]) - 1), "sharpe": sharpe(col[n][m])}
                               for n in ("Q", "S", "K", "ALL")},
                            "sleeves_scaled": {k: {"sum": float(np.sum(w[k] * col[k][m])), "sharpe": sharpe(col[k][m])}
                                               for k in S_SET}}

    # 4. overnight exposure of S: how often it breaks a Reg-T 2x overnight limit
    L = col["L_S"]
    out["overnight_S"] = {"share_L_gt_2": float(np.mean(L > 2)), "share_L_lt_0": float(np.mean(L < 0)),
                          "sessions_L_gt_2": int(np.sum(L > 2)), "sessions_L_lt_0": int(np.sum(L < 0)),
                          "p99_abs_L": float(np.percentile(np.abs(L), 99)),
                          "worst_net_short": {"session": ses[int(np.argmin(L))], "L": float(L.min())},
                          "worst_net_long": {"session": ses[int(np.argmax(L))], "L": float(L.max())}}

    # 5. contribution of each scaled sleeve by calendar year
    yrs = sorted({s[:4] for s in ses})
    out["contribution_by_year"] = {"years": yrs, "Q": [float(np.sum(col["Q"][[s[:4] == y for s in ses]])) for y in yrs]}
    for k in SLEEVES:
        out["contribution_by_year"][k] = [float(np.sum(w[k] * col[k][[s[:4] == y for s in ses]])) for y in yrs]

    # 6. S without the April 2025 sessions (tariff crash and rebound)
    apr = np.array([s.startswith("2025-04") for s in ses])
    out["ex_april_2025"] = {n: {"oos_sharpe": sharpe(col[n][oos & ~apr]), "full_sharpe": sharpe(col[n][~apr])}
                            for n in ("Q", "S", "K")}

    # 7. annual 1x trading-cost drag of each sleeve at its 5% scale (from backtest.load's x_i)
    import sys
    sys.path.insert(0, str(HERE))
    from backtest import load
    x = load()["x"]
    out["cost_drag_annual"] = {k: {"full": float(w[k] * np.mean(x[k]) * ANN), "oos": float(w[k] * np.mean(x[k][oos]) * ANN)}
                               for k in SLEEVES}

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
    brief = {k: v for k, v in out.items() if k not in ("rolling_252", "contribution_by_year")}
    print(json.dumps(brief, indent=1))
    print({n: {kk: vv for kk, vv in roll[n].items() if kk != "series"} for n in roll})


if __name__ == "__main__":
    main()
