# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""TLT/IEF 252-session time-series momentum. Pre-registered runs.

Refuses to run unless RULES.md hashes to RULES.lock (CRLF normalized to LF).
Runs the synthetic self-test before opening the store. Appends RUNLOG.md on
every store run that writes results.
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
from mdq import MarketData, session_date_of  # noqa: E402

# ---- constants mirroring RULES.md -------------------------------------------
FUNDS = ("TLT", "IEF")
LOOKBACK = 252
COST_BPS = 1.0
SKIP = {dt.date(2012, 10, 29), dt.date(2012, 10, 30), dt.date(2018, 12, 5)}
OOS_START = dt.date(2024, 7, 1)
FIRST = dt.date(2011, 1, 4)
LAST = dt.date(2026, 10, 1)
N_BARS = 3959
GRID = (126, 189, 252, 315, 378)
COST_SWEEP = (0.0, 0.5, 1.0, 2.0, 3.0)
SEED_DIRECTION = 20261051
SEED_BOOTSTRAP = 20261052
SEED_TIMING = 20261053
ANNUAL = 252
N_DIR = 2000
N_BOOT = 2000
N_TIMING = 500
BLOCK = 20


def rules_hash_check():
    lock_lines = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()
    want = lock_lines[0].split()[1]
    have = hashlib.sha256(
        (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    ).hexdigest()
    if want != have:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    locked = lock_lines[1].split(" ", 1)[1]
    return have, locked


def sgn(x):
    if x > 0.0:
        return 1
    if x < 0.0:
        return -1
    return 0


def _signal_dates(book):
    last = {}
    for d in book:
        last[(d.year, d.month)] = d
    return [last[k] for k in sorted(last)]


class _Trip:
    __slots__ = (
        "sym", "side", "entry_date", "entry_px", "entry_equity",
        "gross", "net", "exit_date", "exit_px", "reason", "daily", "_today", "hold",
    )

    def __init__(self, sym, side, entry_date, entry_px, entry_equity):
        self.sym = sym
        self.side = side
        self.entry_date = entry_date
        self.entry_px = entry_px
        self.entry_equity = entry_equity
        self.gross = 0.0
        self.net = 0.0
        self.exit_date = None
        self.exit_px = None
        self.reason = None
        self.daily = []
        self._today = 0.0


def simulate(book, bars, lookback, cost_bps, mode="next_open",
             weight_override=None, always_long=False):
    """Next-open accounting in RULES.md. `bars[sym]` maps session -> (open, close).

    `mode` is `next_open`, `delay1`, or `same_close`. `always_long` forces the
    uncosted benchmark weights. `weight_override` maps a signal date to
    per-fund weights and replaces the lookback signal on eligible dates.
    """
    offset = {"next_open": 1, "delay1": 2, "same_close": 0}[mode]
    rate = cost_bps / 10000.0
    own = {}
    index = {}
    for sym in FUNDS:
        own[sym] = sorted(bars[sym])
        index[sym] = {d: i for i, d in enumerate(own[sym])}
    pos = {d: i for i, d in enumerate(book)}
    sig_dates = _signal_dates(book)

    def weight_of(sd, sym):
        if weight_override is not None:
            return weight_override[sd][sym]
        i = index[sym].get(sd)
        if i is None or i < lookback:
            return 0.0
        if always_long:
            return 0.5
        c1 = bars[sym][sd][1]
        c0 = bars[sym][own[sym][i - lookback]][1]
        return sgn(c1 / c0 - 1.0) / 2.0

    fills = {}
    scheduled = []
    for sd in sig_dates:
        fi = pos[sd] + offset
        if fi >= len(book) or fi < 0:
            continue
        eligible = False
        weights = {}
        for sym in FUNDS:
            i = index[sym].get(sd)
            if i is not None and i >= lookback:
                eligible = True
            weights[sym] = weight_of(sd, sym)
        if not eligible:
            continue
        fd = book[fi]
        if fd in fills:
            raise RuntimeError(f"two signals fill on {fd}")
        fills[fd] = weights
        scheduled.append(sd)
    if not fills:
        raise RuntimeError("no fills")
    first_fill = min(fills)

    shares = {sym: 0.0 for sym in FUNDS}
    last_close = {sym: None for sym in FUNDS}
    prev_had = {sym: False for sym in FUNDS}
    active = {sym: None for sym in FUNDS}
    closed = []
    equity = 1.0
    days = []

    def add_pnl(sym, amount):
        trip = active[sym]
        if trip is None or amount == 0.0:
            return
        trip.gross += amount
        trip.net += amount
        trip._today += amount

    def pay_cost(sym, amount):
        if amount != 0.0 and active[sym] is not None:
            active[sym].net -= amount

    def close_trip(sym, day, px, reason):
        trip = active[sym]
        if trip._today != 0.0:
            trip.daily.append((day, trip._today))
        trip._today = 0.0
        trip.exit_date = day
        trip.exit_px = px
        trip.reason = reason
        closed.append(trip)
        active[sym] = None

    def open_trip(sym, new_shares, day, px, entry_equity):
        trip = _Trip(sym, "long" if new_shares > 0.0 else "short", day, px, entry_equity)
        active[sym] = trip

    def trade(sym, new_shares, px, day, entry_equity):
        old = shares[sym]
        old_sign = sgn(old)
        new_sign = sgn(new_shares)
        if old_sign == 0 and new_sign == 0:
            shares[sym] = 0.0
            return 0.0
        cost = 0.0
        if old_sign != 0 and new_sign == old_sign:
            cost = abs(new_shares - old) * px * rate
            pay_cost(sym, cost)
            shares[sym] = new_shares
            return cost
        if old_sign != 0:
            cost = abs(old) * px * rate
            pay_cost(sym, cost)
            close_trip(sym, day, px, "flip" if new_sign != 0 else "flat")
            shares[sym] = 0.0
        if new_sign != 0:
            open_cost = abs(new_shares) * px * rate
            shares[sym] = new_shares
            open_trip(sym, new_shares, day, px, entry_equity)
            pay_cost(sym, open_cost)
            cost += open_cost
        return cost

    for day in book:
        if day == first_fill and abs(equity - 1.0) > 1e-9:
            raise AssertionError(f"equity at first fill is {equity}, not 1")
        e_prev = equity
        gap_sum = 0.0
        for sym in FUNDS:
            bar = bars[sym].get(day)
            if shares[sym] != 0.0 and bar is not None and prev_had[sym]:
                gap = shares[sym] * (bar[0] - last_close[sym])
            else:
                gap = 0.0
            gap_sum += gap
            add_pnl(sym, gap)

        if mode == "same_close":
            oc_sum = 0.0
            for sym in FUNDS:
                bar = bars[sym].get(day)
                if bar is not None and shares[sym] != 0.0:
                    oc = shares[sym] * (bar[1] - bar[0])
                else:
                    oc = 0.0
                oc_sum += oc
                add_pnl(sym, oc)
            e_mark = e_prev + gap_sum + oc_sum
            cost_sum = 0.0
            if day in fills:
                for sym in FUNDS:
                    bar = bars[sym].get(day)
                    if bar is None:
                        continue
                    new_shares = fills[day][sym] * e_mark / bar[1]
                    cost_sum += trade(sym, new_shares, bar[1], day, e_mark)
            for sym in FUNDS:
                bar = bars[sym].get(day)
                if bar is not None:
                    last_close[sym] = bar[1]
                    prev_had[sym] = True
                else:
                    prev_had[sym] = False
            equity = e_mark - cost_sum
        else:
            e_open = e_prev + gap_sum
            cost_sum = 0.0
            if day in fills:
                for sym in FUNDS:
                    bar = bars[sym].get(day)
                    if bar is None:
                        continue
                    new_shares = fills[day][sym] * e_open / bar[0]
                    cost_sum += trade(sym, new_shares, bar[0], day, e_open)
            oc_sum = 0.0
            for sym in FUNDS:
                bar = bars[sym].get(day)
                if bar is not None and shares[sym] != 0.0:
                    oc = shares[sym] * (bar[1] - bar[0])
                else:
                    oc = 0.0
                oc_sum += oc
                add_pnl(sym, oc)
                if bar is not None:
                    last_close[sym] = bar[1]
                    prev_had[sym] = True
                else:
                    prev_had[sym] = False
            equity = e_open - cost_sum + oc_sum

        for sym in FUNDS:
            trip = active[sym]
            if trip is not None and trip._today != 0.0:
                trip.daily.append((day, trip._today))
            if trip is not None:
                trip._today = 0.0

        if day >= first_fill:
            days.append({
                "date": day,
                "ret": equity / e_prev - 1.0,
                "equity": equity,
                "exposed": any(shares[sym] != 0.0 for sym in FUNDS),
                "shares": {sym: shares[sym] for sym in FUNDS},
            })

    last_day = book[-1]
    for sym in FUNDS:
        if active[sym] is not None:
            px = last_close[sym]
            if px is None:
                raise RuntimeError(f"open {sym} trip has no close at sample end")
            close_trip(sym, last_day, px, "end_of_sample")

    net_sum = sum(t.net for t in closed)
    if abs(net_sum - (equity - 1.0)) > 1e-6:
        raise AssertionError(f"trip net {net_sum} != equity change {equity - 1.0}")
    for t in closed:
        if t.reason == "end_of_sample":
            t.hold = pos[t.exit_date] - pos[t.entry_date] + 1
        else:
            t.hold = pos[t.exit_date] - pos[t.entry_date]
    return {
        "days": days,
        "trips": closed,
        "fills": fills,
        "first_fill": first_fill,
        "scheduled": scheduled,
    }


def _sharpe(r):
    r = np.asarray(r, float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0:
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def _stats(days, trips):
    r = np.array([d["ret"] for d in days], float)
    n = int(len(r))
    out = {
        "sessions": n,
        "total_return": None,
        "cagr": None,
        "vol": None,
        "sharpe": None,
        "max_dd": None,
        "tstat": None,
        "trades": len(trips),
        "profit_factor": None,
        "profit_factor_infinite": False,
        "win_rate": None,
        "avg_net_trade_bp": None,
        "avg_winner_bp": None,
        "avg_loser_bp": None,
        "exposure": None,
        "hold_median": None,
        "hold_mean": None,
        "trades_per_year": None,
    }
    if n == 0:
        return out
    eq = np.cumprod(1.0 + r)
    out["total_return"] = float(eq[-1] - 1.0)
    out["max_dd"] = float((eq / np.maximum.accumulate(eq) - 1.0).min())
    sd = float(r.std(ddof=1)) if n > 1 else 0.0
    out["vol"] = float(sd * math.sqrt(ANNUAL)) if n > 1 else 0.0
    out["sharpe"] = _sharpe(r)
    out["tstat"] = float(r.mean() / sd * math.sqrt(n)) if sd > 0.0 else None
    out["cagr"] = float(eq[-1] ** (ANNUAL / n) - 1.0) if eq[-1] > 0.0 else None
    out["exposure"] = float(sum(1 for d in days if d["exposed"]) / n)
    if trips:
        dn = np.array([t.net for t in trips], float)
        ee = np.array([t.entry_equity for t in trips], float)
        bp = dn / ee * 1e4
        wins = dn[dn > 0.0]
        losses = dn[dn < 0.0]
        out["win_rate"] = float(np.mean(dn > 0.0))
        out["avg_net_trade_bp"] = float(bp.mean())
        if len(wins) and len(losses):
            out["profit_factor"] = float(wins.sum() / abs(losses.sum()))
            w_bp = dn[dn > 0.0] / ee[dn > 0.0] * 1e4
            l_bp = dn[dn < 0.0] / ee[dn < 0.0] * 1e4
            out["avg_winner_bp"] = float(w_bp.mean())
            out["avg_loser_bp"] = float(l_bp.mean())
        elif len(wins) and not len(losses):
            out["profit_factor_infinite"] = True
            out["avg_winner_bp"] = float((dn[dn > 0.0] / ee[dn > 0.0] * 1e4).mean())
        elif len(losses):
            out["profit_factor"] = 0.0
            out["avg_loser_bp"] = float((dn[dn < 0.0] / ee[dn < 0.0] * 1e4).mean())
        holds = [t.hold for t in trips]
        out["hold_median"] = float(np.median(holds))
        out["hold_mean"] = float(np.mean(holds))
        out["trades_per_year"] = float(len(trips) / (n / ANNUAL))
    return out


def _side_stats(trips, side):
    sub = [t for t in trips if t.side == side]
    dn = np.array([t.net for t in sub], float) if sub else np.array([])
    wins = dn[dn > 0.0] if len(dn) else dn
    losses = dn[dn < 0.0] if len(dn) else dn
    if len(losses):
        pf = float(wins.sum() / abs(losses.sum())) if len(wins) else 0.0
        inf = False
    elif len(wins):
        pf, inf = None, True
    else:
        pf, inf = None, False
    return {
        "trades": len(sub),
        "win_rate": float(np.mean(dn > 0.0)) if len(dn) else None,
        "profit_factor": pf,
        "profit_factor_infinite": inf,
        "net_dollars": float(dn.sum()) if len(dn) else 0.0,
        "gross_dollars": float(sum(t.gross for t in sub)),
    }


def _compound(rs):
    eq = 1.0
    for x in rs:
        eq *= 1.0 + x
    return eq - 1.0


def _quintile_table(rows, spy_key, strat_key):
    rows = sorted(rows, key=lambda z: (z[spy_key], z["key"]))
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
            "spy_mean": float(np.mean([c[spy_key] for c in chunk])) if chunk else None,
            "strategy_mean": float(np.mean([c[strat_key] for c in chunk])) if chunk else None,
            "benchmark_mean": float(np.mean([c["benchmark"] for c in chunk])) if chunk else None,
        })
    return out


def _spearman(xs, ys):
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
    rx, ry = ranks(xs), ranks(ys)
    x = np.array(rx)
    y = np.array(ry)
    if x.std(ddof=1) == 0.0 or y.std(ddof=1) == 0.0:
        return None
    return float(np.corrcoef(x, y)[0, 1])


def _pf_passes(stats, floor):
    if stats["profit_factor_infinite"]:
        return True
    return stats["profit_factor"] is not None and stats["profit_factor"] >= floor


# ---- self-test ----------------------------------------------------------------
def _bars(spec):
    out = {sym: {} for sym in FUNDS}
    for sym, rows in spec.items():
        for d, o, c in rows:
            out[sym][dt.date.fromisoformat(d)] = (o, c)
    return out


def _book(spec):
    days = set()
    for rows in spec.values():
        for d, _, _ in rows:
            days.add(dt.date.fromisoformat(d))
    return sorted(days)


def _check(name, cond):
    if not cond:
        raise AssertionError(name)


def self_test():
    both = {
        "TLT": [("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 120.0), ("2020-02-03", 120.0, 132.0)],
        "IEF": [("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 120.0), ("2020-02-03", 120.0, 132.0)],
    }
    sim = simulate(_book(both), _bars(both), 1, 1.0)
    _check("both long equity", abs(sim["days"][-1]["equity"] - 1.0999) < 1e-9)
    _check("both long one day", len(sim["days"]) == 1 and sim["days"][0]["date"] == dt.date(2020, 2, 3))
    _check("both long return", abs(sim["days"][0]["ret"] - 0.0999) < 1e-9)
    for sym in FUNDS:
        _check(f"both long shares {sym}", abs(sim["days"][0]["shares"][sym] - 0.5 / 120.0) < 1e-12)
    _check("both long two trips", len(sim["trips"]) == 2)
    for t in sim["trips"]:
        _check("both long side", t.side == "long" and t.reason == "end_of_sample")
        _check("both long prices", t.entry_px == 120.0 and t.exit_px == 132.0)
        _check("both long dates", t.entry_date == dt.date(2020, 2, 3) and t.exit_date == dt.date(2020, 2, 3))
        _check("both long hold", t.hold == 1)
        _check("both long net", abs(t.net - 0.04995) < 1e-12)

    short = {
        "TLT": [("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 80.0), ("2020-02-03", 80.0, 72.0)],
        "IEF": [("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 80.0), ("2020-02-03", 80.0, 72.0)],
    }
    sim = simulate(_book(short), _bars(short), 1, 1.0)
    _check("both short equity", abs(sim["days"][-1]["equity"] - 1.0999) < 1e-9)
    _check("both short side", all(t.side == "short" and t.entry_px == 80.0 and t.exit_px == 72.0 for t in sim["trips"]))
    _check("both short shares", abs(sim["days"][0]["shares"]["TLT"] + 0.5 / 80.0) < 1e-12)

    flat = {
        "TLT": [("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 100.0), ("2020-02-03", 100.0, 110.0)],
        "IEF": [("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 120.0), ("2020-02-03", 120.0, 132.0)],
    }
    sim = simulate(_book(flat), _bars(flat), 1, 1.0)
    _check("one flat equity", abs(sim["days"][-1]["equity"] - 1.04995) < 1e-9)
    _check("one flat tlt shares", sim["days"][0]["shares"]["TLT"] == 0.0)
    _check("one flat not rescaled", abs(sim["days"][0]["shares"]["IEF"] - 0.5 / 120.0) < 1e-12)
    _check("one flat one trip", len(sim["trips"]) == 1 and sim["trips"][0].sym == "IEF")

    flip_days = ["2020-01-30", "2020-01-31", "2020-02-03", "2020-02-28", "2020-03-02"]
    flip = {
        sym: [(d, 100.0, 100.0) for d in flip_days] for sym in FUNDS
    }
    for sym in FUNDS:
        flip[sym][1] = ("2020-01-31", 100.0, 120.0)
        flip[sym][2] = ("2020-02-03", 120.0, 120.0)
        flip[sym][3] = ("2020-02-28", 120.0, 100.0)
        flip[sym][4] = ("2020-03-02", 100.0, 100.0)
    sim = simulate(_book(flip), _bars(flip), 1, 0.0)
    _check("flip equity", abs(sim["days"][-1]["equity"] - 5.0 / 6.0) < 1e-12)
    _check("flip shares", abs(sim["days"][-1]["shares"]["TLT"] + 5.0 / 1200.0) < 1e-12)
    reasons = sorted(t.reason for t in sim["trips"])
    _check("flip reasons", reasons == ["end_of_sample", "end_of_sample", "flip", "flip"])
    longs = [t for t in sim["trips"] if t.side == "long"]
    shorts = [t for t in sim["trips"] if t.side == "short"]
    _check("flip exit", all(t.exit_date == dt.date(2020, 3, 2) and t.exit_px == 100.0 and t.reason == "flip" for t in longs))
    _check("flip entry short", all(t.entry_date == dt.date(2020, 3, 2) and t.entry_px == 100.0 for t in shorts))
    _check("flip gross", all(abs(t.gross + 1.0 / 12.0) < 1e-12 for t in longs))

    one = {
        "TLT": [
            ("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 200.0),
            ("2020-02-03", 200.0, 200.0), ("2020-02-28", 200.0, 100.0),
            ("2020-03-02", 100.0, 100.0),
        ],
        "IEF": [(d, 100.0, 100.0) for d in flip_days],
    }
    sim = simulate(_book(one), _bars(one), 1, 1.0)
    _check("costed flip equity", abs(sim["days"][-1]["equity"] - 0.7498875025) < 1e-9)
    tlt_long = next(t for t in sim["trips"] if t.sym == "TLT" and t.side == "long")
    tlt_short = next(t for t in sim["trips"] if t.sym == "TLT" and t.side == "short")
    _check("costed flip no ief trip", all(t.sym == "TLT" for t in sim["trips"]))
    _check("costed long net", abs(tlt_long.net + 0.250075) < 1e-9)
    _check("costed short net", abs(tlt_short.net + 0.0000374975) < 1e-9)
    _check("costed long exit", tlt_long.reason == "flip" and tlt_long.exit_px == 100.0)

    hole_book_dates = ["2020-01-30", "2020-01-31", "2020-02-03", "2020-02-04", "2020-02-05", "2020-02-28", "2020-03-02"]
    hole = {sym: [] for sym in FUNDS}
    for d in hole_book_dates:
        hole["IEF"].append((d, 100.0, 100.0))
    for d, o, c in (
        ("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 110.0),
        ("2020-02-03", 100.0, 100.0), ("2020-02-05", 80.0, 80.0),
        ("2020-03-02", 80.0, 80.0),
    ):
        hole["TLT"].append((d, o, c))
    book = [dt.date.fromisoformat(d) for d in hole_book_dates]
    sim = simulate(book, _bars(hole), 1, 0.0)
    _check("hole equity", abs(sim["days"][-1]["equity"] - 1.0) < 1e-12)
    _check("hole flat", sim["days"][-1]["shares"]["TLT"] == 0.0)
    tlt = [t for t in sim["trips"] if t.sym == "TLT"]
    _check("hole one trip", len(tlt) == 1 and tlt[0].reason == "flat" and tlt[0].exit_date == dt.date(2020, 3, 2))
    _check("hole gross", abs(tlt[0].gross) < 1e-12)
    feb5 = next(d for d in sim["days"] if d["date"] == dt.date(2020, 2, 5))
    _check("hole day still long", abs(feb5["shares"]["TLT"] - 0.005) < 1e-12)
    _check("hole day return 0", abs(feb5["ret"]) < 1e-12)

    delay_spec = {
        sym: [
            ("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 120.0),
            ("2020-02-03", 120.0, 120.0), ("2020-02-04", 120.0, 132.0),
        ]
        for sym in FUNDS
    }
    sim = simulate(_book(delay_spec), _bars(delay_spec), 1, 1.0, mode="delay1")
    _check("delay date", len(sim["days"]) == 1 and sim["days"][0]["date"] == dt.date(2020, 2, 4))
    _check("delay equity", abs(sim["days"][0]["equity"] - 1.0999) < 1e-9)

    same_spec = {
        sym: [
            ("2020-01-30", 100.0, 100.0), ("2020-01-31", 100.0, 120.0),
            ("2020-02-03", 120.0, 132.0), ("2020-02-04", 132.0, 132.0),
        ]
        for sym in FUNDS
    }
    sim = simulate(_book(same_spec), _bars(same_spec), 1, 1.0, mode="same_close")
    jan31 = sim["days"][0]
    feb3 = sim["days"][1]
    _check("same-close no same-day drift", abs(jan31["equity"] - 0.9999) < 1e-9)
    _check("same-close shares", abs(jan31["shares"]["TLT"] - 0.5 / 120.0) < 1e-12)
    _check("same-close next day", abs(feb3["equity"] - 1.0999) < 1e-9)
    _check("same-close not resized early", abs(feb3["shares"]["TLT"] - 0.5 / 120.0) < 1e-12)


def _git():
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    return head, "yes" if dirty else "no"


def _load(md):
    raw = {}
    actions = {}
    for sym in ("TLT", "IEF", "SPY"):
        bars = md.bars(sym, "1d")
        raw[sym] = bars
        actions[sym] = md.corporate_actions(sym)
    problems = []
    for sym in FUNDS:
        days = [session_date_of(b.ts) for b in raw[sym]]
        if len(raw[sym]) != N_BARS:
            problems.append(f"{sym} bars {len(raw[sym])} != {N_BARS}")
        if not days or days[0] != FIRST or days[-1] != LAST:
            problems.append(f"{sym} range {days[:1]} {days[-1:]}")
        if actions[sym]:
            problems.append(f"{sym} has corporate actions")
        if any(b.open <= 0.0 or b.close <= 0.0 for b in raw[sym]):
            problems.append(f"{sym} non-positive price")
        if len(days) != len(set(days)):
            problems.append(f"{sym} duplicate sessions")
    if problems:
        raise SystemExit("data check failed: " + "; ".join(problems))
    book = [session_date_of(b.ts) for b in raw["SPY"] if session_date_of(b.ts) not in SKIP]
    bars = {sym: {} for sym in FUNDS}
    for sym in FUNDS:
        for b in raw[sym]:
            d = session_date_of(b.ts)
            if d in SKIP:
                continue
            bars[sym][d] = (b.open, b.close)
    spy = {}
    prev = None
    spy_ret_by_date = {}
    for b in raw["SPY"]:
        d = session_date_of(b.ts)
        if d in SKIP:
            continue
        spy[d] = b.close
        spy_ret_by_date[d] = 0.0 if prev is None else b.close / prev - 1.0
        prev = b.close
    return book, bars, spy_ret_by_date, {sym: len(actions[sym]) for sym in ("TLT", "IEF", "SPY")}


def _aligned(days, other_days):
    m = {d["date"]: d["ret"] for d in other_days}
    out = []
    for d in days:
        if d["date"] not in m:
            raise RuntimeError(f"missing aligned return on {d['date']}")
        out.append(m[d["date"]])
    return out


def _window(days, trips, pred):
    d2 = [d for d in days if pred(d["date"])]
    t2 = [t for t in trips if pred(t.entry_date)]
    return d2, t2


def _series_stats(rs):
    """Benchmark or SPY: no trades."""
    fake_days = [{"ret": x, "exposed": False} for x in rs]
    return _stats(fake_days, [])


def _signal_counts(book, bars, lookback):
    own = {sym: sorted(bars[sym]) for sym in FUNDS}
    index = {sym: {d: i for i, d in enumerate(own[sym])} for sym in FUNDS}
    counts = {sym: {"positive": 0, "negative": 0, "zero": 0, "ineligible": 0} for sym in FUNDS}
    eligible = {sym: [] for sym in FUNDS}
    weights = {sym: [] for sym in FUNDS}
    labels = {1: "positive", -1: "negative", 0: "zero"}
    for sd in _signal_dates(book):
        for sym in FUNDS:
            i = index[sym].get(sd)
            if i is None or i < lookback:
                counts[sym]["ineligible"] += 1
                continue
            c1 = bars[sym][sd][1]
            c0 = bars[sym][own[sym][i - lookback]][1]
            s = sgn(c1 / c0 - 1.0)
            counts[sym][labels[s]] += 1
            eligible[sym].append(sd)
            weights[sym].append(s / 2.0)
    return counts, eligible, weights


def _placebo_direction(gross):
    days = gross["days"]
    trips = gross["trips"]
    dates = [d["date"] for d in days]
    idx = {d: i for i, d in enumerate(dates)}
    mat = np.zeros((len(trips), len(dates)))
    for k, t in enumerate(trips):
        for day, g in t.daily:
            mat[k, idx[day]] += g
    eq = np.array([d["equity"] for d in days])
    pnl = np.empty(len(dates))
    pnl[0] = eq[0] - 1.0
    pnl[1:] = np.diff(eq)
    if np.max(np.abs(mat.sum(axis=0) - pnl)) > 1e-6:
        raise AssertionError("direction placebo dollars do not sum to equity changes")
    e_prev = np.empty(len(dates))
    e_prev[0] = 1.0
    e_prev[1:] = eq[:-1]
    actual = _sharpe([d["ret"] for d in days])
    rng = np.random.default_rng(SEED_DIRECTION)
    eps = rng.choice(np.array([-1.0, 1.0]), size=(N_DIR, len(trips)))
    flipped = (eps @ mat) / e_prev
    null = np.empty(N_DIR)
    for i in range(N_DIR):
        null[i] = _sharpe(flipped[i]) or 0.0
    p = (1 + int(np.sum(null >= actual))) / (N_DIR + 1)
    return {
        "actual_gross_sharpe": actual,
        "null_mean": float(null.mean()),
        "null_p95": float(np.quantile(null, 0.95)),
        "p": p,
        "draws": N_DIR,
        "seed": SEED_DIRECTION,
        "null_sharpes": [float(x) for x in null],
    }


def _placebo_timing(book, bars, gross_days):
    counts, eligible, weights = _signal_counts(book, bars, LOOKBACK)
    primary_dates = [d["date"] for d in gross_days]
    rng = np.random.default_rng(SEED_TIMING)
    null = np.empty(N_TIMING)
    sig_dates = _signal_dates(book)
    for draw in range(N_TIMING):
        ov = {sd: {sym: 0.0 for sym in FUNDS} for sd in sig_dates}
        for sym in FUNDS:
            perm = rng.permutation(len(eligible[sym]))
            for j, sd in enumerate(eligible[sym]):
                ov[sd][sym] = weights[sym][int(perm[j])]
        sim = simulate(book, bars, LOOKBACK, 0.0, weight_override=ov)
        m = {d["date"]: d["ret"] for d in sim["days"]}
        rs = [m[d] for d in primary_dates]
        null[draw] = _sharpe(rs) or 0.0
    actual = _sharpe([d["ret"] for d in gross_days])
    p = (1 + int(np.sum(null >= actual))) / (N_TIMING + 1)
    return counts, {
        "actual_gross_sharpe": actual,
        "null_mean": float(null.mean()),
        "null_p95": float(np.quantile(null, 0.95)),
        "p": p,
        "draws": N_TIMING,
        "seed": SEED_TIMING,
        "null_sharpes": [float(x) for x in null],
    }


def _bootstrap(rs):
    r = np.asarray(rs, float)
    n = len(r)
    n_blocks = int(math.ceil(n / BLOCK))
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    starts = rng.integers(0, n, size=(N_BOOT, n_blocks))
    offs = np.arange(BLOCK)
    idx = (starts[..., None] + offs) % n
    idx = idx.reshape(N_BOOT, -1)[:, :n]
    samples = r[idx]
    sharpes = np.empty(N_BOOT)
    for i in range(N_BOOT):
        sharpes[i] = _sharpe(samples[i]) or 0.0
    return {
        "p2_5": float(np.quantile(sharpes, 0.025)),
        "p97_5": float(np.quantile(sharpes, 0.975)),
        "median": float(np.median(sharpes)),
        "frac_le_0": float(np.mean(sharpes <= 0.0)),
        "draws": N_BOOT,
        "block": BLOCK,
        "seed": SEED_BOOTSTRAP,
    }


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
    if isinstance(obj, dt.date):
        return obj.isoformat()
    return obj


def _metric_row(label, st):
    return st


def run_store(rules_hash, locked_utc, reason):
    with MarketData() as md:
        book, bars, spy_ret, action_counts = _load(md)
        schema = md.schema_version
    primary = simulate(book, bars, LOOKBACK, COST_BPS)
    gross = simulate(book, bars, LOOKBACK, 0.0)
    bench = simulate(book, bars, LOOKBACK, 0.0, always_long=True)
    if [d["date"] for d in primary["days"]] != [d["date"] for d in gross["days"]]:
        raise AssertionError("gross dates differ from net dates")
    if [d["date"] for d in primary["days"]] != [d["date"] for d in bench["days"]]:
        raise AssertionError("benchmark dates differ from strategy dates")
    dates = [d["date"] for d in primary["days"]]
    if dates[0] >= OOS_START:
        raise AssertionError("first fill is not before the OOS window")
    if OOS_START not in set(book):
        raise AssertionError("2024-07-01 is not a book session")

    def is_is(d):
        return d < OOS_START

    def is_oos(d):
        return d >= OOS_START

    windows = {}
    for name, pred in ("full", lambda d: True), ("is", is_is), ("oos", is_oos):
        d_s, t_s = _window(primary["days"], primary["trips"], pred)
        d_b, _ = _window(bench["days"], [], pred)
        windows[name] = {
            "strategy": _stats(d_s, t_s),
            "benchmark": _stats(d_b, []),
            "spy": _series_stats([spy_ret[d["date"]] for d in d_s]),
            "start": d_s[0]["date"].isoformat(),
            "end": d_s[-1]["date"].isoformat(),
        }

    by_year = {}
    years = sorted({d.year for d in dates})
    for y in years:
        d_s, t_s = _window(primary["days"], primary["trips"], lambda d, y=y: d.year == y)
        d_b, _ = _window(bench["days"], [], lambda d, y=y: d.year == y)
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

    months = {}
    for d, b in zip(primary["days"], bench["days"]):
        key = (d["date"].year, d["date"].month)
        months.setdefault(key, {"strat": [], "spy": [], "benchmark": []})
        months[key]["strat"].append(d["ret"])
        months[key]["spy"].append(spy_ret[d["date"]])
        months[key]["benchmark"].append(b["ret"])
    month_rows = []
    for key in sorted(months):
        month_rows.append({
            "key": f"{key[0]:04d}-{key[1]:02d}",
            "strategy": _compound(months[key]["strat"]),
            "spy": _compound(months[key]["spy"]),
            "benchmark": _compound(months[key]["benchmark"]),
        })
    monthly_q = _quintile_table(month_rows, "spy", "strategy")
    q1 = monthly_q[0]["strategy_mean"]
    q3 = monthly_q[2]["strategy_mean"]
    daily_rows = []
    for d, b in zip(primary["days"], bench["days"]):
        daily_rows.append({
            "key": d["date"].isoformat(),
            "strategy": d["ret"],
            "spy": spy_ret[d["date"]],
            "benchmark": b["ret"],
        })
    daily_q = _quintile_table(daily_rows, "spy", "strategy")

    april = [d for d in primary["days"] if d["date"].year == 2025 and d["date"].month == 4]
    april_b = [d for d in bench["days"] if d["date"].year == 2025 and d["date"].month == 4]
    april_stats = {
        "sessions": len(april),
        "strategy": _compound([d["ret"] for d in april]) if april else None,
        "benchmark": _compound([d["ret"] for d in april_b]) if april_b else None,
        "spy": _compound([spy_ret[d["date"]] for d in april]) if april else None,
    }

    fund_gross = {sym: float(sum(t.gross for t in primary["trips"] if t.sym == sym)) for sym in FUNDS}
    fund_net = {sym: float(sum(t.net for t in primary["trips"] if t.sym == sym)) for sym in FUNDS}
    same_sign = sgn(fund_gross["TLT"]) != 0 and sgn(fund_gross["TLT"]) == sgn(fund_gross["IEF"])
    convex = q1 is not None and q3 is not None and q1 > q3

    grid = []
    for lb in GRID:
        sim = simulate(book, bars, lb, COST_BPS)
        d_is, _ = _window(sim["days"], sim["trips"], is_is)
        d_oos, _ = _window(sim["days"], sim["trips"], is_oos)
        grid.append({
            "lookback": lb,
            "is_sharpe": _sharpe([d["ret"] for d in d_is]),
            "oos_sharpe": _sharpe([d["ret"] for d in d_oos]),
            "is_return": _compound([d["ret"] for d in d_is]),
            "oos_return": _compound([d["ret"] for d in d_oos]),
            "first_fill": sim["first_fill"].isoformat(),
        })
    is_sharpes = [c["is_sharpe"] for c in grid]
    n_pos = sum(1 for s in is_sharpes if s is not None and s > 0.0)
    primary_is = next(c["is_sharpe"] for c in grid if c["lookback"] == LOOKBACK)
    better = sum(1 for s in is_sharpes if s is not None and primary_is is not None and s > primary_is)
    grid_summary = {
        "cells": grid,
        "is_positive": n_pos,
        "is_cells": len(grid),
        "primary_is_rank": better + 1,
        "is_oos_spearman": _spearman(
            [c["is_sharpe"] if c["is_sharpe"] is not None else 0.0 for c in grid],
            [c["oos_sharpe"] if c["oos_sharpe"] is not None else 0.0 for c in grid],
        ),
    }

    costs = []
    for bps in COST_SWEEP:
        sim = simulate(book, bars, LOOKBACK, bps)
        d_full = sim["days"]
        d_oos, _ = _window(sim["days"], sim["trips"], is_oos)
        costs.append({
            "bps": bps,
            "multiple": bps / COST_BPS if COST_BPS else None,
            "full_sharpe": _sharpe([d["ret"] for d in d_full]),
            "oos_sharpe": _sharpe([d["ret"] for d in d_oos]),
            "full_return": _compound([d["ret"] for d in d_full]),
            "oos_return": _compound([d["ret"] for d in d_oos]),
        })
    if abs(costs[0]["full_sharpe"] - _sharpe([d["ret"] for d in gross["days"]])) > 1e-9:
        raise AssertionError("zero-cost sweep does not match the gross run")

    def _delay_block(mode):
        sim = simulate(book, bars, LOOKBACK, COST_BPS, mode=mode)
        d_oos, _ = _window(sim["days"], sim["trips"], is_oos)
        return {
            "full_sharpe": _sharpe([d["ret"] for d in sim["days"]]),
            "oos_sharpe": _sharpe([d["ret"] for d in d_oos]),
            "full_return": _compound([d["ret"] for d in sim["days"]]),
            "oos_return": _compound([d["ret"] for d in d_oos]),
            "first_fill": sim["first_fill"].isoformat(),
            "sessions": len(sim["days"]),
        }

    delay = _delay_block("delay1")
    same_close = _delay_block("same_close")

    direction = _placebo_direction(gross)
    signal_counts, timing = _placebo_timing(book, bars, gross["days"])
    np.save(HERE / "placebo_direction.npy", np.array(direction.pop("null_sharpes")))
    np.save(HERE / "placebo_timing.npy", np.array(timing.pop("null_sharpes")))
    boot = _bootstrap([d["ret"] for d in primary["days"]])

    exits = {}
    for t in primary["trips"]:
        exits[t.reason] = exits.get(t.reason, 0) + 1

    oos = windows["oos"]["strategy"]
    is_ = windows["is"]["strategy"]
    full = windows["full"]["strategy"]
    cost_2x = next(c for c in costs if c["bps"] == 2.0)
    line1_s = oos["sharpe"] is not None and oos["sharpe"] >= 0.5
    line1_p = _pf_passes(oos, 1.10)
    line2 = direction["p"] <= 0.05
    line3_s = is_["sharpe"] is not None and is_["sharpe"] > 0.0
    line3_g = n_pos >= 3
    line4 = cost_2x["full_return"] is not None and cost_2x["full_return"] > 0.0
    line6 = oos["trades"] >= 24
    if not line6:
        status = "Inconclusive"
    elif line1_s and line1_p and line2 and line3_s and line3_g and line4:
        status = "Paper-trading candidate"
    else:
        status = "Rejected"

    def _pf_text(st):
        if st["profit_factor_infinite"]:
            return "infinite"
        if st["profit_factor"] is None:
            return "undefined"
        return st["profit_factor"]

    acceptance = [
        {"line": "1 OOS Sharpe", "required": ">= 0.5", "actual": oos["sharpe"], "pass": line1_s},
        {"line": "1 OOS profit factor", "required": ">= 1.10", "actual": _pf_text(oos), "pass": line1_p},
        {"line": "2 direction placebo p", "required": "<= 0.05", "actual": direction["p"], "pass": line2},
        {"line": "3 IS Sharpe", "required": "> 0", "actual": is_["sharpe"], "pass": line3_s},
        {"line": "3 IS grid Sharpe > 0", "required": ">= 3 of 5", "actual": f"{n_pos} of 5", "pass": line3_g},
        {"line": "4 full return at 2 bp", "required": "> 0", "actual": cost_2x["full_return"], "pass": line4},
        {"line": "5 cross-market", "required": "not applicable", "actual": "not scored", "pass": None},
        {"line": "6 OOS round trips", "required": ">= 24", "actual": oos["trades"], "pass": line6},
    ]

    results = {
        "rules_sha256": rules_hash,
        "locked_utc": locked_utc,
        "schema_version": schema,
        "action_counts": action_counts,
        "book_sessions": len(book),
        "first_fill": primary["first_fill"].isoformat(),
        "last_session": dates[-1].isoformat(),
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOTSTRAP,
            "timing": SEED_TIMING,
            "verify": 20261054,
        },
        "windows": windows,
        "by_year": by_year,
        "by_side": {"long": _side_stats(primary["trips"], "long"), "short": _side_stats(primary["trips"], "short")},
        "by_exit": exits,
        "by_fund": {
            sym: {
                "gross_dollars": fund_gross[sym],
                "net_dollars": fund_net[sym],
                "trips": sum(1 for t in primary["trips"] if t.sym == sym),
            }
            for sym in FUNDS
        },
        "predictions": {
            "same_sign_gross": {
                "tlt_gross": fund_gross["TLT"],
                "ief_gross": fund_gross["IEF"],
                "same_sign": same_sign,
                "score": "consistent" if same_sign else "not consistent",
            },
            "crisis_convexity": {
                "q1_mean": q1,
                "q3_mean": q3,
                "q1_above_q3": bool(convex),
                "score": "consistent" if convex else "not consistent",
            },
        },
        "monthly_quintiles": monthly_q,
        "daily_quintiles": daily_q,
        "april_2025": april_stats,
        "signal_counts": signal_counts,
        "direction_placebo": direction,
        "timing_placebo": timing,
        "bootstrap": boot,
        "grid": grid_summary,
        "costs": costs,
        "fill_delay": delay,
        "same_close_upper_bound": same_close,
        "acceptance": acceptance,
        "status": status,
        "entry_sessions": len({t.entry_date for t in primary["trips"]}),
        "avg_winner_over_avg_loser_bp": (
            None if full["avg_winner_bp"] is None or full["avg_loser_bp"] in (None, 0.0)
            else full["avg_winner_bp"] / abs(full["avg_loser_bp"])
        ),
    }
    results = _clean(results)

    daily_path = HERE / "daily.csv"
    with daily_path.open("w", newline="\n", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["date", "strategy_net", "strategy_gross", "benchmark", "spy_c2c"])
        for d, g, b in zip(primary["days"], gross["days"], bench["days"]):
            w.writerow([
                d["date"].isoformat(),
                f"{d['ret']:.12g}",
                f"{g['ret']:.12g}",
                f"{b['ret']:.12g}",
                f"{spy_ret[d['date']]:.12g}",
            ])
    trades_path = HERE / "trades.csv"
    with trades_path.open("w", newline="\n", encoding="utf-8") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow([
            "symbol", "side", "entry_date", "entry_px", "exit_date", "exit_px",
            "exit_reason", "gross_dollars", "net_dollars", "entry_equity", "hold_sessions",
        ])
        for t in primary["trips"]:
            w.writerow([
                t.sym, t.side, t.entry_date.isoformat(), f"{t.entry_px:.12g}",
                t.exit_date.isoformat(), f"{t.exit_px:.12g}", t.reason,
                f"{t.gross:.12g}", f"{t.net:.12g}", f"{t.entry_equity:.12g}", t.hold,
            ])
    (HERE / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8", newline="\n")

    head, dirty = _git()
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nAppend-only. One entry per store run.\n\n", encoding="utf-8", newline="\n")
    entry = (
        f"## {now}\n"
        f"- rules_sha256 {rules_hash}\n"
        f"- git_head {head} dirty={dirty}\n"
        f"- reason: {reason}\n"
        f"- full Sharpe {full['sharpe']} return {full['total_return']} | "
        f"IS Sharpe {is_['sharpe']} | "
        f"OOS Sharpe {oos['sharpe']} return {oos['total_return']} | "
        f"trades full/IS/OOS {full['trades']}/{is_['trades']}/{oos['trades']} | "
        f"status {status}\n"
    )
    with log.open("a", encoding="utf-8", newline="\n") as f:
        f.write(entry)
    print(entry)


def main():
    rules_hash, locked_utc = rules_hash_check()
    self_test()
    reason = " ".join(sys.argv[1:]).strip() or "initial pre-registered run"
    run_store(rules_hash, locked_utc, reason)


if __name__ == "__main__":
    main()
