# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc diagnostics for the QQQ strategy portfolio. Written after the first run.

Nothing here changes the verdict or becomes the recommended portfolio. It reads the
same source files through backtest.py's loader and writes posthoc.json.

    python research/qqq-strategy-portfolio/research/posthoc.py
"""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import backtest as bt  # noqa: E402

data = bt.load()
is_m = data["is"]
sessions = np.array(data["sessions"])
out: dict = {"label": "post hoc; does not affect the verdict"}

QP = ("Q", "P1")
R_qp = {k: bt.sleeves_at(data, QP, k) for k in (1.0, 2.0)}


def qp(wq: float, wp: float, k: float = 1.0) -> np.ndarray:
    return bt.portfolio(R_qp[k], np.array([wq, wp]), QP, k)


# 1. Overlay frontier: QQQ held at 1x, P1 overlaid at 0 ... 2x.
front = []
for wp in np.round(np.arange(0, 2.01, 0.1), 2):
    v, v2 = qp(1.0, wp), qp(1.0, wp, 2.0)
    front.append({"w_p1": float(wp), "full_sharpe": bt.sharpe(v), "is_sharpe": bt.sharpe(v[is_m]),
                  "oos_sharpe": bt.sharpe(v[~is_m]), "oos_sharpe_2x": bt.sharpe(v2[~is_m]),
                  "full_max_dd": bt.max_dd(v), "oos_max_dd": bt.max_dd(v[~is_m]),
                  "full_cagr": bt.metrics(v)["cagr"], "full_vol": bt.metrics(v)["ann_vol"]})
out["overlay_frontier_q1x"] = front

# 2. In-hindsight max-Sharpe directions (fitted on the data they are scored on).
D4 = bt.simplex_grid(4)
R4 = bt.sleeves_at(data, bt.ELIGIBLE, 1.0)
D2 = bt.simplex_grid(2)
R2 = R_qp[1.0]
out["hindsight_direction"] = {
    "eligible_full": dict(zip(bt.ELIGIBLE, bt.best_direction(R4, D4).tolist())),
    "eligible_oos": dict(zip(bt.ELIGIBLE, bt.best_direction(R4[~is_m], D4).tolist())),
    "q_p1_full": dict(zip(QP, bt.best_direction(R2, D2).tolist())),
    "q_p1_oos": dict(zip(QP, bt.best_direction(R2[~is_m], D2).tolist())),
}
for key, (names, R, mask) in {"eligible_full": (bt.ELIGIBLE, R4, slice(None)), "eligible_oos": (bt.ELIGIBLE, R4, ~is_m),
                              "q_p1_full": (QP, R2, slice(None)), "q_p1_oos": (QP, R2, ~is_m)}.items():
    d = np.array(list(out["hindsight_direction"][key].values()))
    out["hindsight_direction"][key + "_sharpe"] = bt.sharpe(R[mask] @ d)

# 3. Benchmark N (Q 1x + P1 1x) against Q: the same paired bootstrap and acceptance lines.
n = qp(1.0, 1.0)
q = data["r"]["Q"]
out["N_vs_Q"] = {"full": bt.paired_boot(n, q, bt.BOOT_SEED), "OOS": bt.paired_boot(n[~is_m], q[~is_m], bt.BOOT_SEED),
                 "oos_sharpe": bt.sharpe(n[~is_m]), "oos_sharpe_2x": bt.sharpe(qp(1.0, 1.0, 2.0)[~is_m]),
                 "oos_max_dd": bt.max_dd(n[~is_m]), "q_oos_sharpe": bt.sharpe(q[~is_m]), "q_oos_max_dd": bt.max_dd(q[~is_m])}

# 4. Rolling 252-session Sharpe, and the recent window since P1's edge flattened.
with (bt.HERE / "daily.csv").open(newline="", encoding="utf-8") as fh:
    rows = list(csv.DictReader(fh))
series = {k: np.array([float(r[k]) for r in rows]) for k in ("A", "B", "N", "Q", "P1")}
roll = {}
for k, v in series.items():
    s = [bt.sharpe(v[i - 252:i]) for i in range(252, len(v) + 1)]
    roll[k] = {"share_positive": float(np.mean(np.array(s) > 0)), "min": float(min(s)), "last": float(s[-1])}
out["rolling_252"] = roll
recent = sessions >= "2025-05-01"
out["since_2025_05"] = {k: {"sessions": int(recent.sum()), **bt.metrics(v[recent])} for k, v in series.items()}

# 5. Dependence on the biggest days: drop A, B, N's 10 best sessions.
out["without_best_10"] = {k: bt.sharpe(np.sort(v)[:-10]) for k, v in series.items()}

(bt.HERE / "posthoc.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
print(json.dumps(out, indent=1))
