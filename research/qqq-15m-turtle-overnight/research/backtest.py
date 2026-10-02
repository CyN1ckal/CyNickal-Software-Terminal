# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""QQQ 15-minute Turtle breakout held overnight, measured against RULES.md.

Run from the repo root:

    python research/qqq-15m-turtle-overnight/research/backtest.py --reason "initial"
    python research/qqq-15m-turtle-overnight/research/backtest.py self-test

The script refuses to run if RULES.md no longer matches RULES.lock. The store
is not opened until the synthetic self-test passes. A store run appends one
entry to RUNLOG.md.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, replace
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np
from numpy.lib.stride_tricks import sliding_window_view

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, ny_datetime, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent

# ---- constants mirrored from RULES.md -------------------------------------
N_IN = 55
N_OUT = 20
N_ATR = 20
STOP_MULT = 2.0
COST_BPS = {"QQQ": 1.0, "SPY": 1.0, "IGV": 2.0}
WARMUP = 20
PRIMARY = "QQQ"
CROSS = ("SPY", "IGV")
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
LAST_SESSION = date(2026, 9, 25)
MISSING_OK = {date(2021, 12, 31)}
FULL_BARS, EARLY_BARS = 26, 15
ANNUAL = 252
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
GRID_IN = (27, 40, 55, 80, 110)
GRID_OUT = (10, 20, 30)
PLACEBO_DRAWS = 2000
BOOT_DRAWS = 2000
BOOT_BLOCK = 20
SEED_DIRECTION = 20260926
SEED_BOOTSTRAP = 20260927
DIV_YIELD = {"QQQ": 0.0014, "SPY": 0.0032, "IGV": 0.0}
ACCEPT = {"oos_sharpe": 0.5, "oos_pf": 1.10, "placebo_p": 0.05, "grid_share": 0.60, "min_oos_trades": 100}


@dataclass(frozen=True)
class Params:
    n_in: int = N_IN
    n_out: int = N_OUT
    n_atr: int = N_ATR
    stop_mult: float = STOP_MULT
    cost_bps: float = 1.0
    delay: int = 1          # 1 = next bar open, 2 = open of bar i+2, 0 = signal close (upper bound)
    forced_flat: bool = False


@dataclass
class Series:
    ts: list
    session: list
    o: np.ndarray
    h: np.ndarray
    l: np.ndarray
    c: np.ndarray

    def __post_init__(self) -> None:
        n = len(self.ts)
        self.last = [i == n - 1 or self.session[i + 1] != self.session[i] for i in range(n)]
        self.first = [i == 0 or self.session[i - 1] != self.session[i] for i in range(n)]
        self.days = []                     # sessions with bars, in order
        self.day_index = {}
        self.first_open = []
        self.last_close = []
        for i in range(n):
            if self.first[i]:
                self.day_index[self.session[i]] = len(self.days)
                self.days.append(self.session[i])
                self.first_open.append(float(self.o[i]))
                self.last_close.append(float("nan"))
            if self.last[i]:
                self.last_close[-1] = float(self.c[i])


# ---- rules integrity --------------------------------------------------------
def rules_hash() -> str:
    return hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def check_lock() -> str:
    h = rules_hash()
    locked = dict(line.split(" ", 1) for line in (HERE / "RULES.lock").read_text().splitlines() if line.strip())
    if locked.get("sha256") != h:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {locked.get('sha256')}. Refusing to run.")
    return h


# ---- engine -------------------------------------------------------------------
def indicators(s: Series, p: Params):
    n = len(s.ts)
    tr = np.empty(n)
    tr[0] = s.h[0] - s.l[0]
    if n > 1:
        pc = s.c[:-1]
        tr[1:] = np.maximum.reduce([s.h[1:] - s.l[1:], np.abs(s.h[1:] - pc), np.abs(s.l[1:] - pc)])
    atr = np.full(n, np.nan)
    if n >= p.n_atr:
        atr[p.n_atr - 1] = tr[:p.n_atr].mean()
        for i in range(p.n_atr, n):
            atr[i] = ((p.n_atr - 1) * atr[i - 1] + tr[i]) / p.n_atr

    def chan(x: np.ndarray, w: int, fn) -> np.ndarray:
        out = np.full(n, np.nan)
        if n > w:
            out[w:] = fn(sliding_window_view(x, w), axis=1)[: n - w]
        return out

    return (atr, chan(s.h, p.n_in, np.max), chan(s.l, p.n_in, np.min),
            chan(s.h, p.n_out, np.max), chan(s.l, p.n_out, np.min))


def simulate(s: Series, p: Params, eval_start: int) -> dict:
    """Run the locked rule. Returns trades, per-session equity marks, and held flags."""
    n = len(s.ts)
    atr, hi_in, lo_in, hi_out, lo_out = indicators(s, p)
    cost = p.cost_bps / 10_000.0
    st = {"pos": 0, "q": 0.0, "cash": 1.0, "cur": None}
    trades: list[dict] = []
    marks: dict = {}
    held = np.zeros(n, dtype=bool)

    def open_pos(side: int, px: float, i: int, at: str, n_sig: float) -> None:
        notional = st["cash"]                      # flat, so equity == cash
        st["cash"] -= cost * notional
        st["q"] = side * notional / px
        st["cash"] -= st["q"] * px
        st["pos"] = side
        st["cur"] = {"side": side, "entry_i": i, "entry_px": px, "entry_at": at,
                     "stop": px - side * p.stop_mult * n_sig, "n_sig": n_sig}

    def close_pos(px: float, i: int, at: str, reason: str, reversal: bool) -> None:
        q = st["q"]
        st["cash"] += q * px - cost * abs(q) * px
        st["q"] = 0.0
        st["pos"] = 0
        t = dict(st["cur"])
        t.update(exit_i=i, exit_px=px, exit_at=at, reason=reason, reversal=reversal)
        trades.append(t)
        st["cur"] = None

    def execute(order: dict, px: float, i: int, at: str, allow_open: bool = True) -> None:
        if order["kind"] == "enter":
            open_pos(order["side"], px, i, at, order["n_sig"])
        else:
            rev = order["reverse"] != 0 and allow_open
            close_pos(px, i, at, order["reason"], rev)
            if rev:
                open_pos(order["reverse"], px, i, at, order["n_sig"])

    def decide(i: int):
        pos, c = st["pos"], s.c[i]
        if pos == 0:
            if np.isnan(hi_in[i]) or np.isnan(atr[i]):
                return None
            if c > hi_in[i]:
                return {"kind": "enter", "side": 1, "n_sig": atr[i]}
            if c < lo_in[i]:
                return {"kind": "enter", "side": -1, "n_sig": atr[i]}
            return None
        stop = st["cur"]["stop"]
        if pos == 1:
            hit_stop = c <= stop
            hit_chan = c < lo_out[i]
            rev = -1 if c < lo_in[i] else 0
        else:
            hit_stop = c >= stop
            hit_chan = c > hi_out[i]
            rev = 1 if c > hi_in[i] else 0
        if not (hit_stop or hit_chan):
            return None
        return {"kind": "exit", "reason": "stop" if hit_stop else "channel", "reverse": rev, "n_sig": atr[i]}

    pending = None
    for i in range(n):
        if pending is not None and pending["fill_i"] == i:
            execute(pending, float(s.o[i]), i, "open")
            pending = None
        if i >= eval_start:
            if p.forced_flat and s.last[i]:
                if pending is None and st["pos"] != 0:
                    close_pos(float(s.c[i]), i, "close", "eod", False)
            elif pending is None:
                order = decide(i)
                if order is not None:
                    if p.delay == 0:
                        execute(order, float(s.c[i]), i, "close")
                    else:
                        order["fill_i"] = i + p.delay
                        pending = order
        held[i] = st["pos"] != 0
        if s.last[i]:
            marks[s.session[i]] = st["cash"] + st["q"] * float(s.c[i])

    last_i = n - 1
    if pending is not None and pending["kind"] == "exit" and st["pos"] != 0:
        execute(pending, float(s.c[last_i]), last_i, "close", allow_open=False)
    if st["pos"] != 0:
        close_pos(float(s.c[last_i]), last_i, "close", "end", False)
    held[last_i] = False
    marks[s.session[last_i]] = st["cash"]
    return {"trades": trades, "marks": marks, "held": held}


def eval_start_index(s: Series, warmup: int) -> int:
    first_eval = s.days[warmup]
    return next(i for i in range(len(s.ts)) if s.session[i] == first_eval)


def daily_returns(marks: dict, sessions: list) -> np.ndarray:
    prev = 1.0
    out = np.zeros(len(sessions))
    for k, d in enumerate(sessions):
        if d in marks:
            out[k] = marks[d] / prev - 1.0
            prev = marks[d]
    return out


def trade_returns(t: dict, cost_bps: float) -> tuple[float, float]:
    gross = t["side"] * (t["exit_px"] / t["entry_px"] - 1.0)
    return gross, gross - 2.0 * cost_bps / 10_000.0


def contributions(s: Series, t: dict) -> tuple[list, list]:
    """Fixed-notional gross contribution per session with bars, and its overnight part."""
    a = s.day_index[s.session[t["entry_i"]]]
    b = s.day_index[s.session[t["exit_i"]]]
    side, e = t["side"], t["entry_px"]
    rows, nights = [], []
    for k in range(a, b + 1):
        p0 = t["entry_px"] if k == a else s.last_close[k - 1]
        p1 = t["exit_px"] if k == b else s.last_close[k]
        rows.append((s.days[k], side * (p1 - p0) / e))
        if k > a:
            nights.append((s.days[k], side * (s.first_open[k] - s.last_close[k - 1]) / e))
    return rows, nights


# ---- statistics ---------------------------------------------------------------
def sharpe(r: np.ndarray) -> float:
    if len(r) < 2:
        return float("nan")
    sd = r.std(ddof=1)
    return float(r.mean() / sd * math.sqrt(ANNUAL)) if sd > 0 else float("nan")


def max_drawdown(r: np.ndarray) -> float:
    eq = np.cumprod(1.0 + r)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return float((eq / peak - 1.0).min()) if len(r) else 0.0


def metrics(r: np.ndarray, tr: list[dict], held_bars: tuple[int, int] | None = None,
            held_nights: tuple[int, int] | None = None) -> dict:
    total = float(np.prod(1.0 + r) - 1.0)
    n = len(r)
    nets = np.array([t["net"] for t in tr]) if tr else np.zeros(0)
    wins, losses = nets[nets > 0].sum(), nets[nets < 0].sum()
    sd = r.std(ddof=1) if n > 1 else float("nan")
    out = {
        "sessions": n,
        "sharpe": sharpe(r),
        "total_return": total,
        "cagr": float((1.0 + total) ** (ANNUAL / n) - 1.0) if n else float("nan"),
        "ann_vol": float(sd * math.sqrt(ANNUAL)),
        "max_dd": max_drawdown(r),
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if sd > 0 else float("nan"),
        "trades": len(tr),
        "win_rate": float((nets > 0).mean()) if len(nets) else float("nan"),
        "profit_factor": float(wins / abs(losses)) if losses < 0 else float("nan"),
        "avg_net_bp": float(nets.mean() * 1e4) if len(nets) else float("nan"),
        "avg_gross_bp": float(np.mean([t["gross"] for t in tr]) * 1e4) if tr else float("nan"),
    }
    if held_bars is not None:
        out["exposure_bars"] = held_bars[0] / held_bars[1] if held_bars[1] else float("nan")
    if held_nights is not None:
        out["overnights_held_share"] = held_nights[0] / held_nights[1] if held_nights[1] else float("nan")
    if tr:
        out["median_bars_held"] = float(np.median([t["bars_held"] for t in tr]))
        out["mean_bars_held"] = float(np.mean([t["bars_held"] for t in tr]))
        out["median_overnights"] = float(np.median([t["overnights"] for t in tr]))
        out["mean_overnights"] = float(np.mean([t["overnights"] for t in tr]))
    return out


def sample_of(d: date) -> str:
    return "IS" if d <= IS_END else "OOS"


# ---- self-test ----------------------------------------------------------------
def _bar(c, o=None, h=None, l=None):
    o = c if o is None else o
    return (o, c + 0.5 if h is None else h, c - 0.5 if l is None else l, c)


def _series(sessions: list[list[tuple]]) -> Series:
    ts, sess, o, h, l, c = [], [], [], [], [], []
    for k, bars in enumerate(sessions):
        d = date(2030, 1, 1) + timedelta(days=k)
        for j, (bo, bh, bl, bc) in enumerate(bars):
            ts.append(k * 100_000 + j * 900)
            sess.append(d)
            o.append(bo), h.append(bh), l.append(bl), c.append(bc)
    return Series(ts, sess, np.array(o, float), np.array(h, float), np.array(l, float), np.array(c, float))


def _summ(res: dict) -> list[tuple]:
    return [(t["side"], t["entry_i"], round(t["entry_px"], 6), t["entry_at"], t["exit_i"],
             round(t["exit_px"], 6), t["exit_at"], t["reason"], t["reversal"]) for t in res["trades"]]


def self_test() -> None:
    small = Params(n_in=3, n_out=2, n_atr=2, stop_mult=100.0, cost_bps=0.0)
    flat4 = [_bar(100)] * 4
    failures = []

    def check(name, got, want):
        if got != want:
            failures.append(f"{name}: got {got}, want {want}")

    # A: long entry, channel exit, position carried through a close.
    a = _series([flat4,
                 [_bar(100), _bar(101), _bar(101.5, o=101.2, h=102, l=101), _bar(102)],
                 [_bar(100.8, o=101, h=101.2, l=100.3), _bar(100.7, o=100.6), _bar(100.7), _bar(100.7)]])
    ea = eval_start_index(a, 1)
    ra = simulate(a, small, ea)
    check("A long/channel", _summ(ra), [(1, 6, 101.2, "open", 9, 100.6, "open", "channel", False)])
    check("A held overnight", bool(ra["held"][7]), True)

    # B: short signal on a session's last bar fills at the next open; reversal; end-of-data close.
    b = _series([flat4,
                 [_bar(100), _bar(100), _bar(100), _bar(99)],
                 [_bar(98.8, o=98.5, h=99, l=98.2), _bar(101.5, o=98.8, h=102, l=98.8),
                  _bar(101.8, o=101.6, h=102.1, l=101.4), _bar(102)]])
    eb = eval_start_index(b, 1)
    rb = simulate(b, small, eb)
    check("B overnight fill/reversal/end", _summ(rb),
          [(-1, 8, 98.5, "open", 10, 101.6, "open", "channel", True),
           (1, 10, 101.6, "open", 11, 102.0, "close", "end", False)])

    # C: 2N stop, stop precedence over channel, overnight gap through the stop, reversal twice.
    cp = Params(n_in=3, n_out=2, n_atr=2, stop_mult=2.0, cost_bps=0.0)
    c = _series([flat4,
                 [_bar(100), _bar(101, o=100.5, h=101.5, l=100.5), _bar(101.2, o=101, h=101.5, l=100.8),
                  _bar(101.4, o=101.2, h=101.6, l=101.1)],
                 [_bar(98.2, o=98, h=98.5, l=97.8), _bar(98.0, o=98.1, h=98.3, l=97.7),
                  _bar(97.5, o=97.9, h=98.0, l=97.3), _bar(97)],
                 [_bar(102.5, o=103, h=103.2, l=102.3), _bar(102.7, o=102.6, h=102.9, l=102.4)]])
    ec = eval_start_index(c, 1)
    rc = simulate(c, cp, ec)
    check("C stops/reversals", _summ(rc),
          [(1, 6, 101.0, "open", 9, 98.1, "open", "stop", True),
           (-1, 9, 98.1, "open", 13, 102.6, "open", "stop", True),
           (1, 13, 102.6, "open", 13, 102.7, "close", "end", False)])
    if len(rc["trades"]) == 3:
        check("C long stop level", round(rc["trades"][0]["stop"], 9), 98.5)
        check("C short stop level", round(rc["trades"][1]["stop"], 9), round(98.1 + 2 * 2.16875, 9))
        check("C last stop level", round(rc["trades"][2]["stop"], 9), round(102.6 - 2 * 3.610546875, 9))

    # D: entry signal on the last bar of the data is cancelled; exit on the last bar fills at its close.
    d1 = _series([flat4, [_bar(100), _bar(100), _bar(100), _bar(102)]])
    check("D cancelled entry", _summ(simulate(d1, small, eval_start_index(d1, 1))), [])
    d2 = _series([flat4, [_bar(100), _bar(101), _bar(101.5, o=101.2, h=102, l=101), _bar(99)]])
    check("D exit on last bar", _summ(simulate(d2, small, eval_start_index(d2, 1))),
          [(1, 6, 101.2, "open", 7, 99.0, "close", "channel", False)])

    # E: a short session (missing buckets or early close): a signal on its last bar fills at the next open.
    e = _series([flat4, [_bar(100), _bar(101)], [_bar(101.3, o=101.4), _bar(101.6), _bar(101.8)]])
    check("E short session", _summ(simulate(e, small, eval_start_index(e, 1))),
          [(1, 6, 101.4, "open", 8, 101.8, "close", "end", False)])

    # F: breakouts during warm-up never trade; the first evaluation close can.
    f = _series([[_bar(100), _bar(100), _bar(100), _bar(102), _bar(104)],
                 [_bar(106), _bar(106.2, o=106.1), _bar(106.3)]])
    check("F warm-up", _summ(simulate(f, small, eval_start_index(f, 1))),
          [(1, 6, 106.1, "open", 7, 106.3, "close", "end", False)])

    # G: cost accounting on a reversal (case B at 1 bp per side).
    rg = simulate(b, replace(small, cost_bps=1.0), eb)
    cst = 1e-4
    eq1 = 1.0 - cst - (1.0 / 98.5) * (101.6 - 98.5) - cst * (1.0 / 98.5) * 101.6
    q2 = eq1 / 101.6
    eq2 = eq1 - cst * eq1 + q2 * (102.0 - 101.6) - cst * q2 * 102.0
    check("G final equity", round(rg["marks"][b.session[-1]], 12), round(eq2, 12))
    check("G session-1 mark", round(rg["marks"][b.session[7]], 12), 1.0)
    g_ret = daily_returns(rg["marks"], [b.session[4], b.session[8]])
    check("G daily", [round(x, 12) for x in g_ret], [0.0, round(eq2 - 1.0, 12)])

    # H: forced-flat variant closes at the last close and drops last-bar decisions.
    ff = replace(small, forced_flat=True)
    check("H forced flat A", _summ(simulate(a, ff, ea)), [(1, 6, 101.2, "open", 7, 102.0, "close", "eod", False)])
    check("H forced flat B", _summ(simulate(b, ff, eb)), [(1, 10, 101.6, "open", 11, 102.0, "close", "eod", False)])

    # I: one-bar-later fills and the signal-close upper bound.
    check("I delay 2", _summ(simulate(a, replace(small, delay=2), ea)),
          [(1, 7, 102.0, "open", 10, 100.7, "open", "channel", False)])
    check("I close fill", _summ(simulate(a, replace(small, delay=0), ea)),
          [(1, 5, 101.0, "close", 8, 100.8, "close", "channel", False)])

    # J: contributions sum to the gross return, and the overnight part is the gap.
    for t in rb["trades"]:
        t["bars_held"] = t["exit_i"] - t["entry_i"]
    rows, nights = contributions(b, rb["trades"][0])
    check("J sum", round(sum(x for _, x in rows), 12), round(-(101.6 / 98.5 - 1.0), 12))
    rows_a, nights_a = contributions(a, ra["trades"][0])
    check("J overnight gap", [round(x, 12) for _, x in nights_a], [round((101 - 102) / 101.2, 12)])
    check("J sum A", round(sum(x for _, x in rows_a), 12), round(100.6 / 101.2 - 1.0, 12))

    if failures:
        print("SELF-TEST FAILED")
        for f_ in failures:
            print("  " + f_)
        sys.exit(1)
    print("self-test passed (10 case groups)")


# ---- store run ---------------------------------------------------------------
def load_series(md: MarketData, sym: str) -> Series:
    bars = md.bars(sym, "15m", end=LAST_SESSION)
    return Series([b.ts for b in bars], [b.session for b in bars],
                  np.array([b.open for b in bars]), np.array([b.high for b in bars]),
                  np.array([b.low for b in bars]), np.array([b.close for b in bars]))


def data_checks(sym: str, s: Series) -> None:
    counts = defaultdict(int)
    for d in s.session:
        counts[d] += 1
    bad = [(d, n) for d, n in counts.items() if n != (EARLY_BARS if d in EARLY_CLOSES else FULL_BARS)]
    if bad:
        sys.exit(f"{sym}: unexpected 15m bar counts {bad[:10]}")
    cal = set(nyse_sessions(s.days[0], s.days[-1]))
    missing = sorted(cal - set(s.days))
    if set(missing) != MISSING_OK:
        sys.exit(f"{sym}: sessions without bars {missing}")
    if any(b <= a for a, b in zip(s.ts, s.ts[1:])):
        sys.exit(f"{sym}: timestamps not strictly increasing")


def enrich(s: Series, trades: list[dict], cost_bps: float) -> None:
    for t in trades:
        t["gross"], t["net"] = trade_returns(t, cost_bps)
        t["bars_held"] = t["exit_i"] - t["entry_i"]
        t["overnights"] = s.day_index[s.session[t["exit_i"]]] - s.day_index[s.session[t["entry_i"]]]
        t["entry_session"] = s.session[t["entry_i"]]
        t["sample"] = sample_of(t["entry_session"])


class Run:
    """One simulated configuration on one symbol, with derived series."""

    def __init__(self, s: Series, p: Params, eval_start: int, sessions: list):
        self.s, self.p = s, p
        res = simulate(s, p, eval_start)
        self.trades, self.marks, self.held = res["trades"], res["marks"], res["held"]
        enrich(s, self.trades, p.cost_bps)
        self.sessions = sessions
        self.ret = daily_returns(self.marks, sessions)
        self.pos_of = {d: k for k, d in enumerate(sessions)}
        self.eval_start = eval_start

    def gross_fixed(self, signs: np.ndarray | None = None) -> np.ndarray:
        g = np.zeros(len(self.sessions))
        for k, t in enumerate(self.trades):
            sg = 1.0 if signs is None else signs[k]
            for d, x in contributions(self.s, t)[0]:
                g[self.pos_of[d]] += sg * x
        return g

    def contrib_matrix(self) -> np.ndarray:
        m = np.zeros((len(self.trades), len(self.sessions)))
        for k, t in enumerate(self.trades):
            for d, x in contributions(self.s, t)[0]:
                m[k, self.pos_of[d]] += x
        return m

    def mask(self, which: str) -> np.ndarray:
        if which == "full":
            return np.ones(len(self.sessions), bool)
        return np.array([sample_of(d) == which for d in self.sessions])

    def summary(self, which: str) -> dict:
        m = self.mask(which)
        tr = [t for t in self.trades if which == "full" or t["sample"] == which]
        s = self.s
        in_s = [i for i in range(self.eval_start, len(s.ts)) if which == "full" or sample_of(s.session[i]) == which]
        nights = [i for i in in_s if s.last[i] and i != len(s.ts) - 1]
        return metrics(self.ret[m], tr, (int(self.held[in_s].sum()), len(in_s)),
                       (int(self.held[nights].sum()), len(nights)))


def fmt_time(ts: int) -> str:
    return ny_datetime(ts).strftime("%Y-%m-%d %H:%M")


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=ROOT).stdout.strip()


def third_friday(y: int, m: int) -> date:
    d = date(y, m, 1)
    d += timedelta(days=(4 - d.weekday()) % 7)
    return d + timedelta(days=14)


def next_session(d: date, cal: set) -> date:
    while d not in cal:
        d += timedelta(days=1)
    return d


def dividend_estimate(run: Run, sym: str) -> dict:
    y = DIV_YIELD[sym]
    cal = set(run.sessions)
    s = run.s
    # side held across each session's open = the position at the previous session-with-bars' last close
    last_idx = {s.session[i]: i for i in range(len(s.ts)) if s.last[i]}
    pos_at_close = {}
    for t in run.trades:
        for i in range(t["entry_i"], t["exit_i"]):
            if s.last[i]:
                pos_at_close[s.session[i]] = t["side"]
    events = []
    for yr in range(run.sessions[0].year, run.sessions[-1].year + 1):
        for mth in (3, 6, 9, 12):
            tf = third_friday(yr, mth)
            ex = tf if sym != "QQQ" else tf + timedelta(days=3)
            if ex < run.sessions[0] or ex > run.sessions[-1]:
                continue
            ex = next_session(ex, cal)
            k = s.day_index.get(ex)
            if k is None or k == 0:
                continue
            prev = s.days[k - 1]
            side = pos_at_close.get(prev, 0) if prev in last_idx else 0
            events.append({"ex_session": ex.isoformat(), "side_held": side, "correction": side * y})
    growth = float(np.prod(1.0 + run.ret))
    corr = float(np.prod([1.0 + e["correction"] for e in events])) if events else 1.0
    return {"assumed_yield": y, "events": events,
            "sum_correction": float(sum(e["correction"] for e in events)),
            "total_return_price_only": growth - 1.0, "total_return_approx_corrected": growth * corr - 1.0,
            "held_long": sum(1 for e in events if e["side_held"] == 1),
            "held_short": sum(1 for e in events if e["side_held"] == -1)}


def quintile_table(x: np.ndarray, y: np.ndarray) -> list[dict]:
    order = np.argsort(x, kind="stable")
    out = []
    for q, idx in enumerate(np.array_split(order, 5)):
        out.append({"quintile": q + 1, "n": int(len(idx)), "x_lo": float(x[idx].min()), "x_hi": float(x[idx].max()),
                    "mean_strategy": float(y[idx].mean()), "share_positive": float((y[idx] > 0).mean())})
    return out


def store_run(reason: str, rules_sha: str) -> None:
    with MarketData() as md:
        series = {sym: load_series(md, sym) for sym in (PRIMARY, *CROSS)}
    for sym, s in series.items():
        data_checks(sym, s)
    base = series[PRIMARY]
    first_eval = base.days[WARMUP]
    sessions = nyse_sessions(first_eval, LAST_SESSION)
    starts = {sym: eval_start_index(s, WARMUP) for sym, s in series.items()}
    for sym, s in series.items():
        if s.days[WARMUP] != first_eval:
            sys.exit(f"{sym}: first evaluation session {s.days[WARMUP]} differs from {first_eval}")

    def run(sym: str, **kw) -> Run:
        p = Params(cost_bps=COST_BPS[sym], **kw)
        return Run(series[sym], p, starts[sym], sessions)

    prim = run(PRIMARY)
    results: dict = {"rules_sha256": rules_sha, "reason": reason,
                     "seeds": {"direction": SEED_DIRECTION, "bootstrap": SEED_BOOTSTRAP},
                     "params": {"n_in": N_IN, "n_out": N_OUT, "n_atr": N_ATR, "stop_mult": STOP_MULT,
                                "cost_bps": COST_BPS, "warmup_sessions": WARMUP},
                     "window": {"first_eval": first_eval.isoformat(), "last": LAST_SESSION.isoformat(),
                                "is_end": IS_END.isoformat(), "oos_start": OOS_START.isoformat(),
                                "sessions_full": len(sessions),
                                "sessions_is": int(prim.mask("IS").sum()), "sessions_oos": int(prim.mask("OOS").sum())}}

    # benchmark: price-only close to close
    bench = np.zeros(len(sessions))
    for k, d in enumerate(sessions):
        j = base.day_index.get(d)
        if j is not None and j > 0:
            bench[k] = base.last_close[j] / base.last_close[j - 1] - 1.0
    results["benchmark"] = {w: metrics(bench[prim.mask(w)], []) for w in ("full", "IS", "OOS")}
    results["primary"] = {w: prim.summary(w) for w in ("full", "IS", "OOS")}

    # cross-market
    cross = {sym: run(sym) for sym in CROSS}
    results["cross_market"] = {sym: {w: r.summary(w) for w in ("full", "IS", "OOS")} for sym, r in cross.items()}
    corr = {}
    for sym, r in cross.items():
        corr[f"{PRIMARY}-{sym}"] = float(np.corrcoef(prim.ret, r.ret)[0, 1])
    corr["SPY-IGV"] = float(np.corrcoef(cross["SPY"].ret, cross["IGV"].ret)[0, 1])
    corr["QQQ-benchmark"] = float(np.corrcoef(prim.ret, bench)[0, 1])
    results["correlations"] = corr

    # costs
    results["cost_sweep"] = []
    for mult in COST_MULTS:
        r = Run(base, Params(cost_bps=COST_BPS[PRIMARY] * mult), starts[PRIMARY], sessions)
        results["cost_sweep"].append({"cost_bps": COST_BPS[PRIMARY] * mult,
                                      **{w: {k: r.summary(w)[k] for k in ("sharpe", "total_return", "profit_factor")}
                                         for w in ("full", "IS", "OOS")}})
    cost2 = next(c for c in results["cost_sweep"] if c["cost_bps"] == 2 * COST_BPS[PRIMARY])

    # fills
    fills = {}
    for name, dl in (("next_open", 1), ("delay_plus_one_bar", 2), ("upper_bound_signal_close", 0)):
        r = run(PRIMARY, delay=dl)
        fills[name] = {w: {k: r.summary(w)[k] for k in ("sharpe", "total_return", "profit_factor", "trades")}
                       for w in ("full", "IS", "OOS")}
    results["fills"] = fills

    # placebo on fixed-notional gross returns
    mat = prim.contrib_matrix()
    g = mat.sum(axis=0)
    actual_gross = sharpe(g)
    rng = np.random.default_rng(SEED_DIRECTION)
    signs = rng.choice([-1.0, 1.0], size=(PLACEBO_DRAWS, mat.shape[0]))
    draws = signs @ mat
    mu, sd = draws.mean(axis=1), draws.std(axis=1, ddof=1)
    placebo_sh = mu / sd * math.sqrt(ANNUAL)
    p_val = (1 + int((placebo_sh >= actual_gross).sum())) / (PLACEBO_DRAWS + 1)
    results["placebo"] = {"actual_gross_sharpe_fixed_notional": actual_gross, "p": p_val,
                          "draws_mean": float(placebo_sh.mean()), "draws_p95": float(np.percentile(placebo_sh, 95)),
                          "draws_max": float(placebo_sh.max()), "n": PLACEBO_DRAWS,
                          "hist": np.histogram(placebo_sh, bins=40)[0].tolist(),
                          "hist_edges": np.histogram(placebo_sh, bins=40)[1].tolist()}
    results["gross_fixed_notional"] = {w: {"sharpe": sharpe(g[prim.mask(w)]), "sum": float(g[prim.mask(w)].sum())}
                                       for w in ("full", "IS", "OOS")}

    # bootstrap
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    n = len(prim.ret)
    nb = math.ceil(n / BOOT_BLOCK)
    boots = np.empty(BOOT_DRAWS)
    for k in range(BOOT_DRAWS):
        st = rng.integers(0, n, nb)
        idx = ((st[:, None] + np.arange(BOOT_BLOCK)[None, :]) % n).ravel()[:n]
        boots[k] = sharpe(prim.ret[idx])
    results["bootstrap"] = {"lo": float(np.percentile(boots, 2.5)), "hi": float(np.percentile(boots, 97.5)),
                            "share_le_0": float((boots <= 0).mean()), "draws": BOOT_DRAWS, "block": BOOT_BLOCK}

    # grid
    grid = []
    for ni in GRID_IN:
        for no in GRID_OUT:
            r = run(PRIMARY, n_in=ni, n_out=no)
            grid.append({"n_in": ni, "n_out": no, "is_sharpe": r.summary("IS")["sharpe"],
                         "oos_sharpe": r.summary("OOS")["sharpe"], "full_sharpe": r.summary("full")["sharpe"],
                         "is_trades": r.summary("IS")["trades"], "oos_trades": r.summary("OOS")["trades"]})
    pos_share = sum(1 for c in grid if not math.isnan(c["is_sharpe"]) and c["is_sharpe"] > 0) / len(grid)
    results["grid"] = {"cells": grid, "is_share_positive": pos_share,
                       "oos_share_positive": sum(1 for c in grid if c["oos_sharpe"] > 0) / len(grid)}

    # forced-flat variant
    flat = run(PRIMARY, forced_flat=True)
    gf = flat.gross_fixed()
    results["forced_flat"] = {w: flat.summary(w) for w in ("full", "IS", "OOS")}
    results["forced_flat"]["gross_fixed_sharpe_full"] = sharpe(gf)

    # decomposition overnight vs intraday
    decomp = {}
    for side in (1, -1):
        on, tot, cnt = [], 0.0, 0
        for t in prim.trades:
            if t["side"] != side:
                continue
            rows, nights = contributions(base, t)
            tot += sum(x for _, x in rows)
            on.extend(x for _, x in nights)
            cnt += 1
        decomp["long" if side == 1 else "short"] = {
            "trades": cnt, "gross_total": tot, "overnight_total": float(sum(on)),
            "intraday_total": tot - float(sum(on)), "overnights": len(on),
            "overnight_mean_bp": float(np.mean(on) * 1e4) if on else float("nan"),
            "overnight_t": float(np.mean(on) / (np.std(on, ddof=1) / math.sqrt(len(on)))) if len(on) > 1 else float("nan")}
    all_on = decomp["long"]["overnight_total"] + decomp["short"]["overnight_total"]
    decomp["all"] = {"gross_total": decomp["long"]["gross_total"] + decomp["short"]["gross_total"],
                     "overnight_total": all_on,
                     "intraday_total": decomp["long"]["intraday_total"] + decomp["short"]["intraday_total"],
                     "overnights": decomp["long"]["overnights"] + decomp["short"]["overnights"]}
    results["decomposition"] = decomp

    # breakdowns
    by_year = {}
    for yr in sorted({d.year for d in sessions}):
        m = np.array([d.year == yr for d in sessions])
        tr = [t for t in prim.trades if t["entry_session"].year == yr]
        by_year[str(yr)] = {"return": float(np.prod(1 + prim.ret[m]) - 1), "sharpe": sharpe(prim.ret[m]),
                            "max_dd": max_drawdown(prim.ret[m]), "trades": len(tr),
                            "benchmark": float(np.prod(1 + bench[m]) - 1)}
    results["by_year"] = by_year

    def group(key) -> dict:
        out = defaultdict(list)
        for t in prim.trades:
            out[key(t)].append(t)
        res = {}
        for k in sorted(out):
            tr = out[k]
            nets = np.array([t["net"] for t in tr])
            w, lo = nets[nets > 0].sum(), nets[nets < 0].sum()
            res[str(k)] = {"trades": len(tr), "avg_net_bp": float(nets.mean() * 1e4),
                           "avg_gross_bp": float(np.mean([t["gross"] for t in tr]) * 1e4),
                           "sum_net": float(nets.sum()), "win_rate": float((nets > 0).mean()),
                           "profit_factor": float(w / abs(lo)) if lo < 0 else float("nan")}
        return res

    def entry_bucket(t):
        i = t["entry_i"]
        if t["entry_at"] == "open" and base.first[i]:
            return "0930_session_open"
        return ny_datetime(base.ts[i]).strftime("%Hh")

    def night_bucket(t):
        n_ = t["overnights"]
        return "0" if n_ == 0 else "1" if n_ == 1 else "2" if n_ == 2 else "3-5" if n_ <= 5 else "6+"

    results["by_side"] = {"all": group(lambda t: "long" if t["side"] == 1 else "short"),
                          "OOS": {}}
    oos_tr = [t for t in prim.trades if t["sample"] == "OOS"]
    for side in (1, -1):
        tr = [t for t in oos_tr if t["side"] == side]
        nets = np.array([t["net"] for t in tr])
        results["by_side"]["OOS"]["long" if side == 1 else "short"] = {
            "trades": len(tr), "avg_net_bp": float(nets.mean() * 1e4) if len(tr) else float("nan"),
            "sum_net": float(nets.sum())}
    results["by_reason"] = group(lambda t: t["reason"] + ("+rev" if t["reversal"] else ""))
    results["by_entry_bucket"] = group(entry_bucket)
    results["by_overnights"] = group(night_bucket)

    # quintiles and predictions
    has_bars = np.array([d in base.day_index and base.day_index[d] > 0 for d in sessions])
    absq = quintile_table(np.abs(bench[has_bars]), prim.ret[has_bars])
    sgnq = quintile_table(bench[has_bars], prim.ret[has_bars])
    results["quintiles_abs_move"] = absq
    results["quintiles_signed_move"] = sgnq
    pred1a = all_on > 0
    pred1b = results["forced_flat"]["gross_fixed_sharpe_full"] < actual_gross
    top = absq[-1]["mean_strategy"]
    pred2 = top > 0 and all(top > q["mean_strategy"] for q in absq[:-1])
    pred3 = sgnq[0]["mean_strategy"] > 0 and sgnq[-1]["mean_strategy"] > 0
    results["predictions"] = {
        "1_overnight_paid": {"overnight_gross_sum": all_on, "overnight_sum_positive": bool(pred1a),
                             "flat_variant_gross_sharpe": results["forced_flat"]["gross_fixed_sharpe_full"],
                             "primary_gross_sharpe": actual_gross, "flat_lower": bool(pred1b),
                             "consistent": bool(pred1a and pred1b)},
        "2_large_move_days": {"top_quintile_mean": top, "consistent": bool(pred2)},
        "3_convexity": {"lowest_quintile_mean": sgnq[0]["mean_strategy"],
                        "highest_quintile_mean": sgnq[-1]["mean_strategy"], "consistent": bool(pred3)}}

    # dividends
    results["dividend_estimate"] = {PRIMARY: dividend_estimate(prim, PRIMARY),
                                    "SPY": dividend_estimate(cross["SPY"], "SPY")}

    # best/worst days and trades
    order = np.argsort(prim.ret)
    results["worst_days"] = [{"session": sessions[k].isoformat(), "ret": float(prim.ret[k]), "bench": float(bench[k])}
                             for k in order[:10]]
    results["best_days"] = [{"session": sessions[k].isoformat(), "ret": float(prim.ret[k]), "bench": float(bench[k])}
                            for k in order[::-1][:10]]

    # acceptance
    P = results["primary"]
    lines = [
        {"line": "1a OOS Sharpe", "required": ">= 0.5", "actual": P["OOS"]["sharpe"],
         "pass": P["OOS"]["sharpe"] >= ACCEPT["oos_sharpe"]},
        {"line": "1b OOS profit factor", "required": ">= 1.10", "actual": P["OOS"]["profit_factor"],
         "pass": P["OOS"]["profit_factor"] >= ACCEPT["oos_pf"]},
        {"line": "2 Direction placebo p (full, gross)", "required": "<= 0.05", "actual": p_val,
         "pass": p_val <= ACCEPT["placebo_p"]},
        {"line": "3a IS Sharpe", "required": "> 0", "actual": P["IS"]["sharpe"], "pass": P["IS"]["sharpe"] > 0},
        {"line": "3b IS grid cells with Sharpe > 0", "required": ">= 60% (9 of 15)", "actual": pos_share,
         "pass": pos_share >= ACCEPT["grid_share"]},
        {"line": "4 Full total return at 2 bp", "required": "> 0", "actual": cost2["full"]["total_return"],
         "pass": cost2["full"]["total_return"] > 0},
        {"line": "5 SPY OOS Sharpe", "required": "> 0", "actual": results["cross_market"]["SPY"]["OOS"]["sharpe"],
         "pass": results["cross_market"]["SPY"]["OOS"]["sharpe"] > 0},
        {"line": "6 OOS trades (minimum sample)", "required": ">= 100", "actual": P["OOS"]["trades"],
         "pass": P["OOS"]["trades"] >= ACCEPT["min_oos_trades"]},
    ]
    if not lines[-1]["pass"]:
        status = "Inconclusive"
    elif all(x["pass"] for x in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    results["acceptance"] = {"lines": lines, "status": status}

    # outputs
    def clean(o):
        if isinstance(o, float) and (math.isnan(o) or math.isinf(o)):
            return None
        if isinstance(o, dict):
            return {k: clean(v) for k, v in o.items()}
        if isinstance(o, list):
            return [clean(v) for v in o]
        if isinstance(o, (np.floating,)):
            return clean(float(o))
        if isinstance(o, (np.integer,)):
            return int(o)
        if isinstance(o, (np.bool_,)):
            return bool(o)
        return o

    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2))
    eq = np.cumprod(1 + prim.ret)
    with open(HERE / "daily.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["session", "sample", "qqq_ret", "qqq_equity", "qqq_gross_fixed", "bench_qqq_ret",
                    "spy_ret", "igv_ret", "qqq_forced_flat_ret", "qqq_2bp_ret"])
        r2 = Run(base, Params(cost_bps=2 * COST_BPS[PRIMARY]), starts[PRIMARY], sessions).ret
        for k, d in enumerate(sessions):
            w.writerow([d.isoformat(), sample_of(d), f"{prim.ret[k]:.10f}", f"{eq[k]:.10f}", f"{g[k]:.10f}",
                        f"{bench[k]:.10f}", f"{cross['SPY'].ret[k]:.10f}", f"{cross['IGV'].ret[k]:.10f}",
                        f"{flat.ret[k]:.10f}", f"{r2[k]:.10f}"])
    with open(HERE / "trades.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["symbol", "side", "entry_time", "entry_at", "entry_price", "exit_time", "exit_at", "exit_price",
                    "reason", "reversal", "gross_ret", "net_ret", "bars_held", "overnights", "sample", "stop", "n_sig"])
        for sym, r in ((PRIMARY, prim), *cross.items()):
            s = r.s
            for t in r.trades:
                w.writerow([sym, "long" if t["side"] == 1 else "short", fmt_time(s.ts[t["entry_i"]]), t["entry_at"],
                            f"{t['entry_px']:.6f}", fmt_time(s.ts[t["exit_i"]]), t["exit_at"], f"{t['exit_px']:.6f}",
                            t["reason"], int(t["reversal"]), f"{t['gross']:.10f}", f"{t['net']:.10f}",
                            t["bars_held"], t["overnights"], t["sample"], f"{t['stop']:.6f}", f"{t['n_sig']:.6f}"])

    # run log
    dirty = bool(git("status", "--porcelain"))
    entry = (f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n"
             f"- reason: {reason}\n- rules_sha256: {rules_sha}\n- git_head: {git('rev-parse', 'HEAD')}\n"
             f"- git_dirty: {str(dirty).lower()}\n"
             f"- full: sharpe {P['full']['sharpe']:.4f}, return {P['full']['total_return'] * 100:.4f}%\n"
             f"- IS: sharpe {P['IS']['sharpe']:.4f}, return {P['IS']['total_return'] * 100:.4f}%\n"
             f"- OOS: sharpe {P['OOS']['sharpe']:.4f}, return {P['OOS']['total_return'] * 100:.4f}%\n"
             f"- status from acceptance table: {status}\n")
    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nAppend-only. One entry per store run.\n")
    with open(log, "a") as fh:
        fh.write(entry)
    print(entry)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", nargs="?", default="run", choices=["run", "self-test"])
    ap.add_argument("--reason", default="initial")
    args = ap.parse_args()
    sha = check_lock()
    self_test()
    if args.mode == "self-test":
        return
    store_run(args.reason, sha)


if __name__ == "__main__":
    main()
