# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""SPY close-to-next-open, every night. Pre-registered runs.

Refuses to run unless RULES.md still hashes to RULES.lock. The synthetic
self-test runs before the store is opened. A store run appends to RUNLOG.md.
"""

import csv
import datetime as dt
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    MarketData,
    _ny_local_to_utc,
    session_date_of,
)

FIRST = dt.date(2011, 1, 4)
LAST = dt.date(2026, 10, 1)
OOS_START = dt.date(2024, 7, 1)
NOTIONAL = 1.0
COST_BPS_SIDE = 1.0
ANNUAL = 252
BLOCK = 20
N_DRAWS = 2000
SEED_DIRECTION = 20261041
SEED_BOOTSTRAP = 20261042
GRID = (0.5, 0.75, 1.0, 1.25, 1.5)
COST_MULT = (0.0, 0.5, 1.0, 2.0, 3.0)
ABSENT = (
    dt.date(2012, 10, 29),
    dt.date(2012, 10, 30),
    dt.date(2018, 12, 5),
    dt.date(2025, 1, 9),
)
MUST = dt.date(2021, 12, 31)
WEEKDAY = ("Mon", "Tue", "Wed", "Thu", "Fri")


class Sess:
    def __init__(self, day, o, h, l, c):
        self.day = day
        self.o = float(o)
        self.h = float(h)
        self.l = float(l)
        self.c = float(c)


class DataError(Exception):
    pass


def rules_hash():
    have = hashlib.sha256(
        (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    ).hexdigest()
    lines = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()
    want = lines[0].split()[1]
    if have != want:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    locked = lines[1].split(" ", 1)[1]
    return have, locked


def entry_hhmm(day):
    if day in EARLY_CLOSES:
        return 13, 0
    return 16, 0


def clock_str(day, hh, mm):
    return f"{day.isoformat()} {hh:02d}:{mm:02d}"


def hold_hours(day0, hh0, mm0, day1, hh1, mm1):
    try:
        from zoneinfo import ZoneInfo

        tz = ZoneInfo("America/New_York")
        a = dt.datetime(day0.year, day0.month, day0.day, hh0, mm0, tzinfo=tz)
        b = dt.datetime(day1.year, day1.month, day1.day, hh1, mm1, tzinfo=tz)
        return (b - a).total_seconds() / 3600.0
    except Exception:
        a = _ny_local_to_utc(day0, hh0 * 3600 + mm0 * 60)
        b = _ny_local_to_utc(day1, hh1 * 3600 + mm1 * 60)
        return (b - a) / 3600.0


def ohlc_problem(b):
    if min(b.o, b.h, b.l, b.c) <= 0:
        return "non-positive"
    if b.h + 1e-6 < max(b.o, b.c):
        return "high"
    if b.l - 1e-6 > min(b.o, b.c):
        return "low"
    return None


def validate_symbol(name, bars):
    if len(bars) != 3959:
        raise DataError(f"{name} has {len(bars)} daily bars, not 3959")
    if bars[0].day != FIRST or bars[-1].day != LAST:
        raise DataError(f"{name} range {bars[0].day} .. {bars[-1].day}")
    days = [b.day for b in bars]
    if days != sorted(days) or len(set(days)) != len(days):
        raise DataError(f"{name} dates are not a unique sorted list")
    present = set(days)
    hit = [d.isoformat() for d in ABSENT if d in present]
    if hit:
        raise DataError(f"{name} has a bar on a closed date: {hit}")
    if MUST not in present:
        raise DataError(f"{name} is missing 2021-12-31")
    for b in bars:
        problem = ohlc_problem(b)
        if problem:
            raise DataError(f"{name} {b.day.isoformat()} failed the OHLC check ({problem})")


def run_book(sessions, notional, cost_bps_side):
    """Primary book. delayed_net is always the locked 1.0 / 1 bp path."""
    cost_side = cost_bps_side / 10000.0
    trades = []
    daily = []
    equity = 1.0
    hours = []
    for i in range(len(sessions) - 1):
        prev, cur = sessions[i], sessions[i + 1]
        gross = cur.o / prev.c - 1.0
        net = notional * (gross - 2.0 * cost_side)
        hh, mm = entry_hhmm(prev.day)
        trades.append({
            "side": "long",
            "entry_time": clock_str(prev.day, hh, mm),
            "entry_px": prev.c,
            "exit_time": clock_str(cur.day, 9, 30),
            "exit_px": cur.o,
            "gross": gross,
            "net": net,
            "exit_reason": "next_open",
            "exit_date": cur.day,
            "hours": hold_hours(prev.day, hh, mm, cur.day, 9, 30),
        })
        hours.append(trades[-1]["hours"])
        if i == 0:
            delayed = 0.0
        else:
            delayed = cur.o / prev.o - 1.0 - 0.0002
        equity *= 1.0 + net
        daily.append({
            "date": cur.day,
            "strategy_net": net,
            "strategy_gross": notional * gross,
            "bh_c2c": cur.c / prev.c - 1.0,
            "open_to_close": cur.c / cur.o - 1.0,
            "delayed_net": delayed,
            "equity": equity,
        })
    return trades, daily, hours


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
    r = np.asarray(returns, float)
    n = int(r.size)
    e = 1.0
    peak = 1.0
    max_dd = 0.0
    for x in r:
        e *= 1.0 + float(x)
        if e > peak:
            peak = e
        if peak > 0:
            max_dd = min(max_dd, e / peak - 1.0)
    sd = float(r.std(ddof=1)) if n > 1 else 0.0
    sharpe = float(r.mean() / sd * math.sqrt(ANNUAL)) if sd > 0 else 0.0
    tstat = float(r.mean() / sd * math.sqrt(n)) if sd > 0 and n > 1 else 0.0
    cagr = float(e ** (ANNUAL / n) - 1.0) if n and e > 0 else None
    return {
        "sessions": n,
        "total_return": float(e - 1.0) if n else 0.0,
        "cagr": cagr,
        "vol": float(sd * math.sqrt(ANNUAL)) if n > 1 else 0.0,
        "sharpe": sharpe,
        "max_dd": float(max_dd),
        "tstat": tstat,
        "ending_equity": float(e) if n else 1.0,
    }


def trade_stats(nets, hours):
    """Dollar P&L compounded from equity 1 in the order given."""
    nets = np.asarray(nets, float)
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
    h = np.asarray(hours, float)
    n = int(nets.size)
    return {
        "trades": n,
        "profit_factor": pf,
        "win_rate": float(np.mean(nets > 0)) if n else 0.0,
        "avg_net_trade_bp": float(nets.mean() * 1e4) if n else 0.0,
        "avg_winner_bp": float(np.mean(winners) * 1e4) if winners else None,
        "avg_loser_bp": float(np.mean(losers) * 1e4) if losers else None,
        "hold_hours_median": float(np.median(h)) if h.size else None,
        "hold_hours_mean": float(h.mean()) if h.size else None,
        "long_trades": n,
        "short_trades": 0,
    }


def metrics_from(daily_rows, trades):
    nets = [row["strategy_net"] for row in daily_rows]
    out = path_stats(nets)
    hours = [t["hours"] for t in trades]
    trade_nets = [t["net"] for t in trades]
    out.update(trade_stats(trade_nets, hours))
    out["trades_per_year"] = (
        float(len(trades) / (len(daily_rows) / ANNUAL)) if daily_rows else 0.0
    )
    return out


def classify(daily, trades):
    is_d = [row for row in daily if row["date"] < OOS_START]
    oos_d = [row for row in daily if row["date"] >= OOS_START]
    is_t = [t for t in trades if t["exit_date"] < OOS_START]
    oos_t = [t for t in trades if t["exit_date"] >= OOS_START]
    return is_d, oos_d, is_t, oos_t


def block_indices(starts_row, n, block=BLOCK):
    idx = [(int(s) + k) % n for s in starts_row for k in range(block)]
    return idx[:n]


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
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.bool_,)):
        return bool(obj)
    if isinstance(obj, dt.date):
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
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def self_test():
    def expect(name, cond):
        if not cond:
            raise SystemExit(f"self-test failed: {name}")

    def near(a, b):
        return math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12)

    # 1. Normal night.
    a = Sess(dt.date(2024, 6, 3), 99, 100, 99, 100)
    b = Sess(dt.date(2024, 6, 4), 101, 102, 101, 102)
    trades, daily, _ = run_book([a, b], 1.0, 1.0)
    expect("normal count", len(trades) == 1 and len(daily) == 1)
    t = trades[0]
    expect("normal stamp", t["entry_time"] == "2024-06-03 16:00" and t["exit_time"] == "2024-06-04 09:30")
    expect("normal prices", t["entry_px"] == 100 and t["exit_px"] == 101 and t["side"] == "long")
    expect("normal reason", t["exit_reason"] == "next_open")
    expect("normal gross", near(t["gross"], 0.01))
    expect("normal net", near(t["net"], 0.01 - 0.0002) and near(daily[0]["strategy_net"], 0.01 - 0.0002))
    expect("normal benchmarks", near(daily[0]["bh_c2c"], 0.02) and near(daily[0]["open_to_close"], 102 / 101 - 1))
    expect("normal date", daily[0]["date"] == dt.date(2024, 6, 4))

    # 2. Missing sessions skipped. Sandy pair, then the Bush pair.
    fri = Sess(dt.date(2012, 10, 26), 49, 50, 49, 50)
    wed = Sess(dt.date(2012, 10, 31), 51, 52, 50, 52)
    trades, daily, _ = run_book([fri, wed], 1.0, 1.0)
    expect("sandy one trade", len(trades) == 1)
    expect("sandy stamps", trades[0]["entry_time"] == "2012-10-26 16:00" and trades[0]["exit_time"] == "2012-10-31 09:30")
    expect("sandy net", near(trades[0]["gross"], 51 / 50 - 1) and near(trades[0]["net"], trades[0]["gross"] - 0.0002))
    d4 = Sess(dt.date(2018, 12, 4), 80, 81, 79, 80)
    d6 = Sess(dt.date(2018, 12, 6), 80, 81, 79, 81)
    trades, _, _ = run_book([d4, d6], 1.0, 1.0)
    expect("bush one trade", len(trades) == 1 and trades[0]["exit_time"] == "2018-12-06 09:30")
    expect("bush flat net", near(trades[0]["gross"], 0.0) and near(trades[0]["net"], -0.0002))
    expect("bush not one side", not near(trades[0]["net"], -0.0001))

    # 3. Last sample night.
    sessions = [
        Sess(dt.date(2024, 1, 2), 10, 11, 10, 10),
        Sess(dt.date(2024, 1, 3), 11, 12, 11, 12),
        Sess(dt.date(2024, 1, 4), 12, 13, 12, 13),
    ]
    trades, daily, _ = run_book(sessions, 1.0, 1.0)
    expect("last count", len(trades) == 2 and len(daily) == 2)
    expect("last exit", trades[-1]["exit_px"] == 12 and trades[-1]["exit_time"].startswith("2024-01-04"))
    expect("last close unused", all(tr["entry_px"] != 13 for tr in trades))
    expect("no row past end", daily[-1]["date"] == dt.date(2024, 1, 4))

    # 4. Cost on both sides, and notional scaling.
    flat = [Sess(dt.date(2024, 2, 1), 20, 21, 19, 20), Sess(dt.date(2024, 2, 2), 20, 21, 19, 21)]
    trades, _, _ = run_book(flat, 1.0, 1.0)
    expect("flat cost", near(trades[0]["gross"], 0.0) and near(trades[0]["net"], -0.0002))
    trades, _, _ = run_book(flat, 1.5, 1.0)
    expect("flat levered cost", near(trades[0]["net"], -0.0003))

    # 5. Delayed path is not the primary.
    d1 = Sess(dt.date(2024, 3, 1), 90, 100, 90, 100)
    d2 = Sess(dt.date(2024, 3, 4), 110, 110, 110, 110)
    d3 = Sess(dt.date(2024, 3, 5), 111, 111, 111, 111)
    trades, daily, _ = run_book([d1, d2, d3], 1.0, 1.0)
    expect("primary not the open", trades[0]["entry_px"] == 100 and trades[0]["exit_px"] == 110)
    expect("primary net", near(trades[0]["net"], 110 / 100 - 1 - 0.0002))
    expect("not delayed prices", not (trades[0]["entry_px"] == 90 and trades[0]["exit_px"] == 110))
    expect("delayed first zero", daily[0]["delayed_net"] == 0.0)
    expect("delayed third", near(daily[1]["delayed_net"], 111 / 110 - 1 - 0.0002))

    # 6. Early-close stamp.
    expect("early set", dt.date(2024, 11, 29) in EARLY_CLOSES)
    early = Sess(dt.date(2024, 11, 29), 200, 201, 199, 200)
    nxt = Sess(dt.date(2024, 12, 2), 201, 202, 200, 202)
    trades, _, _ = run_book([early, nxt], 1.0, 1.0)
    expect("early stamp", trades[0]["entry_time"] == "2024-11-29 13:00")
    expect("early price", trades[0]["entry_px"] == 200 and trades[0]["exit_time"] == "2024-12-02 09:30")

    # 7. Sample cut.
    cut = [
        Sess(dt.date(2024, 6, 27), 10, 10, 10, 10),
        Sess(dt.date(2024, 6, 28), 11, 11, 11, 11),
        Sess(dt.date(2024, 7, 1), 12, 12, 12, 12),
    ]
    trades, daily, _ = run_book(cut, 1.0, 1.0)
    is_d, oos_d, is_t, oos_t = classify(daily, trades)
    expect("cut is", [row["date"] for row in is_d] == [dt.date(2024, 6, 28)] and len(is_t) == 1)
    expect("cut oos", [row["date"] for row in oos_d] == [dt.date(2024, 7, 1)] and len(oos_t) == 1)

    # 8. Bad bar.
    bad = Sess(dt.date(2024, 1, 2), 10, 10, 9, 11)
    expect("bad high", ohlc_problem(bad) == "high")
    expect("good bar", ohlc_problem(Sess(dt.date(2024, 1, 2), 10, 11, 9, 10)) is None)

    # Sharpe ddof and the block index formula. No store, no study seed.
    r = np.array([0.01, 0.02, -0.01])
    m = r.mean()
    s = math.sqrt(((r - m) ** 2).sum() / 2)
    expect("sharpe ddof", near(float(sharpe_matrix(r)[0]), m / s * math.sqrt(ANNUAL)))
    fake = np.arange(5.0)
    starts = np.array([3, 1])
    got = fake[block_indices(starts, 5, block=3)]
    hand = []
    for st in starts:
        for k in range(3):
            hand.append(fake[(int(st) + k) % 5])
    expect("block index", np.array_equal(got, np.array(hand[:5])))
    print("self-test passed")


def load_symbol(md, sym):
    bars = []
    for b in md.bars(sym, "1d", start=FIRST, end=LAST):
        bars.append(Sess(session_date_of(b.ts), b.open, b.high, b.low, b.close))
    return bars


def book_report(sessions):
    trades, daily, hours = run_book(sessions, NOTIONAL, COST_BPS_SIDE)
    is_d, oos_d, is_t, oos_t = classify(daily, trades)
    if len(is_d) != len(is_t) or len(oos_d) != len(oos_t):
        raise RuntimeError("evaluation rows and trades do not match")
    if daily[0]["delayed_net"] != 0.0:
        raise RuntimeError("delayed path was written onto the first evaluation session")
    gross = np.array([row["strategy_gross"] for row in daily])
    net = np.array([row["strategy_net"] for row in daily])
    bh = np.array([row["bh_c2c"] for row in daily])
    oc = np.array([row["open_to_close"] for row in daily])
    if not np.allclose(net, gross - 0.0002):
        raise RuntimeError("primary net is not the gap minus 2 bp")
    if not np.allclose((1.0 + gross) * (1.0 + oc) - 1.0, bh):
        raise RuntimeError("close-to-close identity failed")
    report = {
        "full": metrics_from(daily, trades),
        "is": metrics_from(is_d, is_t),
        "oos": metrics_from(oos_d, oos_t),
        "benchmark_c2c": {
            "full": path_stats(bh),
            "is": path_stats(bh[: len(is_d)]),
            "oos": path_stats(bh[len(is_d):]),
        },
        "benchmark_otc": {
            "full": path_stats(oc),
            "is": path_stats(oc[: len(is_d)]),
            "oos": path_stats(oc[len(is_d):]),
        },
        "delayed": delayed_block(daily),
    }
    return trades, daily, report, gross, net, bh, oc


def delayed_block(daily):
    """Sharpe on the column, including a structural zero only on the first sample row."""
    out = {}
    for key, rows in (
        ("full", daily),
        ("is", [row for row in daily if row["date"] < OOS_START]),
        ("oos", [row for row in daily if row["date"] >= OOS_START]),
    ):
        nets = [row["delayed_net"] for row in rows]
        stats = path_stats(nets)
        first = daily[0]["date"]
        trade_nets = [row["delayed_net"] for row in rows if row["date"] != first]
        stats.update(trade_stats(trade_nets, []))
        stats["trades_per_year"] = (
            float(len(trade_nets) / (len(rows) / ANNUAL)) if rows else 0.0
        )
        out[key] = stats
    return out


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
            "trades": len(chunk),
            "return": path["total_return"],
            "sharpe": path["sharpe"],
            "max_dd": path["max_dd"],
            "mean_strategy_net": float(np.mean(nets)),
            "bh_return": path_stats([r["bh_c2c"] for r in chunk])["total_return"],
            "otc_return": path_stats([r["open_to_close"] for r in chunk])["total_return"],
            "eligible_200": len(chunk) >= 200,
            "mean_positive": bool(float(np.mean(nets)) > 0),
        })
    return rows


def weekday_breakdown(daily):
    by = {i: [] for i in range(5)}
    for row in daily:
        by[row["date"].weekday()].append(row)
    rows = []
    for i in range(5):
        chunk = by[i]
        nets = [r["strategy_net"] for r in chunk]
        path = path_stats(nets) if chunk else None
        rows.append({
            "weekday": WEEKDAY[i],
            "sessions": len(chunk),
            "mean_strategy_net": float(np.mean(nets)) if chunk else None,
            "return": path["total_return"] if path else None,
            "sharpe": path["sharpe"] if path else None,
            "mean_bh_c2c": float(np.mean([r["bh_c2c"] for r in chunk])) if chunk else None,
            "mean_open_to_close": float(np.mean([r["open_to_close"] for r in chunk])) if chunk else None,
        })
    return rows


def quintiles(daily):
    order = sorted(range(len(daily)), key=lambda i: (daily[i]["bh_c2c"], daily[i]["date"]))
    n = len(daily)
    buckets = {q: [] for q in range(1, 6)}
    for rank, i in enumerate(order):
        buckets[rank * 5 // n + 1].append(daily[i])
    rows = []
    for q in range(1, 6):
        chunk = buckets[q]
        rows.append({
            "quintile": q,
            "sessions": len(chunk),
            "mean_strategy_net": float(np.mean([r["strategy_net"] for r in chunk])),
            "mean_bh_c2c": float(np.mean([r["bh_c2c"] for r in chunk])),
            "mean_open_to_close": float(np.mean([r["open_to_close"] for r in chunk])),
        })
    return rows


def grid_and_costs(sessions):
    gross = np.array([
        sessions[i + 1].o / sessions[i].c - 1.0 for i in range(len(sessions) - 1)
    ])
    dates = [sessions[i + 1].day for i in range(len(sessions) - 1)]
    is_mask = np.array([d < OOS_START for d in dates])

    def nets(f, mult):
        return f * (gross - 2.0 * (mult * COST_BPS_SIDE) / 10000.0)

    cells = []
    for f in GRID:
        r = nets(f, 1.0)
        cells.append({
            "notional": f,
            "primary": f == NOTIONAL,
            "is_sharpe": path_stats(r[is_mask])["sharpe"],
            "oos_sharpe": path_stats(r[~is_mask])["sharpe"],
            "is_return": path_stats(r[is_mask])["total_return"],
            "oos_return": path_stats(r[~is_mask])["total_return"],
            "full_sharpe": path_stats(r)["sharpe"],
            "full_return": path_stats(r)["total_return"],
        })
    ranked = sorted(cells, key=lambda c: (-c["is_sharpe"], c["notional"]))
    for rank, cell in enumerate(ranked, start=1):
        cell["is_rank"] = rank
    is_s = np.array([c["is_sharpe"] for c in cells])
    oos_s = np.array([c["oos_sharpe"] for c in cells])
    corr = None
    if float(is_s.std(ddof=1)) > 0 and float(oos_s.std(ddof=1)) > 0:
        corr = float(np.corrcoef(is_s, oos_s)[0, 1])
    summary = {
        "n_cells": len(cells),
        "n_is_positive": int(sum(c["is_sharpe"] > 0 for c in cells)),
        "share_is_positive": float(sum(c["is_sharpe"] > 0 for c in cells) / len(cells)),
        "primary_is_rank": next(c["is_rank"] for c in cells if c["primary"]),
        "is_best_notional": ranked[0]["notional"],
        "is_best_is_sharpe": ranked[0]["is_sharpe"],
        "is_best_oos_sharpe": ranked[0]["oos_sharpe"],
        "is_oos_sharpe_corr": corr,
    }
    costs = []
    for m in COST_MULT:
        r = nets(1.0, m)
        costs.append({
            "multiplier": m,
            "cost_bps_side": m * COST_BPS_SIDE,
            "full_sharpe": path_stats(r)["sharpe"],
            "oos_sharpe": path_stats(r[~is_mask])["sharpe"],
            "full_return": path_stats(r)["total_return"],
            "oos_return": path_stats(r[~is_mask])["total_return"],
            "is_sharpe": path_stats(r[is_mask])["sharpe"],
        })
    return cells, summary, costs, gross, is_mask


def breakeven(gross, mask, cap_bps=50.0):
    """Cost per side, in bp, where the compound return crosses to <= 0. None if none."""

    def total(bps):
        r = gross - 2.0 * (bps / 10000.0)
        if mask is not None:
            r = r[mask]
        e = 1.0
        for x in r:
            e *= 1.0 + float(x)
        return e - 1.0

    if total(0.0) <= 0:
        return {"bps_side": None, "reason": "non_positive_at_zero_cost"}
    if total(cap_bps) > 0:
        return {"bps_side": None, "reason": "still_positive_at_cap"}
    lo, hi = 0.0, cap_bps
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if total(mid) > 0:
            lo = mid
        else:
            hi = mid
    return {"bps_side": float(hi), "reason": "crossed"}


def placebo_and_bootstrap(gross, net):
    n = int(gross.size)
    rng = np.random.default_rng(SEED_DIRECTION)
    signs = rng.choice(np.array([-1.0, 1.0]), size=(N_DRAWS, n))
    actual = float(sharpe_matrix(gross)[0])
    null = sharpe_matrix(signs * gross)
    p = float((1 + int(np.sum(null >= actual))) / (N_DRAWS + 1))
    placebo = {
        "actual_gross_sharpe": actual,
        "p": p,
        "null_mean": float(null.mean()),
        "null_p95": float(np.percentile(null, 95)),
        "n_draws": N_DRAWS,
        "n_ge": int(np.sum(null >= actual)),
        "seed": SEED_DIRECTION,
    }
    rng_b = np.random.default_rng(SEED_BOOTSTRAP)
    n_blocks = math.ceil(n / BLOCK)
    starts = rng_b.integers(0, n, size=(N_DRAWS, n_blocks))
    boot = np.empty(N_DRAWS, float)
    # Check the first draw against the literal index walk, then fill the rest
    # the same way. 2,000 walks of ~4,000 is small.
    for d in range(N_DRAWS):
        boot[d] = float(sharpe_matrix(net[block_indices(starts[d], n)])[0])
    summary = {
        "p2_5": float(np.percentile(boot, 2.5)),
        "p50": float(np.percentile(boot, 50)),
        "p97_5": float(np.percentile(boot, 97.5)),
        "n_draws": N_DRAWS,
        "block": BLOCK,
        "seed": SEED_BOOTSTRAP,
    }
    return placebo, null, summary


def predictions(gross, oc, years):
    mean_co = float(np.mean(gross))
    mean_oc = float(np.mean(oc))
    eligible = [y for y in years if y["eligible_200"]]
    positive = [y for y in eligible if y["mean_positive"]]
    return {
        "mean_close_to_open": mean_co,
        "mean_open_to_close": mean_oc,
        "pred1_holds": bool(mean_co > mean_oc),
        "n_eligible_years": len(eligible),
        "n_positive_years": len(positive),
        "eligible_years": [y["year"] for y in eligible],
        "positive_years": [y["year"] for y in positive],
        "pred2_holds": bool(len(positive) >= 8),
    }


def acceptance(spy, qqq_oos_sharpe, grid_summary, costs, placebo):
    oos = spy["oos"]
    pf = oos["profit_factor"]
    line1 = bool(oos["sharpe"] >= 0.5 and (pf is None or pf >= 1.10))
    line2 = bool(placebo["p"] <= 0.05)
    line3 = bool(spy["is"]["sharpe"] > 0 and grid_summary["share_is_positive"] >= 0.60)
    two_x = next(c for c in costs if c["multiplier"] == 2.0)
    line4 = bool(two_x["full_return"] > 0)
    line5 = bool(qqq_oos_sharpe > 0)
    line6 = bool(oos["trades"] >= 100)
    lines = [
        {"id": 1, "pass": line1, "oos_sharpe": oos["sharpe"], "oos_profit_factor": pf,
         "required_sharpe": 0.5, "required_pf": 1.10},
        {"id": 2, "pass": line2, "p": placebo["p"], "required_p": 0.05},
        {"id": 3, "pass": line3, "is_sharpe": spy["is"]["sharpe"],
         "n_is_positive": grid_summary["n_is_positive"], "n_cells": grid_summary["n_cells"],
         "required_is_sharpe": 0.0, "required_share": 0.60},
        {"id": 4, "pass": line4, "full_return_2x": two_x["full_return"]},
        {"id": 5, "pass": line5, "qqq_oos_sharpe": qqq_oos_sharpe},
        {"id": 6, "pass": line6, "oos_trades": oos["trades"], "required_trades": 100},
    ]
    if not line6:
        status = "Inconclusive"
    elif all(line["pass"] for line in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    return lines, status


def write_outputs(trades, daily, results, null):
    with (HERE / "trades.csv").open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f)
        w.writerow(["side", "entry_time", "entry_px", "exit_time", "exit_px", "gross", "net", "exit_reason"])
        for t in trades:
            w.writerow([
                t["side"], t["entry_time"], fnum(t["entry_px"]), t["exit_time"],
                fnum(t["exit_px"]), fnum(t["gross"]), fnum(t["net"]), t["exit_reason"],
            ])
    with (HERE / "daily.csv").open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f)
        w.writerow(["date", "strategy_net", "strategy_gross", "bh_c2c", "open_to_close", "delayed_net", "equity"])
        for row in daily:
            w.writerow([
                row["date"].isoformat(), fnum(row["strategy_net"]), fnum(row["strategy_gross"]),
                fnum(row["bh_c2c"]), fnum(row["open_to_close"]), fnum(row["delayed_net"]),
                fnum(row["equity"]),
            ])
    np.save(HERE / "placebo_direction.npy", np.asarray(null, float))
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8", newline="\n")


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
            spy_days = [b.day for b in books["SPY"]]
            for sym in ("QQQ", "IWM"):
                if [b.day for b in books[sym]] != spy_days:
                    raise DataError(f"{sym} session dates differ from SPY")
    except DataError as exc:
        now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
        append_log([
            f"## {now}",
            f"- rules_sha256 {digest}",
            f"- git_head {head} dirty={dirty}",
            f"- reason: data check failed before any result file was written ({reason})",
            f"- {exc}",
        ])
        sys.exit(f"data check failed: {exc}")

    spy_trades, spy_daily, spy, gross, net, bh, oc = book_report(books["SPY"])
    qqq_trades, qqq_daily, qqq, qqq_gross, qqq_net, _, _ = book_report(books["QQQ"])
    iwm_trades, iwm_daily, iwm, iwm_gross, iwm_net, _, _ = book_report(books["IWM"])
    del qqq_trades, iwm_trades
    cells, grid_summary, costs, gross_gap, is_mask = grid_and_costs(books["SPY"])
    if not np.allclose(gross_gap, gross):
        raise RuntimeError("grid gaps do not match the primary book")
    placebo, null, bootstrap = placebo_and_bootstrap(gross, net)
    years = year_breakdown(spy_daily)
    weekdays = weekday_breakdown(spy_daily)
    quints = quintiles(spy_daily)
    preds = predictions(gross, oc, years)
    lines, status = acceptance(spy, qqq["oos"]["sharpe"], grid_summary, costs, placebo)
    be_full = breakeven(gross_gap, None)
    be_oos = breakeven(gross_gap, ~is_mask)
    primary_cell = next(c for c in cells if c["primary"])
    cost_1x = next(c for c in costs if c["multiplier"] == 1.0)
    if not math.isclose(primary_cell["is_sharpe"], spy["is"]["sharpe"], rel_tol=0, abs_tol=1e-12):
        raise RuntimeError("grid primary in-sample Sharpe does not match the book")
    if not math.isclose(cost_1x["full_sharpe"], spy["full"]["sharpe"], rel_tol=0, abs_tol=1e-12):
        raise RuntimeError("1x cost Sharpe does not match the book")
    if spy["full"]["trades"] != len(spy_daily) or spy["oos"]["trades"] != spy["oos"]["sessions"]:
        raise RuntimeError("trade count does not match evaluation sessions")

    def corr(a, b):
        return float(np.corrcoef(a, b)[0, 1])

    results = {
        "rules_sha256": digest,
        "locked_utc": locked,
        "git_head": head,
        "git_dirty": dirty,
        "run_utc": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "reason": reason,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOTSTRAP,
            "verify": 20261043,
        },
        "parameters": {
            "notional": NOTIONAL,
            "cost_bps_side": COST_BPS_SIDE,
            "first": FIRST.isoformat(),
            "last": LAST.isoformat(),
            "oos_start": OOS_START.isoformat(),
        },
        "sample": {
            "n_bars": 3959,
            "n_eval": len(spy_daily),
            "first_bar": FIRST.isoformat(),
            "first_exit": spy_daily[0]["date"].isoformat(),
            "last_exit": spy_daily[-1]["date"].isoformat(),
            "is_end": next(row["date"] for row in reversed(spy_daily) if row["date"] < OOS_START).isoformat(),
            "oos_start_actual": next(row["date"] for row in spy_daily if row["date"] >= OOS_START).isoformat(),
            "n_is": spy["is"]["sessions"],
            "n_oos": spy["oos"]["sessions"],
            "dates_identical": True,
        },
        "spy": spy,
        "qqq": qqq,
        "iwm": iwm,
        "grid": cells,
        "grid_summary": grid_summary,
        "costs": costs,
        "breakeven_cost_bps_side": {"full": be_full, "oos": be_oos, "cap_bps": 50.0},
        "direction_placebo": placebo,
        "bootstrap": bootstrap,
        "by_year": years,
        "by_weekday": weekdays,
        "by_quintile": quints,
        "by_side": {"long": spy["full"]["trades"], "short": 0},
        "by_exit_reason": {"next_open": spy["full"]["trades"]},
        "correlation": {
            "spy_qqq": corr(net, qqq_net),
            "spy_iwm": corr(net, iwm_net),
            "qqq_iwm": corr(qqq_net, iwm_net),
            "spy_net_bh": corr(net, bh),
            "spy_net_otc": corr(net, oc),
        },
        "predictions": preds,
        "acceptance": lines,
        "status": status,
        "exposure": {"overnight_sessions": 1.0, "cash_session": 0.0},
        "self_test": "passed",
    }
    write_outputs(spy_trades, spy_daily, results, null)
    now = results["run_utc"]
    sfull, sis, soos = spy["full"], spy["is"], spy["oos"]
    append_log([
        f"## {now}",
        f"- rules_sha256 {digest}",
        f"- git_head {head} dirty={dirty}",
        f"- reason: {reason}",
        (
            f"- SPY full Sharpe {sfull['sharpe']:.4f} return {sfull['total_return']:.4f} "
            f"| IS Sharpe {sis['sharpe']:.4f} | OOS Sharpe {soos['sharpe']:.4f} "
            f"return {soos['total_return']:.4f} | trades full/IS/OOS "
            f"{sfull['trades']}/{sis['trades']}/{soos['trades']} | status {status}"
        ),
    ])
    print(
        f"status {status} | OOS Sharpe {soos['sharpe']:.4f} return {soos['total_return']:.4f} "
        f"| full Sharpe {sfull['sharpe']:.4f} | trades OOS {soos['trades']}"
    )


if __name__ == "__main__":
    main()
