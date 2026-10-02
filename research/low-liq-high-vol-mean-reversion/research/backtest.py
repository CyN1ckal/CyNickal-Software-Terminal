# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Weekly quintile reversal on the low-liquidity small-cap screen.

Run from the repo root:

    python research/low-liq-high-vol-mean-reversion/research/backtest.py self-test
    python research/low-liq-high-vol-mean-reversion/research/backtest.py --reason initial

The store is not opened until the synthetic self-test passes. A store run
refuses to start if RULES.md no longer matches RULES.lock, and appends one
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
from dataclasses import dataclass, field
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent

# ---- constants mirrored from RULES.md ---------------------------------------
FORMATION_WEEKS = 1
EXTREME_K = 5
VOL_LOOKBACK = 63
MIN_OBS = 50
MIN_RETURNS = 40
VOL_LO, VOL_HI = 0.20, 0.60
DOLLAR_LO, DOLLAR_HI = 1_000_000.0, 10_000_000.0
COST_BPS = 20.0
BORROW_ANNUAL = 0.05
MIN_BARS = 252
OOS_START = date(2024, 1, 2)
HISTORY_START = date(2016, 1, 4)
HISTORY_END = date(2026, 9, 25)
ANNUAL = 252
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
GRID_FORMATION = (1, 2, 4)
GRID_K = (4, 5, 8, 10)
PLACEBO_DRAWS = 2000
TIMING_DRAWS = 500
BOOT_DRAWS = 2000
BOOT_BLOCK = 20
SEED_DIRECTION = 20260926
SEED_BOOTSTRAP = 20260927
SEED_TIMING = 20260928
Q_MIN = 2

# Even/odd alphabetical assignment after the coverage drops in RULES.md.
PRIMARY = (
    "ACCO", "AUDC", "BGS", "BOOM", "CHCT", "CLW", "CYH", "DSX", "ELME", "FNWD",
    "FSBW", "FXNC", "HDSN", "III", "IMMR", "JILL", "LMNR", "NAGE", "OSUR", "PTLO",
    "RM", "RWAY", "SMTI", "STRT", "VFF", "XPER", "ZUMZ",
)
CROSS = (
    "AIV", "AVNW", "BNED", "CCCC", "CION", "CZFS", "EGAN", "FNKO", "FRAF", "FSTR",
    "GCO", "HLLY", "HRZN", "IIIV", "INGN", "LE", "LOVE", "OPI", "OVBC", "PERI",
    "RAIL", "RMNI", "SAR", "SPOK", "TBCH", "UIS", "VNDA", "ZH",
)


@dataclass(frozen=True)
class Params:
    formation_weeks: int = FORMATION_WEEKS
    extreme_k: int = EXTREME_K
    vol_lookback: int = VOL_LOOKBACK
    min_obs: int = MIN_OBS
    min_returns: int = MIN_RETURNS
    vol_lo: float = VOL_LO
    vol_hi: float = VOL_HI
    dollar_lo: float = DOLLAR_LO
    dollar_hi: float = DOLLAR_HI
    cost_bps: float = COST_BPS
    borrow_annual: float = BORROW_ANNUAL
    cost_mult: float = 1.0
    entry_lag: int = 0
    fill: str = "next_open"  # or "signal_close"
    long_only: bool = False

    @property
    def cost_rate(self) -> float:
        return self.cost_bps / 10_000.0 * self.cost_mult

    @property
    def borrow_rate(self) -> float:
        return self.borrow_annual * self.cost_mult


@dataclass
class Panel:
    sessions: list[date]
    index: dict[date, int]
    weeks: list[list[date]]
    symbols: tuple[str, ...]
    o: np.ndarray
    c: np.ndarray
    v: np.ndarray
    present: np.ndarray
    ret: np.ndarray
    ret_ok: np.ndarray


@dataclass
class Lot:
    symbol: str
    side: int
    shares: float
    entry_session: date
    entry_px: float
    entry_notional: float
    equity_at_entry: float
    formation_return: float
    signal_week: int
    cost: float = 0.0
    borrow: float = 0.0
    gross_pnl: float = 0.0
    pnl_events: list = field(default_factory=list)
    reason: str = "schedule"


@dataclass
class SimResult:
    sessions: list[date]
    ret: np.ndarray
    exposure: np.ndarray
    trades: list[dict]
    price_events: list[tuple[int, int, float]]
    holds: list[dict]
    weeks: list[dict]


def rules_hash() -> str:
    return hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def check_lock() -> str:
    h = rules_hash()
    locked = {}
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        if line.strip():
            k, v = line.split(" ", 1)
            locked[k] = v
    if locked.get("sha256") != h:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {locked.get('sha256')}. Refusing to run.")
    return h


def group_weeks(sessions: list[date]) -> list[list[date]]:
    weeks: list[list[date]] = []
    for day in sessions:
        key = day.isocalendar()[:2]
        if not weeks or weeks[-1][0].isocalendar()[:2] != key:
            weeks.append([day])
        else:
            weeks[-1].append(day)
    return weeks


def make_panel(sessions: list[date], symbols: tuple[str, ...], o, c, v, present) -> Panel:
    o = np.asarray(o, float)
    c = np.asarray(c, float)
    v = np.asarray(v, float)
    present = np.asarray(present, bool)
    ret = np.zeros(c.shape)
    ret_ok = np.zeros(c.shape, bool)
    if c.shape[1] > 1:
        prev = c[:, :-1]
        nxt = c[:, 1:]
        ok = present[:, 1:] & present[:, :-1] & (prev > 0) & (nxt > 0)
        np.divide(nxt, prev, out=ret[:, 1:], where=ok)
        ret[:, 1:] -= ok
        ret_ok[:, 1:] = ok
    return Panel(list(sessions), {d: i for i, d in enumerate(sessions)}, group_weeks(sessions),
                 tuple(symbols), o, c, v, present, ret, ret_ok)


def load_panel(symbols: list[str] | tuple[str, ...], spy: bool = False) -> Panel:
    names = list(symbols)
    if spy and "SPY" not in names:
        pass
    with MarketData() as md:
        raw = {}
        for sym in names:
            raw[sym] = {b.session: b for b in md.bars(sym, "1d", start=HISTORY_START.isoformat(), end=HISTORY_END.isoformat())}
        spy_bars = {}
        if spy:
            spy_bars = {b.session: b for b in md.bars("SPY", "1d", start=HISTORY_START.isoformat(), end=HISTORY_END.isoformat())}
    present_dates = [d for d in nyse_sessions(HISTORY_START, HISTORY_END) if any(d in raw[s] for s in names)]
    if not present_dates:
        raise RuntimeError("no daily bars in the requested window")
    sessions = [d for d in nyse_sessions(min(present_dates), max(present_dates))]
    n, m = len(names), len(sessions)
    o = np.zeros((n, m))
    c = np.zeros((n, m))
    v = np.zeros((n, m))
    present = np.zeros((n, m), bool)
    for i, sym in enumerate(names):
        for j, day in enumerate(sessions):
            bar = raw[sym].get(day)
            if bar is None:
                continue
            if bar.open <= 0 or bar.close <= 0:
                continue
            o[i, j] = bar.open
            c[i, j] = bar.close
            v[i, j] = bar.volume
            present[i, j] = True
    panel = make_panel(sessions, tuple(names), o, c, v, present)
    panel.spy_c = np.zeros(m)  # type: ignore[attr-defined]
    panel.spy_ok = np.zeros(m, bool)  # type: ignore[attr-defined]
    if spy:
        for j, day in enumerate(sessions):
            bar = spy_bars.get(day)
            if bar is not None and bar.close > 0:
                panel.spy_c[j] = bar.close
                panel.spy_ok[j] = True
    return panel


def _stats_ok(panel: Panel, i: int, sig_i: int, p: Params) -> bool:
    lo = sig_i - p.vol_lookback + 1
    if lo < 0:
        return False
    if int(panel.present[i, lo:sig_i + 1].sum()) < p.min_obs:
        return False
    r0 = max(lo, 1)
    mask = panel.ret_ok[i, r0:sig_i + 1]
    if int(mask.sum()) < p.min_returns:
        return False
    vol = float(panel.ret[i, r0:sig_i + 1][mask].std(ddof=1) * math.sqrt(ANNUAL))
    if not p.vol_lo <= vol <= p.vol_hi:
        return False
    dollars = (panel.c[i, lo:sig_i + 1] * panel.v[i, lo:sig_i + 1])[panel.present[i, lo:sig_i + 1]]
    med = float(np.median(dollars))
    return bool(p.dollar_lo <= med <= p.dollar_hi)


def build_eligible(panel: Panel, p: Params) -> dict[int, list[tuple[str, float]]]:
    """Signal-week index -> (symbol, formation return) for names that pass the filters."""
    out: dict[int, list[tuple[str, float]]] = {}
    f = p.formation_weeks
    for s in range(f, len(panel.weeks)):
        sig_i = panel.index[panel.weeks[s][-1]]
        base_i = panel.index[panel.weeks[s - f][-1]]
        rows = []
        for i, sym in enumerate(panel.symbols):
            if not _stats_ok(panel, i, sig_i, p):
                continue
            if not panel.present[i, base_i] or not panel.present[i, sig_i]:
                continue
            c0, c1 = panel.c[i, base_i], panel.c[i, sig_i]
            if c0 <= 0 or c1 <= 0:
                continue
            rows.append((sym, float(c1 / c0 - 1.0)))
        out[s] = rows
    return out


def _choose(rows: list[tuple[str, float]], p: Params, rng: np.random.Generator | None) -> dict[str, dict]:
    n = len(rows)
    q = n // p.extreme_k
    if q < Q_MIN:
        return {}
    if rng is None:
        ordered = sorted(rows, key=lambda row: (row[1], row[0]))
        longs = ordered[:q]
        shorts = [] if p.long_only else ordered[-q:]
    else:
        pick = list(rows)
        rng.shuffle(pick)
        longs = pick[:q]
        shorts = [] if p.long_only else pick[q:2 * q]
    if p.long_only:
        w = 1.0 / q
        return {sym: {"weight": w, "formation_return": fr} for sym, fr in longs}
    out = {sym: {"weight": 0.5 / q, "formation_return": fr} for sym, fr in longs}
    for sym, fr in shorts:
        out[sym] = {"weight": -0.5 / q, "formation_return": fr}
    return out


def build_selections(eligible: dict[int, list[tuple[str, float]]], p: Params,
                     rng: np.random.Generator | None) -> dict[int, dict[str, dict]]:
    return {s: _choose(rows, p, rng) for s, rows in eligible.items()}


def build_plan(panel: Panel, p: Params) -> dict[date, tuple[int | None, str]]:
    """Rebalance date -> (signal week index or None to flatten, 'open' or 'close').

    A signal is placed only when the hold week and the exit week are both inside
    the sample. An exit that is not also a new entry flattens the book.
    """
    entries: dict[date, tuple[int, str]] = {}
    exits: dict[date, str] = {}
    weeks = panel.weeks
    f = p.formation_weeks
    if p.fill == "signal_close":
        for s in range(f, len(weeks) - 1):
            entries[weeks[s][-1]] = (s, "close")
            exits[weeks[s + 1][-1]] = "close"
    else:
        lag = p.entry_lag
        for s in range(f, len(weeks) - 2):
            hold, exit_week = weeks[s + 1], weeks[s + 2]
            if lag >= len(hold) or lag >= len(exit_week):
                continue
            entries[hold[lag]] = (s, "open")
            exits[exit_week[lag]] = "open"
    plan: dict[date, tuple[int | None, str]] = {}
    for day, field_name in exits.items():
        plan[day] = (None, field_name)
    for day, (signal, field_name) in entries.items():
        plan[day] = (signal, field_name)
    return plan


def _sign(shares: float) -> int:
    if abs(shares) < 1e-12:
        return 0
    return 1 if shares > 0 else -1


def simulate(panel: Panel, p: Params, eval_start: date, plan: dict[date, tuple[int | None, str]],
             selections: dict[int, dict[str, dict]]) -> SimResult:
    ix = {s: i for i, s in enumerate(panel.symbols)}
    shares = {s: 0.0 for s in panel.symbols}
    desired = {s: 0.0 for s in panel.symbols}
    last_px: dict[str, float] = {}
    lots: dict[str, Lot] = {}
    trades: list[dict] = []
    events: list[tuple[int, int, float]] = []
    hold_signed: dict[tuple[int, str], list[float]] = {}
    week_gross: dict[int, float] = {}
    week_rows: list[dict] = []
    active: int | None = None
    equity = 1.0
    out_sessions: list[date] = []
    out_ret: list[float] = []
    out_exp: list[float] = []

    def add_price(sym: str, pnl: float, day_i: int) -> None:
        nonlocal equity
        equity += pnl
        lot = lots.get(sym)
        if lot is not None:
            lot.gross_pnl += pnl
            lot.pnl_events.append((day_i, pnl))
        if active is not None:
            week_gross[active] = week_gross.get(active, 0.0) + pnl

    def record_signed(sym: str, prev: float, px: float) -> None:
        if active is None or prev <= 0 or px <= 0 or shares[sym] == 0:
            return
        side = 1 if shares[sym] > 0 else -1
        hold_signed.setdefault((active, sym), []).append(side * (px / prev - 1.0))

    def close_symbol(sym: str, px: float, day: date, reason: str) -> None:
        lot = lots.pop(sym)
        shares[sym] = 0.0
        ti = len(trades)
        for day_i, pnl in lot.pnl_events:
            events.append((ti, day_i, pnl))
        gross_ret = lot.gross_pnl / lot.entry_notional if lot.entry_notional else 0.0
        net_pnl = lot.gross_pnl - lot.cost - lot.borrow
        net_ret = net_pnl / lot.entry_notional if lot.entry_notional else 0.0
        trades.append({
            "symbol": sym, "side": lot.side, "signal_week": lot.signal_week,
            "entry_session": lot.entry_session.isoformat(), "entry_px": lot.entry_px,
            "exit_session": day.isoformat(), "exit_px": px, "reason": reason,
            "formation_return": lot.formation_return, "gross_ret": gross_ret, "net_ret": net_ret,
            "gross_pnl": lot.gross_pnl, "net_pnl": net_pnl, "cost": lot.cost, "borrow": lot.borrow,
            "equity_at_entry": lot.equity_at_entry, "entry_notional": lot.entry_notional,
            "weight": (lot.side * lot.entry_notional / lot.equity_at_entry) if lot.equity_at_entry else 0.0,
        })

    def open_symbol(sym: str, target: float, px: float, day: date, cost: float,
                    entry_equity: float, info: dict) -> None:
        lots[sym] = Lot(
            symbol=sym, side=1 if target > 0 else -1, shares=target, entry_session=day, entry_px=px,
            entry_notional=abs(target) * px, equity_at_entry=entry_equity,
            formation_return=float(info.get("formation_return", 0.0)),
            signal_week=int(info.get("signal_week", -1)), cost=cost,
        )
        shares[sym] = target

    def trade_to(sym: str, new: float, px: float, day: date, reason: str,
                 entry_equity: float, info: dict) -> None:
        nonlocal equity
        old = shares[sym]
        if abs(new - old) < 1e-12:
            return
        rate = p.cost_rate
        old_sign, new_sign = _sign(old), _sign(new)
        if old_sign and new_sign and old_sign != new_sign:
            close_cost = rate * abs(old) * px
            open_cost = rate * abs(new) * px
            equity -= close_cost
            lots[sym].cost += close_cost
            close_symbol(sym, px, day, "flip")
            equity -= open_cost
            open_symbol(sym, new, px, day, open_cost, entry_equity, info)
            return
        cost = rate * abs(new - old) * px
        equity -= cost
        if old_sign == 0:
            open_symbol(sym, new, px, day, cost, entry_equity, info)
            return
        if new_sign == 0:
            lots[sym].cost += cost
            close_symbol(sym, px, day, reason)
            return
        lots[sym].cost += cost
        lot = lots[sym]
        if abs(new) > abs(old):
            lot.entry_notional += (abs(new) - abs(old)) * px
        lot.shares = new
        shares[sym] = new

    def px_of(sym: str, day_i: int, field_name: str) -> float:
        i = ix[sym]
        if not panel.present[i, day_i]:
            return 0.0
        return float(panel.o[i, day_i] if field_name == "open" else panel.c[i, day_i])

    def mark(day_i: int, field_name: str) -> None:
        for sym, sh in list(shares.items()):
            if sh == 0 or sym not in last_px:
                continue
            px = px_of(sym, day_i, field_name)
            if px <= 0:
                continue
            prev = last_px[sym]
            add_price(sym, sh * (px - prev), day_i)
            record_signed(sym, prev, px)
            last_px[sym] = px

    def go_flat(day: date, day_i: int, field_name: str, reason: str) -> None:
        for sym in panel.symbols:
            if shares[sym] == 0:
                continue
            px = px_of(sym, day_i, field_name)
            if px <= 0:
                px = last_px.get(sym, 0.0)
                if px <= 0 or field_name == "open":
                    continue
            trade_to(sym, 0.0, px, day, reason, equity, {})
            last_px[sym] = px
        for sym in panel.symbols:
            desired[sym] = 0.0

    def install(signal: int, day: date, day_i: int, field_name: str) -> None:
        nonlocal active
        if equity <= 0:
            go_flat(day, day_i, field_name, "insolvent")
            active = None
            return
        chosen = selections.get(signal) or {}
        tradable: dict[str, dict] = {}
        for sym, info in chosen.items():
            px = px_of(sym, day_i, field_name)
            if px > 0:
                tradable[sym] = info
        longs = [s for s, info in tradable.items() if info["weight"] > 0]
        shorts = [s for s, info in tradable.items() if info["weight"] < 0]
        weights: dict[str, dict] = {}
        if p.long_only and longs:
            for sym in longs:
                weights[sym] = {"weight": 1.0 / len(longs), "formation_return": tradable[sym]["formation_return"],
                                "signal_week": signal}
        elif (not p.long_only) and longs and shorts:
            for sym in longs:
                weights[sym] = {"weight": 0.5 / len(longs), "formation_return": tradable[sym]["formation_return"],
                                "signal_week": signal}
            for sym in shorts:
                weights[sym] = {"weight": -0.5 / len(shorts), "formation_return": tradable[sym]["formation_return"],
                                "signal_week": signal}
        base = equity
        for sym in panel.symbols:
            desired[sym] = 0.0
        if not weights:
            go_flat(day, day_i, field_name, "schedule")
            active = None
            return
        for sym, info in weights.items():
            px = px_of(sym, day_i, field_name)
            desired[sym] = info["weight"] * base / px
        for sym in panel.symbols:
            px = px_of(sym, day_i, field_name)
            if px <= 0:
                continue
            info = weights.get(sym, {})
            trade_to(sym, desired[sym], px, day, "schedule", base, info)
            last_px[sym] = px
        active = signal
        week_rows.append({"signal_week": signal, "entry_session": day.isoformat(), "equity_start": base,
                          "gross_pnl": 0.0})

    prev: date | None = None
    for day_i, day in enumerate(panel.sessions):
        if day < eval_start:
            for sym in panel.symbols:
                px = px_of(sym, day_i, "close")
                if px > 0:
                    last_px[sym] = px
            prev = day
            continue
        eq0 = equity
        if prev is not None and p.borrow_rate:
            gap_days = (day - prev).days
            for sym, sh in shares.items():
                if sh < 0 and sym in last_px and sym in lots:
                    cost = abs(sh) * last_px[sym] * p.borrow_rate * gap_days / 365.0
                    equity -= cost
                    lots[sym].borrow += cost
        event = plan.get(day)
        if event is not None:
            signal, field_name = event
            mark(day_i, "open" if field_name == "open" else "close")
            if signal is None:
                go_flat(day, day_i, field_name, "schedule")
                active = None
            else:
                install(signal, day, day_i, field_name)
            if field_name == "open":
                mark(day_i, "close")
        else:
            for sym in panel.symbols:
                if abs(shares[sym] - desired[sym]) < 1e-12:
                    continue
                px = px_of(sym, day_i, "open")
                if px <= 0:
                    continue
                if shares[sym] != 0 and sym in last_px:
                    prev_px = last_px[sym]
                    add_price(sym, shares[sym] * (px - prev_px), day_i)
                    record_signed(sym, prev_px, px)
                    last_px[sym] = px
                trade_to(sym, desired[sym], px, day, "schedule", equity, {})
                last_px[sym] = px
            mark(day_i, "close")
        if day == panel.sessions[-1]:
            for sym in list(shares):
                if abs(shares[sym]) < 1e-12:
                    continue
                px = px_of(sym, day_i, "close")
                if px <= 0:
                    px = last_px.get(sym, 0.0)
                if px <= 0:
                    continue
                trade_to(sym, 0.0, px, day, "end_of_sample", equity, {})
        out_sessions.append(day)
        out_ret.append(equity / eq0 - 1.0 if eq0 > 0 else 0.0)
        out_exp.append(1.0 if any(abs(sh) > 1e-12 for sh in shares.values()) else 0.0)
        prev = day

    for row in week_rows:
        row["gross_pnl"] = week_gross.get(row["signal_week"], 0.0)
    net_sum = sum(t["net_pnl"] for t in trades)
    if abs(net_sum - (equity - 1.0)) > 1e-6:
        raise RuntimeError(f"trade P&L {net_sum:.8f} does not reconcile to equity change {equity - 1.0:.8f}")
    if lots:
        raise RuntimeError("positions still open at the end of the sample")
    holds = [{"signal_week": sig, "symbol": sym, "signed_rets": pieces}
             for (sig, sym), pieces in sorted(hold_signed.items())]
    return SimResult(out_sessions, np.array(out_ret), np.array(out_exp), trades, events, holds, week_rows)


def run_book(panel: Panel, p: Params, eval_start: date | None = None,
             rng: np.random.Generator | None = None,
             eligible: dict[int, list[tuple[str, float]]] | None = None) -> tuple[SimResult, date]:
    if eligible is None:
        eligible = build_eligible(panel, p)
    selections = build_selections(eligible, p, rng)
    plan = build_plan(panel, p)
    if eval_start is None:
        entries = [day for day, (sig, _) in plan.items() if sig is not None]
        if not entries:
            eval_start = panel.sessions[0]
        else:
            eval_start = min(entries)
    return simulate(panel, p, eval_start, plan, selections), eval_start


def weekdays(start: date, n: int) -> list[date]:
    out = []
    day = start
    while len(out) < n:
        if day.weekday() < 5:
            out.append(day)
        day += timedelta(days=1)
    return out


def _panel(symbols: list[str], sessions: list[date], close: np.ndarray, open_: np.ndarray | None = None,
           volume: float = 20_000.0, drop: set[tuple[str, date]] | None = None) -> Panel:
    n, m = len(symbols), len(sessions)
    o = np.array(close if open_ is None else open_, float).copy()
    c = np.array(close, float).copy()
    v = np.full((n, m), volume)
    present = np.ones((n, m), bool)
    if drop:
        for sym, day in drop:
            j = sessions.index(day)
            i = symbols.index(sym)
            present[i, j] = False
    return make_panel(sessions, tuple(symbols), o, c, v, present)


def self_test() -> None:
    failures: list[str] = []

    def check(name: str, cond: bool, detail: str = "") -> None:
        if not cond:
            failures.append(f"{name}: {detail}")

    def near(a: float, b: float, tol: float = 1e-8) -> bool:
        return abs(a - b) <= tol

    # --- Real filters, one round trip, hand-computed P&L. ---
    symbols = list("ABCDEFGHIJ")
    sessions = weekdays(date(2023, 1, 2), 75)
    m = len(sessions)
    close = np.empty((10, m))
    px = 100.0
    for j in range(m):
        close[:, j] = px
        if j + 1 < m:
            px = px * (1.02 if (j + 1) % 2 else 1.0 / 1.02)
    base = float(close[0, 59])
    forms = [-0.08, -0.06, -0.04, -0.02, -0.01, 0.01, 0.02, 0.04, 0.06, 0.08]
    for i, f in enumerate(forms):
        close[i, 64] = base * (1.0 + f)
        close[i, 65:70] = close[i, 64]
    opens = close.copy()
    opens[:, 65] = close[:, 64]
    opens[0, 70] = close[0, 64] * 1.10
    opens[1, 70] = close[1, 64] * 1.10
    opens[8, 70] = close[8, 64] * 0.90
    opens[9, 70] = close[9, 64] * 0.90
    panel = _panel(symbols, sessions, close, opens)
    result, _ = run_book(panel, Params())
    by = {t["symbol"]: t for t in result.trades}
    check("main count", len(result.trades) == 4, str(sorted(by)))
    check("main longs", set(by) >= {"A", "B"} and by.get("A", {}).get("side") == 1 and by.get("B", {}).get("side") == 1,
          str({k: v.get("side") for k, v in by.items()}))
    check("main shorts", by.get("I", {}).get("side") == -1 and by.get("J", {}).get("side") == -1, str(sorted(by)))
    check("main no middle", not ({"C", "D", "E", "F", "G", "H"} & set(by)), str(sorted(by)))
    borrow = 0.05 * 7.0 / 365.0 * 0.25
    if len(result.trades) == 4 and "A" in by:
        for sym in ("A", "B"):
            check(f"{sym} gross", near(by[sym]["gross_pnl"], 0.025), str(by[sym]["gross_pnl"]))
            check(f"{sym} cost", near(by[sym]["cost"], 0.00105), str(by[sym]["cost"]))
            check(f"{sym} borrow", near(by[sym]["borrow"], 0.0), str(by[sym]["borrow"]))
            check(f"{sym} reason", by[sym]["reason"] == "schedule", by[sym]["reason"])
        for sym in ("I", "J"):
            check(f"{sym} gross", near(by[sym]["gross_pnl"], 0.025), str(by[sym]["gross_pnl"]))
            check(f"{sym} cost", near(by[sym]["cost"], 0.00095), str(by[sym]["cost"]))
            check(f"{sym} borrow", near(by[sym]["borrow"], borrow), str(by[sym]["borrow"]))
        equity = 1.0 + 0.10 - 0.004 - 2.0 * borrow
        check("main equity", near(float(np.prod(1.0 + result.ret)), equity), str(float(np.prod(1.0 + result.ret))))

    # --- Long-only secondary: the two losers, half the book each, no borrow. ---
    long_only, _ = run_book(panel, Params(long_only=True))
    lby = {t["symbol"]: t for t in long_only.trades}
    check("long-only names", set(lby) == {"A", "B"}, str(sorted(lby)))
    if set(lby) == {"A", "B"}:
        check("long-only gross", near(lby["A"]["gross_pnl"], 0.05) and near(lby["B"]["gross_pnl"], 0.05),
              str((lby["A"]["gross_pnl"], lby["B"]["gross_pnl"])))
        check("long-only borrow", near(lby["A"]["borrow"], 0.0) and near(lby["B"]["borrow"], 0.0), "")

    # --- Fewer than two names per side: no trades. ---
    small = _panel(symbols[:9], sessions, close[:9], opens[:9])
    skipped, _ = run_book(small, Params())
    check("q<2 skips", skipped.trades == [], str(len(skipped.trades)))

    # --- Tie on the cut is broken by ticker, not by row order. ---
    tie_sessions = weekdays(date(2023, 1, 2), 20)
    tie_close = np.full((10, 20), 100.0)
    tie_forms = [-0.10, -0.05, -0.05, -0.01, 0.01, 0.02, 0.03, 0.04, 0.06, 0.08]
    for i, f in enumerate(tie_forms):
        tie_close[i, 9] = 100.0 * (1.0 + f)
        tie_close[i, 10:15] = tie_close[i, 9]
    tie_open = tie_close.copy()
    tie = _panel(symbols, tie_sessions, tie_close, tie_open, volume=20_000.0)
    wide = Params(vol_lookback=4, min_obs=3, min_returns=2, vol_lo=0.0, vol_hi=10.0, dollar_lo=0.0, dollar_hi=1e18)
    tie_res, _ = run_book(tie, wide)
    tie_names = {t["symbol"] for t in tie_res.trades if t["side"] == 1}
    check("tie longs", tie_names == {"A", "B"}, str(sorted(tie_names)))

    # --- Missing exit bar: flatten at the last marked price, reason end_of_sample. ---
    drop = {(symbols[0], sessions[j]) for j in range(70, 75)}
    stuck = _panel(symbols, sessions, close, opens, drop=drop)
    stuck_res, _ = run_book(stuck, Params())
    stuck_by = {t["symbol"]: t for t in stuck_res.trades}
    check("stuck reason", stuck_by.get("A", {}).get("reason") == "end_of_sample",
          str(stuck_by.get("A", {}).get("reason")))
    if "A" in stuck_by:
        check("stuck exit px", near(stuck_by["A"]["exit_px"], float(close[0, 69])), str(stuck_by["A"]["exit_px"]))
        check("B still scheduled", stuck_by.get("B", {}).get("reason") == "schedule", str(stuck_by.get("B")))

    # --- A name that was short becomes long: the first stint closes as a flip. ---
    flip_sym = ["W", "X", "Y", "Z"]
    flip_sessions = weekdays(date(2023, 1, 2), 25)
    flip_close = np.full((4, 25), 100.0)
    # Signal week 1 ends at index 9; signal week 2 ends at index 14.
    first = [-0.06, -0.03, 0.03, 0.06]
    second = [0.06, 0.03, -0.03, -0.06]
    for i, f in enumerate(first):
        flip_close[i, 9] = 100.0 * (1.0 + f)
    for i, f in enumerate(second):
        flip_close[i, 14] = 100.0 * (1.0 + f)
    flip_panel = _panel(flip_sym, flip_sessions, flip_close)
    flip_params = Params(vol_lookback=4, min_obs=3, min_returns=2, vol_lo=0.0, vol_hi=10.0,
                         dollar_lo=0.0, dollar_hi=1e18, extreme_k=2, cost_bps=0.0, borrow_annual=0.0)
    flip_res, _ = run_book(flip_panel, flip_params)
    flips = [t for t in flip_res.trades if t["reason"] == "flip"]
    check("flip happened", len(flips) == 4, str([(t["symbol"], t["reason"], t["side"]) for t in flip_res.trades]))

    # --- Same-bar close fill uses the signal close, not the next open. ---
    close_params = Params(vol_lookback=4, min_obs=3, min_returns=2, vol_lo=0.0, vol_hi=10.0,
                          dollar_lo=0.0, dollar_hi=1e18, extreme_k=2, fill="signal_close",
                          cost_bps=0.0, borrow_annual=0.0)
    close_res, _ = run_book(flip_panel, close_params)
    if close_res.trades:
        check("close fill entry", near(close_res.trades[0]["entry_px"], 100.0 * (1.0 + first[flip_sym.index(close_res.trades[0]["symbol"])])),
              str(close_res.trades[0]["entry_px"]))
    else:
        check("close fill traded", False, "no trades")

    if failures:
        raise SystemExit("self-test failed:\n" + "\n".join(failures))
    print(f"self-test passed ({len(result.trades)} trades on the hand case)")


def sharpe(r: np.ndarray) -> float:
    if len(r) < 2:
        return float("nan")
    sd = float(r.std(ddof=1))
    return float(r.mean() / sd * math.sqrt(ANNUAL)) if sd > 0 else float("nan")


def max_drawdown(r: np.ndarray) -> float:
    if len(r) == 0:
        return 0.0
    eq = np.cumprod(1.0 + r)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return float((eq / peak - 1.0).min())


def metrics(r: np.ndarray, trades: list[dict]) -> dict:
    total = float(np.prod(1.0 + r) - 1.0) if len(r) else float("nan")
    n = len(r)
    nets = np.array([t["net_ret"] for t in trades]) if trades else np.zeros(0)
    wins = float(nets[nets > 0].sum()) if len(nets) else 0.0
    losses = float(nets[nets < 0].sum()) if len(nets) else 0.0
    sd = float(r.std(ddof=1)) if n > 1 else float("nan")
    return {
        "sessions": n,
        "sharpe": sharpe(r),
        "total_return": total,
        "cagr": float((1.0 + total) ** (ANNUAL / n) - 1.0) if n and total > -1 else float("nan"),
        "ann_vol": float(sd * math.sqrt(ANNUAL)) if n > 1 else float("nan"),
        "max_dd": max_drawdown(r),
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if n > 1 and sd > 0 else float("nan"),
        "trades": len(trades),
        "win_rate": float((nets > 0).mean()) if len(nets) else float("nan"),
        "profit_factor": float(wins / abs(losses)) if losses < 0 else float("nan"),
        "avg_net_bp": float(nets.mean() * 1e4) if len(nets) else float("nan"),
        "avg_gross_bp": float(np.mean([t["gross_ret"] for t in trades]) * 1e4) if trades else float("nan"),
        "exposure": None,
    }


def split_mask(sessions: list[date], which: str) -> np.ndarray:
    if which == "IS":
        return np.array([d < OOS_START for d in sessions])
    if which == "OOS":
        return np.array([d >= OOS_START for d in sessions])
    return np.ones(len(sessions), bool)


def slice_metrics(result: SimResult, which: str) -> dict:
    mask = split_mask(result.sessions, which)
    trades = []
    for t in result.trades:
        entry = date.fromisoformat(t["entry_session"])
        in_sample = entry < OOS_START
        if which == "full" or (which == "IS" and in_sample) or (which == "OOS" and not in_sample):
            trades.append(t)
    out = metrics(result.ret[mask], trades)
    out["exposure"] = float(result.exposure[mask].mean()) if mask.any() else float("nan")
    if trades:
        spans = []
        for t in trades:
            a = date.fromisoformat(t["entry_session"])
            b = date.fromisoformat(t["exit_session"])
            spans.append((b - a).days)
        out["median_hold_calendar_days"] = float(np.median(spans))
        out["mean_hold_calendar_days"] = float(np.mean(spans))
    return out


def bench_ew(panel: Panel, sessions: list[date]) -> np.ndarray:
    out = np.zeros(len(sessions))
    for k, day in enumerate(sessions):
        j = panel.index[day]
        if j == 0:
            continue
        rs = []
        for i in range(len(panel.symbols)):
            if panel.present[i, j] and panel.present[i, j - 1] and panel.c[i, j - 1] > 0:
                rs.append(panel.c[i, j] / panel.c[i, j - 1] - 1.0)
        if rs:
            out[k] = float(np.mean(rs))
    return out


def bench_spy(panel: Panel, sessions: list[date]) -> np.ndarray:
    out = np.zeros(len(sessions))
    spy_c = getattr(panel, "spy_c", None)
    spy_ok = getattr(panel, "spy_ok", None)
    if spy_c is None:
        return out
    for k, day in enumerate(sessions):
        j = panel.index[day]
        if j == 0 or not spy_ok[j] or not spy_ok[j - 1] or spy_c[j - 1] <= 0:
            continue
        out[k] = float(spy_c[j] / spy_c[j - 1] - 1.0)
    return out


def compound_sharpe(by_trade: list[list[tuple[int, float]]], signs: np.ndarray | None, n: int) -> float:
    pnl = np.zeros(n)
    for ti, rows in enumerate(by_trade):
        sgn = 1.0 if signs is None else float(signs[ti])
        for si, value in rows:
            if 0 <= si < n:
                pnl[si] += sgn * value
    eq = 1.0
    rets = np.zeros(n)
    for i in range(n):
        if eq <= 1e-12:
            break
        rets[i] = pnl[i] / eq
        eq += pnl[i]
    return sharpe(rets)


def direction_placebo(result: SimResult, panel: Panel) -> dict:
    if not result.sessions or not result.trades:
        return {"actual_gross_sharpe": float("nan"), "p": float("nan"), "null_mean": float("nan"),
                "null": []}
    shift = panel.index[result.sessions[0]]
    n = len(result.sessions)
    by_trade: list[list[tuple[int, float]]] = [[] for _ in result.trades]
    for ti, day_i, pnl in result.price_events:
        by_trade[ti].append((day_i - shift, pnl))
    actual = compound_sharpe(by_trade, None, n)
    rng = np.random.default_rng(SEED_DIRECTION)
    null = np.empty(PLACEBO_DRAWS)
    beats = 0
    for draw in range(PLACEBO_DRAWS):
        signs = rng.choice(np.array([-1.0, 1.0]), size=len(result.trades))
        null[draw] = compound_sharpe(by_trade, signs, n)
        if null[draw] >= actual:
            beats += 1
    return {"actual_gross_sharpe": actual, "p": (1 + beats) / (PLACEBO_DRAWS + 1),
            "null_mean": float(np.nanmean(null)), "null": null.tolist()}


def block_bootstrap(r: np.ndarray) -> dict:
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    n = len(r)
    n_blocks = int(math.ceil(n / BOOT_BLOCK))
    sharpes = np.empty(BOOT_DRAWS)
    for draw in range(BOOT_DRAWS):
        starts = rng.integers(0, n, n_blocks)
        pieces = np.empty(n_blocks * BOOT_BLOCK)
        for b, start in enumerate(starts):
            for k in range(BOOT_BLOCK):
                pieces[b * BOOT_BLOCK + k] = r[(int(start) + k) % n]
        sharpes[draw] = sharpe(pieces[:n])
    lo, hi = np.nanquantile(sharpes, [0.025, 0.975])
    return {"lo": float(lo), "hi": float(hi), "draws": BOOT_DRAWS, "block": BOOT_BLOCK}


def predictions(result: SimResult, eligible: dict[int, list[tuple[str, float]]]) -> dict:
    trades = result.trades
    long_g = float(sum(t["gross_pnl"] for t in trades if t["side"] > 0))
    short_g = float(sum(t["gross_pnl"] for t in trades if t["side"] < 0))
    hi, lo = [], []
    if trades:
        abs_f = np.array([abs(t["formation_return"]) for t in trades])
        med = float(np.median(abs_f))
        hi = [t["gross_ret"] for t in trades if abs(t["formation_return"]) >= med]
        lo = [t["gross_ret"] for t in trades if abs(t["formation_return"]) < med]
        hi_m = float(np.mean(hi)) if hi else float("nan")
        lo_m = float(np.mean(lo)) if lo else float("nan")
    else:
        med = hi_m = lo_m = float("nan")
    firsts, lasts = [], []
    for hold in result.holds:
        pieces = hold["signed_rets"]
        if len(pieces) < 4:
            continue
        firsts.append(float(np.mean(pieces[:2])))
        lasts.append(float(np.mean(pieces[-2:])))
    first_m = float(np.mean(firsts)) if firsts else float("nan")
    last_m = float(np.mean(lasts)) if lasts else float("nan")
    week_rows = []
    for row in result.weeks:
        names = eligible.get(row["signal_week"], [])
        if len(names) < 2 or row["equity_start"] <= 0:
            continue
        disp = float(np.std([fr for _, fr in names], ddof=1))
        week_rows.append((disp, row["gross_pnl"] / row["equity_start"]))
    if week_rows:
        disp_med = float(np.median([d for d, _ in week_rows]))
        high = [g for d, g in week_rows if d >= disp_med]
        low = [g for d, g in week_rows if d < disp_med]
        high_m = float(np.mean(high)) if high else float("nan")
        low_m = float(np.mean(low)) if low else float("nan")
    else:
        disp_med = high_m = low_m = float("nan")
    return {
        "long_gross_pnl": long_g, "short_gross_pnl": short_g,
        "both_legs_positive": bool(long_g > 0 and short_g > 0),
        "formation_abs_median": med, "gross_ret_above_median": hi_m, "gross_ret_below_median": lo_m,
        "larger_dislocation_reverts_more": bool(hi_m > lo_m) if hi and lo else False,
        "holds_with_four_sessions": len(firsts), "mean_first_two": first_m, "mean_last_two": last_m,
        "edge_is_front_loaded": bool(first_m > last_m) if firsts else False,
        "weeks_scored": len(week_rows), "dispersion_median": disp_med,
        "gross_week_high_dispersion": high_m, "gross_week_low_dispersion": low_m,
        "stronger_when_dispersed": bool(high_m > low_m) if week_rows and high and low else False,
    }


def breakdowns(result: SimResult, spy: np.ndarray) -> dict:
    by_year = []
    years = sorted({d.year for d in result.sessions})
    for year in years:
        mask = np.array([d.year == year for d in result.sessions])
        trades = [t for t in result.trades if date.fromisoformat(t["entry_session"]).year == year]
        row = metrics(result.ret[mask], trades)
        row["year"] = year
        row["exposure"] = float(result.exposure[mask].mean()) if mask.any() else float("nan")
        by_year.append(row)
    sides = {}
    for side, name in ((1, "long"), (-1, "short")):
        group = [t for t in result.trades if t["side"] == side]
        sides[name] = metrics(np.zeros(1), group)
        sides[name]["gross_pnl"] = float(sum(t["gross_pnl"] for t in group))
        sides[name]["net_pnl"] = float(sum(t["net_pnl"] for t in group))
    reasons: dict[str, dict] = {}
    for reason in sorted({t["reason"] for t in result.trades}):
        group = [t for t in result.trades if t["reason"] == reason]
        reasons[reason] = {"trades": len(group), "avg_net_bp": float(np.mean([t["net_ret"] for t in group]) * 1e4),
                           "net_pnl": float(sum(t["net_pnl"] for t in group))}
    valid = np.array([abs(x) > 0 or True for x in spy])
    edges = [float(x) for x in np.quantile(spy, [0.2, 0.4, 0.6, 0.8])] if len(spy) else []
    bins = np.digitize(spy, edges) if edges else np.zeros(len(spy), int)
    quintiles = []
    for b in range(5):
        mask = bins == b
        quintiles.append({
            "bin": b + 1,
            "days": int(mask.sum()),
            "mean_strategy": float(result.ret[mask].mean()) if mask.any() else float("nan"),
            "mean_spy": float(spy[mask].mean()) if mask.any() else float("nan"),
        })
    return {"by_year": by_year, "by_side": sides, "by_reason": reasons,
            "spy_quintile_edges": edges, "spy_quintiles": quintiles, "spy_days_used": int(valid.sum())}


def book_report(panel: Panel, params: Params, eval_start: date | None = None,
                 rng: np.random.Generator | None = None,
                 eligible: dict | None = None) -> tuple[SimResult, dict, date, dict]:
    if eligible is None:
        eligible = build_eligible(panel, params)
    result, start = run_book(panel, params, eval_start, rng, eligible)
    summary = {which: slice_metrics(result, which) for which in ("full", "IS", "OOS")}
    return result, summary, start, eligible


def require_history(panel: Panel) -> None:
    for i, sym in enumerate(panel.symbols):
        n = int(panel.present[i].sum())
        if n < MIN_BARS:
            raise RuntimeError(f"{sym} has {n} daily bars, under the {MIN_BARS} floor")
        if np.any(panel.present[i] & ((panel.o[i] <= 0) | (panel.c[i] <= 0))):
            raise RuntimeError(f"{sym} has a nonpositive price marked present")


def clean(obj):
    if isinstance(obj, float) and (math.isnan(obj) or math.isinf(obj)):
        return None
    if isinstance(obj, dict):
        return {k: clean(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [clean(v) for v in obj]
    if isinstance(obj, (np.floating,)):
        return clean(float(obj))
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.bool_,)):
        return bool(obj)
    return obj


def git(*args: str) -> str:
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    except OSError:
        return ""


def store_run(reason: str, rules_sha: str) -> None:
    if not PRIMARY or not CROSS:
        raise RuntimeError("PRIMARY and CROSS are empty")
    panel = load_panel(PRIMARY, spy=True)
    cross_panel = load_panel(CROSS, spy=False)
    require_history(panel)
    require_history(cross_panel)
    base = Params()
    primary, summary, eval_start, eligible = book_report(panel, base)
    ew = bench_ew(panel, primary.sessions)
    spy = bench_spy(panel, primary.sessions)
    bench = {
        "ew": {which: metrics(ew[split_mask(primary.sessions, which)], []) for which in ("full", "IS", "OOS")},
        "spy": {which: metrics(spy[split_mask(primary.sessions, which)], []) for which in ("full", "IS", "OOS")},
    }
    sweep = {}
    for mult in COST_MULTS:
        if mult == 1.0:
            res, summ = primary, summary
        else:
            res, summ, _, _ = book_report(panel, Params(cost_mult=mult), eval_start)
        sweep[str(mult)] = {"full_sharpe": summ["full"]["sharpe"], "full_return": summ["full"]["total_return"],
                            "oos_sharpe": summ["OOS"]["sharpe"], "oos_return": summ["OOS"]["total_return"],
                            "oos_pf": summ["OOS"]["profit_factor"]}
    delay, delay_sum, _, _ = book_report(panel, Params(entry_lag=1), eval_start)
    upper, upper_sum, _, _ = book_report(panel, Params(fill="signal_close"), eval_start)
    secondary, sec_sum, _, _ = book_report(panel, Params(long_only=True), eval_start)
    cross, cross_sum, _, _ = book_report(cross_panel, base, None)
    grid = []
    positive = 0
    for formation in GRID_FORMATION:
        for k in GRID_K:
            _, cell, _, _ = book_report(panel, Params(formation_weeks=formation, extreme_k=k), eval_start)
            grid.append({"formation_weeks": formation, "k": k, "is_sharpe": cell["IS"]["sharpe"],
                         "oos_sharpe": cell["OOS"]["sharpe"], "is_return": cell["IS"]["total_return"]})
            if cell["IS"]["sharpe"] is not None and cell["IS"]["sharpe"] > 0:
                positive += 1
    grid_share = positive / len(grid)
    # Timing placebo reuses the primary eligibility table.
    rng = np.random.default_rng(SEED_TIMING)
    timing = np.empty(TIMING_DRAWS)
    plan = build_plan(panel, base)
    for draw in range(TIMING_DRAWS):
        selections = build_selections(eligible, base, rng)
        timed = simulate(panel, base, eval_start, plan, selections)
        timing[draw] = sharpe(timed.ret)
    actual_net = summary["full"]["sharpe"]
    timing_beats = int(np.sum(timing >= actual_net))
    placebo = direction_placebo(primary, panel)
    boot = block_bootstrap(primary.ret)
    pred = predictions(primary, eligible)
    parts = breakdowns(primary, spy)
    # Acceptance. Line 6 gates the status.
    oos = summary["OOS"]
    is_ = summary["IS"]
    lines = [
        {"id": 1, "name": "OOS Sharpe >= 0.5 and OOS profit factor >= 1.10",
         "actual": {"sharpe": oos["sharpe"], "profit_factor": oos["profit_factor"]},
         "pass": bool(oos["sharpe"] >= 0.5 and oos["profit_factor"] >= 1.10)},
        {"id": 2, "name": "Direction placebo p <= 0.05",
         "actual": placebo["p"], "pass": bool(placebo["p"] <= 0.05)},
        {"id": 3, "name": "IS Sharpe > 0 and at least 60% of IS grid cells > 0",
         "actual": {"is_sharpe": is_["sharpe"], "grid_share": grid_share},
         "pass": bool(is_["sharpe"] > 0 and grid_share >= 0.60)},
        {"id": 4, "name": "Full-sample total return > 0 at 2x cost",
         "actual": sweep["2.0"]["full_return"], "pass": bool(sweep["2.0"]["full_return"] > 0)},
        {"id": 5, "name": "Cross-market OOS Sharpe > 0",
         "actual": cross_sum["OOS"]["sharpe"], "pass": bool(cross_sum["OOS"]["sharpe"] > 0)},
        {"id": 6, "name": "At least 100 OOS trades",
         "actual": oos["trades"], "pass": bool(oos["trades"] >= 100)},
    ]
    if not lines[5]["pass"]:
        status = "Inconclusive"
    elif all(line["pass"] for line in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    sec_pass = bool(sec_sum["OOS"]["sharpe"] >= 0.5 and sec_sum["OOS"]["profit_factor"] >= 1.10
                    and sec_sum["OOS"]["trades"] >= 100)
    results = {
        "rules_sha256": rules_sha, "reason": reason, "seeds": {
            "direction": SEED_DIRECTION, "bootstrap": SEED_BOOTSTRAP, "timing": SEED_TIMING},
        "eval_start": eval_start.isoformat(),
        "eval_end": primary.sessions[-1].isoformat() if primary.sessions else None,
        "primary": summary, "benchmarks": bench, "cost_sweep": sweep,
        "delay": {"full": delay_sum["full"], "OOS": delay_sum["OOS"]},
        "signal_close_upper_bound": {"full": upper_sum["full"], "OOS": upper_sum["OOS"]},
        "secondary_long_only": sec_sum, "secondary_passes_its_line": sec_pass,
        "cross": cross_sum, "grid": grid, "grid_is_positive_share": grid_share,
        "placebo": {k: v for k, v in placebo.items() if k != "null"},
        "placebo_null": placebo["null"],
        "timing_placebo": {"actual_net_sharpe": actual_net,
                           "p": (1 + timing_beats) / (TIMING_DRAWS + 1),
                           "null_mean": float(np.nanmean(timing)), "null": timing.tolist()},
        "bootstrap": boot, "predictions": pred, "breakdowns": parts,
        "acceptance": {"lines": lines, "status": status, "passed": sum(line["pass"] for line in lines),
                       "failed": sum(not line["pass"] for line in lines)},
        "symbols": {"primary": list(PRIMARY), "cross": list(CROSS)},
    }
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8")
    equity = np.cumprod(1.0 + primary.ret)
    with (HERE / "daily.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(["session", "sample", "ret", "equity", "bench_ew_ret", "bench_spy_ret", "exposure"])
        for k, day in enumerate(primary.sessions):
            sample = "IS" if day < OOS_START else "OOS"
            writer.writerow([day.isoformat(), sample, f"{primary.ret[k]:.10f}", f"{equity[k]:.10f}",
                             f"{ew[k]:.10f}", f"{spy[k]:.10f}", f"{primary.exposure[k]:.0f}"])
    with (HERE / "trades.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(["symbol", "side", "signal_week", "entry_session", "entry_price", "exit_session",
                         "exit_price", "reason", "formation_return", "gross_ret", "net_ret", "gross_pnl",
                         "net_pnl", "cost", "borrow", "equity_at_entry", "entry_notional", "weight", "sample"])
        for t in primary.trades:
            entry = date.fromisoformat(t["entry_session"])
            writer.writerow([
                t["symbol"], "long" if t["side"] > 0 else "short", t["signal_week"], t["entry_session"],
                f"{t['entry_px']:.6f}", t["exit_session"], f"{t['exit_px']:.6f}", t["reason"],
                f"{t['formation_return']:.8f}", f"{t['gross_ret']:.10f}", f"{t['net_ret']:.10f}",
                f"{t['gross_pnl']:.10f}", f"{t['net_pnl']:.10f}", f"{t['cost']:.10f}", f"{t['borrow']:.10f}",
                f"{t['equity_at_entry']:.10f}", f"{t['entry_notional']:.10f}", f"{t['weight']:.8f}",
                "IS" if entry < OOS_START else "OOS"])
    head = git("rev-parse", "HEAD")
    dirty = bool(git("status", "--porcelain"))
    entry = (
        f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n"
        f"- reason: {reason}\n- rules_sha256: {rules_sha}\n- git_head: {head}\n"
        f"- git_dirty: {str(dirty).lower()}\n"
        f"- full: sharpe {summary['full']['sharpe']:.4f}, return {summary['full']['total_return'] * 100:.4f}%\n"
        f"- IS: sharpe {summary['IS']['sharpe']:.4f}, return {summary['IS']['total_return'] * 100:.4f}%\n"
        f"- OOS: sharpe {summary['OOS']['sharpe']:.4f}, return {summary['OOS']['total_return'] * 100:.4f}%\n"
        f"- status from acceptance table: {status}\n"
    )
    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nAppend-only. One entry per store run.\n", encoding="utf-8")
    with log.open("a", encoding="utf-8") as fh:
        fh.write(entry)
    print(entry)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", nargs="?", default="run", choices=["run", "self-test"])
    parser.add_argument("--reason", default="initial")
    args = parser.parse_args()
    if args.mode == "self-test":
        self_test()
        return
    rules_sha = check_lock()
    self_test()
    store_run(args.reason, rules_sha)


if __name__ == "__main__":
    main()

