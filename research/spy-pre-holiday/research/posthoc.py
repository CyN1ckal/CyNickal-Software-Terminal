# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the pre-registered run. Not part of the verdict.

Reads daily.csv. Does not open the store and does not import backtest.py.
"""

import csv
import json
import math
from datetime import date, timedelta
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
import sys
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import is_nyse_holiday

HERE = Path(__file__).resolve().parent
ANNUAL = 252.0


def path_stats(returns):
    r = np.asarray(list(returns), float)
    n = int(r.size)
    equity = 1.0
    peak = 1.0
    max_dd = 0.0
    for x in r:
        equity *= 1.0 + float(x)
        if equity > peak:
            peak = equity
        if peak > 0:
            max_dd = min(max_dd, equity / peak - 1.0)
    sd = float(r.std(ddof=1)) if n > 1 else 0.0
    mean = float(r.mean()) if n else 0.0
    sharpe = float(mean / sd * math.sqrt(ANNUAL)) if sd > 0 else 0.0
    cagr = float(equity ** (ANNUAL / n) - 1.0) if n and equity > 0 else None
    return {
        "sessions": n,
        "total_return": float(equity - 1.0) if n else 0.0,
        "sharpe": sharpe,
        "max_dd": float(max_dd),
        "cagr": cagr,
        "ending_equity": float(equity) if n else 1.0,
    }


def next_weekday(session: date) -> date:
    nxt = session + timedelta(days=1)
    while nxt.weekday() >= 5:
        nxt += timedelta(days=1)
    return nxt


def holiday_name(holiday: date) -> str:
    if holiday == date(2025, 1, 9):
        return "Carter mourning"
    if holiday.month == 1 and holiday.day <= 2:
        return "New Year"
    if holiday.month == 1 and holiday.weekday() == 0:
        return "MLK Day"
    if holiday.month == 2 and holiday.weekday() == 0:
        return "Presidents Day"
    if holiday.month in (3, 4) and holiday.weekday() == 4:
        return "Good Friday"
    if holiday.month == 5 and holiday.weekday() == 0:
        return "Memorial Day"
    if holiday.month == 6:
        return "Juneteenth"
    if holiday.month == 7:
        return "Independence Day"
    if holiday.month == 9 and holiday.weekday() == 0:
        return "Labor Day"
    if holiday.month == 11 and holiday.weekday() == 3:
        return "Thanksgiving"
    if holiday.month == 12:
        return "Christmas"
    return "other"


def sharpe_of(window):
    sd = float(window.std(ddof=1))
    if sd <= 0:
        return 0.0
    return float(window.mean() / sd * math.sqrt(ANNUAL))


def main():
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    dates = [date.fromisoformat(r["date"]) for r in rows]
    net = np.array([float(r["strategy_net"]) for r in rows], float)
    gross = np.array([float(r["strategy_gross"]) for r in rows], float)
    otc = np.array([float(r["open_to_close"]) for r in rows], float)
    c2c = np.array([float(r["close_to_close"]) for r in rows], float)
    traded = np.array([int(r["traded"]) for r in rows])

    roll = []
    for i in range(int(ANNUAL) - 1, len(net)):
        roll.append((dates[i].isoformat(), sharpe_of(net[i - int(ANNUAL) + 1:i + 1])))
    with (HERE / "rolling_sharpe.csv").open("w", encoding="utf-8", newline="\n") as handle:
        writer = csv.writer(handle)
        writer.writerow(["date", "sharpe"])
        writer.writerows(roll)
    roll_s = np.array([s for _, s in roll], float)

    trail = path_stats(net[-int(ANNUAL):])
    trail_otc = path_stats(otc[-int(ANNUAL):])
    trail_c2c = path_stats(c2c[-int(ANNUAL):])

    order = sorted(range(len(net)), key=lambda i: (-net[i], dates[i].isoformat()))
    best_i = order[:10]
    worst_i = sorted(range(len(net)), key=lambda i: (net[i], dates[i].isoformat()))[:10]
    cleared = net.copy()
    cleared[best_i] = 0.0
    cleared_path = path_stats(cleared)
    total_sum = float(net.sum())
    best_sum = float(net[best_i].sum())
    share = float(best_sum / total_sum) if total_sum > 0 else None

    groups = {}
    for i, day in enumerate(dates):
        if traded[i] != 1:
            continue
        holiday = next_weekday(day)
        if not is_nyse_holiday(holiday):
            name = "other"
        else:
            name = holiday_name(holiday)
        bucket = groups.setdefault(name, {"n": 0, "net": [], "gross": [], "wins": 0})
        bucket["n"] += 1
        bucket["net"].append(float(net[i]))
        bucket["gross"].append(float(gross[i]))
        bucket["wins"] += int(net[i] > 0)
    order_names = [
        "New Year", "MLK Day", "Presidents Day", "Good Friday", "Memorial Day",
        "Juneteenth", "Independence Day", "Labor Day", "Thanksgiving", "Christmas",
        "Carter mourning", "other",
    ]
    holidays = []
    for name in order_names:
        bucket = groups.get(name)
        if not bucket:
            continue
        nets = np.array(bucket["net"], float)
        grosses = np.array(bucket["gross"], float)
        holidays.append({
            "holiday": name,
            "trades": bucket["n"],
            "win_rate": float(bucket["wins"] / bucket["n"]),
            "mean_net_bp": float(nets.mean() * 1e4),
            "mean_gross_bp": float(grosses.mean() * 1e4),
            "sum_net": float(nets.sum()),
        })

    def listed(idxs):
        return [{"date": dates[i].isoformat(), "strategy_net": float(net[i])} for i in idxs]

    out = {
        "label": "post hoc",
        "rolling_252": {
            "windows": len(roll),
            "last": roll[-1][1],
            "last_date": roll[-1][0],
            "min": float(roll_s.min()),
            "max": float(roll_s.max()),
            "fraction_negative": float(np.mean(roll_s < 0)),
        },
        "trailing_252": {
            "start": dates[-int(ANNUAL)].isoformat(),
            "end": dates[-1].isoformat(),
            "strategy": trail,
            "open_to_close": trail_otc,
            "close_to_close": trail_c2c,
        },
        "best_10": {
            "sessions": listed(best_i),
            "sum_net": best_sum,
            "sum_all_net": total_sum,
            "share_of_sum": share,
            "zeroed_return": cleared_path["total_return"],
            "zeroed_sharpe": cleared_path["sharpe"],
            "zeroed_ending_equity": cleared_path["ending_equity"],
        },
        "worst_10": {"sessions": listed(worst_i), "sum_net": float(net[worst_i].sum())},
        "by_holiday": holidays,
        "holiday_other": int(groups.get("other", {"n": 0})["n"]),
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(
        f"rolling last {out['rolling_252']['last']:.3f} "
        f"trailing return {trail['total_return']:.4f} "
        f"best10 share {share}"
    )


if __name__ == "__main__":
    main()
