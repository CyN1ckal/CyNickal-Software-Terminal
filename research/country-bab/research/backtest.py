# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered country betting-against-beta backtest.

Run from the repo root:

    python research/country-bab/research/backtest.py
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

UNIVERSE = [
    "EWA", "EWC", "EWG", "EWH", "EWJ", "EWS", "EWU", "EWW", "EWZ", "EWL", "EWT", "EWY",
]
EXPECT_SPLIT = {
    "EWJ": (date(2016, 11, 7), 0.25),
    "EWS": (date(2016, 11, 7), 0.5),
    "EWU": (date(2016, 11, 7), 0.5),
    "EWT": (date(2016, 11, 7), 0.5),
}
BETA_WINDOW = 252
MIN_NAMES = 6
COST = 0.0005
OOS_START = date(2024, 7, 1)
SAMPLE_END = date(2026, 10, 1)
FIRST_SIGNAL = date(2012, 1, 31)
FIRST_FILL = date(2012, 2, 1)
GRID = [126, 189, 252, 315, 378]
SEED_DIRECTION = 20261061
SEED_BOOT = 20261062
SEED_TIMING = 20261063
N_DIRECTION = 2000
N_BOOT = 2000
N_TIMING = 500
BLOCK = 20

TRADE_FIELDS = [
    "symbol", "side", "entry_date", "entry_price", "exit_date", "exit_price",
    "exit_reason", "entry_shares", "exit_shares", "entry_equity", "gross_pnl",
    "cost", "net_pnl", "holding_sessions", "beta",
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
        if not line.strip():
            continue
        key, value = line.split(" ", 1)
        lines[key] = value.strip()
    got = rules_hash()
    if lines.get("sha256") != got:
        raise SystemExit("RULES.md hash does not match RULES.lock; refusing to run")
    return lines


def sharpe(rets: np.ndarray) -> float | None:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0 or math.isnan(sd):
        return None
    return float(r.mean() / sd * math.sqrt(252.0))


def sharpe_rows(mat: np.ndarray) -> np.ndarray:
    mu = mat.mean(axis=1)
    sd = mat.std(axis=1, ddof=1)
    out = np.full(mat.shape[0], np.nan, dtype=float)
    ok = sd > 0.0
    out[ok] = mu[ok] / sd[ok] * math.sqrt(252.0)
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
    total = 1.0 + total_return(r)
    if total <= 0.0:
        return None
    return float(total ** (252.0 / len(r)) - 1.0)


def ann_vol(rets: np.ndarray) -> float | None:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return None
    return float(r.std(ddof=1) * math.sqrt(252.0))


def tstat(rets: np.ndarray) -> float | None:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0:
        return None
    return float(r.mean() / (sd / math.sqrt(len(r))))


def profit_factor(pnls: list[float]) -> float | None:
    if not pnls:
        return None
    losses = sum(p for p in pnls if p < 0.0)
    if losses == 0.0:
        return None
    wins = sum(p for p in pnls if p > 0.0)
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


def align_to(primary_dates: list[date], sim: dict, key: str = "net") -> np.ndarray:
    got = {d: v for d, v in zip(sim["dates"], sim[key])}
    return np.asarray([got.get(d, 0.0) for d in primary_dates], dtype=float)


def ols_beta(name_px: np.ndarray, spy_px: np.ndarray) -> float | None:
    if np.any(name_px <= 0.0) or np.any(spy_px <= 0.0):
        raise SystemExit("nonpositive close inside a beta window")
    ri = name_px[1:] / name_px[:-1] - 1.0
    rs = spy_px[1:] / spy_px[:-1] - 1.0
    rs_c = rs - rs.mean()
    den = float(np.dot(rs_c, rs_c))
    if den == 0.0:
        return None
    ri_c = ri - ri.mean()
    return float(np.dot(ri_c, rs_c) / den)


class Book:
    """Dollar-neutral low-minus-high beta book, or the uncosted equal-weight book."""

    def __init__(
        self,
        sessions: list[date],
        opens: dict[str, list],
        closes: dict[str, list],
        spy: list[float],
        signal_indexes: list[int],
        universe: list[str],
        window: int,
        cost_rate: float,
        mode: str,
        fill_mode: str = "next_open",
        rng: np.random.Generator | None = None,
        collect_trips: bool = True,
    ) -> None:
        self.sessions = sessions
        self.opens = opens
        self.closes = closes
        self.spy = spy
        self.signal_indexes = signal_indexes
        self.universe = universe
        self.window = window
        self.cost_rate = cost_rate
        self.mode = mode
        self.fill_mode = fill_mode
        self.rng = rng
        self.collect_trips = collect_trips
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

    def beta_at(self, sym: str, sidx: int) -> float | None:
        pos = self.own_pos[sym]
        lo, hi = 0, len(pos)
        while lo < hi:
            mid = (lo + hi) // 2
            if pos[mid] < sidx:
                lo = mid + 1
            else:
                hi = mid
        if lo >= len(pos) or pos[lo] != sidx:
            return None
        k = lo
        if k < self.window:
            return None
        sl = pos[k - self.window : k + 1]
        name_px = np.asarray(self.own_px[sym][k - self.window : k + 1], dtype=float)
        spy_px = np.asarray([self.spy[j] for j in sl], dtype=float)
        return ols_beta(name_px, spy_px)

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
        betas: dict[str, float] = {}
        open_trips: dict[str, dict] = {}
        closed: list[dict] = []
        scheduled: int | None = None
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
            open_trips[sym] = {
                "symbol": sym,
                "side": "long" if new_sh > 0.0 else "short",
                "entry_date": sessions[day_i].isoformat(),
                "entry_price": price,
                "entry_shares": new_sh,
                "entry_equity": eq_size,
                "beta": betas.get(sym),
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
                dirty[sym] = False
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
            dirty[sym] = False
            return cost

        def fill_dirty(eq_size: float, price_of, day_i: int) -> tuple[float, int]:
            spent = 0.0
            n_filled = 0
            for sym in universe:
                if not dirty[sym]:
                    continue
                px = price_of(sym)
                if px is None:
                    continue
                new_sh = 0.0 if target[sym] == 0.0 else target[sym] * eq_size / px
                spent += apply_trade(sym, new_sh, px, eq_size, day_i)
                n_filled += 1
            return spent, n_filled

        def marked_equity() -> float:
            eq = cash
            for sym in universe:
                if shares[sym] == 0.0:
                    continue
                px = cl[sym] if has[sym] else last_px[sym]
                eq += shares[sym] * px
            return eq

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
            if ruined and any(dirty.values()):
                spent, n_filled = fill_dirty(eq_open, lambda sym: op[sym] if has[sym] else None, i)
                cost_today += spent
                if n_filled:
                    rebalance = 1
                    started = True
            elif self.fill_mode != "signal_close" and scheduled is not None and i >= scheduled and any(dirty.values()):
                spent, n_filled = fill_dirty(eq_open, lambda sym: op[sym] if has[sym] else None, i)
                cost_today += spent
                if n_filled:
                    rebalance = 1
                    started = True

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

            eq_close = marked_equity()

            if i in signal_set and not ruined:
                raw_beta: dict[str, float] = {}
                eligible: list[str] = []
                for sym in universe:
                    b = self.beta_at(sym, i)
                    if b is not None:
                        raw_beta[sym] = b
                        eligible.append(sym)
                use_beta = dict(raw_beta)
                if self.rng is not None and eligible:
                    elig = sorted(eligible)
                    vals = [use_beta[s] for s in elig]
                    self.rng.shuffle(vals)
                    for s, v in zip(elig, vals):
                        use_beta[s] = v
                ranked = sorted(eligible, key=lambda s: (use_beta[s], s))
                new_target = {sym: 0.0 for sym in universe}
                if self.mode == "ls":
                    if len(ranked) >= MIN_NAMES:
                        for s in ranked[:3]:
                            new_target[s] = 1.0 / 3.0
                        for s in ranked[-3:]:
                            new_target[s] = -1.0 / 3.0
                elif ranked:
                    w = 1.0 / len(ranked)
                    for s in ranked:
                        new_target[s] = w
                step = 2 if self.fill_mode == "delay" else 1
                if self.fill_mode == "signal_close" or i + step < n:
                    betas = use_beta
                    target = new_target
                    for sym in universe:
                        dirty[sym] = shares[sym] != 0.0 or target[sym] != 0.0
                    if self.fill_mode == "signal_close":
                        pre_trade = eq_close
                        spent, n_filled = fill_dirty(
                            pre_trade, lambda sym: cl[sym] if has[sym] else None, i
                        )
                        cost_today += spent
                        if n_filled:
                            rebalance = 1
                            started = True
                        scheduled = i + 1 if any(dirty.values()) and i + 1 < n else None
                        eq_close = marked_equity()
                    else:
                        scheduled = i + step

            if started:
                price_pnl = long_pnl + short_pnl
                if prev_eq == 0.0:
                    raise SystemExit("zero equity denominator")
                if abs((prev_eq + price_pnl - cost_today) - eq_close) > 1e-6:
                    raise SystemExit(
                        f"equity identity failed on {sessions[i]}: "
                        f"{prev_eq + price_pnl - cost_today} vs {eq_close}"
                    )
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
                prev_eq = eq_close

        if self.collect_trips and out_dates:
            last_i = out_index[-1]
            for sym in list(open_trips):
                px = last_px[sym]
                if px is None:
                    raise SystemExit(f"sample end with no mark for {sym}")
                close_trip(sym, px, shares[sym], "sample_end", last_i, 0.0)
        if self.collect_trips and closed and out_eq:
            net_sum = sum(tr["net_pnl"] for tr in closed)
            if abs(net_sum - (out_eq[-1] - 1.0)) > 1e-6:
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
            "ruined": ruined,
        }


def _fail(msg: str) -> None:
    raise SystemExit(f"self-test failed: {msg}")


def _synth(rows: list[dict], symbols: list[str], spy: list[float]):
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
    return sessions, opens, closes, spy


def _path(symbols: list[str], closes_at: dict[str, list[float]], spy: list[float], missing=None):
    """Open equals the previous close. Session 0 opens at its close. `missing` is {(session, sym)}."""
    missing = missing or set()
    n = len(spy)
    rows = []
    prev = {s: None for s in symbols}
    for i in range(n):
        row = {}
        for s in symbols:
            if (i, s) in missing:
                row[s] = None
                continue
            c = closes_at[s][i]
            o = c if prev[s] is None else prev[s]
            row[s] = (o, c)
            prev[s] = c
        rows.append(row)
    return _synth(rows, symbols, spy)


def _factor_closes(beta: float, spy: list[float], start: float = 100.0) -> list[float]:
    px = [start]
    for i in range(1, len(spy)):
        rs = spy[i] / spy[i - 1] - 1.0
        px.append(px[-1] * (1.0 + beta * rs))
    return px


def self_test() -> None:
    symbols = ["A", "B", "C", "D", "E", "F"]
    # SPY returns vary, so a perfect factor has a defined OLS beta.
    spy = [100.0, 110.0, 104.5]
    betas = {"F": -1.0, "D": 0.0, "A": 0.5, "B": 1.0, "C": 1.5, "E": 2.0}
    closes = {s: _factor_closes(betas[s], spy) for s in symbols}
    # Flat fill day, then a common +1% hold. Dollar-neutral equal weights net to 0.
    spy4 = spy + [104.5, 104.5]
    for s in symbols:
        c3 = closes[s][-1]
        closes[s] = closes[s] + [c3, c3 * 1.01]
    sessions, opens, closes_m, spy_m = _path(symbols, closes, spy4)
    res = Book(sessions, opens, closes_m, spy_m, [2], symbols, 2, COST, "ls").run()
    by = {}
    for tr in res["trades"]:
        by.setdefault(tr["symbol"], []).append(tr)
    expect_side = {"F": "long", "D": "long", "A": "long", "B": "short", "C": "short", "E": "short"}
    for sym, side in expect_side.items():
        trips = by.get(sym, [])
        if len(trips) != 1:
            _fail(f"{sym} trips {len(trips)}")
        tr = trips[0]
        if tr["side"] != side or tr["entry_date"] != sessions[3].isoformat():
            _fail(f"{sym} entry {tr['side']} {tr['entry_date']}")
        if abs(tr["entry_price"] - closes[sym][2]) > 1e-9:
            _fail(f"{sym} entry price {tr['entry_price']}")
        if abs(tr["beta"] - betas[sym]) > 1e-9:
            _fail(f"{sym} beta {tr['beta']} != {betas[sym]}")
        if tr["exit_reason"] != "sample_end" or tr["exit_date"] != sessions[4].isoformat():
            _fail(f"{sym} sample end {tr}")
    if any(tr["entry_date"] == sessions[2].isoformat() for tr in res["trades"]):
        _fail("filled on the signal close")
    if abs(res["equity"][0] - (1.0 - 0.001)) > 1e-9:
        _fail(f"entry-day equity {res['equity'][0]}")
    if abs(res["long_pnl"][1] + res["short_pnl"][1]) > 1e-9:
        _fail(f"common-move hold was not flat {res['long_pnl'][1] + res['short_pnl'][1]}")
    if abs(res["equity"][1] - res["equity"][0]) > 1e-9:
        _fail("hold-day equity moved")

    # High past return, low beta, is long. High beta is short. The signal is not the return.
    spy_r = [100.0, 120.0, 108.0]
    spec = {
        "M1": [-0.20, 0.10],
        "Z": [0.30, 0.30],
        "M2": [0.10, -0.05],
        "M3": [0.20, -0.10],
        "H": [0.40, -0.20],
        "M4": [0.60, -0.30],
    }
    names_r = list(spec)
    closes_r = {}
    for s, rets in spec.items():
        px = [100.0]
        for r in rets:
            px.append(px[-1] * (1.0 + r))
        closes_r[s] = px + [px[-1]]
    spy_r = spy_r + [spy_r[-1]]
    sr, or_, cr, spr = _path(names_r, closes_r, spy_r)
    res_r = Book(sr, or_, cr, spr, [2], names_r, 2, COST, "ls").run()
    sides = {tr["symbol"]: tr["side"] for tr in res_r["trades"]}
    if sides.get("Z") != "long" or sides.get("H") != "short" or sides.get("M1") != "long":
        _fail(f"ranked on return instead of beta {sides}")
    if sides.get("M4") != "short" or sides.get("M2") != "long" or sides.get("M3") != "short":
        _fail(f"beta rank {sides}")

    five = ["A", "B", "C", "D", "E"]
    spy5 = [100.0, 110.0, 104.5, 104.5]
    c5 = {s: _factor_closes({"A": 0.2, "B": 0.4, "C": 0.6, "D": 0.8, "E": 1.0}[s], spy5[:3]) + [1.0] for s in five}
    # rebuild properly
    c5 = {}
    for s, b in zip(five, (0.2, 0.4, 0.6, 0.8, 1.0)):
        px = _factor_closes(b, spy5[:3])
        c5[s] = px + [px[-1]]
    s5, o5, c5m, sp5 = _path(five, c5, spy5)
    res5 = Book(s5, o5, c5m, sp5, [2], five, 2, COST, "ls").run()
    if res5["trades"] or res5["dates"]:
        _fail("flat book traded")

    spy_z = [100.0, 100.0, 100.0, 100.0]
    cz = {s: [100.0, 100.0 + i, 100.0, 100.0] for i, s in enumerate(symbols)}
    sz, oz, czm, spz = _path(symbols, cz, spy_z)
    resz = Book(sz, oz, czm, spz, [2], symbols, 2, COST, "ls").run()
    if resz["trades"] or resz["dates"]:
        _fail("zero SPY variance traded")

    # Tie-break: equal beta, earlier symbol takes the long slot. B is the middle name.
    seven = ["A", "B", "C", "D", "E", "F", "G"]
    spy_t = [100.0, 110.0, 104.5, 104.5]
    bt = {"C": 0.0, "D": 0.5, "A": 1.0, "B": 1.0, "E": 2.0, "F": 3.0, "G": 4.0}
    ct = {}
    for s, b in bt.items():
        px = _factor_closes(b, spy_t[:3])
        ct[s] = px + [px[-1]]
    st, ot, ctm, spt = _path(seven, ct, spy_t)
    rest = Book(st, ot, ctm, spt, [2], seven, 2, COST, "ls").run()
    side_t = {tr["symbol"]: tr["side"] for tr in rest["trades"]}
    if side_t.get("A") != "long" or "B" in side_t:
        _fail(f"tie-break {side_t}")
    if side_t.get("C") != "long" or side_t.get("D") != "long":
        _fail(f"low beta {side_t}")
    if side_t.get("E") != "short" or side_t.get("F") != "short" or side_t.get("G") != "short":
        _fail(f"high beta {side_t}")

    # F misses an interior session. The pair drops it. Beta is hand-computed, not forward-filled.
    spy_p = [100.0, 110.0, 121.0, 108.9, 108.9]
    cp = {}
    hand = {
        "A": [100.0, 100.0, 80.0, 96.0],
        "B": [100.0, 100.0, 100.0, 100.0],
        "C": [100.0, 100.0, 110.0, 99.0],
        "D": [100.0, 100.0, 120.0, 96.0],
        "E": [100.0, 100.0, 130.0, 91.0],
        "F": [100.0, None, 100.0, 110.0],
    }
    for s in symbols:
        cp[s] = hand[s] + [hand[s][-1]]
    sp, op, cpm, spp = _path(symbols, {s: [x if x is not None else 0.0 for x in cp[s]] for s in symbols}, spy_p, missing={(1, "F")})
    # _path wrote a 0 close then we marked missing. Rebuild F's closes without the dummy.
    # missing prevents the bar from being stored, so the dummy close is unused. Good.
    resp = Book(sp, op, cpm, spp, [3], symbols, 2, COST, "ls").run()
    fp = [tr for tr in resp["trades"] if tr["symbol"] == "F"]
    want_b = -0.0155 / 0.04805
    if len(fp) != 1 or fp[0]["side"] != "long" or abs(fp[0]["beta"] - want_b) > 1e-9:
        _fail(f"pair-drop beta {fp}")

    # Missing bar while held earns 0. The later gap uses the last observed close.
    spy_m = spy + [spy[-1], spy[-1], spy[-1]]
    cm = {s: _factor_closes(betas[s], spy) for s in symbols}
    for s in symbols:
        c2 = cm[s][-1]
        cm[s] = cm[s] + [c2, c2, c2 * 1.10 if s == "F" else c2]
    sm, om, cmm, spm = _path(symbols, cm, spy_m, missing={(4, "F")})
    resm = Book(sm, om, cmm, spm, [2], symbols, 2, COST, "ls").run()
    fm = [tr for tr in resm["trades"] if tr["symbol"] == "F"][0]
    if fm["exit_reason"] != "sample_end" or fm["entry_date"] != sm[3].isoformat():
        _fail(f"missing bar exited F {fm}")
    # Day index 1 of the result is session 4, the missing day. F earns 0. Others are flat.
    if abs(resm["long_pnl"][1] + resm["short_pnl"][1]) > 1e-9:
        _fail("missing day was not zero")
    if abs(fm["gross_pnl"] - fm["entry_shares"] * (cm["F"][-1] - cm["F"][2])) > 1e-8:
        _fail(f"missing-bar gap {fm['gross_pnl']}")

    # Deferred fill: F has no bar on the scheduled fill and trades the next open.
    spy_d = spy + [spy[-1], spy[-1]]
    cd = {s: _factor_closes(betas[s], spy) for s in symbols}
    for s in symbols:
        cd[s] = cd[s] + [cd[s][-1], cd[s][-1]]
    sd, od, cdm, spd = _path(symbols, cd, spy_d, missing={(3, "F")})
    resd = Book(sd, od, cdm, spd, [2], symbols, 2, COST, "ls").run()
    fd = [tr for tr in resd["trades"] if tr["symbol"] == "F"][0]
    ad = [tr for tr in resd["trades"] if tr["symbol"] == "A"][0]
    if fd["entry_date"] != sd[4].isoformat() or fd["side"] != "long":
        _fail(f"deferred fill {fd}")
    if ad["entry_date"] != sd[3].isoformat() or abs(ad["exit_shares"] - ad["entry_shares"]) > 1e-12:
        _fail("deferred day resized a name that had already filled")

    # Same-side resize is one trip. The second window repeats the factor, so the rank does not flip.
    spy_s = [100.0, 110.0, 104.5]
    spy_s = spy_s + [spy_s[-1] * 1.10, spy_s[-1] * 1.10 * 0.95, spy_s[-1] * 1.10 * 0.95]
    # sessions: 0,1,2 signal, 3 flat fill, 4 and 5 rebuild the same SPY returns, 6 fill, 7 end
    spy_b = [100.0, 110.0, 104.5, 104.5, 114.95, 109.2025, 109.2025, 109.2025]
    cb = {}
    for s, b in betas.items():
        px = _factor_closes(b, spy_b[:3])
        # session 3 flat, then factor off session 3 using the same betas and SPY returns +10%, -5%
        px3 = px[-1]
        px4 = px3 * (1.0 + b * 0.10)
        px5 = px4 * (1.0 + b * -0.05)
        cb[s] = px + [px3, px4, px5, px5, px5]
    sb, ob, cbm, spb = _path(symbols, cb, spy_b)
    resb = Book(sb, ob, cbm, spb, [2, 5], symbols, 2, COST, "ls").run()
    if len(resb["trades"]) != 6:
        _fail(f"resize split a trip: {len(resb['trades'])}")
    if any(tr["exit_reason"] != "sample_end" for tr in resb["trades"]):
        _fail("resize created an exit")
    if any(tr["cost"] <= (1.0 / 3.0) * COST + 1e-15 for tr in resb["trades"]):
        _fail("resize was not charged")

    # A later signal with five names flattens. Exit reason is flat.
    spy_f = spy_b
    cf = {s: list(cb[s]) for s in symbols}
    sf, of_, cfm, spf = _path(symbols, cf, spy_f, missing={(5, "F")})
    resf = Book(sf, of_, cfm, spf, [2, 5], symbols, 2, COST, "ls").run()
    if len(resf["trades"]) != 6 or any(tr["exit_reason"] != "flat" for tr in resf["trades"]):
        _fail(f"flatten {[tr['exit_reason'] for tr in resf['trades']]}")
    if resf["n_long"][-1] != 0 or resf["n_short"][-1] != 0:
        _fail("book not flat at the sample end")

    # One extra session of delay fills two sessions after the signal.
    res_delay = Book(sessions, opens, closes_m, spy_m, [2], symbols, 2, COST, "ls", fill_mode="delay").run()
    if any(tr["entry_date"] != sessions[4].isoformat() for tr in res_delay["trades"]):
        _fail("delay fill timing")

    # Signal-close upper bound fills at the signal close, not the next open.
    res_c = Book(sessions, opens, closes_m, spy_m, [2], symbols, 2, COST, "ls", fill_mode="signal_close").run()
    if res_c["dates"][0] != sessions[2]:
        _fail("signal-close evaluation did not start on the signal")
    if any(tr["entry_date"] != sessions[2].isoformat() or abs(tr["entry_price"] - closes[tr["symbol"]][2]) > 1e-9 for tr in res_c["trades"]):
        _fail("signal-close price")
    if abs(res_c["equity"][0] - (1.0 - 0.001)) > 1e-9:
        _fail(f"signal-close equity {res_c['equity'][0]}")


def month_signals(spy_dates: list[date]) -> list[date]:
    from mdq import nyse_sessions

    spy_set = set(spy_dates)
    first, last = spy_dates[0], min(spy_dates[-1], SAMPLE_END)
    y, m = first.year, first.month
    out: list[date] = []
    while (y, m) <= (last.year, last.month):
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        days = nyse_sessions(date(y, m, 1), nxt - timedelta(days=1))
        if days and days[-1] <= last and days[-1] in spy_set:
            out.append(days[-1])
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return out


def load_store():
    from mdq import MarketData, nyse_sessions

    with MarketData() as md:
        spy_all = md.bars("SPY", "1d")
        if not spy_all or spy_all[0].session != date(2011, 1, 4) or spy_all[-1].session != SAMPLE_END:
            raise SystemExit(f"SPY range {spy_all[0].session if spy_all else None} {spy_all[-1].session if spy_all else None}")
        if len(spy_all) != 3959:
            raise SystemExit(f"SPY bar count {len(spy_all)}")
        sessions = [b.session for b in spy_all]
        if len(sessions) != len(set(sessions)):
            raise SystemExit("duplicate SPY dates")
        spy_close = [float(b.close) for b in spy_all]
        cal = nyse_sessions(sessions[0], SAMPLE_END)
        missing = [d for d in cal if d not in set(sessions)]
        if missing != [date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)]:
            raise SystemExit(f"unexpected SPY gaps {missing}")
        if date(2021, 12, 31) not in set(sessions) or date(2025, 1, 9) in set(sessions):
            raise SystemExit("2021-12-31 or 2025-01-09 calendar check failed")
        if md.corporate_actions("SPY"):
            raise SystemExit("SPY has a corporate action")

        opens: dict[str, list] = {s: [] for s in UNIVERSE}
        closes: dict[str, list] = {s: [] for s in UNIVERSE}
        for sym in UNIVERSE:
            bars = md.bars(sym, "1d")
            raw = md.bars(sym, "1d", adjust=False)
            if len(bars) != 3960 or bars[0].session != date(2011, 1, 4) or bars[-1].session != date(2026, 10, 2):
                raise SystemExit(f"{sym} range {len(bars)} {bars[0].session if bars else None} {bars[-1].session if bars else None}")
            if any(b.session > SAMPLE_END and b.session != date(2026, 10, 2) for b in bars):
                raise SystemExit(f"{sym} unexpected bar after the sample")
            actions = md.corporate_actions(sym)
            expect = EXPECT_SPLIT.get(sym)
            if expect is None:
                if actions:
                    raise SystemExit(f"{sym} unexpected corporate action")
            else:
                if len(actions) != 1 or actions[0]["type"] != "split":
                    raise SystemExit(f"{sym} split set {actions}")
                if actions[0]["ex_date"] != expect[0].isoformat() or actions[0]["split_ratio"] != expect[1]:
                    raise SystemExit(f"{sym} split {actions[0]}")
                ex = expect[0]
                by_a = {b.session: b for b in bars}
                by_r = {b.session: b for b in raw}
                prevs = [b.session for b in bars if b.session < ex]
                if ex not in by_a or not prevs:
                    raise SystemExit(f"{sym} split dates missing")
                prev = prevs[-1]
                adj_ratio = by_a[ex].close / by_a[prev].close
                raw_ratio = by_r[ex].close / by_r[prev].close
                if not (0.90 < adj_ratio < 1.10):
                    raise SystemExit(f"{sym} adjusted split jump {adj_ratio}")
                if abs(raw_ratio - (1.0 / expect[1])) / (1.0 / expect[1]) > 0.05:
                    raise SystemExit(f"{sym} raw split ratio {raw_ratio}")
            kept = [b for b in bars if b.session <= SAMPLE_END]
            if len(kept) != 3959 or [b.session for b in kept] != sessions:
                raise SystemExit(f"{sym} does not match SPY through {SAMPLE_END}")
            for b in kept:
                if b.open <= 0 or b.high <= 0 or b.low <= 0 or b.close <= 0:
                    raise SystemExit(f"{sym} nonpositive {b.session}")
                if b.high + 1e-6 < max(b.open, b.close) or b.low - 1e-6 > min(b.open, b.close):
                    raise SystemExit(f"{sym} high/low {b.session}")
            opens[sym] = [float(b.open) for b in kept]
            closes[sym] = [float(b.close) for b in kept]
        for b in spy_all:
            if b.open <= 0 or b.close <= 0:
                raise SystemExit(f"SPY nonpositive {b.session}")

    signals = month_signals(sessions)
    if len(signals) != 189 or signals[0] != date(2011, 1, 31) or signals[-1] != date(2026, 9, 30):
        raise SystemExit(f"signals {len(signals)} {signals[0] if signals else None} {signals[-1] if signals else None}")
    if SAMPLE_END in signals or date(2026, 10, 2) in signals:
        raise SystemExit("sample end was treated as a month-end")
    if sum(1 for d in signals if d >= OOS_START) != 27:
        raise SystemExit("out-of-sample signal count")
    index = {d: i for i, d in enumerate(sessions)}
    if FIRST_SIGNAL not in index or sessions[index[FIRST_SIGNAL] + 1] != FIRST_FILL:
        raise SystemExit("first fill is not 2012-02-01")
    probe = Book(sessions, opens, closes, spy_close, [], UNIVERSE, BETA_WINDOW, COST, "ls", collect_trips=False)
    n_elig = sum(1 for sym in UNIVERSE if probe.beta_at(sym, index[FIRST_SIGNAL]) is not None)
    if n_elig < MIN_NAMES:
        raise SystemExit(f"first signal eligible {n_elig}")
    prev_sig = signals[signals.index(FIRST_SIGNAL) - 1]
    n_prev = sum(1 for sym in UNIVERSE if probe.beta_at(sym, index[prev_sig]) is not None)
    if n_prev >= MIN_NAMES:
        raise SystemExit(f"eligibility starts before {FIRST_SIGNAL}")
    return sessions, opens, closes, spy_close, signals


def trade_rows(trades: list[dict]) -> list[dict]:
    rows = []
    for tr in trades:
        rows.append({k: tr.get(k) for k in TRADE_FIELDS})
    rows.sort(key=lambda r: (r["entry_date"], r["symbol"], r["side"], r["exit_date"]))
    return rows


def metrics_block(dates: list[date], rets: list[float], trades: list[dict] | None) -> dict:
    r = np.asarray(rets, dtype=float)
    out = {
        "full": packet(r),
        "is": packet(slice_window(dates, r, None, date(2024, 6, 28))),
        "oos": packet(slice_window(dates, r, OOS_START, None)),
    }
    if trades is None:
        return out

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
            "net_pnl": float(sum(pnls)) if pnls else 0.0,
            "gross_pnl": float(sum(tr["gross_pnl"] for tr in tr_list)) if tr_list else 0.0,
        }

    is_tr = [tr for tr in trades if tr["entry_date"] < OOS_START.isoformat()]
    oos_tr = [tr for tr in trades if tr["entry_date"] >= OOS_START.isoformat()]
    out["full"].update(pack(trades))
    out["is"].update(pack(is_tr))
    out["oos"].update(pack(oos_tr))
    out["by_side"] = {}
    for side in ("long", "short"):
        out["by_side"][side] = pack([tr for tr in trades if tr["side"] == side])
    out["by_exit_reason"] = {}
    for reason in ("flip", "flat", "sample_end"):
        out["by_exit_reason"][reason] = pack([tr for tr in trades if tr["exit_reason"] == reason])
    by_symbol = {}
    for sym in UNIVERSE:
        by_symbol[sym] = pack([tr for tr in trades if tr["symbol"] == sym])
    out["by_symbol"] = by_symbol
    return out


def by_year(dates: list[date], strat: np.ndarray, ew: np.ndarray, spy: np.ndarray) -> list[dict]:
    rows = []
    for y in sorted({d.year for d in dates}):
        idx = [i for i, d in enumerate(dates) if d.year == y]
        rs, re, rb = strat[idx], ew[idx], spy[idx]
        rows.append({
            "year": y,
            "sessions": len(idx),
            "strategy_return": total_return(rs),
            "strategy_sharpe": sharpe(rs),
            "strategy_max_dd": max_drawdown(rs),
            "ew_return": total_return(re),
            "spy_return": total_return(rb),
            "spy_sharpe": sharpe(rb),
        })
    return rows


def quintiles(dates: list[date], strat: np.ndarray, spy: np.ndarray) -> list[dict]:
    n = len(spy)
    order = sorted(range(n), key=lambda i: (float(spy[i]), dates[i].isoformat()))
    base, rem = divmod(n, 5)
    sizes = [base + (1 if k < rem else 0) for k in range(5)]
    bucket = [0] * n
    at = 0
    for q, sz in enumerate(sizes, start=1):
        for i in order[at : at + sz]:
            bucket[i] = q
        at += sz
    rows = []
    for q in range(1, 6):
        m = np.asarray([b == q for b in bucket])
        rows.append({
            "quintile": q,
            "sessions": int(m.sum()),
            "mean_strategy": float(strat[m].mean()) if m.any() else None,
            "mean_spy": float(spy[m].mean()) if m.any() else None,
            "strategy_return": total_return(strat[m]) if m.any() else None,
        })
    return rows


def rankdata(a: np.ndarray) -> np.ndarray:
    order = np.argsort(a, kind="mergesort")
    ranks = np.empty(len(a), dtype=float)
    i = 0
    while i < len(a):
        j = i
        while j + 1 < len(a) and a[order[j + 1]] == a[order[i]]:
            j += 1
        avg = 0.5 * (i + j)
        for k in range(i, j + 1):
            ranks[order[k]] = avg
        i = j + 1
    return ranks


def spearman(a: list[float], b: list[float]) -> float | None:
    aa = np.asarray(a, dtype=float)
    bb = np.asarray(b, dtype=float)
    if np.any(~np.isfinite(aa)) or np.any(~np.isfinite(bb)):
        return None
    ra, rb = rankdata(aa), rankdata(bb)
    if float(ra.std()) == 0.0 or float(rb.std()) == 0.0:
        return None
    return float(np.corrcoef(ra, rb)[0, 1])


def pearson(a: np.ndarray, b: np.ndarray) -> float | None:
    aa = np.asarray(a, dtype=float)
    bb = np.asarray(b, dtype=float)
    if len(aa) < 2 or float(aa.std(ddof=1)) == 0.0 or float(bb.std(ddof=1)) == 0.0:
        return None
    return float(np.corrcoef(aa, bb)[0, 1])


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


def run_variant(sessions, opens, closes, spy, sig_idx, window, cost, mode, fill_mode, rng=None, collect=False) -> dict:
    return Book(
        sessions, opens, closes, spy, sig_idx, UNIVERSE, window, cost, mode, fill_mode, rng, collect
    ).run()


def fmt(x) -> str:
    if x is None:
        return "na"
    return f"{x:.6f}"


def main() -> None:
    reason = sys.argv[1] if len(sys.argv) > 1 else "initial pre-registered run"
    lock = check_lock()
    self_test()
    if reason == "self-test":
        print("self-test passed")
        return

    sessions, opens, closes, spy_close, signals = load_store()
    index = {d: i for i, d in enumerate(sessions)}
    sig_idx = [index[d] for d in signals]

    primary = run_variant(sessions, opens, closes, spy_close, sig_idx, BETA_WINDOW, COST, "ls", "next_open", collect=True)
    ew = run_variant(sessions, opens, closes, spy_close, sig_idx, BETA_WINDOW, 0.0, "ew", "next_open", collect=False)
    if not primary["dates"] or primary["dates"][0] != FIRST_FILL or primary["dates"][-1] != SAMPLE_END:
        raise SystemExit(f"evaluation window {primary['dates'][:1]} {primary['dates'][-1:]}")
    if ew["dates"] != primary["dates"]:
        raise SystemExit("equal-weight calendar does not match the primary")
    print(
        f"primary {primary['dates'][0]} -> {primary['dates'][-1]} "
        f"sessions {len(primary['dates'])} trades {len(primary['trades'])}",
        flush=True,
    )

    first_i = index[primary["dates"][0]]
    prev_c = spy_close[first_i - 1]
    spy_net = []
    spy_eq = []
    eq = 1.0
    for d in primary["dates"]:
        c = spy_close[index[d]]
        r = c / prev_c - 1.0
        spy_net.append(r)
        eq *= 1.0 + r
        spy_eq.append(eq)
        prev_c = c
    spy_net_a = np.asarray(spy_net, dtype=float)
    strat = np.asarray(primary["net"], dtype=float)
    gross = np.asarray(primary["gross"], dtype=float)
    ew_net = np.asarray(ew["net"], dtype=float)

    trades = trade_rows(primary["trades"])
    perf = metrics_block(primary["dates"], primary["net"], trades)
    perf_gross = metrics_block(primary["dates"], primary["gross"], None)
    perf_spy = metrics_block(primary["dates"], spy_net, None)
    perf_ew = metrics_block(primary["dates"], ew["net"], None)

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
    if actual_gross is None:
        dir_p = None
        dir_sharpes = np.full(N_DIRECTION, np.nan)
    else:
        rng_d = np.random.default_rng(SEED_DIRECTION)
        signs = rng_d.integers(0, 2, size=(N_DIRECTION, n_trips)) * 2 - 1
        signed = signs @ mat
        dir_sharpes = sharpe_rows(signed / prev_eq)
        n_ge = int(np.sum(np.isfinite(dir_sharpes) & (dir_sharpes >= actual_gross)))
        dir_p = (1 + n_ge) / (N_DIRECTION + 1)

    rng_b = np.random.default_rng(SEED_BOOT)
    t_len = len(strat)
    n_blocks = (t_len + BLOCK - 1) // BLOCK
    starts = rng_b.integers(0, t_len, size=(N_BOOT, n_blocks))
    offsets = np.arange(BLOCK)
    take = ((starts[..., None] + offsets) % t_len).reshape(N_BOOT, n_blocks * BLOCK)[:, :t_len]
    boot = sharpe_rows(strat[take])
    finite_boot = boot[np.isfinite(boot)]
    boot_lo, boot_hi = (float(x) for x in np.quantile(finite_boot, [0.025, 0.975]))
    boot_le0 = float(np.mean(finite_boot <= 0.0)) if len(finite_boot) else None

    print("timing placebo", flush=True)
    rng_t = np.random.default_rng(SEED_TIMING)
    timing = np.empty(N_TIMING, dtype=float)
    for draw in range(N_TIMING):
        sim = run_variant(
            sessions, opens, closes, spy_close, sig_idx, BETA_WINDOW, COST, "ls", "next_open",
            rng=rng_t, collect=False,
        )
        g = sharpe(np.asarray(sim["gross"], dtype=float))
        timing[draw] = 0.0 if g is None else g
    if actual_gross is None:
        timing_p = None
    else:
        n_ge_t = int(np.sum(timing >= actual_gross))
        timing_p = (1 + n_ge_t) / (N_TIMING + 1)

    grid = []
    for look in GRID:
        sim = run_variant(
            sessions, opens, closes, spy_close, sig_idx, look, COST, "ls", "next_open", collect=False
        )
        aligned = align_to(primary["dates"], sim)
        is_r = aligned[np.asarray([d < OOS_START for d in primary["dates"]])]
        oos_r = aligned[np.asarray([d >= OOS_START for d in primary["dates"]])]
        grid.append({
            "window": look,
            "first_date": sim["dates"][0].isoformat() if sim["dates"] else None,
            "is_sharpe": sharpe(is_r),
            "oos_sharpe": sharpe(oos_r),
            "full_sharpe": sharpe(aligned),
            "is_return": total_return(is_r),
            "oos_return": total_return(oos_r),
            "full_return": total_return(aligned),
        })
    cell_252 = next(g for g in grid if g["window"] == BETA_WINDOW)
    if cell_252["is_sharpe"] is None or perf["is"]["sharpe"] is None or abs(cell_252["is_sharpe"] - perf["is"]["sharpe"]) > 1e-12:
        raise SystemExit("252-session grid cell does not match the primary in-sample Sharpe")
    is_vals = [g["is_sharpe"] for g in grid]
    oos_vals = [g["oos_sharpe"] for g in grid]
    n_pos = sum(1 for x in is_vals if x is not None and x > 0.0)
    finite_is = [(i, v) for i, v in enumerate(is_vals) if v is not None]
    order_is = sorted(range(len(grid)), key=lambda i: (is_vals[i] is None, -(is_vals[i] or 0.0)))
    primary_rank = order_is.index(GRID.index(BETA_WINDOW)) + 1

    sweep = []
    for mult in (0.0, 0.5, 1.0, 2.0, 3.0):
        if mult == 1.0:
            sim = primary
        else:
            sim = run_variant(
                sessions, opens, closes, spy_close, sig_idx, BETA_WINDOW, COST * mult, "ls", "next_open",
                collect=False,
            )
        net = align_to(primary["dates"], sim) if mult != 1.0 else np.asarray(sim["net"], dtype=float)
        # A different cost can change the first fill only if the book never starts. Keep the primary calendar.
        if mult != 1.0 and sim["dates"] != primary["dates"]:
            net = align_to(primary["dates"], sim)
        is_mask = np.asarray([d >= OOS_START for d in primary["dates"]])
        sweep.append({
            "multiplier": mult,
            "bp": COST * mult * 10000.0,
            "full_sharpe": sharpe(net),
            "oos_sharpe": sharpe(net[is_mask]),
            "full_return": total_return(net),
            "oos_return": total_return(net[is_mask]),
        })

    delayed = run_variant(
        sessions, opens, closes, spy_close, sig_idx, BETA_WINDOW, COST, "ls", "delay", collect=False
    )
    close_fill = run_variant(
        sessions, opens, closes, spy_close, sig_idx, BETA_WINDOW, COST, "ls", "signal_close", collect=False
    )

    def variant_summary(sim: dict) -> dict:
        net = align_to(primary["dates"], sim)
        mask = np.asarray([d >= OOS_START for d in primary["dates"]])
        return {
            "own_first_date": sim["dates"][0].isoformat() if sim["dates"] else None,
            "sessions": len(sim["dates"]),
            "full_sharpe": sharpe(net),
            "oos_sharpe": sharpe(net[mask]),
            "full_return": total_return(net),
            "oos_return": total_return(net[mask]),
        }

    long_sum = float(sum(primary["long_pnl"]))
    short_sum = float(sum(primary["short_pnl"]))
    corr_s = pearson(strat, spy_net_a)
    corr_e = pearson(ew_net, spy_net_a)
    pred1 = bool(long_sum > 0.0 and short_sum > 0.0)
    if corr_s is None or corr_e is None:
        pred2 = None
    else:
        pred2 = bool(corr_s < corr_e)

    years = by_year(primary["dates"], strat, ew_net, spy_net_a)
    quints = quintiles(primary["dates"], strat, spy_net_a)

    oos_sharpe = perf["oos"]["sharpe"]
    oos_pf = perf["oos"]["profit_factor"]
    is_sharpe = perf["is"]["sharpe"]
    full_2x = next(row["full_return"] for row in sweep if row["multiplier"] == 2.0)
    oos_trips = perf["oos"]["trades"]
    line1 = bool(oos_sharpe is not None and oos_pf is not None and oos_sharpe >= 0.5 and oos_pf >= 1.10)
    line2 = bool(dir_p is not None and dir_p <= 0.05)
    line3 = bool(is_sharpe is not None and is_sharpe > 0.0 and n_pos >= 3)
    line4 = bool(full_2x is not None and full_2x > 0.0)
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
            "verify": 20261064,
        },
        "parameters": {
            "beta_window": BETA_WINDOW,
            "n_long": 3,
            "n_short": 3,
            "min_names": MIN_NAMES,
            "cost_bp": 5.0,
            "oos_start": OOS_START.isoformat(),
            "sample_end": SAMPLE_END.isoformat(),
            "universe": UNIVERSE,
        },
        "window": {
            "first": primary["dates"][0].isoformat(),
            "last": primary["dates"][-1].isoformat(),
            "is_last": "2024-06-28",
            "first_signal": FIRST_SIGNAL.isoformat(),
            "last_signal": signals[-1].isoformat(),
            "n_signals": len(signals),
            "n_oos_signals": sum(1 for d in signals if d >= OOS_START),
            "ruined": primary["ruined"],
        },
        "strategy": perf,
        "gross": perf_gross,
        "spy": perf_spy,
        "ew": perf_ew,
        "long_gross_pnl": long_sum,
        "short_gross_pnl": short_sum,
        "corr_strategy_spy": corr_s,
        "corr_ew_spy": corr_e,
        "prediction_1_consistent": pred1,
        "prediction_2_consistent": pred2,
        "exposure_mean_gross": exposure,
        "time_in_market": time_in,
        "direction_placebo": {
            "n": N_DIRECTION,
            "seed": SEED_DIRECTION,
            "actual_gross_sharpe": actual_gross,
            "null_mean": float(np.nanmean(dir_sharpes)),
            "null_p95": float(np.nanquantile(dir_sharpes, 0.95)),
            "p": dir_p,
            "draws": dir_sharpes.tolist(),
        },
        "timing_placebo": {
            "n": N_TIMING,
            "seed": SEED_TIMING,
            "actual_gross_sharpe": actual_gross,
            "null_mean": float(np.mean(timing)),
            "null_p95": float(np.quantile(timing, 0.95)),
            "p": timing_p,
            "draws": timing.tolist(),
        },
        "bootstrap": {
            "n": N_BOOT,
            "seed": SEED_BOOT,
            "block": BLOCK,
            "p2_5": boot_lo,
            "p97_5": boot_hi,
            "share_sharpe_le_0": boot_le0,
            "draws": boot.tolist(),
        },
        "grid": grid,
        "grid_is_positive": n_pos,
        "grid_primary_is_rank": primary_rank,
        "grid_spearman": spearman(
            [0.0 if v is None else v for v in is_vals],
            [0.0 if v is None else v for v in oos_vals],
        ) if all(v is not None for v in is_vals + oos_vals) else spearman(
            [v if v is not None else float("nan") for v in is_vals],
            [v if v is not None else float("nan") for v in oos_vals],
        ),
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
            "ew": ew["net"][i],
            "ew_equity": ew["equity"][i],
            "long_pnl": primary["long_pnl"][i],
            "short_pnl": primary["short_pnl"][i],
            "cost": primary["cost"][i],
            "gross_exposure": primary["exposure"][i],
            "n_long": primary["n_long"][i],
            "n_short": primary["n_short"][i],
            "rebalance": primary["rebalance"][i],
        })

    for tr in trades:
        tr.pop("pnl_by_day", None)
        tr.pop("entry_index", None)
        tr.pop("exit_index", None)

    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8")
    write_csv(HERE / "daily.csv", DAILY_FIELDS, daily_rows)
    write_csv(HERE / "trades.csv", TRADE_FIELDS, trades)
    np.save(HERE / "placebo_direction.npy", dir_sharpes)
    np.save(HERE / "placebo_timing.npy", timing)

    entry = (
        f"## {now}\n"
        f"- rules_sha256 {lock['sha256']}\n"
        f"- git_head {head} dirty={'yes' if dirty else 'no'}\n"
        f"- reason: {reason}\n"
        f"- full Sharpe {fmt(perf['full']['sharpe'])} return {fmt(perf['full']['total_return'])} | "
        f"IS Sharpe {fmt(perf['is']['sharpe'])} return {fmt(perf['is']['total_return'])} | "
        f"OOS Sharpe {fmt(perf['oos']['sharpe'])} return {fmt(perf['oos']['total_return'])} | "
        f"OOS PF {oos_pf} | OOS trips {oos_trips} | direction p {dir_p} | status {status}\n"
    )
    append_runlog(entry)
    print(entry)


if __name__ == "__main__":
    main()
