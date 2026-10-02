# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. None of them is an acceptance input.

The daily-versus-minute block reads the store and counts price discrepancies.
It does not build a strategy return and it does not change the verdict.
"""

import csv
import datetime as dt
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, session_date_of  # noqa: E402

ANNUAL = 252


def sharpe(r):
    r = np.asarray(r, float)
    if r.size < 2:
        return 0.0
    sd = float(r.std(ddof=1))
    if sd <= 0:
        return 0.0
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def compound(r):
    e = 1.0
    for x in r:
        e *= 1.0 + float(x)
    return float(e - 1.0)


def load_daily():
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    dates = [r["date"] for r in rows]
    cols = {}
    for key in ("strategy_net", "strategy_gross", "bh_c2c", "open_to_close", "delayed_net"):
        cols[key] = np.array([float(r[key]) for r in rows])
    return dates, cols


def zeroed(r, idx):
    z = r.copy()
    z[idx] = 0.0
    return {"return": compound(z), "sharpe": sharpe(z)}


def concentration(dates, r, bh, oc):
    order = np.argsort(r)
    out = {"sum_strategy_net": float(r.sum())}
    day_rows = []
    for label, idx in (("best", order[::-1]), ("worst", order)):
        picked = []
        for i in idx[:5]:
            picked.append({
                "date": dates[int(i)],
                "strategy_net": float(r[int(i)]),
                "bh_c2c": float(bh[int(i)]),
                "open_to_close": float(oc[int(i)]),
            })
        day_rows.append((label, picked))
    out["best_days"] = day_rows[0][1]
    out["worst_days"] = day_rows[1][1]
    for n, name in ((10, "best_10"), (20, "best_20")):
        idx = order[-n:]
        total = float(r.sum())
        out[name] = {
            "n": n,
            "sum": float(r[idx].sum()),
            "share_of_sum": float(r[idx].sum() / total) if total != 0 else None,
            "zeroed": zeroed(r, idx),
        }
    return out


def rolling(dates, cols):
    n = len(dates)
    path = HERE / "rolling_sharpe.csv"
    keys = ("strategy_net", "bh_c2c", "open_to_close")
    names = ("strategy", "bh_c2c", "open_to_close")
    series = {name: [] for name in names}
    with path.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f)
        w.writerow(["date", "strategy", "bh_c2c", "open_to_close"])
        for i in range(ANNUAL - 1, n):
            sl = slice(i - (ANNUAL - 1), i + 1)
            vals = [sharpe(cols[k][sl]) for k in keys]
            w.writerow([dates[i], *[format(v, ".17g") for v in vals]])
            for name, v in zip(names, vals):
                series[name].append(v)
    summary = {}
    for name in names:
        a = np.array(series[name])
        summary[name] = {
            "windows": int(a.size),
            "min": float(a.min()),
            "max": float(a.max()),
            "frac_positive": float(np.mean(a > 0)),
            "last": float(a[-1]),
            "last_date": dates[-1],
        }
    return summary


def minute_gap(md, sym):
    daily = md.bars(sym, "1d", start="2021-09-27", end="2026-10-01")
    mins = md.bars(sym, "1m", start="2021-09-27", end="2026-10-01")
    first, last = {}, {}
    for b in mins:
        d = session_date_of(b.ts)
        if d not in first:
            first[d] = b.open
        last[d] = b.close
    open_bp, close_bp = [], []
    no_minute = 0
    open_gt = close_gt = 0
    first_open = first_close = None
    max_open = {"bp": 0.0, "date": None}
    max_close = {"bp": 0.0, "date": None}
    for b in daily:
        d = session_date_of(b.ts)
        if d not in first or first[d] == 0 or last[d] == 0:
            no_minute += 1
            continue
        ob = abs(b.open - first[d]) / first[d] * 1e4
        cb = abs(b.close - last[d]) / last[d] * 1e4
        open_bp.append(ob)
        close_bp.append(cb)
        if ob > 20.0:
            open_gt += 1
            if first_open is None:
                first_open = d.isoformat()
        if cb > 20.0:
            close_gt += 1
            if first_close is None:
                first_close = d.isoformat()
        if ob > max_open["bp"]:
            max_open = {"bp": float(ob), "date": d.isoformat()}
        if cb > max_close["bp"]:
            max_close = {"bp": float(cb), "date": d.isoformat()}
    return {
        "compared": len(open_bp),
        "no_minute": no_minute,
        "open_gt_20bp": open_gt,
        "close_gt_20bp": close_gt,
        "first_open_gt_20bp": first_open,
        "first_close_gt_20bp": first_close,
        "mean_abs_open_bp": float(np.mean(open_bp)) if open_bp else None,
        "mean_abs_close_bp": float(np.mean(close_bp)) if close_bp else None,
        "max_abs_open_bp": max_open,
        "max_abs_close_bp": max_close,
        "threshold_bp": 20.0,
        "note": "price discrepancy only; not a strategy return",
    }


def append_log(text):
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    dates, cols = load_daily()
    pred = res["predictions"]
    diff = pred["mean_close_to_open"] - pred["mean_open_to_close"]
    payload = {
        "label": "post hoc",
        "unit_conversion": {
            "mean_close_to_open_bp": pred["mean_close_to_open"] * 1e4,
            "mean_open_to_close_bp": pred["mean_open_to_close"] * 1e4,
            "mean_difference_bp": diff * 1e4,
        },
        "concentration": concentration(dates, cols["strategy_net"], cols["bh_c2c"], cols["open_to_close"]),
        "rolling_252": rolling(dates, cols),
        "negative_compound_years": [y["year"] for y in res["by_year"] if y["return"] < 0],
    }
    with MarketData() as md:
        payload["daily_vs_minute"] = {
            "SPY": minute_gap(md, "SPY"),
            "QQQ": minute_gap(md, "QQQ"),
        }
    (HERE / "posthoc.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8", newline="\n")
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = "yes" if subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip() else "no"
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    spy = payload["daily_vs_minute"]["SPY"]
    qqq = payload["daily_vs_minute"]["QQQ"]
    append_log(
        f"\n## {now}\n"
        f"- rules_sha256 {res['rules_sha256']}\n"
        f"- git_head {head} dirty={dirty}\n"
        f"- reason: post hoc daily-versus-1-minute price discrepancy count; "
        f"no strategy return; verdict unchanged\n"
        f"- SPY compared {spy['compared']} close>20bp {spy['close_gt_20bp']} "
        f"open>20bp {spy['open_gt_20bp']}; "
        f"QQQ compared {qqq['compared']} close>20bp {qqq['close_gt_20bp']} "
        f"open>20bp {qqq['open_gt_20bp']}\n"
    )
    print(
        f"posthoc written; SPY close>20bp {spy['close_gt_20bp']} of {spy['compared']}; "
        f"QQQ close>20bp {qqq['close_gt_20bp']} of {qqq['compared']}"
    )


if __name__ == "__main__":
    main()
