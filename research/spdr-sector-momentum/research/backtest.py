# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered SPDR sector-momentum backtest.

Run from the repo root:

    python research/spdr-sector-momentum/research/backtest.py
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))

UNIVERSE = ["XLK", "XLF", "XLE", "XLV", "XLI", "XLY", "XLP", "XLU", "XLB", "XLRE", "XLC"]
SPLIT_NAMES = ["XLK", "XLE", "XLY", "XLU", "XLB"]
SPLIT_EX = date(2025, 12, 5)
SKIP = 21
LOOKBACK = 252
FAR = LOOKBACK + SKIP
COST = 0.0001
OOS_START = date(2024, 7, 1)
SAMPLE_END = date(2026, 10, 1)
GRID = [126, 189, 252, 315, 378]
SEED_DIRECTION = 20261011
SEED_BOOT = 20261012
SEED_TIMING = 20261013
N_DIRECTION = 2000
N_BOOT = 2000
N_TIMING = 500
BLOCK = 20

TRADE_FIELDS = [
    "symbol", "side", "entry_date", "entry_price", "exit_date", "exit_price",
    "exit_reason", "entry_shares", "exit_shares", "entry_equity", "gross_pnl",
    "cost", "net_pnl", "holding_sessions", "formation",
]
DAILY_FIELDS = [
    "date", "strategy_net", "strategy_gross", "equity", "spy", "spy_equity",
    "ew", "ew_equity", "long_pnl", "short_pnl", "cost", "gross_exposure",
    "n_long", "n_short", "rebalance",
]


def rules_hash() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(raw).hexdigest()


def check_lock() -> dict[str, str]:
    lines = {}
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        key, value = line.split(" ", 1)
        lines[key] = value.strip()
    got = rules_hash()
    if lines.get("sha256") != got:
        raise SystemExit("RULES.md hash does not match RULES.lock; refusing to run")
    return lines


def sharpe(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return 0.0
    sd = float(r.std(ddof=1))
    mu = float(r.mean())
    if sd == 0.0:
        return 0.0 if mu == 0.0 else float("nan")
    return mu / sd * math.sqrt(252.0)


def sharpe_rows(mat: np.ndarray) -> np.ndarray:
    mu = mat.mean(axis=1)
    sd = mat.std(axis=1, ddof=1)
    out = np.zeros(mat.shape[0], dtype=float)
    ok = sd > 0.0
    out[ok] = mu[ok] / sd[ok] * math.sqrt(252.0)
    bad = (~ok) & (np.abs(mu) > 0.0)
    out[bad] = np.nan
    return out


def max_drawdown(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) == 0:
        return 0.0
    eq = np.cumprod(1.0 + r)
    eq = np.concatenate([[1.0], eq])
    peak = np.maximum.accumulate(eq)
    return float(np.min(eq / peak - 1.0))


def total_return(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) == 0:
        return 0.0
    return float(np.prod(1.0 + r) - 1.0)


def cagr(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) == 0:
        return 0.0
    return float((1.0 + total_return(r)) ** (252.0 / len(r)) - 1.0)


def ann_vol(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return 0.0
    return float(r.std(ddof=1) * math.sqrt(252.0))


def tstat(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return 0.0
    sd = float(r.std(ddof=1))
    mu = float(r.mean())
    if sd == 0.0:
        return 0.0 if mu == 0.0 else float("nan")
    return mu / (sd / math.sqrt(len(r)))


def profit_factor(pnls: list[float]) -> float:
    wins = sum(p for p in pnls if p > 0.0)
    losses = sum(p for p in pnls if p < 0.0)
    if losses == 0.0:
        if wins == 0.0:
            return 0.0
        return float("inf")
    return wins / abs(losses)


def packet(rets: np.ndarray) -> dict:
    r = np.asarray(rets, dtype=float)
    return {
        "sessions": int(len(r)),
        "total_return": total_return(r),
        "cagr": cagr(r),
        "ann_vol": ann_vol(r),
        "sharpe": sharpe(r),
        "max_drawdown": max_drawdown(r),
        "tstat": tstat(r),
    }


def slice_window(dates: list[date], rets: np.ndarray, start: date | None, end: date | None) -> np.ndarray:
    keep = []
    for d, x in zip(dates, rets):
        if start is not None and d < start:
            continue
        if end is not None and d > end:
            continue
        keep.append(x)
    return np.asarray(keep, dtype=float)


class Book:
    """One simulated book. `mode` is 'ls' or 'ew'. `fill_mode` is next_open, delay, or signal_close."""

    def __init__(
        self,
        sessions: list[date],
        opens: dict[str, list],
        closes: dict[str, list],
        signal_indexes: list[int],
        universe: list[str],
        lookback: int,
        skip: int,
        cost_rate: float,
        mode: str,
        fill_mode: str = "next_open",
        rng: np.random.Generator | None = None,
        collect_trips: bool = True,
    ) -> None:
        self.sessions = sessions
        self.opens = opens
        self.closes = closes
        self.signal_indexes = signal_indexes
        self.universe = universe
        self.lookback = lookback
        self.skip = skip
        self.far = lookback + skip
        self.cost_rate = cost_rate
        self.mode = mode
        self.fill_mode = fill_mode
        self.rng = rng
        self.collect_trips = collect_trips
        if self.far <= self.skip:
            raise SystemExit("far lag must exceed the skip")
        self.own_pos: dict[str, list[int]] = {}
        self.own_px: dict[str, list[float]] = {}
        for sym in universe:
            pos, px = [], []
            for i, c in enumerate(closes[sym]):
                if c is not None:
                    pos.append(i)
                    px.append(float(c))
            self.own_pos[sym] = pos
            self.own_px[sym] = px

    def formation_at(self, sym: str, sidx: int) -> float | None:
        pos = self.own_pos[sym]
        lo, hi = 0, len(pos)
        while lo < hi:
            mid = (lo + hi) // 2
            if pos[mid] <= sidx:
                lo = mid + 1
            else:
                hi = mid
        if lo == 0 or pos[lo - 1] != sidx:
            return None
        k = lo - 1
        if k < self.far:
            return None
        base = self.own_px[sym][k - self.far]
        if base <= 0.0:
            return None
        return self.own_px[sym][k - self.skip] / base - 1.0

    def run(self) -> dict:
        sessions = self.sessions
        universe = self.universe
        n = len(sessions)
        signal_set = set(self.signal_indexes)
        cash = 1.0
        shares = {sym: 0.0 for sym in universe}
        last_px: dict[str, float | None] = {sym: None for sym in universe}
        target = {sym: 0.0 for sym in universe}
        dirty = {sym: False for sym in universe}
        forms: dict[str, float] = {}
        open_trips: dict[str, dict] = {}
        closed: list[dict] = []
        scheduled: int | None = None
        scheduled_gap: float | None = None
        scheduled_signal: date | None = None
        pending_hold: dict | None = None
        holds: list[dict] = []
        started = False
        prev_eq = 1.0
        ruined = False
        out_dates: list[date] = []
        out_net: list[float] = []
        out_gross: list[float] = []
        out_eq: list[float] = []
        out_long: list[float] = []
        out_short: list[float] = []
        out_cost: list[float] = []
        out_expo: list[float] = []
        out_nl: list[int] = []
        out_ns: list[int] = []
        out_reb: list[int] = []
        out_index: list[int] = []

        def add_trip_pnl(sym: str, day_i: int, dollars: float) -> None:
            if not self.collect_trips or dollars == 0.0 or sym not in open_trips:
                return
            tr = open_trips[sym]
            tr["gross_pnl"] += dollars
            bucket = tr["pnl_by_day"]
            bucket[day_i] = bucket.get(day_i, 0.0) + dollars

        def close_trip(sym: str, price: float, old_sh: float, reason: str, day_i: int, extra_cost: float) -> None:
            if not self.collect_trips:
                return
            tr = open_trips.pop(sym)
            tr["cost"] += extra_cost
            tr["exit_date"] = sessions[day_i].isoformat()
            tr["exit_price"] = price
            tr["exit_shares"] = old_sh
            tr["exit_reason"] = reason
            tr["exit_index"] = day_i
            tr["net_pnl"] = tr["gross_pnl"] - tr["cost"]
            tr["holding_sessions"] = day_i - tr["entry_index"] + 1
            closed.append(tr)

        def open_trip(sym: str, new_sh: float, price: float, eq_size: float, day_i: int, cost: float) -> None:
            if not self.collect_trips:
                return
            side = "long" if new_sh > 0.0 else "short"
            open_trips[sym] = {
                "symbol": sym,
                "side": side,
                "entry_date": sessions[day_i].isoformat(),
                "entry_price": price,
                "entry_shares": new_sh,
                "entry_equity": eq_size,
                "formation": forms.get(sym),
                "gross_pnl": 0.0,
                "cost": cost,
                "pnl_by_day": {},
                "entry_index": day_i,
            }

        def apply_trade(sym: str, new_sh: float, price: float, eq_size: float, day_i: int) -> float:
            nonlocal cash
            old_sh = shares[sym]
            delta = new_sh - old_sh
            if delta == 0.0:
                return 0.0
            cost = abs(delta) * price * self.cost_rate
            cash -= delta * price + cost
            old_side = 1 if old_sh > 0.0 else (-1 if old_sh < 0.0 else 0)
            new_side = 1 if new_sh > 0.0 else (-1 if new_sh < 0.0 else 0)
            if old_side == 0 and new_side != 0:
                open_trip(sym, new_sh, price, eq_size, day_i, cost)
            elif old_side != 0 and new_side == old_side:
                if self.collect_trips:
                    open_trips[sym]["cost"] += cost
            elif old_side != 0 and new_side == 0:
                close_trip(sym, price, old_sh, "flat", day_i, cost)
            elif old_side != 0 and new_side == -old_side:
                close_cost = abs(old_sh) * price * self.cost_rate
                open_cost = abs(new_sh) * price * self.cost_rate
                close_trip(sym, price, old_sh, "flip", day_i, close_cost)
                open_trip(sym, new_sh, price, eq_size, day_i, open_cost)
            else:
                raise SystemExit(f"unhandled share change {sym}")
            shares[sym] = new_sh
            return cost

        def fill_dirty(eq_size: float, price_of, day_i: int) -> float:
            spent = 0.0
            for sym in universe:
                if not dirty[sym]:
                    continue
                px = price_of(sym)
                if px is None:
                    continue
                new_sh = 0.0 if target[sym] == 0.0 else target[sym] * eq_size / px
                spent += apply_trade(sym, new_sh, px, eq_size, day_i)
                dirty[sym] = False
            return spent

        def snapshot_hold(eq0_now: float, fill_day: date) -> None:
            nonlocal pending_hold
            if self.mode != "ls" or self.rng is not None:
                return
            if pending_hold is not None:
                pending_hold["ret"] = eq0_now / pending_hold["eq0"] - 1.0
                holds.append(pending_hold)
                pending_hold = None
            if scheduled_gap is not None:
                pending_hold = {
                    "signal_date": scheduled_signal.isoformat() if scheduled_signal else None,
                    "fill_date": fill_day.isoformat(),
                    "gap": scheduled_gap,
                    "eq0": eq0_now,
                }

        for i in range(n):
            has = {}
            op = {}
            cl = {}
            for sym in universe:
                o = self.opens[sym][i]
                c = self.closes[sym][i]
                if o is None or c is None:
                    has[sym] = False
                else:
                    if o <= 0.0 or c <= 0.0:
                        raise SystemExit(f"nonpositive price {sym} {sessions[i]}")
                    has[sym] = True
                    op[sym] = float(o)
                    cl[sym] = float(c)

            eq_open = cash
            for sym in universe:
                if shares[sym] == 0.0:
                    continue
                if last_px[sym] is None:
                    raise SystemExit(f"held {sym} without a mark")
                px = op[sym] if has[sym] else last_px[sym]
                eq_open += shares[sym] * px
            if eq_open <= 0.0 and not ruined:
                ruined = True
                for sym in universe:
                    target[sym] = 0.0
                    if shares[sym] != 0.0:
                        dirty[sym] = True

            long_pnl = 0.0
            short_pnl = 0.0
            for sym in universe:
                if shares[sym] == 0.0 or not has[sym]:
                    continue
                gap = shares[sym] * (op[sym] - last_px[sym])
                if shares[sym] > 0.0:
                    long_pnl += gap
                else:
                    short_pnl += gap
                add_trip_pnl(sym, i, gap)

            cost_today = 0.0
            rebalance = 0
            allow_open = self.fill_mode != "signal_close" or (
                scheduled is not None and i >= scheduled and any(dirty.values())
            )
            if scheduled is not None and i >= scheduled and (self.fill_mode != "signal_close" or i > scheduled - 1):
                # next_open / delay: fills from the scheduled index onward.
                # signal_close uses this branch only for names still dirty after a close fill,
                # and `scheduled` then points at the next session, not at today.
                pass
            if self.fill_mode != "signal_close" and scheduled is not None and i >= scheduled:
                if i == scheduled:
                    actionable = any(dirty.values()) or any(abs(target[sym]) > 0.0 for sym in universe)
                    if actionable and not started:
                        started = True
                    if started:
                        snapshot_hold(eq_open, sessions[i])
                if any(dirty.values()):
                    cost_today += fill_dirty(eq_open, lambda sym: op[sym] if has[sym] else None, i)
                    rebalance = 1
            elif self.fill_mode == "signal_close" and scheduled is not None and i >= scheduled and any(dirty.values()):
                if any(has[sym] and dirty[sym] for sym in universe):
                    cost_today += fill_dirty(eq_open, lambda sym: op[sym] if has[sym] else None, i)
                    rebalance = 1

            for sym in universe:
                if not has[sym]:
                    continue
                if shares[sym] != 0.0:
                    oc = shares[sym] * (cl[sym] - op[sym])
                    if shares[sym] > 0.0:
                        long_pnl += oc
                    else:
                        short_pnl += oc
                    add_trip_pnl(sym, i, oc)
                last_px[sym] = cl[sym]

            def marked_equity() -> float:
                eq = cash
                for sym in universe:
                    if shares[sym] == 0.0:
                        continue
                    px = cl[sym] if has[sym] else last_px[sym]
                    eq += shares[sym] * px
                return eq

            eq_close = marked_equity()

            if i in signal_set and not ruined:
                raw_forms: dict[str, float] = {}
                eligible: list[str] = []
                for sym in universe:
                    f = self.formation_at(sym, i)
                    if f is not None:
                        raw_forms[sym] = f
                        eligible.append(sym)
                use_forms = dict(raw_forms)
                if self.rng is not None and eligible:
                    elig = sorted(eligible)
                    vals = [use_forms[s] for s in elig]
                    self.rng.shuffle(vals)
                    for s, v in zip(elig, vals):
                        use_forms[s] = v
                ranked = sorted(eligible, key=lambda s: (-use_forms[s], s))
                new_target = {sym: 0.0 for sym in universe}
                gap_val = None
                if self.mode == "ls":
                    if len(ranked) >= 6:
                        longs = ranked[:3]
                        shorts = ranked[-3:]
                        for s in longs:
                            new_target[s] = 1.0 / 3.0
                        for s in shorts:
                            new_target[s] = -1.0 / 3.0
                        # Gap uses the formation returns that were ranked, after any permutation.
                        gap_val = sum(use_forms[s] for s in longs) / 3.0 - sum(use_forms[s] for s in shorts) / 3.0
                elif ranked:
                    w = 1.0 / len(ranked)
                    for s in ranked:
                        new_target[s] = w
                step = 2 if self.fill_mode == "delay" else 1
                can_fill = (i if self.fill_mode == "signal_close" else i + step) < n or self.fill_mode == "signal_close"
                if self.fill_mode == "signal_close":
                    can_fill = True
                elif i + step >= n:
                    can_fill = False
                if can_fill:
                    forms = use_forms
                    target = new_target
                    for sym in universe:
                        dirty[sym] = shares[sym] != 0.0 or target[sym] != 0.0
                    if self.fill_mode == "signal_close":
                        eq_size = eq_close
                        if not started and any(abs(target[sym]) > 0.0 or shares[sym] != 0.0 for sym in universe):
                            started = True
                        if started:
                            # Snapshot against the close equity before the trade.
                            save_sched_gap, save_sig = scheduled_gap, scheduled_signal
                            scheduled_gap = gap_val
                            scheduled_signal = sessions[i]
                            snapshot_hold(eq_size, sessions[i])
                            scheduled_gap, scheduled_signal = save_sched_gap, save_sig
                        cost_today += fill_dirty(eq_size, lambda sym: cl[sym] if has[sym] else None, i)
                        if any(dirty.values()):
                            scheduled = i + 1 if i + 1 < n else None
                        else:
                            scheduled = None
                        rebalance = 1
                        eq_close = marked_equity()
                        # The hold that just started belongs to this signal, already snapshotted.
                        scheduled_gap = gap_val
                        scheduled_signal = sessions[i]
                    else:
                        scheduled = i + step
                        scheduled_gap = gap_val
                        scheduled_signal = sessions[i]

            if started:
                price_pnl = long_pnl + short_pnl
                if prev_eq == 0.0:
                    raise SystemExit("zero equity denominator")
                out_dates.append(sessions[i])
                out_net.append((eq_close / prev_eq) - 1.0)
                out_gross.append(price_pnl / prev_eq)
                out_eq.append(eq_close)
                out_long.append(long_pnl)
                out_short.append(short_pnl)
                out_cost.append(cost_today)
                gross_notional = 0.0
                nl = ns = 0
                for sym in universe:
                    if shares[sym] == 0.0:
                        continue
                    px = cl[sym] if has[sym] else last_px[sym]
                    gross_notional += abs(shares[sym] * px)
                    if shares[sym] > 0.0:
                        nl += 1
                    else:
                        ns += 1
                out_expo.append(gross_notional / eq_close if eq_close else 0.0)
                out_nl.append(nl)
                out_ns.append(ns)
                out_reb.append(rebalance)
                out_index.append(i)
                if abs((prev_eq + price_pnl - cost_today) - eq_close) > 1e-6:
                    raise SystemExit(
                        f"equity identity failed on {sessions[i]}: "
                        f"{prev_eq + price_pnl - cost_today} vs {eq_close}"
                    )
                prev_eq = eq_close

        if self.collect_trips and out_dates:
            last_i = out_index[-1]
            for sym in list(open_trips):
                px = last_px[sym]
                if px is None:
                    raise SystemExit(f"sample end with no mark for {sym}")
                close_trip(sym, px, shares[sym], "sample_end", last_i, 0.0)
        if pending_hold is not None and out_eq:
            pending_hold["ret"] = out_eq[-1] / pending_hold["eq0"] - 1.0
            holds.append(pending_hold)

        if self.collect_trips and closed:
            net_sum = sum(tr["net_pnl"] for tr in closed)
            if out_eq and abs(net_sum - (out_eq[-1] - 1.0)) > 1e-6:
                raise SystemExit(f"trip net P&L {net_sum} != equity change {out_eq[-1] - 1.0}")

        return {
            "dates": out_dates,
            "net": out_net,
            "gross": out_gross,
            "equity": out_eq,
            "long_pnl": out_long,
            "short_pnl": out_short,
            "cost": out_cost,
            "exposure": out_expo,
            "n_long": out_nl,
            "n_short": out_ns,
            "rebalance": out_reb,
            "index": out_index,
            "trades": closed,
            "holds": holds,
            "ruined": ruined,
        }


def _fail(msg: str) -> None:
    raise SystemExit(f"self-test failed: {msg}")


def _synth(rows: list[dict], symbols: list[str]):
    sessions = [date(2020, 1, 2) + timedelta(days=i) for i in range(len(rows))]
    opens = {s: [] for s in symbols}
    closes = {s: [] for s in symbols}
    for row in rows:
        for s in symbols:
            cell = row.get(s)
            if cell is None:
                opens[s].append(None)
                closes[s].append(None)
            else:
                opens[s].append(cell[0])
                closes[s].append(cell[1])
    return sessions, opens, closes


def _px(symbols: list[str], price, overrides=None):
    row = {s: (price, price) if not isinstance(price, tuple) else price for s in symbols}
    if overrides:
        row.update(overrides)
    return row


def self_test() -> None:
    symbols = ["A", "B", "C", "D", "E", "F"]
    # Day 0 history, day 1 formation, day 2 signal, day 3 entry, day 4 hold with a gap,
    # day 5 second signal that flips the book, day 6 flip fill, day 7 sample end.
    d1 = {"A": (130, 130), "B": (120, 120), "C": (110, 110), "D": (90, 90), "E": (80, 80), "F": (70, 70)}
    d3 = {
        "A": (100, 110), "B": (100, 110), "C": (100, 110),
        "D": (100, 90), "E": (100, 90), "F": (100, 90),
    }
    d4 = {
        "A": (111, 99), "B": (111, 88), "C": (111, 77),
        "D": (89, 108), "E": (89, 117), "F": (89, 126),
    }
    c4 = {s: d4[s][1] for s in symbols}
    flat_from_c4 = {s: (c4[s], c4[s]) for s in symbols}
    rows = [
        _px(symbols, 100),
        {s: d1[s] for s in symbols},
        _px(symbols, 100),
        d3,
        d4,
        flat_from_c4,
        flat_from_c4,
        flat_from_c4,
    ]
    sessions, opens, closes = _synth(rows, symbols)
    book = Book(sessions, opens, closes, [2, 5], symbols, 1, 1, COST, "ls", collect_trips=True)
    res = book.run()
    by = {}
    for tr in res["trades"]:
        by.setdefault(tr["symbol"], []).append(tr)
    for sym, side1, exit_px, side2 in (
        ("A", "long", 99.0, "short"),
        ("B", "long", 88.0, "short"),
        ("C", "long", 77.0, "short"),
        ("D", "short", 108.0, "long"),
        ("E", "short", 117.0, "long"),
        ("F", "short", 126.0, "long"),
    ):
        trips = by.get(sym, [])
        if len(trips) != 2:
            _fail(f"{sym} expected 2 trips, got {len(trips)}")
        a, b = trips
        if a["side"] != side1 or a["entry_date"] != sessions[3].isoformat() or a["entry_price"] != 100.0:
            _fail(f"{sym} entry {a}")
        if a["exit_reason"] != "flip" or a["exit_date"] != sessions[6].isoformat() or a["exit_price"] != exit_px:
            _fail(f"{sym} flip exit {a}")
        if b["side"] != side2 or b["entry_date"] != sessions[6].isoformat() or b["entry_price"] != exit_px:
            _fail(f"{sym} second entry {b}")
        if b["exit_reason"] != "sample_end" or b["exit_date"] != sessions[7].isoformat() or b["exit_price"] != exit_px:
            _fail(f"{sym} sample end {b}")
    if res["dates"][0] != sessions[3] or res["dates"][-1] != sessions[7]:
        _fail("evaluation window")
    # Hand equity after the entry day: cost 6 * (1/3) * 1bp, plus 0.20 of open-to-close.
    if abs(res["equity"][0] - (1.0 - 0.0002 + 0.2)) > 1e-9:
        _fail(f"entry-day equity {res['equity'][0]}")
    # Day-4 gap is +0.02 on the old shares; close-to-close price P&L is -0.49.
    if abs(res["long_pnl"][1] + res["short_pnl"][1] - (-0.49)) > 1e-9:
        _fail(f"hold-day price P&L {res['long_pnl'][1] + res['short_pnl'][1]}")
    if abs(res["equity"][1] - (res["equity"][0] - 0.49)) > 1e-9:
        _fail("hold-day equity")
    # No fill on the signal day.
    if any(tr["entry_date"] == sessions[2].isoformat() for tr in res["trades"]):
        _fail("filled on the signal close")

    # Fewer than six names: the book stays flat and writes no trip.
    five = ["A", "B", "C", "D", "E"]
    rows5 = [_px(five, 100), _px(five, 110), _px(five, 100), _px(five, 100)]
    s5, o5, c5 = _synth(rows5, five)
    res5 = Book(s5, o5, c5, [2], five, 1, 1, COST, "ls").run()
    if res5["trades"] or res5["dates"]:
        _fail("flat book traded")

    # Missing bar while held earns 0, and the name is not exited.
    rows_m = [
        _px(symbols, 100),
        _px(symbols, 100, {"A": (130, 130), "B": (120, 120), "C": (110, 110), "D": (90, 90), "E": (80, 80), "F": (70, 70)}),
        _px(symbols, 100),
        _px(symbols, 100),
        _px(symbols, 100, {"F": None, "A": (100, 110), "D": (100, 90)}),
        _px(symbols, 100),
    ]
    # Day 5 reopens F at 110. The gap is from the last observed close (100), not a filled day-4 close.
    rows_m[5] = _px(symbols, 100, {"F": (110, 110)})
    sm, om, cm = _synth(rows_m, symbols)
    resm = Book(sm, om, cm, [2], symbols, 1, 1, COST, "ls").run()
    fm = [tr for tr in resm["trades"] if tr["symbol"] == "F"]
    if len(fm) != 1 or fm[0]["exit_reason"] != "sample_end" or fm[0]["entry_date"] != sm[3].isoformat():
        _fail(f"missing bar exited F {fm}")
    f_sh = fm[0]["entry_shares"]
    if abs(fm[0]["gross_pnl"] - f_sh * (110.0 - 100.0)) > 1e-9:
        _fail(f"missing-bar gap {fm[0]['gross_pnl']} vs {f_sh}")
    if resm["dates"][1] != sm[4]:
        _fail("missing session dropped from the calendar")
    # Day 4's book P&L is A's and D's moves only. F earns 0 that session.
    a_sh = [tr for tr in resm["trades"] if tr["symbol"] == "A"][0]["entry_shares"]
    d_sh = [tr for tr in resm["trades"] if tr["symbol"] == "D"][0]["entry_shares"]
    day4 = a_sh * (110.0 - 100.0) + d_sh * (90.0 - 100.0)
    if abs(resm["long_pnl"][1] + resm["short_pnl"][1] - day4) > 1e-9:
        _fail(f"missing day P&L {resm['long_pnl'][1] + resm['short_pnl'][1]} vs {day4}")

    # Deferred fill: F has no bar on the scheduled fill and trades the next open.
    rows_d = [
        _px(symbols, 100),
        _px(symbols, 100, d1),
        _px(symbols, 100),
        _px(symbols, 100, {"F": None}),
        _px(symbols, 100),
    ]
    sd, od, cd = _synth(rows_d, symbols)
    resd = Book(sd, od, cd, [2], symbols, 1, 1, COST, "ls").run()
    fd = [tr for tr in resd["trades"] if tr["symbol"] == "F"][0]
    ad = [tr for tr in resd["trades"] if tr["symbol"] == "A"][0]
    if fd["entry_date"] != sd[4].isoformat() or fd["entry_price"] != 100.0 or fd["side"] != "short":
        _fail(f"deferred fill {fd}")
    if ad["entry_date"] != sd[3].isoformat() or ad["exit_shares"] != ad["entry_shares"]:
        _fail("deferred day resized a name that had already filled")

    # Tie-break: equal formation, alphabetical name takes the long slot.
    seven = ["A", "B", "C", "D", "E", "F", "G"]
    form = {"C": 150, "D": 140, "A": 130, "B": 130, "E": 100, "F": 90, "G": 80}
    rows_t = [
        _px(seven, 100),
        {s: (form[s], form[s]) for s in seven},
        _px(seven, 100),
        _px(seven, 100),
    ]
    st, ot, ct = _synth(rows_t, seven)
    rest = Book(st, ot, ct, [2], seven, 1, 1, COST, "ls").run()
    sides = {tr["symbol"]: tr["side"] for tr in rest["trades"]}
    if sides.get("A") != "long" or "B" in sides:
        _fail(f"tie-break {sides}")
    if sides.get("C") != "long" or sides.get("D") != "long":
        _fail(f"top ranks {sides}")
    if sides.get("E") != "short" or sides.get("F") != "short" or sides.get("G") != "short":
        _fail(f"bottom ranks {sides}")

    # Own-bar lag skips a missing session instead of requiring the calendar day.
    # F misses day 2. At the day-3 signal, F's previous own close is day 1 (80),
    # and the close two own bars back is day 0 (100): formation -0.20.
    rows_o = [
        _px(symbols, 100),
        _px(symbols, 100, {"F": (80, 80), "A": (130, 130), "B": (120, 120), "C": (110, 110), "D": (90, 90), "E": (70, 70)}),
        _px(symbols, 100, {"F": None}),
        _px(symbols, 100),
        _px(symbols, 100),
    ]
    so, oo, co = _synth(rows_o, symbols)
    reso = Book(so, oo, co, [3], symbols, 1, 1, COST, "ls").run()
    fo = [tr for tr in reso["trades"] if tr["symbol"] == "F"]
    if len(fo) != 1 or fo[0]["side"] != "short" or abs(fo[0]["formation"] - (-0.20)) > 1e-12:
        _fail(f"own-bar formation {fo}")
    if fo[0]["entry_date"] != so[4].isoformat():
        _fail("own-bar fill timing")

    # Same-side resize is one trip, not two, and the sample still ends the trip.
    rows_r = [
        _px(symbols, 100),
        {s: d1[s] for s in symbols},
        _px(symbols, 100),
        _px(symbols, 100),
        {s: d1[s] for s in symbols},
        _px(symbols, 100),
        _px(symbols, 100),
        _px(symbols, 100),
    ]
    sr, orr, cr = _synth(rows_r, symbols)
    resr = Book(sr, orr, cr, [2, 5], symbols, 1, 1, COST, "ls").run()
    if len(resr["trades"]) != 6:
        _fail(f"resize split a trip: {len(resr['trades'])}")
    if any(tr["exit_reason"] != "sample_end" for tr in resr["trades"]):
        _fail("resize created an exit")
    if any(tr["cost"] <= (1.0 / 3.0) * COST + 1e-15 for tr in resr["trades"]):
        _fail("resize was not charged")

    # A later signal with only five bars flattens the book. Exit reason is flat.
    rows_f = [
        _px(symbols, 100),
        {s: d1[s] for s in symbols},
        _px(symbols, 100),
        _px(symbols, 100),
        _px(symbols, 100),
        _px(symbols, 100, {"F": None}),
        _px(symbols, 100),
        _px(symbols, 100),
    ]
    sf, of_, cf = _synth(rows_f, symbols)
    resf = Book(sf, of_, cf, [2, 5], symbols, 1, 1, COST, "ls").run()
    if len(resf["trades"]) != 6 or any(tr["exit_reason"] != "flat" for tr in resf["trades"]):
        _fail(f"flatten {[tr['exit_reason'] for tr in resf['trades']]}")
    if any(tr["exit_date"] != sf[6].isoformat() for tr in resf["trades"]):
        _fail("flatten timing")
    if resf["n_long"][-1] != 0 or resf["n_short"][-1] != 0:
        _fail("book not flat at the sample end")


def month_signals(spy_dates: list[date], nyse_sessions) -> list[date]:
    spy_set = set(spy_dates)
    first, last = spy_dates[0], spy_dates[-1]
    y, m = first.year, first.month
    out: list[date] = []
    while (y, m) <= (last.year, last.month):
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        days = nyse_sessions(date(y, m, 1), nxt - timedelta(days=1))
        if days and days[-1] <= last:
            for d in reversed(days):
                if d in spy_set:
                    out.append(d)
                    break
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return out


def load_store():
    from mdq import MarketData, nyse_sessions

    opens: dict[str, list] = {}
    closes: dict[str, list] = {}
    with MarketData() as md:
        spy = md.bars("SPY", "1d")
        sessions = [b.session for b in spy]
        spy_close = [b.close for b in spy]
        if not sessions or sessions[-1] != SAMPLE_END:
            raise SystemExit(f"SPY last session {sessions[-1] if sessions else None}")
        actions = {}
        counts = {}
        for sym in UNIVERSE + ["SPY"]:
            bars = md.bars(sym, "1d")
            raw = {b.session: b for b in md.bars(sym, "1d", adjust=False)}
            by = {b.session: b for b in bars}
            counts[sym] = (len(bars), bars[0].session if bars else None, bars[-1].session if bars else None)
            actions[sym] = md.corporate_actions(sym)
            if sym == "SPY":
                continue
            opens[sym] = [by[d].open if d in by else None for d in sessions]
            closes[sym] = [by[d].close if d in by else None for d in sessions]
            if sym in SPLIT_NAMES:
                if SPLIT_EX not in by:
                    raise SystemExit(f"{sym} missing split ex-date")
                prev = max(d for d in by if d < SPLIT_EX)
                ratio = by[SPLIT_EX].close / by[prev].close
                raw_ratio = raw[SPLIT_EX].close / raw[prev].close
                if not (0.9 < ratio < 1.1):
                    raise SystemExit(f"{sym} adjusted close ratio {ratio} on the split")
                if not (0.4 < raw_ratio < 0.6):
                    raise SystemExit(f"{sym} raw close ratio {raw_ratio} is not a 2-for-1")
        signals = month_signals(sessions, nyse_sessions)
    expected_counts = {sym: (3959, date(2011, 1, 4), SAMPLE_END) for sym in UNIVERSE if sym not in ("XLRE", "XLC")}
    expected_counts["SPY"] = (3959, date(2011, 1, 4), SAMPLE_END)
    expected_counts["XLRE"] = (2759, date(2015, 10, 8), SAMPLE_END)
    expected_counts["XLC"] = (2083, date(2018, 6, 19), SAMPLE_END)
    for sym, exp in expected_counts.items():
        if counts[sym] != exp:
            raise SystemExit(f"{sym} bars {counts[sym]} != {exp}")
    for sym in UNIVERSE + ["SPY"]:
        splits = [a for a in actions[sym] if a["type"] == "split"]
        divs = [a for a in actions[sym] if a["type"] != "split"]
        if divs:
            raise SystemExit(f"{sym} has a non-split corporate action")
        if sym in SPLIT_NAMES:
            if len(splits) != 1 or splits[0]["ex_date"] != "2025-12-05" or float(splits[0]["split_ratio"]) != 2.0:
                raise SystemExit(f"{sym} splits {splits}")
        elif splits:
            raise SystemExit(f"{sym} unexpected split {splits}")
    oos_ends = [d for d in signals if OOS_START <= d <= SAMPLE_END]
    if len(oos_ends) != 27:
        raise SystemExit(f"OOS month-ends {len(oos_ends)}")
    if signals[13] != date(2012, 2, 29):
        # The first 13 month-ends (Jan 2011 through Jan 2012) are before anyone has 273 bars.
        raise SystemExit(f"signal index 13 is {signals[13]}")
    if date(2024, 6, 28) not in sessions or OOS_START not in sessions:
        raise SystemExit("sample boundary sessions missing")
    if signals[-1] != date(2026, 9, 30):
        raise SystemExit(f"last signal {signals[-1]}")
    return sessions, opens, closes, spy_close, signals


def trade_rows(trades: list[dict]) -> list[dict]:
    rows = []
    for tr in trades:
        rows.append({k: tr.get(k) for k in TRADE_FIELDS})
    rows.sort(key=lambda r: (r["entry_date"], r["symbol"], r["side"], r["exit_date"]))
    return rows


def metrics_block(dates: list[date], rets: list[float], trades: list[dict] | None) -> dict:
    r = np.asarray(rets, dtype=float)
    d = dates
    full = packet(r)
    is_m = packet(slice_window(d, r, None, date(2024, 6, 28)))
    oos_m = packet(slice_window(d, r, OOS_START, None))
    out = {"full": full, "is": is_m, "oos": oos_m}
    if trades is not None:
        def pack(tr_list: list[dict]) -> dict:
            pnls = [tr["net_pnl"] for tr in tr_list]
            wins = [p for p in pnls if p > 0.0]
            losses = [p for p in pnls if p < 0.0]
            bps = [tr["net_pnl"] / tr["entry_equity"] * 10000.0 for tr in tr_list if tr["entry_equity"]]
            holds = [tr["holding_sessions"] for tr in tr_list]
            return {
                "trades": len(tr_list),
                "profit_factor": profit_factor(pnls),
                "win_rate": (len(wins) / len(tr_list)) if tr_list else None,
                "avg_net_trade_bp": float(np.mean(bps)) if bps else None,
                "avg_winner": float(np.mean(wins)) if wins else None,
                "avg_loser": float(np.mean(losses)) if losses else None,
                "median_holding_sessions": float(np.median(holds)) if holds else None,
                "mean_holding_sessions": float(np.mean(holds)) if holds else None,
            }
        is_tr = [tr for tr in trades if tr["entry_date"] < OOS_START.isoformat()]
        oos_tr = [tr for tr in trades if tr["entry_date"] >= OOS_START.isoformat()]
        out["full"].update(pack(trades))
        out["is"].update(pack(is_tr))
        out["oos"].update(pack(oos_tr))
        by_side = {}
        for side in ("long", "short"):
            by_side[side] = pack([tr for tr in trades if tr["side"] == side])
            by_side[side]["net_pnl"] = sum(tr["net_pnl"] for tr in trades if tr["side"] == side)
            by_side[side]["gross_pnl"] = sum(tr["gross_pnl"] for tr in trades if tr["side"] == side)
        by_reason = {}
        for reason in ("flip", "flat", "sample_end"):
            by_reason[reason] = pack([tr for tr in trades if tr["exit_reason"] == reason])
        out["by_side"] = by_side
        out["by_exit_reason"] = by_reason
    return out


def prediction2(holds: list[dict]) -> dict:
    if not holds:
        return {"n": 0, "consistent": None}
    gaps = np.array([h["gap"] for h in holds], dtype=float)
    rets = np.array([h["ret"] for h in holds], dtype=float)
    med = float(np.median(gaps))
    top = rets[gaps >= med]
    bot = rets[gaps < med]
    mean_top = float(top.mean()) if len(top) else None
    mean_bot = float(bot.mean()) if len(bot) else None
    return {
        "n": int(len(holds)),
        "n_top": int(len(top)),
        "n_bottom": int(len(bot)),
        "median_gap": med,
        "mean_top": mean_top,
        "mean_bottom": mean_bot,
        "consistent": bool(mean_top > mean_bot) if mean_top is not None and mean_bot is not None else None,
    }


def by_year(dates: list[date], strat: np.ndarray, spy: np.ndarray) -> list[dict]:
    years = sorted({d.year for d in dates})
    rows = []
    for y in years:
        idx = [i for i, d in enumerate(dates) if d.year == y]
        rs = strat[idx]
        rb = spy[idx]
        rows.append({
            "year": y,
            "sessions": len(idx),
            "strategy_return": total_return(rs),
            "strategy_sharpe": sharpe(rs),
            "strategy_max_dd": max_drawdown(rs),
            "spy_return": total_return(rb),
            "spy_sharpe": sharpe(rb),
        })
    return rows


def quintiles(dates: list[date], strat: np.ndarray, spy: np.ndarray) -> list[dict]:
    n = len(spy)
    order = np.argsort(spy, kind="mergesort")
    bucket = np.empty(n, dtype=int)
    for k in range(5):
        bucket[order[k * n // 5:(k + 1) * n // 5]] = k + 1
    rows = []
    for k in range(1, 6):
        m = bucket == k
        rows.append({
            "quintile": k,
            "sessions": int(m.sum()),
            "mean_strategy": float(strat[m].mean()),
            "mean_spy": float(spy[m].mean()),
        })
    return rows


def spearman(a: list[float], b: list[float]) -> float | None:
    aa = np.asarray(a, dtype=float)
    bb = np.asarray(b, dtype=float)
    ra = np.argsort(np.argsort(aa)).astype(float)
    rb = np.argsort(np.argsort(bb)).astype(float)
    if float(ra.std()) == 0.0 or float(rb.std()) == 0.0:
        return None
    return float(np.corrcoef(ra, rb)[0, 1])


def clean(obj):
    if isinstance(obj, dict):
        return {k: clean(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [clean(v) for v in obj]
    if isinstance(obj, np.ndarray):
        return clean(obj.tolist())
    if isinstance(obj, (np.floating, float)):
        x = float(obj)
        if math.isnan(x):
            return None
        if math.isinf(x):
            return "inf" if x > 0 else "-inf"
        return x
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, date):
        return obj.isoformat()
    return obj


def write_csv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for row in rows:
            out = {}
            for k in fields:
                v = row[k]
                if isinstance(v, float):
                    out[k] = format(v, ".12g")
                elif isinstance(v, date):
                    out[k] = v.isoformat()
                else:
                    out[k] = v
            w.writerow(out)


def append_runlog(text: str) -> None:
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nAppend-only. One entry per store run.\n\n", encoding="utf-8")
    with path.open("a", encoding="utf-8") as f:
        f.write(text)
        if not text.endswith("\n"):
            f.write("\n")


def run_variant(sessions, opens, closes, sig_idx, lookback, cost, mode, fill_mode, rng=None, collect=False) -> dict:
    return Book(
        sessions, opens, closes, sig_idx, UNIVERSE, lookback, SKIP, cost, mode, fill_mode, rng, collect
    ).run()


def window_sharpe(res: dict, start: date | None, end: date | None, key: str = "net") -> float:
    return sharpe(slice_window(res["dates"], np.asarray(res[key], dtype=float), start, end))


def main() -> None:
    reason = sys.argv[1] if len(sys.argv) > 1 else "initial"
    lock = check_lock()
    self_test()
    if reason == "self-test":
        print("self-test passed")
        return

    sessions, opens, closes, spy_close, signals = load_store()
    index = {d: i for i, d in enumerate(sessions)}
    sig_idx = [index[d] for d in signals]

    primary = run_variant(sessions, opens, closes, sig_idx, LOOKBACK, COST, "ls", "next_open", collect=True)
    ew = run_variant(sessions, opens, closes, sig_idx, LOOKBACK, 0.0, "ew", "next_open", collect=False)
    if primary["dates"][0] != date(2012, 3, 1):
        raise SystemExit(f"first fill {primary['dates'][0]}")
    if primary["dates"][-1] != SAMPLE_END:
        raise SystemExit(f"last session {primary['dates'][-1]}")
    if ew["dates"] != primary["dates"]:
        raise SystemExit("equal-weight calendar does not match the primary")
    if len(primary["holds"]) != 176:
        raise SystemExit(f"holding periods {len(primary['holds'])} != 176 non-flat signals")
    print(
        f"primary {primary['dates'][0]} -> {primary['dates'][-1]} "
        f"sessions {len(primary['dates'])} trades {len(primary['trades'])} holds {len(primary['holds'])}",
        flush=True,
    )
    ew_net = ew["net"]
    ew_eq = ew["equity"]

    spy_by = {d: spy_close[i] for i, d in enumerate(sessions)}
    spy_net = []
    prev_c = None
    # Previous close is the session before the first fill, which is in the store.
    first_i = index[primary["dates"][0]]
    prev_c = spy_close[first_i - 1]
    spy_eq = []
    eq = 1.0
    for d in primary["dates"]:
        c = spy_by[d]
        r = c / prev_c - 1.0
        spy_net.append(r)
        eq *= 1.0 + r
        spy_eq.append(eq)
        prev_c = c
    spy_net_a = np.asarray(spy_net, dtype=float)
    strat = np.asarray(primary["net"], dtype=float)
    gross = np.asarray(primary["gross"], dtype=float)

    trades = trade_rows(primary["trades"])
    perf = metrics_block(primary["dates"], primary["net"], trades)
    perf_gross = metrics_block(primary["dates"], primary["gross"], None)
    perf_spy = metrics_block(primary["dates"], spy_net, None)
    perf_ew = metrics_block(primary["dates"], ew_net, None)

    # Direction placebo on the primary trip dollar paths.
    eval_pos = {idx: k for k, idx in enumerate(primary["index"])}
    n_days = len(primary["dates"])
    n_trips = len(primary["trades"])
    mat = np.zeros((n_trips, n_days), dtype=float)
    for t_i, tr in enumerate(primary["trades"]):
        for day_i, dollars in tr["pnl_by_day"].items():
            k = eval_pos.get(day_i)
            if k is not None:
                mat[t_i, k] += dollars
    prev_eq = np.concatenate([[1.0], np.asarray(primary["equity"][:-1], dtype=float)])
    recon = mat.sum(axis=0)
    price_path = np.asarray(primary["long_pnl"], dtype=float) + np.asarray(primary["short_pnl"], dtype=float)
    if len(recon) and float(np.max(np.abs(recon - price_path))) > 1e-6:
        raise SystemExit("trip price P&L does not add up to the daily long and short books")
    actual_gross = sharpe(gross)
    rng_d = np.random.default_rng(SEED_DIRECTION)
    signs = rng_d.integers(0, 2, size=(N_DIRECTION, n_trips)) * 2 - 1
    signed = signs @ mat
    dir_sharpes = sharpe_rows(signed / prev_eq)
    n_ge = int(np.sum(dir_sharpes >= actual_gross))
    dir_p = (1 + n_ge) / (N_DIRECTION + 1)

    # Block bootstrap of net daily returns.
    rng_b = np.random.default_rng(SEED_BOOT)
    t_len = len(strat)
    n_blocks = (t_len + BLOCK - 1) // BLOCK
    starts = rng_b.integers(0, t_len, size=(N_BOOT, n_blocks))
    offsets = np.arange(BLOCK)
    take = ((starts[..., None] + offsets) % t_len).reshape(N_BOOT, n_blocks * BLOCK)[:, :t_len]
    boot = sharpe_rows(strat[take])
    boot_lo, boot_hi = (float(x) for x in np.quantile(boot, [0.025, 0.975]))

    # Timing placebo. One stream, one rebuild per draw.
    print("timing placebo", flush=True)
    rng_t = np.random.default_rng(SEED_TIMING)
    timing = np.empty(N_TIMING, dtype=float)
    for draw in range(N_TIMING):
        sim = run_variant(
            sessions, opens, closes, sig_idx, LOOKBACK, COST, "ls", "next_open", rng=rng_t, collect=False
        )
        # Gross Sharpe on that draw's own path. Compare like with like to actual_gross,
        # which is the primary path's gross Sharpe over the primary dates. Use the draw's
        # full evaluation window, the same definition.
        timing[draw] = sharpe(np.asarray(sim["gross"], dtype=float))
    n_ge_t = int(np.sum(timing >= actual_gross))
    timing_p = (1 + n_ge_t) / (N_TIMING + 1)

    grid = []
    for look in GRID:
        sim = run_variant(sessions, opens, closes, sig_idx, look, COST, "ls", "next_open", collect=False)
        is_s = window_sharpe(sim, None, date(2024, 6, 28))
        oos_s = window_sharpe(sim, OOS_START, None)
        grid.append({
            "lookback": look,
            "far": look + SKIP,
            "first_date": sim["dates"][0].isoformat() if sim["dates"] else None,
            "is_sharpe": is_s,
            "oos_sharpe": oos_s,
            "full_sharpe": sharpe(np.asarray(sim["net"], dtype=float)),
            "is_return": total_return(slice_window(sim["dates"], np.asarray(sim["net"]), None, date(2024, 6, 28))),
            "oos_return": total_return(slice_window(sim["dates"], np.asarray(sim["net"]), OOS_START, None)),
        })
    is_vals = [g["is_sharpe"] for g in grid]
    oos_vals = [g["oos_sharpe"] for g in grid]
    n_pos = sum(1 for x in is_vals if x > 0.0)
    order_is = sorted(range(len(grid)), key=lambda i: is_vals[i], reverse=True)
    best_is = grid[order_is[0]]
    primary_rank = order_is.index(GRID.index(LOOKBACK)) + 1

    sweep = []
    for mult in (0.0, 0.5, 1.0, 2.0, 3.0):
        if mult == 1.0:
            sim = primary
        else:
            sim = run_variant(sessions, opens, closes, sig_idx, LOOKBACK, COST * mult, "ls", "next_open", collect=False)
        net = np.asarray(sim["net"], dtype=float)
        sweep.append({
            "multiplier": mult,
            "bp": COST * mult * 10000.0,
            "full_sharpe": sharpe(net),
            "oos_sharpe": window_sharpe(sim, OOS_START, None),
            "full_return": total_return(net),
            "oos_return": total_return(slice_window(sim["dates"], net, OOS_START, None)),
        })

    delayed = run_variant(sessions, opens, closes, sig_idx, LOOKBACK, COST, "ls", "delay", collect=False)
    close_fill = run_variant(sessions, opens, closes, sig_idx, LOOKBACK, COST, "ls", "signal_close", collect=False)

    def variant_summary(sim: dict) -> dict:
        net = np.asarray(sim["net"], dtype=float)
        return {
            "first_date": sim["dates"][0].isoformat() if sim["dates"] else None,
            "full_sharpe": sharpe(net),
            "is_sharpe": window_sharpe(sim, None, date(2024, 6, 28)),
            "oos_sharpe": window_sharpe(sim, OOS_START, None),
            "full_return": total_return(net),
            "oos_return": total_return(slice_window(sim["dates"], net, OOS_START, None)),
        }

    long_sum = float(sum(primary["long_pnl"]))
    short_sum = float(sum(primary["short_pnl"]))
    pred1 = long_sum > 0.0 and short_sum > 0.0
    pred2 = prediction2(primary["holds"])
    years = by_year(primary["dates"], strat, spy_net_a)
    quints = quintiles(primary["dates"], strat, spy_net_a)

    oos_sharpe = perf["oos"]["sharpe"]
    oos_pf = perf["oos"]["profit_factor"]
    is_sharpe = perf["is"]["sharpe"]
    full_2x = next(row["full_return"] for row in sweep if row["multiplier"] == 2.0)
    oos_trips = perf["oos"]["trades"]
    line1 = bool(oos_sharpe >= 0.5 and oos_pf >= 1.10)
    line2 = bool(dir_p <= 0.05)
    line3 = bool(is_sharpe > 0.0 and n_pos >= 3)
    line4 = bool(full_2x > 0.0)
    line6 = bool(oos_trips >= 24)
    if not line6:
        status = "Inconclusive"
    elif line1 and line2 and line3 and line4:
        status = "Paper-trading candidate"
    else:
        status = "Rejected"

    exposure = float(np.mean(primary["exposure"])) if primary["exposure"] else 0.0
    time_in = float(np.mean([1.0 if (a or b) else 0.0 for a, b in zip(primary["n_long"], primary["n_short"])]))
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT).stdout.strip())
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")

    results = {
        "rules_sha256": lock["sha256"],
        "locked_utc": lock["locked_utc"],
        "git_head_at_lock": lock["git_head"],
        "git_head": head,
        "git_dirty": dirty,
        "run_utc": now,
        "reason": reason,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOT,
            "timing": SEED_TIMING,
            "verify": 20261014,
        },
        "parameters": {
            "skip": SKIP,
            "lookback": LOOKBACK,
            "far": FAR,
            "cost_bp": 1.0,
            "oos_start": OOS_START.isoformat(),
            "sample_end": SAMPLE_END.isoformat(),
        },
        "window": {
            "first": primary["dates"][0].isoformat(),
            "last": primary["dates"][-1].isoformat(),
            "first_signal": "2012-02-29",
            "last_signal": signals[-1].isoformat(),
            "n_signals": len(signals),
            "ruined": primary["ruined"],
        },
        "strategy": perf,
        "gross": perf_gross,
        "spy": perf_spy,
        "ew": perf_ew,
        "long_gross_pnl": long_sum,
        "short_gross_pnl": short_sum,
        "prediction_1_consistent": pred1,
        "prediction_2": pred2,
        "exposure_mean_gross": exposure,
        "time_in_market": time_in,
        "direction_placebo": {
            "n": N_DIRECTION,
            "actual_gross_sharpe": actual_gross,
            "null_mean": float(np.nanmean(dir_sharpes)),
            "null_p95": float(np.nanquantile(dir_sharpes, 0.95)),
            "p": dir_p,
            "draws": dir_sharpes.tolist(),
        },
        "timing_placebo": {
            "n": N_TIMING,
            "actual_gross_sharpe": actual_gross,
            "null_mean": float(np.nanmean(timing)),
            "null_p95": float(np.nanquantile(timing, 0.95)),
            "p": timing_p,
            "draws": timing.tolist(),
        },
        "bootstrap": {
            "n": N_BOOT,
            "block": BLOCK,
            "p2_5": boot_lo,
            "p97_5": boot_hi,
            "draws": boot.tolist(),
        },
        "grid": grid,
        "grid_is_positive": n_pos,
        "grid_primary_is_rank": primary_rank,
        "grid_best_is": best_is,
        "grid_spearman": spearman(is_vals, oos_vals),
        "cost_sweep": sweep,
        "fill_delay": variant_summary(delayed),
        "signal_close_upper_bound": variant_summary(close_fill),
        "by_year": years,
        "move_quintiles": quints,
        "acceptance": {
            "line_1": line1,
            "line_2": line2,
            "line_3": line3,
            "line_4": line4,
            "line_5": "not_applicable",
            "line_6": line6,
            "oos_sharpe": oos_sharpe,
            "oos_profit_factor": oos_pf,
            "is_sharpe": is_sharpe,
            "grid_positive": n_pos,
            "full_return_2x": full_2x,
            "oos_trips": oos_trips,
            "direction_p": dir_p,
            "status": status,
        },
    }

    daily_rows = []
    for i, d in enumerate(primary["dates"]):
        daily_rows.append({
            "date": d,
            "strategy_net": primary["net"][i],
            "strategy_gross": primary["gross"][i],
            "equity": primary["equity"][i],
            "spy": spy_net[i],
            "spy_equity": spy_eq[i],
            "ew": ew_net[i],
            "ew_equity": ew_eq[i],
            "long_pnl": primary["long_pnl"][i],
            "short_pnl": primary["short_pnl"][i],
            "cost": primary["cost"][i],
            "gross_exposure": primary["exposure"][i],
            "n_long": primary["n_long"][i],
            "n_short": primary["n_short"][i],
            "rebalance": primary["rebalance"][i],
        })

    # Drop the trip dollar maps before writing trades. They are not a column.
    for tr in trades:
        tr.pop("pnl_by_day", None)
        tr.pop("entry_index", None)
        tr.pop("exit_index", None)

    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8")
    write_csv(HERE / "daily.csv", DAILY_FIELDS, daily_rows)
    write_csv(HERE / "trades.csv", TRADE_FIELDS, trades)

    acc = results["acceptance"]
    entry = (
        f"## {now}\n"
        f"- rules_sha256 {lock['sha256']}\n"
        f"- git_head {head} dirty={'yes' if dirty else 'no'}\n"
        f"- reason: {reason}\n"
        f"- full Sharpe {perf['full']['sharpe']:.6f} return {perf['full']['total_return']:.6f} | "
        f"IS Sharpe {perf['is']['sharpe']:.6f} return {perf['is']['total_return']:.6f} | "
        f"OOS Sharpe {perf['oos']['sharpe']:.6f} return {perf['oos']['total_return']:.6f} | "
        f"OOS PF {oos_pf} | OOS trips {oos_trips} | direction p {dir_p:.6f} | status {status}\n"
    )
    append_runlog(entry)
    print(entry)


if __name__ == "__main__":
    main()
