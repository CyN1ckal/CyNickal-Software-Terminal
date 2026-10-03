# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc descriptions. Does not open the store and does not change the verdict."""

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent


def sharpe(r: np.ndarray):
    sd = float(np.std(r, ddof=1))
    if sd == 0.0:
        return None
    return float(np.mean(r) / sd * math.sqrt(252))


def total_return(r: np.ndarray) -> float:
    eq = 1.0
    for x in r:
        eq *= 1.0 + float(x)
    return float(eq - 1.0)


def spearman(a, b) -> float:
    ra = np.argsort(np.argsort(np.asarray(a, dtype=float))).astype(float)
    rb = np.argsort(np.argsort(np.asarray(b, dtype=float))).astype(float)
    return float(np.corrcoef(ra, rb)[0, 1])


def main() -> None:
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
        daily = list(csv.DictReader(f))
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    net = np.array([float(r["strategy_net"]) for r in daily])
    gross = np.array([float(r["strategy_gross"]) for r in daily])
    weight = np.array([int(r["weight"]) for r in daily])
    eem = np.array([float(r["eem_otc"]) for r in daily])
    spy = np.array([float(r["spy_c2c"]) for r in daily])
    sample = np.array([r["sample"] for r in daily])
    dates = [r["date"] for r in daily]

    roll_rows = []
    rolls = []
    for i in range(251, len(net)):
        s = sharpe(net[i - 251:i + 1])
        rolls.append(s)
        roll_rows.append((dates[i], s))
    rolls_a = np.array(rolls, dtype=float)
    with (HERE / "rolling_sharpe.csv").open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["date", "sharpe"])
        for d, s in roll_rows:
            w.writerow([d, format(s, ".17g")])

    order = np.argsort(-net, kind="mergesort")
    top20 = order[:20]
    worst20 = order[-20:]
    dropped = net.copy()
    dropped[top20] = 0.0
    dropped_worst = net.copy()
    dropped_worst[worst20] = 0.0
    pos = net[net > 0]
    share = float(net[top20].sum() / pos.sum()) if pos.sum() != 0 else None

    oos = sample == "oos"
    oos_net = net[oos]
    oos_order = np.argsort(-oos_net, kind="mergesort")
    oos_dropped = oos_net.copy()
    oos_dropped[oos_order[:10]] = 0.0

    def avg_gross(mask):
        g = gross[mask & (weight != 0)]
        return float(np.mean(g) * 10000) if len(g) else None

    is_m = sample == "is"
    grid_is = [c["is_sharpe"] for c in res["grid"]]
    grid_oos = [c["oos_sharpe"] for c in res["grid"]]
    out = {
        "label": "post hoc",
        "rolling_252": {
            "n": int(len(rolls_a)),
            "min": float(np.min(rolls_a)),
            "max": float(np.max(rolls_a)),
            "last": float(rolls_a[-1]),
            "last_date": dates[-1],
            "fraction_positive": float(np.mean(rolls_a > 0)),
        },
        "drop_best_20_full": {
            "total_return": total_return(dropped),
            "sharpe": sharpe(dropped),
            "sum_of_days": float(net[top20].sum()),
            "share_of_positive_day_sum": share,
        },
        "drop_worst_20_full": {
            "total_return": total_return(dropped_worst),
            "sharpe": sharpe(dropped_worst),
            "sum_of_days": float(net[worst20].sum()),
        },
        "drop_best_10_oos": {
            "oos_return": total_return(oos_dropped),
            "oos_sharpe": sharpe(oos_dropped),
            "sum_of_days": float(oos_net[oos_order[:10]].sum()),
        },
        "correlation": {
            "full_eem_otc": float(np.corrcoef(net, eem)[0, 1]),
            "full_spy_c2c": float(np.corrcoef(net, spy)[0, 1]),
            "oos_eem_otc": float(np.corrcoef(net[oos], eem[oos])[0, 1]),
            "oos_spy_c2c": float(np.corrcoef(net[oos], spy[oos])[0, 1]),
        },
        "avg_gross_trade_bp": {
            "full": avg_gross(np.ones(len(net), dtype=bool)),
            "is": avg_gross(is_m),
            "oos": avg_gross(oos),
        },
        "grid_spearman_is_oos": spearman(grid_is, grid_oos),
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
