# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered VIXY variance-premium backtest.

Refuses to run if RULES.md does not match RULES.lock. The self-test runs
before the store is opened.
"""

import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, nyse_sessions, session_date_of  # noqa: E402

HERE = Path(__file__).resolve().parent
LAST = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
SKIP = {date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)}
ANNUAL = 252
WEIGHT = -1.0
COST_BPS = 5.0
BORROW = 0.01
GRID = (-0.5, -0.75, -1.0, -1.25, -1.5)
COST_MULTIPLES = (0.0, 0.5, 1.0, 2.0, 3.0)
SEED_DIRECTION = 20261071
SEED_BOOTSTRAP = 20261072
SEED_TIMING = 20261073
SEED_VERIFY = 20261074
N_DRAWS = 2000
BLOCK = 20

VIXY_SPLITS = (
    (date(2017, 7, 17), 0.25),
    (date(2021, 5, 26), 0.25),
    (date(2023, 6, 23), 0.2),
    (date(2024, 11, 7), 0.25),
)
UVXY_SPLITS = (
    (date(2017, 1, 12), 0.2),
    (date(2017, 7, 17), 0.25),
    (date(2018, 9, 18), 0.2),
    (date(2021, 5, 26), 0.1),
    (date(2023, 6, 23), 0.1),
    (date(2024, 4, 11), 0.2),
    (date(2025, 11, 20), 0.2),
)


class Trip:
    __slots__ = (
        "side", "entry_date", "entry_px", "entry_equity", "gross", "net",
        "exit_date", "exit_px", "reason", "hold", "at_close", "daily", "_g",
    )

    def __init__(self, entry_date, entry_px, entry_equity):
        self.side = "short"
        self.entry_date = entry_date
        self.entry_px = entry_px
        self.entry_equity = entry_equity
        self.gross = 0.0
        self.net = 0.0
        self.exit_date = None
        self.exit_px = None
        self.reason = None
        self.hold = None
        self.at_close = False
        self.daily = []
        self._g = 0.0

    def add_gross(self, amount):
        self.gross += amount
        self.net += amount
        self._g += amount

    def add_cost(self, amount):
        self.net -= amount

    def flush(self, day):
        if self._g != 0.0:
            self.daily.append((day, self._g))
        self._g = 0.0


def rules_hash_check():
    lock_lines = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()
    want = lock_lines[0].split()[1]
    have = hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    if want != have:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    locked = lock_lines[1].split(" ", 1)[1]
    return have, locked


def git_state():
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    return head, "yes" if dirty else "no"


def schedule(book, bars, mode):
    offset = {"next_open": 1, "delay1": 2, "same_close": 0}[mode]
    pos = {d: i for i, d in enumerate(book)}
    last = {}
    for d in book:
        last[(d.year, d.month)] = d
    fills = {}
    for sd in (last[k] for k in sorted(last)):
        target = pos[sd] + offset
        if target < 0:
            continue
        fi = target
        while fi < len(book) and book[fi] not in bars:
            fi += 1
        if fi >= len(book):
            continue
        fd = book[fi]
        if fd in fills:
            raise RuntimeError(f"two signals fill on {fd}")
        fills[fd] = sd
    return fills


def _day_row(day, ret, equity, shares):
    return {"date": day, "ret": ret, "equity": equity, "shares": shares,
            "exposed": shares != 0.0}


def simulate(book, bars, weight, cost_bps, borrow_rate, mode="next_open"):
    """Share-path accounting in RULES.md. `bars` maps a session to (open, close)."""
    if weight >= 0.0:
        raise RuntimeError("weight must stay short")
    fills = schedule(book, bars, mode)
    if not fills:
        raise RuntimeError("no fills")
    first_fill = min(fills)
    pos = {d: i for i, d in enumerate(book)}
    rate = cost_bps / 10000.0
    cash = 1.0
    shares = 0.0
    last_close = None
    held = False
    active = None
    closed = []
    frozen = False
    ruin_date = None
    days = []

    def close_active(day, px, reason, inclusive):
        nonlocal active
        active.flush(day)
        active.exit_date = day
        active.exit_px = px
        active.reason = reason
        active.at_close = inclusive
        if inclusive:
            active.hold = pos[day] - pos[active.entry_date] + 1
        else:
            active.hold = pos[day] - pos[active.entry_date]
        closed.append(active)
        active = None

    def open_active(day, px, entry_equity):
        nonlocal active
        active = Trip(day, px, entry_equity)

    for day in book:
        if day < first_fill:
            continue
        if frozen:
            days.append(_day_row(day, 0.0, cash, 0.0))
            continue
        bar = bars.get(day)
        if bar is None:
            equity = cash if last_close is None else cash + shares * last_close
            days.append(_day_row(day, 0.0, equity, shares))
            continue
        o, c = bar
        e0 = cash if not held else cash + shares * last_close

        if mode == "same_close":
            if held and shares != 0.0:
                borrow = abs(shares) * last_close * borrow_rate / ANNUAL
                cash -= borrow
                active.add_cost(borrow)
                active.add_gross(shares * (o - last_close))
                active.add_gross(shares * (c - o))
            e_mark = cash + shares * c
            if day in fills and e_mark > 0.0:
                if active is not None:
                    close_active(day, c, "same_close", True)
                s_new = weight * e_mark / c
                if s_new >= 0.0:
                    raise RuntimeError("refusing to open a long")
                delta = s_new - shares
                cost = abs(delta) * c * rate
                cash = cash - delta * c - cost
                shares = s_new
                held = True
                open_active(day, c, e_mark)
                active.add_cost(cost)
            elif day in fills and shares != 0.0:
                delta = -shares
                cost = abs(shares) * c * rate
                cash = cash - delta * c - cost
                active.add_cost(cost)
                shares = 0.0
                held = False
                close_active(day, c, "ruin", True)
                frozen = True
                ruin_date = day
            e1 = cash if shares == 0.0 else cash + shares * c
            if active is not None:
                active.flush(day)
            last_close = c
            if e1 <= 0.0 and not frozen:
                if active is not None:
                    close_active(day, c, "ruin", True)
                cash = e1
                shares = 0.0
                held = False
                frozen = True
                ruin_date = day
            ret = e1 / e0 - 1.0 if e0 > 0.0 else 0.0
            days.append(_day_row(day, ret, e1, 0.0 if frozen else shares))
            continue

        if held and shares != 0.0:
            borrow = abs(shares) * last_close * borrow_rate / ANNUAL
            cash -= borrow
            active.add_cost(borrow)
            active.add_gross(shares * (o - last_close))
        e_open = cash + shares * o
        if day in fills:
            if e_open <= 0.0 and shares != 0.0:
                delta = -shares
                cost = abs(shares) * o * rate
                cash = cash - delta * o - cost
                active.add_cost(cost)
                shares = 0.0
                held = False
                close_active(day, o, "ruin", False)
                frozen = True
                ruin_date = day
                e1 = cash
                days.append(_day_row(day, e1 / e0 - 1.0, e1, 0.0))
                continue
            if e_open > 0.0:
                if active is not None:
                    close_active(day, o, "next_open", False)
                s_new = weight * e_open / o
                if s_new >= 0.0:
                    raise RuntimeError("refusing to open a long")
                delta = s_new - shares
                cost = abs(delta) * o * rate
                cash = cash - delta * o - cost
                shares = s_new
                held = True
                open_active(day, o, e_open)
                active.add_cost(cost)
        if shares != 0.0:
            active.add_gross(shares * (c - o))
        e1 = cash + shares * c
        if active is not None:
            active.flush(day)
        last_close = c
        if e1 <= 0.0:
            if active is not None:
                close_active(day, c, "ruin", True)
            cash = e1
            shares = 0.0
            held = False
            frozen = True
            ruin_date = day
        ret = e1 / e0 - 1.0 if e0 > 0.0 else 0.0
        days.append(_day_row(day, ret, e1, 0.0 if frozen else shares))

    if active is not None:
        close_active(book[-1] if book[-1] >= first_fill else days[-1]["date"],
                     last_close, "end_of_sample", True)
    equity_end = days[-1]["equity"] if days else 1.0
    net_sum = sum(t.net for t in closed)
    if abs(net_sum - (equity_end - 1.0)) > 1e-6:
        raise AssertionError(f"trip net {net_sum} != equity change {equity_end - 1.0}")
    return {
        "days": days,
        "trips": closed,
        "first_fill": first_fill,
        "fills": fills,
        "ruined": frozen,
        "ruin_date": ruin_date,
        "terminal_equity": equity_end,
    }


def buy_and_hold(book, bars, first_fill):
    o0 = bars[first_fill][0]
    shares = 1.0 / o0
    equity = 1.0
    prev = None
    out = []
    for day in book:
        if day < first_fill:
            continue
        bar = bars.get(day)
        e0 = equity
        if bar is None:
            out.append(_day_row(day, 0.0, equity, shares))
            continue
        equity = shares * bar[1]
        ret = equity / e0 - 1.0
        prev = bar[1]
        out.append(_day_row(day, ret, equity, shares))
    del prev
    return out


def product_c2c(book, bars, first_fill):
    """Close-to-close of the product on evaluation dates. A missing bar is 0."""
    prev = None
    out = {}
    for day in book:
        bar = bars.get(day)
        if bar is None:
            if day >= first_fill:
                out[day] = 0.0
            continue
        if prev is not None and day >= first_fill:
            out[day] = bar[1] / prev - 1.0
        elif day >= first_fill:
            out[day] = 0.0
        prev = bar[1]
    return out


def _sharpe(r):
    r = np.asarray(r, float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0 or math.isnan(sd):
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def _stats(days, trips):
    r = np.array([d["ret"] for d in days], float)
    n = int(len(r))
    out = {
        "sessions": n, "total_return": None, "cagr": None, "vol": None,
        "sharpe": None, "max_dd": None, "tstat": None, "trades": len(trips),
        "profit_factor": None, "profit_factor_infinite": False, "win_rate": None,
        "avg_net_trade_bp": None, "avg_winner_bp": None, "avg_loser_bp": None,
        "exposure": None, "hold_median": None, "hold_mean": None,
        "trades_per_year": None, "best_trade": None, "worst_trade": None,
        "terminal_equity": None,
    }
    if n == 0:
        return out
    eq = np.cumprod(1.0 + r)
    out["total_return"] = float(eq[-1] - 1.0)
    out["terminal_equity"] = float(eq[-1])
    out["max_dd"] = float((eq / np.maximum.accumulate(eq) - 1.0).min())
    sd = float(r.std(ddof=1)) if n > 1 else 0.0
    out["vol"] = float(sd * math.sqrt(ANNUAL)) if n > 1 and sd > 0.0 else (0.0 if n > 1 else None)
    out["sharpe"] = _sharpe(r)
    out["tstat"] = float(r.mean() / sd * math.sqrt(n)) if sd > 0.0 else None
    out["cagr"] = float(eq[-1] ** (ANNUAL / n) - 1.0) if eq[-1] > 0.0 else None
    out["exposure"] = float(sum(1 for d in days if d["exposed"]) / n)
    if trips:
        dn = np.array([t.net for t in trips], float)
        ee = np.array([t.entry_equity for t in trips], float)
        bp = dn / ee * 1e4
        wins = dn > 0.0
        losses = dn < 0.0
        out["win_rate"] = float(np.mean(wins))
        out["avg_net_trade_bp"] = float(bp.mean())
        out["best_trade"] = float((dn / ee).max())
        out["worst_trade"] = float((dn / ee).min())
        if wins.any() and losses.any():
            out["profit_factor"] = float(dn[wins].sum() / abs(dn[losses].sum()))
            out["avg_winner_bp"] = float(bp[wins].mean())
            out["avg_loser_bp"] = float(bp[losses].mean())
        elif wins.any():
            out["profit_factor_infinite"] = True
            out["avg_winner_bp"] = float(bp[wins].mean())
        elif losses.any():
            out["profit_factor"] = 0.0
            out["avg_loser_bp"] = float(bp[losses].mean())
        holds = [t.hold for t in trips]
        out["hold_median"] = float(np.median(holds))
        out["hold_mean"] = float(np.mean(holds))
        out["trades_per_year"] = float(len(trips) / (n / ANNUAL))
    return out


def _window(days, trips, pred):
    return [d for d in days if pred(d["date"])], [t for t in trips if pred(t.entry_date)]


def _compound(rs):
    eq = 1.0
    for x in rs:
        eq *= 1.0 + x
    return eq - 1.0


def _series_stats(rs):
    return _stats([{"ret": x, "exposed": False} for x in rs], [])


def _quintiles(rows, sort_key):
    rows = sorted(rows, key=lambda z: (z[sort_key], z["date"]))
    n = len(rows)
    sizes = [n // 5] * 5
    for i in range(n % 5):
        sizes[i] += 1
    out = []
    i = 0
    for q, sz in enumerate(sizes, start=1):
        chunk = rows[i:i + sz]
        i += sz
        out.append({
            "quintile": q,
            "n": len(chunk),
            "sort_mean": float(np.mean([c[sort_key] for c in chunk])) if chunk else None,
            "strategy_mean": float(np.mean([c["strategy"] for c in chunk])) if chunk else None,
            "benchmark_mean": float(np.mean([c["benchmark"] for c in chunk])) if chunk else None,
        })
    return out


def _spearman(xs, ys):
    if any(v is None for v in list(xs) + list(ys)):
        return None

    def ranks(a):
        order = sorted(range(len(a)), key=lambda i: a[i])
        r = [0.0] * len(a)
        i = 0
        while i < len(a):
            j = i
            while j + 1 < len(a) and a[order[j + 1]] == a[order[i]]:
                j += 1
            avg = 0.5 * (i + j) + 1.0
            for k in range(i, j + 1):
                r[order[k]] = avg
            i = j + 1
        return r
    if len(xs) < 2:
        return None
    x = np.array(ranks(xs))
    y = np.array(ranks(ys))
    if x.std(ddof=1) == 0.0 or y.std(ddof=1) == 0.0:
        return None
    return float(np.corrcoef(x, y)[0, 1])


def _corr(a, b):
    x = np.asarray(a, float)
    y = np.asarray(b, float)
    if len(x) < 2 or x.std(ddof=1) == 0.0 or y.std(ddof=1) == 0.0:
        return None
    return float(np.corrcoef(x, y)[0, 1])


def _pf_pass(st, floor):
    if st["profit_factor_infinite"]:
        return True
    return st["profit_factor"] is not None and st["profit_factor"] >= floor


def _stamp(day, at_close):
    if not at_close:
        hh = "09:30"
    else:
        hh = "13:00" if day in EARLY_CLOSES else "16:00"
    return f"{day.isoformat()} {hh}"


def _check(name, cond):
    if not cond:
        raise AssertionError(name)


def _bars_of(rows):
    out = {}
    book = []
    for d, o, c in rows:
        dd = date.fromisoformat(d)
        book.append(dd)
        if o is not None:
            out[dd] = (o, c)
    return book, out


def self_test():
    # Down month, one holding, end of sample. Open 100, close 90.
    book, bars = _bars_of([
        ("2020-01-31", 100.0, 100.0),
        ("2020-02-03", 100.0, 90.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01)
    _check("down equity", abs(sim["terminal_equity"] - 1.0995) < 1e-12)
    _check("down gross", abs(sim["trips"][0].gross - 0.10) < 1e-12)
    _check("down net", abs(sim["trips"][0].net - 0.0995) < 1e-12)
    _check("down reason", sim["trips"][0].reason == "end_of_sample")
    _check("down exit px", sim["trips"][0].exit_px == 90.0)
    _check("down side", sim["trips"][0].side == "short")
    _check("down shares", abs(sim["days"][0]["shares"] - (-0.01)) < 1e-12)

    # Up month. Short loses.
    book, bars = _bars_of([
        ("2020-01-31", 100.0, 100.0),
        ("2020-02-03", 100.0, 110.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01)
    _check("up equity", abs(sim["terminal_equity"] - 0.8995) < 1e-12)
    _check("up gross", abs(sim["trips"][0].gross - (-0.10)) < 1e-12)
    _check("up net", abs(sim["trips"][0].net - (-0.1005)) < 1e-12)

    # Half weight is half the shares and half the price P&L, not a long.
    sim = simulate(book, bars, -0.5, 5.0, 0.01)
    _check("half shares", abs(sim["days"][-1]["shares"] - (-0.005)) < 1e-12)
    _check("half still short", sim["days"][-1]["shares"] < 0.0)

    # Borrow on a flat day, missing bar, smooth drift, resize, up month, end.
    book, bars = _bars_of([
        ("2020-01-31", 100.0, 100.0),
        ("2020-02-03", 100.0, 90.0),
        ("2020-02-04", None, None),
        ("2020-02-05", 90.0, 90.0),
        ("2020-02-06", 90.0, 88.0),
        ("2020-02-28", 88.0, 80.0),
        ("2020-03-02", 80.0, 100.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01)
    days = sim["days"]
    _check("path length", len(days) == 6)
    _check("feb3", abs(days[0]["equity"] - 1.0995) < 1e-12)
    _check("missing ret", days[1]["ret"] == 0.0)
    _check("missing shares", abs(days[1]["shares"] - (-0.01)) < 1e-12)
    b1 = 0.01 * 90.0 * 0.01 / 252.0
    e5 = 1.0995 - b1
    _check("borrow flat", abs(days[2]["equity"] - e5) < 1e-12)
    e6 = e5 - b1 + 0.02
    _check("smooth drift", abs(days[3]["equity"] - e6) < 1e-12)
    _check("smooth not a split jump", 0.0 < days[3]["ret"] < 0.05)
    b28 = 0.01 * 88.0 * 0.01 / 252.0
    e28 = e6 - b28 + 0.08
    _check("into month end", abs(days[4]["equity"] - e28) < 1e-12)
    b32 = 0.01 * 80.0 * 0.01 / 252.0
    e_open = e28 - b32
    s_new = -1.0 * e_open / 80.0
    cost = abs(s_new - (-0.01)) * 80.0 * 0.0005
    e_final = e_open - cost + s_new * 20.0
    _check("resize equity", abs(days[5]["equity"] - e_final) < 1e-9)
    _check("resize not the whole short", cost < 0.0004)
    _check("resize grew the short", s_new < -0.01)
    _check("up month loses", days[5]["ret"] < 0.0)
    t1, t2 = sim["trips"]
    _check("two holdings", len(sim["trips"]) == 2)
    _check("t1 exit", t1.reason == "next_open" and t1.exit_px == 80.0 and t1.entry_px == 100.0)
    _check("t1 gross", abs(t1.gross - 0.20) < 1e-12)
    _check("t1 hold", t1.hold == 5)
    _check("t2 end", t2.reason == "end_of_sample" and t2.exit_px == 100.0 and t2.entry_px == 80.0)
    _check("t2 hold", t2.hold == 1)
    _check("t2 no borrow", abs(t2.gross - s_new * 20.0) < 1e-9)
    _check("t2 cost", abs(t2.net - (t2.gross - cost)) < 1e-9)

    # Same-close upper bound does not earn the signal day's open-to-close.
    book, bars = _bars_of([
        ("2020-01-30", 100.0, 100.0),
        ("2020-01-31", 100.0, 110.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01, mode="same_close")
    _check("same-close equity", abs(sim["terminal_equity"] - 0.9995) < 1e-12)
    _check("same-close px", sim["trips"][0].entry_px == 110.0)
    _check("same-close gross", abs(sim["trips"][0].gross) < 1e-12)
    _check("same-close net", abs(sim["trips"][0].net - (-0.0005)) < 1e-12)

    # Delay fills two sessions after the signal, not at the next open.
    book, bars = _bars_of([
        ("2020-01-31", 100.0, 100.0),
        ("2020-02-03", 100.0, 100.0),
        ("2020-02-04", 100.0, 90.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01, mode="delay1")
    _check("delay entry", sim["trips"][0].entry_date == date(2020, 2, 4))
    _check("delay one day", len(sim["days"]) == 1)
    _check("delay down", abs(sim["terminal_equity"] - 1.0995) < 1e-12)

    # Gap through zero freezes the short and does not open a long.
    book, bars = _bars_of([
        ("2020-01-31", 100.0, 100.0),
        ("2020-02-03", 100.0, 100.0),
        ("2020-02-04", 300.0, 300.0),
        ("2020-02-05", 300.0, 300.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01)
    _check("gap ruin", sim["ruined"] and sim["terminal_equity"] < 0.0)
    _check("gap ruin shares", sim["days"][-1]["shares"] == 0.0 and sim["days"][-1]["ret"] == 0.0)
    _check("gap ruin reason", sim["trips"][-1].reason == "ruin")
    _check("no long after gap", all(d["shares"] <= 0.0 for d in sim["days"]))

    # Fill-day cover when the open mark is already through zero.
    book, bars = _bars_of([
        ("2020-01-31", 100.0, 100.0),
        ("2020-02-03", 100.0, 100.0),
        ("2020-02-28", 100.0, 100.0),
        ("2020-03-02", 300.0, 300.0),
    ])
    sim = simulate(book, bars, -1.0, 5.0, 0.01)
    _check("open ruin", sim["ruined"] and sim["trips"][-1].reason == "ruin")
    _check("open ruin px", sim["trips"][-1].exit_px == 300.0)
    _check("open ruin flat", sim["days"][-1]["shares"] == 0.0)
    _check("no long after cover", all(t.side == "short" for t in sim["trips"]))


def _clean(obj):
    if isinstance(obj, dict):
        return {k: _clean(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_clean(v) for v in obj]
    if isinstance(obj, float):
        if math.isnan(obj) or math.isinf(obj):
            return None
        return obj
    if isinstance(obj, (np.floating,)):
        x = float(obj)
        return None if math.isnan(x) or math.isinf(x) else x
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, date):
        return obj.isoformat()
    return obj


def _load_symbol(md, sym):
    adj = md.bars(sym, "1d", adjust=True)
    raw = md.bars(sym, "1d", adjust=False)
    actions = md.corporate_actions(sym)
    return adj, raw, actions


def _book_from(adj):
    days = []
    bars = {}
    extra = []
    for b in adj:
        d = session_date_of(b.ts)
        if d in SKIP:
            raise SystemExit(f"skip date {d} has a bar")
        if d > LAST:
            extra.append(d)
            continue
        if d in bars:
            raise SystemExit(f"duplicate session {d}")
        if min(b.open, b.high, b.low, b.close) <= 0.0:
            raise SystemExit(f"non-positive price on {d}")
        days.append(d)
        bars[d] = (b.open, b.close)
    return days, bars, extra


def _jump_check(adj, raw, splits):
    adj_by = {session_date_of(b.ts): b.close for b in adj}
    raw_by = {session_date_of(b.ts): b.close for b in raw}
    days = sorted(adj_by)
    problems = []
    seen = []
    for ex, ratio in splits:
        if ex not in adj_by:
            problems.append(f"missing ex-date {ex}")
            continue
        prev = max(d for d in days if d < ex)
        raw_ratio = raw_by[ex] / raw_by[prev]
        adj_ratio = adj_by[ex] / adj_by[prev]
        seen.append((ex, ratio))
        if not (raw_ratio > 3.0 and 0.5 < adj_ratio < 1.5):
            problems.append(f"{ex} raw {raw_ratio} adj {adj_ratio}")
    if seen != list(splits):
        problems.append(f"split list {seen}")
    return problems


def _validate(vixy_adj, vixy_raw, vixy_actions, uvxy_adj, uvxy_raw, uvxy_actions, spy_adj):
    problems = []
    if [a["type"] for a in vixy_actions] != ["split"] * 4:
        problems.append("VIXY actions")
    if [a["type"] for a in uvxy_actions] != ["split"] * 7:
        problems.append("UVXY actions")
    problems.extend("VIXY " + p for p in _jump_check(vixy_adj, vixy_raw, VIXY_SPLITS))
    problems.extend("UVXY " + p for p in _jump_check(uvxy_adj, uvxy_raw, UVXY_SPLITS))
    vixy_days, vixy_bars, vixy_extra = _book_from(vixy_adj)
    uvxy_days, uvxy_bars, uvxy_extra = _book_from(uvxy_adj)
    spy_days, spy_bars, spy_extra = _book_from(spy_adj)
    if vixy_days[0] != date(2011, 1, 4) or len(vixy_days) != 3959:
        problems.append(f"VIXY book {vixy_days[:1]} n={len(vixy_days)}")
    if date(2026, 10, 2) not in vixy_extra:
        problems.append("VIXY 2026-10-02 missing")
    if uvxy_days[0] != date(2011, 10, 4) or len(uvxy_days) != 3770:
        problems.append(f"UVXY book {uvxy_days[:1]} n={len(uvxy_days)}")
    if date(2026, 10, 2) not in uvxy_extra:
        problems.append("UVXY 2026-10-02 missing")
    if spy_days != vixy_days:
        problems.append("SPY sessions differ from VIXY")
    if spy_extra:
        problems.append(f"SPY bars after cutoff {spy_extra[:3]}")
    for label, days in ("VIXY", vixy_days), ("UVXY", uvxy_days):
        cal = [d for d in nyse_sessions(days[0], LAST) if d not in SKIP]
        if cal != days:
            problems.append(f"{label} calendar mismatch {len(cal)} vs {len(days)}")
    vixy_fills = schedule(vixy_days, vixy_bars, "next_open")
    uvxy_fills = schedule(uvxy_days, uvxy_bars, "next_open")
    if min(vixy_fills) != date(2011, 2, 1) or len(vixy_fills) != 189:
        problems.append(f"VIXY fills {len(vixy_fills)} first {min(vixy_fills) if vixy_fills else None}")
    if sum(1 for d in vixy_fills if d >= OOS_START) != 28:
        problems.append("VIXY OOS fill count")
    if min(uvxy_fills) != date(2011, 11, 1) or len(uvxy_fills) != 180:
        problems.append(f"UVXY fills {len(uvxy_fills)}")
    if sum(1 for d in uvxy_fills if d >= OOS_START) != 28:
        problems.append("UVXY OOS fill count")
    if problems:
        raise SystemExit("data check failed: " + "; ".join(problems))
    spy_ret = {}
    prev = None
    for d in spy_days:
        px = spy_bars[d][1]
        spy_ret[d] = 0.0 if prev is None else px / prev - 1.0
        prev = px
    return vixy_days, vixy_bars, uvxy_days, uvxy_bars, spy_ret


def _placebo(gross):
    days = gross["days"]
    dates = [d["date"] for d in days]
    index = {d: i for i, d in enumerate(dates)}
    n_days = len(dates)
    trips = gross["trips"]
    g = np.zeros((n_days, len(trips)))
    for j, trip in enumerate(trips):
        for day, dollars in trip.daily:
            if day in index:
                g[index[day], j] += dollars
    eq_prev = np.empty(n_days)
    eq_prev[0] = 1.0
    for i in range(1, n_days):
        eq_prev[i] = days[i - 1]["equity"]
    actual = np.array([d["ret"] for d in days], float)
    actual_sharpe = _sharpe(actual)
    rng = np.random.default_rng(SEED_DIRECTION)
    signs = rng.choice(np.array([-1.0, 1.0]), size=(N_DRAWS, len(trips)))
    flipped = signs @ g.T
    rets = np.where(eq_prev > 0.0, flipped / eq_prev, 0.0)
    mu = rets.mean(axis=1)
    sd = rets.std(axis=1, ddof=1)
    sh = np.where(sd > 0.0, mu / sd * math.sqrt(ANNUAL), np.nan)
    if actual_sharpe is None:
        p = None
        n_ge = None
    else:
        n_ge = int(np.sum(sh >= actual_sharpe))
        p = (1 + n_ge) / (N_DRAWS + 1)
    finite = sh[np.isfinite(sh)]
    return {
        "actual_gross_sharpe": actual_sharpe,
        "null_mean": float(np.nanmean(sh)),
        "null_p95": float(np.nanquantile(sh, 0.95)),
        "p": None if p is None else float(p),
        "n_ge": n_ge,
        "n_draws": N_DRAWS,
        "seed": SEED_DIRECTION,
        "sharpes": finite,
    }


def _bootstrap(rs):
    r = np.asarray(rs, float)
    n = len(r)
    n_blocks = int(math.ceil(n / BLOCK))
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    starts = rng.integers(0, n, size=(N_DRAWS, n_blocks))
    idx = (starts[..., None] + np.arange(BLOCK)) % n
    idx = idx.reshape(N_DRAWS, n_blocks * BLOCK)[:, :n]
    samples = r[idx]
    mu = samples.mean(axis=1)
    sd = samples.std(axis=1, ddof=1)
    sh = np.where(sd > 0.0, mu / sd * math.sqrt(ANNUAL), np.nan)
    lo, hi = np.nanquantile(sh, [0.025, 0.975])
    return {
        "low": float(lo), "high": float(hi), "median": float(np.nanmedian(sh)),
        "fraction_le_0": float(np.nanmean(sh <= 0.0)),
        "block": BLOCK, "n_draws": N_DRAWS, "seed": SEED_BOOTSTRAP,
    }


def _pack_window(days, trips, bench_days, spy_map, pred):
    d_s, t_s = _window(days, trips, pred)
    d_b, _ = _window(bench_days, [], pred)
    return {
        "strategy": _stats(d_s, t_s),
        "benchmark": _stats(d_b, []),
        "spy": _series_stats([spy_map[d["date"]] for d in d_s]),
        "cash": _series_stats([0.0] * len(d_s)),
        "start": d_s[0]["date"] if d_s else None,
        "end": d_s[-1]["date"] if d_s else None,
    }


def _reason_table(trips):
    out = {}
    for reason in sorted({t.reason for t in trips}):
        sub = [t for t in trips if t.reason == reason]
        dn = np.array([t.net for t in sub])
        out[reason] = {
            "trades": len(sub),
            "net_dollars": float(dn.sum()),
            "gross_dollars": float(sum(t.gross for t in sub)),
            "win_rate": float(np.mean(dn > 0.0)),
        }
    return out


def _stress(days):
    windows = {
        "2018-02": (date(2018, 2, 1), date(2018, 2, 28)),
        "2020-03": (date(2020, 3, 1), date(2020, 3, 31)),
        "2024-08": (date(2024, 8, 1), date(2024, 8, 31)),
        "2025-04": (date(2025, 4, 1), date(2025, 4, 30)),
    }
    out = {}
    for name, (a, b) in windows.items():
        rs = [d["ret"] for d in days if a <= d["date"] <= b]
        out[name] = {
            "sessions": len(rs),
            "strategy_return": _compound(rs) if rs else None,
        }
    return out


def _predictions(full_stats, daily_rets):
    r = np.asarray(daily_rets, float)
    worst = float(np.sort(r)[:10].sum())
    avg = float(r.mean())
    pred1 = full_stats["total_return"] is not None and full_stats["total_return"] > 0.0
    pred2 = worst < 0.0 and abs(worst) > abs(avg * 50.0)
    return {
        "full_net_total_return_positive": {
            "value": full_stats["total_return"], "pass": pred1,
            "score": "consistent" if pred1 else "not consistent",
        },
        "ten_worst_vs_50_average_days": {
            "sum_ten_worst": worst,
            "mean_daily": avg,
            "threshold_abs": abs(avg * 50.0),
            "pass": pred2,
            "score": "consistent" if pred2 else "not consistent",
        },
    }


def _fnum(x):
    return format(x, ".16g")


def run_store(rules_hash, locked_utc, reason):
    with MarketData() as md:
        schema = md.schema_version
        vixy = _load_symbol(md, "VIXY")
        uvxy = _load_symbol(md, "UVXY")
        spy_adj, _, _ = _load_symbol(md, "SPY")
    book, bars, uvxy_book, uvxy_bars, spy_ret = _validate(
        vixy[0], vixy[1], vixy[2], uvxy[0], uvxy[1], uvxy[2], spy_adj
    )

    def is_is(d):
        return d < OOS_START

    def is_oos(d):
        return d >= OOS_START

    primary = simulate(book, bars, WEIGHT, COST_BPS, BORROW)
    gross = simulate(book, bars, WEIGHT, 0.0, 0.0)
    bench = buy_and_hold(book, bars, primary["first_fill"])
    if [d["date"] for d in primary["days"]] != [d["date"] for d in gross["days"]]:
        raise AssertionError("gross dates differ")
    if [d["date"] for d in primary["days"]] != [d["date"] for d in bench]:
        raise AssertionError("benchmark dates differ")
    c2c = product_c2c(book, bars, primary["first_fill"])

    windows = {}
    for name, pred in ("full", lambda d: True), ("is", is_is), ("oos", is_oos):
        windows[name] = _pack_window(primary["days"], primary["trips"], bench, spy_ret, pred)

    # The OOS compound restarts at 1. It must still match the path ratio.
    oos_days = [d for d in primary["days"] if d["date"] >= OOS_START]
    is_days = [d for d in primary["days"] if d["date"] < OOS_START]
    path_oos = oos_days[-1]["equity"] / is_days[-1]["equity"] - 1.0
    if abs(path_oos - windows["oos"]["strategy"]["total_return"]) > 1e-8:
        raise AssertionError("OOS compound does not match the equity path")

    grid = []
    for w in GRID:
        sim_w = simulate(book, bars, w, COST_BPS, BORROW)
        d_is, t_is = _window(sim_w["days"], sim_w["trips"], is_is)
        d_oos, t_oos = _window(sim_w["days"], sim_w["trips"], is_oos)
        d_full, t_full = sim_w["days"], sim_w["trips"]
        grid.append({
            "weight": w,
            "is_sharpe": _stats(d_is, t_is)["sharpe"],
            "is_return": _stats(d_is, t_is)["total_return"],
            "oos_sharpe": _stats(d_oos, t_oos)["sharpe"],
            "oos_return": _stats(d_oos, t_oos)["total_return"],
            "full_sharpe": _stats(d_full, t_full)["sharpe"],
            "full_return": _stats(d_full, t_full)["total_return"],
        })
    n_is_pos = sum(1 for g in grid if g["is_sharpe"] is not None and g["is_sharpe"] > 0.0)
    is_ranks = sorted(range(len(grid)), key=lambda i: -(grid[i]["is_sharpe"] or -1e9))
    primary_rank = is_ranks.index(GRID.index(WEIGHT)) + 1
    grid_summary = {
        "cells": grid,
        "n_cells": len(grid),
        "n_is_positive": n_is_pos,
        "primary_is_rank": primary_rank,
        "spearman_is_oos": _spearman([g["is_sharpe"] for g in grid], [g["oos_sharpe"] for g in grid]),
    }

    costs = []
    for mult in COST_MULTIPLES:
        sim_c = simulate(book, bars, WEIGHT, COST_BPS * mult, BORROW * mult)
        d_full, t_full = sim_c["days"], sim_c["trips"]
        d_oos, t_oos = _window(d_full, t_full, is_oos)
        st_f = _stats(d_full, t_full)
        st_o = _stats(d_oos, t_oos)
        costs.append({
            "multiple": mult,
            "cost_bps": COST_BPS * mult,
            "borrow": BORROW * mult,
            "full_sharpe": st_f["sharpe"],
            "full_return": st_f["total_return"],
            "oos_sharpe": st_o["sharpe"],
            "oos_return": st_o["total_return"],
        })
    base_cost = next(c for c in costs if c["multiple"] == 1.0)
    if abs(base_cost["full_sharpe"] - windows["full"]["strategy"]["sharpe"]) > 1e-12:
        raise AssertionError("cost sweep 1x does not match the primary")

    delay = simulate(book, bars, WEIGHT, COST_BPS, BORROW, mode="delay1")
    same = simulate(book, bars, WEIGHT, COST_BPS, BORROW, mode="same_close")

    def _fill_summary(sim):
        d_full, t_full = sim["days"], sim["trips"]
        d_oos, t_oos = _window(d_full, t_full, is_oos)
        return {
            "first_fill": sim["first_fill"],
            "full": _stats(d_full, t_full),
            "oos": _stats(d_oos, t_oos),
            "ruined": sim["ruined"],
        }

    placebo = _placebo(gross)
    boot = _bootstrap([d["ret"] for d in primary["days"]])
    preds = _predictions(windows["full"]["strategy"], [d["ret"] for d in primary["days"]])

    uvxy = simulate(uvxy_book, uvxy_bars, WEIGHT, COST_BPS, BORROW)
    uvxy_c2c_dates = [d["date"] for d in uvxy["days"]]
    # SPY returns exist on every UVXY date because UVXY's span is inside SPY's.
    uvxy_windows = {}
    uvxy_bench = buy_and_hold(uvxy_book, uvxy_bars, uvxy["first_fill"])
    for name, pred in ("full", lambda d: True), ("is", is_is), ("oos", is_oos):
        uvxy_windows[name] = _pack_window(uvxy["days"], uvxy["trips"], uvxy_bench, spy_ret, pred)

    by_year = {}
    for y in sorted({d["date"].year for d in primary["days"]}):
        d_s, t_s = _window(primary["days"], primary["trips"], lambda d, y=y: d.year == y)
        d_b, _ = _window(bench, [], lambda d, y=y: d.year == y)
        st = _stats(d_s, t_s)
        by_year[str(y)] = {
            "sessions": st["sessions"],
            "strategy_return": st["total_return"],
            "strategy_sharpe": st["sharpe"],
            "strategy_max_dd": st["max_dd"],
            "benchmark_return": _compound([d["ret"] for d in d_b]),
            "spy_return": _compound([spy_ret[d["date"]] for d in d_s]),
            "trades": st["trades"],
        }

    q_rows = []
    for d, b in zip(primary["days"], bench):
        q_rows.append({
            "date": d["date"].isoformat(),
            "strategy": d["ret"],
            "benchmark": b["ret"],
            "spy": spy_ret[d["date"]],
            "product": c2c[d["date"]],
        })
    quint_spy = _quintiles(q_rows, "spy")
    quint_product = _quintiles(q_rows, "product")

    def _corr_window(pred):
        ds = [d for d in primary["days"] if pred(d["date"])]
        return {
            "spy": _corr([d["ret"] for d in ds], [spy_ret[d["date"]] for d in ds]),
            "benchmark": _corr(
                [d["ret"] for d in ds],
                [b["ret"] for b in bench if pred(b["date"])],
            ),
        }

    head, dirty = git_state()
    full_st = windows["full"]["strategy"]
    is_st = windows["is"]["strategy"]
    oos_st = windows["oos"]["strategy"]
    uvxy_oos = uvxy_windows["oos"]["strategy"]
    two_x = next(c for c in costs if c["multiple"] == 2.0)
    line1 = (
        oos_st["sharpe"] is not None and oos_st["sharpe"] >= 0.5
        and _pf_pass(oos_st, 1.10)
    )
    line2 = placebo["p"] is not None and placebo["p"] <= 0.05
    line3 = (
        is_st["sharpe"] is not None and is_st["sharpe"] > 0.0
        and n_is_pos >= 3
    )
    line4 = two_x["full_return"] is not None and two_x["full_return"] > 0.0
    line5 = uvxy_oos["sharpe"] is not None and uvxy_oos["sharpe"] > 0.0
    line6 = oos_st["trades"] >= 24
    lines = [
        {"id": 1, "pass": line1, "oos_sharpe": oos_st["sharpe"], "oos_profit_factor": oos_st["profit_factor"],
         "oos_profit_factor_infinite": oos_st["profit_factor_infinite"]},
        {"id": 2, "pass": line2, "p": placebo["p"]},
        {"id": 3, "pass": line3, "is_sharpe": is_st["sharpe"], "n_is_positive": n_is_pos, "n_cells": 5},
        {"id": 4, "pass": line4, "full_return_2x": two_x["full_return"]},
        {"id": 5, "pass": line5, "uvxy_oos_sharpe": uvxy_oos["sharpe"]},
        {"id": 6, "pass": line6, "oos_round_trips": oos_st["trades"]},
    ]
    if not line6:
        status = "Inconclusive"
    elif all(line["pass"] for line in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"

    results = {
        "rules_sha256": rules_hash,
        "locked_utc": locked_utc,
        "git_head": head,
        "dirty": dirty,
        "schema_user_version": schema,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOTSTRAP,
            "timing": SEED_TIMING,
            "verify": SEED_VERIFY,
        },
        "timing_placebo": {
            "ran": False,
            "seed_recorded_unused": SEED_TIMING,
            "reason": "The rule is short at every rebalance, so redrawing entry months with the same always-short count reproduces the strategy.",
        },
        "primary": {"symbol": "VIXY", "weight": WEIGHT, "cost_bps": COST_BPS, "borrow": BORROW},
        "first_fill": primary["first_fill"],
        "last_session": primary["days"][-1]["date"],
        "ruined": primary["ruined"],
        "ruin_date": primary["ruin_date"],
        "terminal_equity": primary["terminal_equity"],
        "friction_dollars": float(sum(t.gross - t.net for t in primary["trips"])),
        "windows": windows,
        "by_year": by_year,
        "by_reason": _reason_table(primary["trips"]),
        "by_side": _reason_table_side(primary["trips"]),
        "stress": _stress(primary["days"]),
        "quintiles_spy": quint_spy,
        "quintiles_product": quint_product,
        "correlation": {
            "full": _corr_window(lambda d: True),
            "is": _corr_window(is_is),
            "oos": _corr_window(is_oos),
        },
        "grid": grid_summary,
        "costs": costs,
        "delay": _fill_summary(delay),
        "same_close": _fill_summary(same),
        "placebo": {k: v for k, v in placebo.items() if k != "sharpes"},
        "bootstrap": boot,
        "predictions": preds,
        "uvxy": {
            "first_fill": uvxy["first_fill"],
            "ruined": uvxy["ruined"],
            "ruin_date": uvxy["ruin_date"],
            "terminal_equity": uvxy["terminal_equity"],
            "windows": uvxy_windows,
            "n_dates": len(uvxy_c2c_dates),
        },
        "acceptance": lines,
        "status": status,
    }
    del uvxy_c2c_dates

    daily_rows = []
    for d, g, b in zip(primary["days"], gross["days"], bench):
        daily_rows.append({
            "date": d["date"],
            "strategy_net": d["ret"],
            "strategy_gross": g["ret"],
            "benchmark_long": b["ret"],
            "spy_c2c": spy_ret[d["date"]],
            "cash": 0.0,
            "equity": d["equity"],
            "shares": d["shares"],
        })

    write_outputs(primary["trips"], daily_rows, results, placebo["sharpes"])
    append_log(rules_hash, head, dirty, reason, results)
    return results


def _reason_table_side(trips):
    dn = np.array([t.net for t in trips]) if trips else np.array([])
    wins = dn[dn > 0.0] if len(dn) else dn
    losses = dn[dn < 0.0] if len(dn) else dn
    if len(losses):
        pf = float(wins.sum() / abs(losses.sum())) if len(wins) else 0.0
    elif len(wins):
        pf = None
    else:
        pf = None
    return {
        "short": {
            "trades": len(trips),
            "net_dollars": float(dn.sum()) if len(dn) else 0.0,
            "gross_dollars": float(sum(t.gross for t in trips)),
            "win_rate": float(np.mean(dn > 0.0)) if len(dn) else None,
            "profit_factor": pf,
        }
    }


def write_outputs(trips, daily_rows, results, sharpes):
    with (HERE / "trades.csv").open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f)
        w.writerow([
            "side", "entry_time", "entry_px", "exit_time", "exit_px",
            "gross", "net", "exit_reason", "entry_equity", "hold_sessions",
        ])
        for t in trips:
            w.writerow([
                t.side,
                _stamp(t.entry_date, False),
                _fnum(t.entry_px),
                _stamp(t.exit_date, t.at_close),
                _fnum(t.exit_px),
                _fnum(t.gross),
                _fnum(t.net),
                t.reason,
                _fnum(t.entry_equity),
                t.hold,
            ])
    with (HERE / "daily.csv").open("w", encoding="utf-8", newline="\n") as f:
        w = csv.writer(f)
        cols = ["date", "strategy_net", "strategy_gross", "benchmark_long",
                "spy_c2c", "cash", "equity", "shares"]
        w.writerow(cols)
        for row in daily_rows:
            w.writerow([
                row["date"].isoformat(),
                _fnum(row["strategy_net"]), _fnum(row["strategy_gross"]),
                _fnum(row["benchmark_long"]), _fnum(row["spy_c2c"]),
                _fnum(row["cash"]), _fnum(row["equity"]), _fnum(row["shares"]),
            ])
    np.save(HERE / "placebo_direction.npy", np.asarray(sharpes, float))
    (HERE / "results.json").write_text(
        json.dumps(_clean(results), indent=2) + "\n", encoding="utf-8", newline="\n"
    )


def append_log(rules_hash, head, dirty, reason, results):
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nAppend-only. One entry per store run.\n", encoding="utf-8", newline="\n")
    full = results["windows"]["full"]["strategy"]
    ins = results["windows"]["is"]["strategy"]
    oos = results["windows"]["oos"]["strategy"]
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    text = (
        f"\n## {now}\n"
        f"- rules_sha256 {rules_hash}\n"
        f"- git_head {head} dirty={dirty}\n"
        f"- reason: {reason}\n"
        f"- full Sharpe {full['sharpe']} return {full['total_return']} | "
        f"IS Sharpe {ins['sharpe']} | "
        f"OOS Sharpe {oos['sharpe']} return {oos['total_return']} | "
        f"trades full/IS/OOS {full['trades']}/{ins['trades']}/{oos['trades']} | "
        f"status {results['status']}\n"
    )
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    digest, locked = rules_hash_check()
    self_test()
    reason = sys.argv[1] if len(sys.argv) > 1 else "initial pre-registered run"
    run_store(digest, locked, reason)


if __name__ == "__main__":
    main()
