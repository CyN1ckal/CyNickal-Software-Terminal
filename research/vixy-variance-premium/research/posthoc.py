# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc descriptions of the stored daily path. Does not open the store.

Does not correct prices, does not rerun the short, and does not feed the verdict.
Zeroing a session sets that stored return to 0. Later zeros stay zeros.
"""

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ANNUAL = 252
OOS_START = "2024-07-01"
WINDOW = 252


def sharpe(r):
    r = np.asarray(r, float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0 or math.isnan(sd):
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total_return(r):
    eq = 1.0
    for x in r:
        eq *= 1.0 + float(x)
    return eq - 1.0


def main():
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    dates = [row["date"] for row in rows]
    r = np.array([float(row["strategy_net"]) for row in rows], float)
    stored = results["windows"]["full"]["strategy"]["total_return"]
    stored_sharpe = results["windows"]["full"]["strategy"]["sharpe"]
    got_return = total_return(r)
    got_sharpe = sharpe(r)
    if abs(got_return - stored) > 1e-8:
        raise SystemExit(f"daily.csv compound {got_return} != results {stored}")
    if got_sharpe is None or abs(got_sharpe - stored_sharpe) > 1e-8:
        raise SystemExit(f"daily.csv Sharpe {got_sharpe} != results {stored_sharpe}")

    roll_rows = []
    defined = []
    for i in range(len(r)):
        if i + 1 < WINDOW:
            value = None
        else:
            value = sharpe(r[i + 1 - WINDOW:i + 1])
        roll_rows.append((dates[i], value))
        if value is not None:
            defined.append((dates[i], value))
    with (HERE / "rolling_sharpe.csv").open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f)
        w.writerow(["date", "sharpe"])
        for day, value in roll_rows:
            w.writerow([day, "" if value is None else format(value, ".16g")])
    finite = np.array([v for _d, v in defined], float)
    fraction = float(np.mean(finite > 0.0)) if len(finite) else None

    order = sorted(range(len(r)), key=lambda i: (r[i], dates[i]))
    worst_i = order[:10]
    best_i = order[::-1][:10]
    best20_i = order[::-1][:20]

    def pack(idxs):
        return [{"date": dates[i], "strategy_net": float(r[i])} for i in idxs]

    zeroed = r.copy()
    for i in best20_i:
        zeroed[i] = 0.0
    oos_idx = [i for i, day in enumerate(dates) if day >= OOS_START]
    oos_r = r[oos_idx]
    oos_all_zero = bool(len(oos_r) and np.all(oos_r == 0.0))
    pos = r[r > 0.0]
    sum_pos = float(pos.sum()) if len(pos) else 0.0
    sum_best = float(r[best_i].sum())
    sum_worst = float(r[worst_i].sum())
    doc = {
        "label": "post hoc",
        "feeds_verdict": False,
        "note": "Computed from daily.csv only. Zeroing a day is not a split correction and not a new backtest. Sessions after the freeze stay at 0.",
        "daily_csv_total_return": got_return,
        "daily_csv_sharpe": got_sharpe,
        "rolling_sharpe": {
            "window": WINDOW,
            "n_sessions": len(r),
            "n_defined": len(defined),
            "min": float(finite.min()) if len(finite) else None,
            "max": float(finite.max()) if len(finite) else None,
            "last": roll_rows[-1][1] if roll_rows else None,
            "last_defined": (
                {"date": defined[-1][0], "sharpe": defined[-1][1]} if defined else None
            ),
            "fraction_positive": fraction,
        },
        "ten_worst": pack(worst_i),
        "ten_best": pack(best_i),
        "zero_20_best_days": {
            "dates": [dates[i] for i in best20_i],
            "total_return": total_return(zeroed),
            "sharpe": sharpe(zeroed),
        },
        "zero_10_best_oos_days": {
            "applicable": not oos_all_zero,
            "n_oos": len(oos_idx),
            "n_oos_nonzero": int(np.count_nonzero(oos_r)),
            "reason": "Every out-of-sample strategy_net is 0, so there is no best out-of-sample day to remove.",
            "total_return": got_return,
            "sharpe": got_sharpe,
        },
        "concentration": {
            "sum_10_best": sum_best,
            "sum_positive_days": sum_pos,
            "sum_10_best_over_positive": (sum_best / sum_pos) if sum_pos != 0.0 else None,
            "sum_10_worst": sum_worst,
            "worst_session": pack(worst_i[:1])[0],
            "worst_over_sum_10_worst": (float(r[worst_i[0]]) / sum_worst) if sum_worst != 0.0 else None,
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(
        f"posthoc ten worst {doc['ten_worst'][0]['date']} {doc['ten_worst'][0]['strategy_net']}; "
        f"rolling defined {len(defined)}; zero-20 return {doc['zero_20_best_days']['total_return']}"
    )


if __name__ == "__main__":
    main()
