# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered SPY pre-holiday open-to-close study.

The self-test runs before the store is opened. A store run appends to RUNLOG.md.
"""

import csv
import hashlib
import json
import math
import subprocess
import sys
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, is_nyse_holiday, nyse_sessions

HERE = Path(__file__).resolve().parent
FIRST = date(2011, 1, 4)
LAST = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
NOTIONAL = 1.0
COST_BPS_SIDE = 1.0
GRID = (0.5, 0.75, 1.0, 1.25, 1.5)
COST_MULT = (0.0, 0.5, 1.0, 2.0, 3.0)
ANNUAL = 252.0
BLOCK = 20
N_DIRECTION = 2000
N_BOOT = 2000
N_TIMING = 500
SEED_DIRECTION = 20261091
SEED_BOOTSTRAP = 20261092
SEED_TIMING = 20261093
SEED_VERIFY = 20261094
EXPECTED_BARS = 3959
EXPECTED_PRE = 146
EXPECTED_PRE_IS = 123
EXPECTED_PRE_OOS = 23
EXPECTED_EVAL = 3951
EXPECTED_EVAL_IS = 3385
EXPECTED_EVAL_OOS = 566
FIRST_EVAL = date(2011, 1, 14)
ABSENT = (date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5), date(2025, 1, 9))
WEEKDAY = ("Monday", "Tuesday", "Wednesday", "Thursday", "Friday")


class DataError(Exception):
    pass


@dataclass
class Sess:
    day: date
    o: float
    h: float
    l: float
    c: float


def is_pre_holiday(session: date) -> bool:
    nxt = session + timedelta(days=1)
    while nxt.weekday() >= 5:
        nxt += timedelta(days=1)
    return is_nyse_holiday(nxt)


def rules_hash():
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    digest = hashlib.sha256(raw).hexdigest()
    text = (HERE / "RULES.lock").read_text(encoding="utf-8").replace("\r\n", "\n")
    fields = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        key, value = line.split(" ", 1)
        fields[key] = value
    if fields.get("sha256") != digest:
        raise SystemExit(
            f"RULES.md hash mismatch: {digest} != {fields.get('sha256')}"
        )
    return digest, fields


def clock_str(day: date, hh: int, mm: int) -> str:
    return f"{day.isoformat()} {hh:02d}:{mm:02d}"


def exit_stamp(day: date):
    if day in EARLY_CLOSES:
        return 13, 0, 3.5
    return 16, 0, 6.5


def run_book(bars, notional, cost_bps_side):
    first = next((i for i, b in enumerate(bars) if is_pre_holiday(b.day)), None)
    if first is None or first == 0:
        raise DataError("no stored bar before the first pre-holiday session")
    cost_side = cost_bps_side / 10000.0
    equity = 1.0
    prev_c = bars[first - 1].c
    trades = []
    daily = []
    for b in bars[first:]:
        gross = b.c / b.o - 1.0
        c2c = b.c / prev_c - 1.0
        prev_c = b.c
        traded = is_pre_holiday(b.day)
        if traded:
            net = notional * (gross - 2.0 * cost_side)
            strat_gross = notional * gross
            hh, mm, hours = exit_stamp(b.day)
            trades.append({
                "date": b.day,
                "side": "long",
                "entry_time": clock_str(b.day, 9, 30),
                "entry_px": b.o,
                "exit_time": clock_str(b.day, hh, mm),
                "exit_px": b.c,
                "gross": gross,
                "net": net,
                "exit_reason": "session_close",
                "hours": hours,
            })
        else:
            net = 0.0
            strat_gross = 0.0
        equity *= 1.0 + net
        daily.append({
            "date": b.day,
            "strategy_net": net,
            "strategy_gross": strat_gross,
            "open_to_close": gross,
            "close_to_close": c2c,
            "traded": 1 if traded else 0,
            "equity": equity,
        })
    return trades, daily


def sharpe_matrix(mat):
    mat = np.asarray(mat, float)
    if mat.ndim == 1:
        mat = mat.reshape(1, -1)
    mean = mat.mean(axis=1)
    sd = mat.std(axis=1, ddof=1)
    out = np.zeros(mat.shape[0], float)
    np.divide(mean, sd, out=out, where=sd > 0)
    out *= math.sqrt(ANNUAL)
    out[sd <= 0] = 0.0
    return out


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
    tstat = float(mean / sd * math.sqrt(n)) if sd > 0 and n > 1 else 0.0
    cagr = float(equity ** (ANNUAL / n) - 1.0) if n and equity > 0 else None
    return {
        "sessions": n,
        "mean": mean,
        "total_return": float(equity - 1.0) if n else 0.0,
        "cagr": cagr,
        "vol": float(sd * math.sqrt(ANNUAL)) if n > 1 else 0.0,
        "sharpe": sharpe,
        "max_dd": float(max_dd),
        "tstat": tstat,
        "ending_equity": float(equity) if n else 1.0,
    }


def trade_stats(nets, grosses, hours):
    nets = np.asarray(list(nets), float)
    grosses = np.asarray(list(grosses), float)
    equity = 1.0
    pos = neg = 0.0
    winners = []
    losers = []
    for net in nets:
        dollar = equity * float(net)
        if dollar > 0:
            pos += dollar
        elif dollar < 0:
            neg += dollar
        if net > 0:
            winners.append(float(net))
        elif net < 0:
            losers.append(float(net))
        equity *= 1.0 + float(net)
    if neg < 0:
        pf = float(pos / abs(neg))
    elif pos > 0:
        pf = None
    else:
        pf = 0.0
    h = np.asarray(list(hours), float)
    n = int(nets.size)
    return {
        "trades": n,
        "profit_factor": pf,
        "win_rate": float(np.mean(nets > 0)) if n else 0.0,
        "avg_net_trade_bp": float(nets.mean() * 1e4) if n else 0.0,
        "avg_gross_trade_bp": float(grosses.mean() * 1e4) if n else 0.0,
        "avg_winner_bp": float(np.mean(winners) * 1e4) if winners else None,
        "avg_loser_bp": float(np.mean(losers) * 1e4) if losers else None,
        "hold_hours_median": float(np.median(h)) if h.size else None,
        "hold_hours_mean": float(h.mean()) if h.size else None,
        "long_trades": n,
        "short_trades": 0,
        "early_close_trades": int(np.sum(h == 3.5)) if h.size else 0,
    }


def metrics_from(daily_rows, trades):
    out = path_stats(r["strategy_net"] for r in daily_rows)
    out.update(trade_stats(
        [t["net"] for t in trades],
        [t["gross"] for t in trades],
        [t["hours"] for t in trades],
    ))
    out["exposure"] = float(out["trades"] / out["sessions"]) if out["sessions"] else 0.0
    out["trades_per_year"] = (
        float(out["trades"] / (out["sessions"] / ANNUAL)) if out["sessions"] else 0.0
    )
    return out


def slice_book(daily, trades):
    is_d = [r for r in daily if r["date"] < OOS_START]
    oos_d = [r for r in daily if r["date"] >= OOS_START]
    is_t = [t for t in trades if t["date"] < OOS_START]
    oos_t = [t for t in trades if t["date"] >= OOS_START]
    return {
        "full": metrics_from(daily, trades),
        "is": metrics_from(is_d, is_t),
        "oos": metrics_from(oos_d, oos_t),
        "last_in_sample": is_d[-1]["date"] if is_d else None,
        "first_oos": oos_d[0]["date"] if oos_d else None,
    }


def bench_block(daily, key):
    is_d = [r for r in daily if r["date"] < OOS_START]
    oos_d = [r for r in daily if r["date"] >= OOS_START]
    return {
        "full": path_stats(r[key] for r in daily),
        "is": path_stats(r[key] for r in is_d),
        "oos": path_stats(r[key] for r in oos_d),
    }


def block_indices(starts_row, n, block=BLOCK):
    idx = [(int(s) + k) % n for s in starts_row for k in range(block)]
    return idx[:n]


def year_breakdown(daily):
    by = {}
    for row in daily:
        by.setdefault(row["date"].year, []).append(row)
    rows = []
    for year in sorted(by):
        chunk = by[year]
        nets = [r["strategy_net"] for r in chunk]
        path = path_stats(nets)
        rows.append({
            "year": year,
            "sessions": len(chunk),
            "trades": int(sum(r["traded"] for r in chunk)),
            "return": path["total_return"],
            "sharpe": path["sharpe"],
            "max_dd": path["max_dd"],
            "mean_strategy_net": path["mean"],
            "c2c_return": path_stats(r["close_to_close"] for r in chunk)["total_return"],
            "otc_return": path_stats(r["open_to_close"] for r in chunk)["total_return"],
            "c2c_sharpe": path_stats(r["close_to_close"] for r in chunk)["sharpe"],
            "otc_sharpe": path_stats(r["open_to_close"] for r in chunk)["sharpe"],
        })
    return rows


def weekday_breakdown(daily):
    by = {i: [] for i in range(5)}
    for row in daily:
        by[row["date"].weekday()].append(row)
    rows = []
    for i in range(5):
        chunk = by[i]
        path = path_stats(r["strategy_net"] for r in chunk) if chunk else None
        rows.append({
            "weekday": WEEKDAY[i],
            "sessions": len(chunk),
            "trades": int(sum(r["traded"] for r in chunk)) if chunk else 0,
            "mean_strategy_net": path["mean"] if path else None,
            "restarted_return": path["total_return"] if path else None,
            "mean_open_to_close": float(np.mean([r["open_to_close"] for r in chunk])) if chunk else None,
        })
    return rows


def quintiles(daily):
    order = sorted(range(len(daily)), key=lambda i: (daily[i]["close_to_close"], daily[i]["date"].isoformat()))
    n = len(daily)
    buckets = {q: [] for q in range(1, 6)}
    for rank, i in enumerate(order):
        buckets[rank * 5 // n + 1].append(daily[i])
    rows = []
    for q in range(1, 6):
        chunk = buckets[q]
        nets = [r["strategy_net"] for r in chunk]
        path = path_stats(nets)
        rows.append({
            "quintile": q,
            "sessions": len(chunk),
            "trades": int(sum(r["traded"] for r in chunk)),
            "mean_strategy_net": path["mean"],
            "restarted_return": path["total_return"],
            "mean_close_to_close": float(np.mean([r["close_to_close"] for r in chunk])),
            "mean_open_to_close": float(np.mean([r["open_to_close"] for r in chunk])),
            "sum_strategy_net": float(np.sum(nets)),
        })
    return rows


def grid_and_costs(daily):
    net = np.array([r["strategy_net"] for r in daily], float)
    gross = np.array([r["strategy_gross"] for r in daily], float)
    traded = np.array([r["traded"] for r in daily], float)
    is_mask = np.array([r["date"] < OOS_START for r in daily])
    cells = []
    for factor in GRID:
        scaled = factor * net
        cells.append({
            "notional": factor,
            "primary": factor == NOTIONAL,
            "is_sharpe": path_stats(scaled[is_mask])["sharpe"],
            "oos_sharpe": path_stats(scaled[~is_mask])["sharpe"],
            "is_return": path_stats(scaled[is_mask])["total_return"],
            "oos_return": path_stats(scaled[~is_mask])["total_return"],
            "full_sharpe": path_stats(scaled)["sharpe"],
            "full_return": path_stats(scaled)["total_return"],
        })
    is_s = np.array([c["is_sharpe"] for c in cells])
    oos_s = np.array([c["oos_sharpe"] for c in cells])
    tied = float(is_s.max() - is_s.min()) <= 1e-12
    ranked = sorted(cells, key=lambda c: (-c["is_sharpe"], c["notional"]))
    for rank, cell in enumerate(ranked, start=1):
        cell["is_rank"] = rank
    corr = None
    if float(is_s.std(ddof=1)) > 1e-12 and float(oos_s.std(ddof=1)) > 1e-12:
        corr = float(np.corrcoef(is_s, oos_s)[0, 1])
    summary = {
        "n_cells": len(cells),
        "n_is_positive": int(sum(c["is_sharpe"] > 0 for c in cells)),
        "share_is_positive": float(sum(c["is_sharpe"] > 0 for c in cells) / len(cells)),
        "primary_is_rank": next(c["is_rank"] for c in cells if c["primary"]),
        "is_sharpes_tied": tied,
        "is_sharpe_span": float(is_s.max() - is_s.min()),
        "is_best_notional": None if tied else ranked[0]["notional"],
        "is_best_is_sharpe": ranked[0]["is_sharpe"],
        "is_best_oos_sharpe": ranked[0]["oos_sharpe"],
        "is_oos_sharpe_corr": corr,
    }
    costs = []
    for mult in COST_MULT:
        series = gross - traded * (2.0 * mult * COST_BPS_SIDE / 10000.0)
        costs.append({
            "multiplier": mult,
            "cost_bps_side": mult * COST_BPS_SIDE,
            "full_sharpe": path_stats(series)["sharpe"],
            "oos_sharpe": path_stats(series[~is_mask])["sharpe"],
            "is_sharpe": path_stats(series[is_mask])["sharpe"],
            "full_return": path_stats(series)["total_return"],
            "oos_return": path_stats(series[~is_mask])["total_return"],
            "is_return": path_stats(series[is_mask])["total_return"],
        })
    primary_sharpe = path_stats(net)["sharpe"]
    base = next(c for c in costs if c["multiplier"] == 1.0)
    if abs(base["full_sharpe"] - primary_sharpe) > 1e-9:
        raise RuntimeError("cost multiplier 1 does not reproduce the primary Sharpe")
    unit = next(c for c in cells if c["primary"])
    if abs(unit["full_return"] - path_stats(net)["total_return"]) > 1e-12:
        raise RuntimeError("notional 1 does not reproduce the primary return")
    return cells, summary, costs, gross, traded


def breakeven(gross, traded, cap_bps=50.0):
    def total(bps):
        series = gross - traded * (2.0 * bps / 10000.0)
        equity = 1.0
        for x in series:
            equity *= 1.0 + float(x)
        return equity - 1.0

    if total(0.0) <= 0.0:
        return {"bps_side": None, "reason": "non_positive_at_zero_cost", "return_at_0": total(0.0)}
    if total(cap_bps) > 0.0:
        return {"bps_side": None, "reason": "still_positive_at_cap", "return_at_cap": total(cap_bps)}
    lo, hi = 0.0, cap_bps
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if total(mid) > 0.0:
            lo = mid
        else:
            hi = mid
    return {"bps_side": float(hi), "reason": "crossed", "return_at_0": total(0.0)}


def placebo_and_bootstrap(daily):
    gross = np.array([r["strategy_gross"] for r in daily], float)
    net = np.array([r["strategy_net"] for r in daily], float)
    otc = np.array([r["open_to_close"] for r in daily], float)
    trade_idx = np.flatnonzero(np.array([r["traded"] for r in daily]) == 1)
    n = int(gross.size)
    k = int(trade_idx.size)
    rng = np.random.default_rng(SEED_DIRECTION)
    signs = rng.choice(np.array([-1.0, 1.0]), size=(N_DIRECTION, k))
    mat = np.zeros((N_DIRECTION, n), float)
    mat[:, trade_idx] = gross[trade_idx] * signs
    actual = float(sharpe_matrix(gross)[0])
    null = sharpe_matrix(mat)
    n_ge = int(np.sum(null >= actual))
    direction = {
        "actual_gross_sharpe": actual,
        "p": float((1 + n_ge) / (N_DIRECTION + 1)),
        "null_mean": float(null.mean()),
        "null_p95": float(np.percentile(null, 95)),
        "n_draws": N_DIRECTION,
        "n_ge": n_ge,
        "seed": SEED_DIRECTION,
    }
    rng_t = np.random.default_rng(SEED_TIMING)
    timing_mat = np.zeros((N_TIMING, n), float)
    for draw in range(N_TIMING):
        picked = rng_t.choice(n, size=k, replace=False)
        timing_mat[draw, picked] = otc[picked]
    timing_null = sharpe_matrix(timing_mat)
    n_ge_t = int(np.sum(timing_null >= actual))
    timing = {
        "actual_gross_sharpe": actual,
        "p": float((1 + n_ge_t) / (N_TIMING + 1)),
        "null_mean": float(timing_null.mean()),
        "null_p95": float(np.percentile(timing_null, 95)),
        "n_draws": N_TIMING,
        "n_ge": n_ge_t,
        "n_picked": k,
        "seed": SEED_TIMING,
    }
    rng_b = np.random.default_rng(SEED_BOOTSTRAP)
    n_blocks = math.ceil(n / BLOCK)
    starts = rng_b.integers(0, n, size=(N_BOOT, n_blocks))
    boot = np.empty(N_BOOT, float)
    for draw in range(N_BOOT):
        boot[draw] = float(sharpe_matrix(net[block_indices(starts[draw], n)])[0])
    bootstrap = {
        "p2_5": float(np.percentile(boot, 2.5)),
        "p50": float(np.percentile(boot, 50)),
        "p97_5": float(np.percentile(boot, 97.5)),
        "n_draws": N_BOOT,
        "block": BLOCK,
        "seed": SEED_BOOTSTRAP,
        "actual_net_sharpe": float(sharpe_matrix(net)[0]),
    }
    return direction, null, timing, timing_null, bootstrap


def predictions(daily):
    otc = np.array([r["open_to_close"] for r in daily], float)
    traded = np.array([r["traded"] == 1 for r in daily])
    oos = np.array([r["date"] >= OOS_START for r in daily])

    def pair(group, other):
        return float(otc[group].mean()), float(otc[other].mean()), int(group.sum()), int(other.sum())

    pre, other, n_pre, n_other = pair(traded, ~traded)
    pre_o, other_o, n_pre_o, n_other_o = pair(traded & oos, ~traded & oos)
    return {
        "full_mean_pre": pre,
        "full_mean_other": other,
        "full_mean_pre_bp": pre * 1e4,
        "full_mean_other_bp": other * 1e4,
        "full_difference_bp": (pre - other) * 1e4,
        "full_n_pre": n_pre,
        "full_n_other": n_other,
        "pred1_holds": bool(pre > other),
        "oos_mean_pre": pre_o,
        "oos_mean_other": other_o,
        "oos_mean_pre_bp": pre_o * 1e4,
        "oos_mean_other_bp": other_o * 1e4,
        "oos_difference_bp": (pre_o - other_o) * 1e4,
        "oos_n_pre": n_pre_o,
        "oos_n_other": n_other_o,
        "pred2_holds": bool(pre_o > other_o),
    }


def pearson(a, b):
    a = np.asarray(a, float)
    b = np.asarray(b, float)
    if float(a.std(ddof=1)) == 0.0 or float(b.std(ddof=1)) == 0.0:
        return None
    return float(np.corrcoef(a, b)[0, 1])


def acceptance(spy, qqq_oos_sharpe, grid_summary, costs, placebo):
    oos = spy["oos"]
    pf = oos["profit_factor"]
    line1 = bool(oos["sharpe"] >= 0.5 and (pf is None or pf >= 1.10))
    line2 = bool(placebo["p"] <= 0.05)
    line3 = bool(spy["is"]["sharpe"] > 0 and grid_summary["share_is_positive"] >= 0.60)
    two_x = next(c for c in costs if c["multiplier"] == 2.0)
    line4 = bool(two_x["full_return"] > 0)
    line5 = bool(qqq_oos_sharpe > 0)
    line6 = bool(oos["trades"] >= 15)
    lines = [
        {"id": 1, "pass": line1, "oos_sharpe": oos["sharpe"], "oos_profit_factor": pf,
         "required_sharpe": 0.5, "required_pf": 1.10},
        {"id": 2, "pass": line2, "p": placebo["p"], "required_p": 0.05},
        {"id": 3, "pass": line3, "is_sharpe": spy["is"]["sharpe"],
         "n_is_positive": grid_summary["n_is_positive"], "n_cells": grid_summary["n_cells"],
         "required_is_sharpe": 0.0, "required_share": 0.60},
        {"id": 4, "pass": line4, "full_return_2x": two_x["full_return"], "required": 0.0},
        {"id": 5, "pass": line5, "qqq_oos_sharpe": qqq_oos_sharpe, "required": 0.0},
        {"id": 6, "pass": line6, "oos_trades": oos["trades"], "required_trades": 15},
    ]
    if not line6:
        status = "Inconclusive"
    elif all(line["pass"] for line in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    return {
        "lines": lines,
        "status": status,
        "n_lines": len(lines),
        "n_failed": int(sum(not line["pass"] for line in lines)),
        "n_passed": int(sum(line["pass"] for line in lines)),
    }


def ohlc_problem(b: Sess):
    if min(b.o, b.h, b.l, b.c) <= 0:
        return "non-positive price"
    if b.h + 1e-6 < max(b.o, b.c):
        return "high below open or close"
    if b.l - 1e-6 > min(b.o, b.c):
        return "low above open or close"
    return None


def validate_symbol(name, bars):
    if len(bars) != EXPECTED_BARS:
        raise DataError(f"{name} has {len(bars)} daily bars, expected {EXPECTED_BARS}")
    if bars[0].day != FIRST or bars[-1].day != LAST:
        raise DataError(f"{name} range is {bars[0].day} .. {bars[-1].day}")
    seen = set()
    for b in bars:
        if b.day in seen:
            raise DataError(f"{name} duplicate {b.day}")
        seen.add(b.day)
        if b.day.weekday() >= 5 or is_nyse_holiday(b.day):
            raise DataError(f"{name} bar on a non-session {b.day}")
        problem = ohlc_problem(b)
        if problem:
            raise DataError(f"{name} {b.day} {problem}")
    for day in ABSENT:
        if day in seen:
            raise DataError(f"{name} should not have {day}")
    if date(2021, 12, 31) not in seen:
        raise DataError(f"{name} is missing 2021-12-31")
    calendar = nyse_sessions(FIRST, LAST)
    missing = [d for d in calendar if d not in seen]
    if missing != list(ABSENT[:3]):
        raise DataError(f"{name} missing sessions {missing}")


def validate_pre(bars):
    days = [b.day for b in bars]
    pre = [d for d in days if is_pre_holiday(d)]
    if len(pre) != EXPECTED_PRE:
        raise DataError(f"pre-holiday count {len(pre)}")
    if sum(d < OOS_START for d in pre) != EXPECTED_PRE_IS:
        raise DataError("in-sample pre-holiday count mismatch")
    if sum(d >= OOS_START for d in pre) != EXPECTED_PRE_OOS:
        raise DataError("out-of-sample pre-holiday count mismatch")
    if pre[0] != FIRST_EVAL:
        raise DataError(f"first pre-holiday is {pre[0]}")


def book_payload(bars):
    trades, daily = run_book(bars, NOTIONAL, COST_BPS_SIDE)
    if len(daily) != EXPECTED_EVAL:
        raise DataError(f"evaluation sessions {len(daily)}")
    if daily[0]["date"] != FIRST_EVAL or daily[-1]["date"] != LAST:
        raise DataError("evaluation window endpoints")
    packed = slice_book(daily, trades)
    if packed["full"]["sessions"] != EXPECTED_EVAL:
        raise DataError("full session count")
    if packed["is"]["sessions"] != EXPECTED_EVAL_IS or packed["oos"]["sessions"] != EXPECTED_EVAL_OOS:
        raise DataError("slice session count")
    if packed["full"]["trades"] != EXPECTED_PRE:
        raise DataError("trade count")
    if packed["is"]["trades"] != EXPECTED_PRE_IS or packed["oos"]["trades"] != EXPECTED_PRE_OOS:
        raise DataError("slice trade count")
    packed["benchmark_otc"] = bench_block(daily, "open_to_close")
    packed["benchmark_c2c"] = bench_block(daily, "close_to_close")
    return trades, daily, packed


def clean(obj):
    if isinstance(obj, dict):
        return {str(k): clean(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [clean(v) for v in obj]
    if isinstance(obj, np.ndarray):
        return clean(obj.tolist())
    if isinstance(obj, (np.floating, float)):
        x = float(obj)
        if math.isnan(x) or math.isinf(x):
            return None
        return x
    if isinstance(obj, np.integer):
        return int(obj)
    if isinstance(obj, np.bool_):
        return bool(obj)
    if isinstance(obj, date):
        return obj.isoformat()
    return obj


def fnum(x):
    return format(float(x), ".17g")


def git_state():
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    return head, "yes" if dirty else "no"


def append_log(lines):
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nAppend-only. One entry per store run.\n", encoding="utf-8", newline="\n")
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def expect(name, cond):
    if not cond:
        raise SystemExit(f"self-test failed: {name}")


def near(a, b):
    return math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12)


def hand(iso, o, c):
    return Sess(date.fromisoformat(iso), o, max(o, c), min(o, c), c)


def self_test():
    expect("thursday eve", is_pre_holiday(date(2024, 3, 28)))
    expect("friday eve", is_pre_holiday(date(2024, 1, 12)))
    expect("ordinary friday", not is_pre_holiday(date(2024, 1, 5)))
    expect("early close not eve", not is_pre_holiday(date(2024, 11, 29)))
    expect("early close is eve", is_pre_holiday(date(2024, 7, 3)) and date(2024, 7, 3) in EARLY_CLOSES)
    expect("juneteenth weekend", is_pre_holiday(date(2022, 6, 17)))
    expect("new year friday", not is_pre_holiday(date(2021, 12, 31)))
    expect("mourning eve", is_pre_holiday(date(2025, 1, 8)))
    expect("mourning day", is_nyse_holiday(date(2025, 1, 9)) and not is_pre_holiday(date(2025, 1, 9)))

    bars = [
        hand("2021-12-31", 10, 10),
        hand("2022-06-17", 100, 101),
        hand("2024-01-05", 200, 210),
        hand("2024-01-12", 50, 49),
        hand("2024-03-28", 80, 80),
        hand("2024-07-03", 40, 42),
        hand("2024-11-29", 70, 77),
        hand("2025-01-08", 90, 91),
    ]
    trades, daily = run_book(bars, 1.0, 1.0)
    expect("window", len(daily) == 7 and len(trades) == 5)
    expect("dropped", daily[0]["date"] == date(2022, 6, 17))
    expect("no leading friday", all(r["date"] != date(2021, 12, 31) for r in daily))
    by = {t["date"]: t for t in trades}
    ordinary = next(r for r in daily if r["date"] == date(2024, 1, 5))
    expect("ordinary zero", ordinary["strategy_net"] == 0.0 and ordinary["traded"] == 0)
    expect("ordinary otc", near(ordinary["open_to_close"], 0.05))
    expect("ordinary c2c", near(ordinary["close_to_close"], 210 / 101 - 1))
    expect("equity held across flat", near(ordinary["equity"], 1.0098))
    early = next(r for r in daily if r["date"] == date(2024, 11, 29))
    expect("early not a trade", early["traded"] == 0 and early["strategy_net"] == 0.0)
    expect("early absent", date(2024, 11, 29) not in by)
    june = by[date(2022, 6, 17)]
    expect("june prices", june["entry_px"] == 100 and june["exit_px"] == 101 and june["side"] == "long")
    expect("june reason", june["exit_reason"] == "session_close")
    expect("june stamp", june["entry_time"] == "2022-06-17 09:30" and june["exit_time"] == "2022-06-17 16:00")
    expect("june cost", near(june["gross"], 0.01) and near(june["net"], 0.0098) and june["hours"] == 6.5)
    friday = by[date(2024, 1, 12)]
    expect("friday down", near(friday["gross"], -0.02) and near(friday["net"], -0.0202))
    expect("friday stamp", friday["exit_time"] == "2024-01-12 16:00")
    flat = by[date(2024, 3, 28)]
    expect("flat both sides", near(flat["gross"], 0.0) and near(flat["net"], -0.0002))
    expect("flat stamp", flat["exit_time"] == "2024-03-28 16:00")
    july = by[date(2024, 7, 3)]
    expect("july early stamp", july["entry_time"] == "2024-07-03 09:30" and july["exit_time"] == "2024-07-03 13:00")
    expect("july hours", july["hours"] == 3.5)
    expect("july net", near(july["gross"], 0.05) and near(july["net"], 0.0498))
    mourning = by[date(2025, 1, 8)]
    expect("mourning gross", near(mourning["gross"], 91 / 90 - 1))
    expect("mourning net", near(mourning["net"], 91 / 90 - 1 - 0.0002))
    after_loss = next(r for r in daily if r["date"] == date(2024, 1, 12))
    expect("compound", near(after_loss["equity"], 1.0098 * (1.0 - 0.0202)))
    expect("daily matches trade", near(after_loss["strategy_net"], friday["net"]))

    down = run_book([hand("2022-06-16", 100, 100), hand("2022-06-17", 100, 99)], 1.0, 1.0)[0][0]
    expect("down both sides", near(down["gross"], -0.01) and near(down["net"], -0.0102))
    scaled = run_book([hand("2022-06-16", 100, 100), hand("2022-06-17", 100, 101)], 1.5, 1.0)[0][0]
    expect("notional scales", near(scaled["net"], 0.0147))
    half = run_book([hand("2022-06-16", 100, 100), hand("2022-06-17", 100, 100)], 0.5, 1.0)[0][0]
    expect("half flat cost", near(half["net"], -0.0001))

    sample = np.array([0.01, 0.0, -0.002, 0.004])
    expect("sharpe helpers", near(float(sharpe_matrix(sample)[0]), path_stats(sample)["sharpe"]))
    hand_mean = 0.005
    hand_sd = math.sqrt((0.005 ** 2 + 0.005 ** 2) / 1)
    expect("sharpe formula", near(path_stats([0.01, 0.0])["sharpe"], hand_mean / hand_sd * math.sqrt(252)))
    expect("zero sd", path_stats([0.01, 0.01])["sharpe"] == 0.0)
    stepped = path_stats([0.10, -0.10])
    expect("path return", near(stepped["total_return"], 0.99 - 1.0))
    expect("path dd", near(stepped["max_dd"], 0.99 / 1.1 - 1.0))
    pf = trade_stats([0.10, -0.05], [0.10, -0.05], [6.5, 6.5])
    expect("profit factor", near(pf["profit_factor"], 0.10 / 0.055))
    expect("blocks", block_indices([3, 1, 4], 5, block=2) == [3, 4, 1, 2, 4])


def load_symbol(md, sym):
    raw = md.bars(sym, "1d", start=FIRST.isoformat(), end=LAST.isoformat())
    return [Sess(b.session, b.open, b.high, b.low, b.close) for b in raw]


def write_outputs(trades, daily, results, direction_null, timing_null):
    with (HERE / "trades.csv").open("w", encoding="utf-8", newline="\n") as handle:
        writer = csv.writer(handle)
        writer.writerow(["side", "entry_time", "entry_px", "exit_time", "exit_px", "gross", "net", "exit_reason"])
        for trade in trades:
            writer.writerow([
                trade["side"], trade["entry_time"], fnum(trade["entry_px"]), trade["exit_time"],
                fnum(trade["exit_px"]), fnum(trade["gross"]), fnum(trade["net"]), trade["exit_reason"],
            ])
    with (HERE / "daily.csv").open("w", encoding="utf-8", newline="\n") as handle:
        writer = csv.writer(handle)
        writer.writerow(["date", "strategy_net", "strategy_gross", "open_to_close", "close_to_close", "traded", "equity"])
        for row in daily:
            writer.writerow([
                row["date"].isoformat(), fnum(row["strategy_net"]), fnum(row["strategy_gross"]),
                fnum(row["open_to_close"]), fnum(row["close_to_close"]), row["traded"], fnum(row["equity"]),
            ])
    np.save(HERE / "placebo_direction.npy", np.asarray(direction_null, float))
    np.save(HERE / "placebo_timing.npy", np.asarray(timing_null, float))
    (HERE / "results.json").write_text(
        json.dumps(clean(results), indent=2) + "\n", encoding="utf-8", newline="\n"
    )


def main():
    digest, locked = rules_hash()
    self_test()
    reason = sys.argv[1] if len(sys.argv) > 1 else "initial pre-registered run"
    head, dirty = git_state()
    try:
        with MarketData() as md:
            books = {sym: load_symbol(md, sym) for sym in ("SPY", "QQQ", "IWM")}
            for sym, bars in books.items():
                validate_symbol(sym, bars)
                if md.corporate_actions(sym):
                    raise DataError(f"{sym} corporate_action is not empty")
            spy_days = [b.day for b in books["SPY"]]
            for sym in ("QQQ", "IWM"):
                if [b.day for b in books[sym]] != spy_days:
                    raise DataError(f"{sym} session dates differ from SPY")
            validate_pre(books["SPY"])
            spy_trades, spy_daily, spy = book_payload(books["SPY"])
            _qqq_trades, qqq_daily, qqq = book_payload(books["QQQ"])
            _iwm_trades, iwm_daily, iwm = book_payload(books["IWM"])
    except DataError as exc:
        now = datetime.now(timezone.utc).isoformat(timespec="seconds")
        append_log([
            f"## {now}",
            f"- rules_sha256 {digest}",
            f"- git_head {head} dirty={dirty}",
            f"- reason: data check failed before results were written",
            f"- {exc}",
        ])
        raise SystemExit(str(exc)) from exc

    cells, summary, costs, gross, traded = grid_and_costs(spy_daily)
    direction, direction_null, timing, timing_null, bootstrap = placebo_and_bootstrap(spy_daily)
    preds = predictions(spy_daily)
    verdict = acceptance(spy, qqq["oos"]["sharpe"], summary, costs, direction)
    spy_net = np.array([r["strategy_net"] for r in spy_daily])
    qqq_net = np.array([r["strategy_net"] for r in qqq_daily])
    iwm_net = np.array([r["strategy_net"] for r in iwm_daily])
    results = {
        "rules_sha256": digest,
        "locked_utc": locked.get("locked_utc"),
        "git_head_at_lock": locked.get("git_head"),
        "git_head": head,
        "git_dirty": dirty,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOTSTRAP,
            "timing": SEED_TIMING,
            "verify": SEED_VERIFY,
        },
        "parameters": {
            "notional": NOTIONAL,
            "cost_bps_side": COST_BPS_SIDE,
            "first": FIRST,
            "last": LAST,
            "oos_start": OOS_START,
            "first_evaluation": spy_daily[0]["date"],
            "last_in_sample": spy["last_in_sample"],
            "first_oos": spy["first_oos"],
        },
        "spy": spy,
        "qqq": qqq,
        "iwm": iwm,
        "correlation": {
            "spy_qqq": pearson(spy_net, qqq_net),
            "spy_iwm": pearson(spy_net, iwm_net),
            "qqq_iwm": pearson(qqq_net, iwm_net),
        },
        "grid": {"cells": cells, "summary": summary},
        "costs": costs,
        "breakeven": breakeven(gross, traded),
        "placebo_direction": direction,
        "placebo_timing": timing,
        "bootstrap": bootstrap,
        "predictions": preds,
        "years": year_breakdown(spy_daily),
        "weekdays": weekday_breakdown(spy_daily),
        "quintiles": quintiles(spy_daily),
        "sides": {"long": spy["full"]["trades"], "short": 0},
        "exit_reasons": {"session_close": spy["full"]["trades"]},
        "acceptance": verdict,
    }
    write_outputs(spy_trades, spy_daily, results, direction_null, timing_null)
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    full, inn, oos = spy["full"], spy["is"], spy["oos"]
    append_log([
        f"## {now}",
        f"- rules_sha256 {digest}",
        f"- git_head {head} dirty={dirty}",
        f"- reason: {reason}",
        (
            f"- SPY full Sharpe {full['sharpe']:.4f} return {full['total_return']:.4f} | "
            f"IS Sharpe {inn['sharpe']:.4f} | "
            f"OOS Sharpe {oos['sharpe']:.4f} return {oos['total_return']:.4f} | "
            f"trades full/IS/OOS {full['trades']}/{inn['trades']}/{oos['trades']} | "
            f"status {verdict['status']}"
        ),
    ])
    print(
        f"status {verdict['status']} OOS Sharpe {oos['sharpe']:.4f} "
        f"return {oos['total_return']:.4f} trades {oos['trades']}"
    )


if __name__ == "__main__":
    main()
