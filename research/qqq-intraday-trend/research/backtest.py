# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""QQQ intraday trend following, measured against RULES.md.

Run from the repo root: python research/qqq-intraday-trend/research/backtest.py
Writes results.json, trades_p1.csv, and daily.csv next to this file.
"""

from __future__ import annotations

import csv
import json
import math
import sys
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData, nyse_sessions, rth_window, session_date_of  # noqa: E402

MINUTES = 390
WARMUP = 15
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
ANNUAL = 252
RNG_SEED = 20260926


# --------------------------------------------------------------------------
# Data
# --------------------------------------------------------------------------


@dataclass
class Tape:
    symbol: str
    days: list[date]
    o: np.ndarray  # (N, 390), NaN where no bar
    h: np.ndarray
    l: np.ndarray
    c: np.ndarray
    v: np.ndarray
    nbars: np.ndarray  # last minute index with a bar, + 1
    c_ff: np.ndarray = field(init=False)  # close, forward-filled inside the session
    o_next: np.ndarray = field(init=False)  # open of this or the next existing bar
    day_open: np.ndarray = field(init=False)
    day_close: np.ndarray = field(init=False)
    prev_close: np.ndarray = field(init=False)  # NaN when the previous NYSE session is absent

    def __post_init__(self) -> None:
        n = len(self.days)
        self.c_ff = _ffill(self.c)
        self.o_next = _bfill(self.o)
        self.day_open = np.array([self.o_next[i, 0] for i in range(n)])
        self.day_close = np.array([self.c_ff[i, self.nbars[i] - 1] for i in range(n)])
        self.prev_close = np.full(n, np.nan)
        calendar = nyse_sessions(self.days[0], self.days[-1])
        prev_of = {d: calendar[i - 1] for i, d in enumerate(calendar) if i}
        for i in range(1, n):
            if prev_of.get(self.days[i]) == self.days[i - 1]:
                self.prev_close[i] = self.day_close[i - 1]


def _ffill(a: np.ndarray) -> np.ndarray:
    idx = np.where(np.isnan(a), 0, np.arange(a.shape[1]))
    np.maximum.accumulate(idx, axis=1, out=idx)
    out = a[np.arange(a.shape[0])[:, None], idx]
    return out


def _bfill(a: np.ndarray) -> np.ndarray:
    return _ffill(a[:, ::-1])[:, ::-1]


def load_tape(md: MarketData, symbol: str) -> Tape:
    bars = md.bars(symbol, "1m")
    days: list[date] = []
    rows: dict[date, list] = {}
    start = end = -1
    for b in bars:
        if not (start <= b.ts < end):
            d = session_date_of(b.ts)
            start, end = rth_window(d)
            if d not in rows:
                rows[d] = []
                days.append(d)
        k = (b.ts - start) // 60
        rows[d].append((k, b.open, b.high, b.low, b.close, b.volume))
    n = len(days)
    arrays = [np.full((n, MINUTES), np.nan) for _ in range(5)]
    nbars = np.zeros(n, dtype=int)
    for i, d in enumerate(days):
        for k, *vals in rows[d]:
            if 0 <= k < MINUTES:
                for a, val in zip(arrays, vals):
                    a[i, k] = val
                nbars[i] = max(nbars[i], k + 1)
    o, h, l, c, v = arrays
    return Tape(symbol, days, o, h, l, c, v, nbars)


# --------------------------------------------------------------------------
# Signals
# --------------------------------------------------------------------------


def noise_sigma(t: Tape, lookback: int) -> np.ndarray:
    """sigma[d, k]: mean |close/open - 1| at minute k over the previous `lookback` sessions."""
    move = np.abs(t.c_ff / t.day_open[:, None] - 1.0)
    valid = np.arange(MINUTES)[None, :] < t.nbars[:, None]
    move = np.where(valid, move, np.nan)
    filled = np.nan_to_num(move)
    count = valid.astype(float)
    cs = np.vstack([np.zeros((1, MINUTES)), np.cumsum(filled, axis=0)])
    cn = np.vstack([np.zeros((1, MINUTES)), np.cumsum(count, axis=0)])
    n = len(t.days)
    sig = np.full((n, MINUTES), np.nan)
    for i in range(lookback, n):
        s = cs[i] - cs[i - lookback]
        m = cn[i] - cn[i - lookback]
        with np.errstate(invalid="ignore", divide="ignore"):
            sig[i] = np.where(m > 0, s / m, np.nan)
    return sig


def session_vwap(t: Tape) -> np.ndarray:
    typical = (t.h + t.l + t.c) / 3.0
    vol = np.nan_to_num(t.v)
    pv = np.nan_to_num(typical) * vol
    with np.errstate(invalid="ignore", divide="ignore"):
        vw = np.cumsum(pv, axis=1) / np.cumsum(vol, axis=1)
    return _ffill(vw)


def close_to_close(t: Tape) -> np.ndarray:
    r = np.full(len(t.days), np.nan)
    r[1:] = t.day_close[1:] / t.day_close[:-1] - 1.0
    return r


def vol_target_leverage(t: Tape, target: float = 0.02, cap: float = 2.0, window: int = 14) -> np.ndarray:
    r = close_to_close(t)
    lev = np.full(len(t.days), np.nan)
    for i in range(window + 1, len(t.days)):
        sd = np.std(r[i - window:i], ddof=1)
        lev[i] = min(cap, target / sd) if sd > 0 else cap
    return lev


# --------------------------------------------------------------------------
# Simulation
# --------------------------------------------------------------------------


@dataclass
class Result:
    daily: np.ndarray  # net daily return, 0 on flat or untradeable days
    exposure: np.ndarray  # fraction of the session's minutes in a position
    trades: list[dict]


def run_noise(t: Tape, *, lookback: int = 14, band: float = 1.0, spacing: int = 30,
              cost_bps: float = 1.0, delay: int = 0, leverage: np.ndarray | None = None,
              sigma: np.ndarray | None = None, vwap: np.ndarray | None = None) -> Result:
    sig = noise_sigma(t, lookback) if sigma is None else sigma
    vw = session_vwap(t) if vwap is None else vwap
    cost = cost_bps / 1e4
    n = len(t.days)
    daily = np.zeros(n)
    exposure = np.zeros(n)
    trades: list[dict] = []
    marks = list(range(spacing - 1, 360, spacing))
    for i in range(WARMUP, n):
        pc = t.prev_close[i]
        if math.isnan(pc):
            continue
        lev = 1.0 if leverage is None else leverage[i]
        if math.isnan(lev):
            continue
        nb = t.nbars[i]
        op = t.day_open[i]
        hi_ref, lo_ref = max(op, pc), min(op, pc)
        pos = 0
        entry_px = entry_k = 0.0
        total = 0.0
        held = 0

        def close_trade(px: float, exit_min: int) -> None:
            nonlocal total, held
            gross = lev * pos * (px / entry_px - 1.0)
            net = gross - 2 * cost * lev
            total += net
            held += exit_min - entry_k
            trades.append({
                "day": t.days[i].isoformat(), "side": "long" if pos > 0 else "short",
                "entry_min": int(entry_k), "exit_min": int(exit_min), "entry": entry_px, "exit": px,
                "lev": lev, "gross": gross, "net": net, "i": i,
            })

        for k in marks:
            j = k + 1 + delay
            if j >= nb:
                break
            s = sig[i, k]
            if math.isnan(s):
                continue
            c = t.c_ff[i, k]
            ub = hi_ref * (1 + band * s)
            lb = lo_ref * (1 - band * s)
            v = vw[i, k]
            if c > max(ub, v):
                tgt = 1
            elif c < min(lb, v):
                tgt = -1
            else:
                tgt = 0
            if tgt == pos:
                continue
            px = t.o_next[i, j]
            if math.isnan(px):
                break
            if pos != 0:
                close_trade(px, j)
            pos = tgt
            if pos != 0:
                entry_px, entry_k = px, j
        if pos != 0:
            close_trade(t.day_close[i], nb)
        daily[i] = total
        exposure[i] = held / nb
    return Result(daily, exposure, trades)


def run_gao(t: Tape, cost_bps: float = 1.0) -> Result:
    cost = cost_bps / 1e4
    n = len(t.days)
    daily = np.zeros(n)
    exposure = np.zeros(n)
    trades: list[dict] = []
    for i in range(WARMUP, n):
        pc = t.prev_close[i]
        if math.isnan(pc):
            continue
        nb = t.nbars[i]
        r1 = t.c_ff[i, 29] / pc - 1.0
        if r1 == 0 or math.isnan(r1):
            continue
        side = 1 if r1 > 0 else -1
        j = 360 if nb >= MINUTES else 180
        entry = t.o_next[i, j]
        gross = side * (t.day_close[i] / entry - 1.0)
        net = gross - 2 * cost
        daily[i] = net
        exposure[i] = (nb - j) / nb
        trades.append({"day": t.days[i].isoformat(), "side": "long" if side > 0 else "short",
                       "entry_min": j, "exit_min": int(nb), "entry": entry, "exit": t.day_close[i],
                       "lev": 1.0, "gross": gross, "net": net, "i": i})
    return Result(daily, exposure, trades)


# --------------------------------------------------------------------------
# Metrics
# --------------------------------------------------------------------------


def sharpe(r: np.ndarray) -> float:
    sd = np.std(r, ddof=1)
    return float(np.mean(r) / sd * math.sqrt(ANNUAL)) if sd > 0 else 0.0


def max_drawdown(r: np.ndarray) -> tuple[float, int]:
    eq = np.cumprod(1 + r)
    peak = np.maximum.accumulate(eq)
    dd = eq / peak - 1
    # longest time under water, in sessions
    longest = cur = 0
    for x in dd:
        cur = cur + 1 if x < 0 else 0
        longest = max(longest, cur)
    return float(dd.min()), longest


def metrics(daily: np.ndarray, trades: list[dict], exposure: np.ndarray | None = None) -> dict:
    r = daily
    n = len(r)
    total = float(np.prod(1 + r) - 1)
    years = n / ANNUAL
    mdd, under = max_drawdown(r)
    nets = np.array([x["net"] for x in trades]) if trades else np.zeros(0)
    wins = nets[nets > 0].sum()
    losses = -nets[nets < 0].sum()
    t_stat = float(np.mean(r) / (np.std(r, ddof=1) / math.sqrt(n))) if n > 1 and np.std(r) > 0 else 0.0
    out = {
        "sessions": n,
        "total_return": total,
        "cagr": float((1 + total) ** (1 / years) - 1) if years > 0 and total > -1 else float("nan"),
        "vol": float(np.std(r, ddof=1) * math.sqrt(ANNUAL)),
        "sharpe": sharpe(r),
        "t_stat": t_stat,
        "max_drawdown": mdd,
        "longest_underwater_sessions": under,
        "trades": int(len(nets)),
        "win_rate": float((nets > 0).mean()) if len(nets) else float("nan"),
        "profit_factor": float(wins / losses) if losses > 0 else float("inf"),
        "avg_trade_bps": float(nets.mean() * 1e4) if len(nets) else float("nan"),
        "days_traded": int((r != 0).sum()),
        "best_day": float(r.max()),
        "worst_day": float(r.min()),
    }
    if exposure is not None:
        out["avg_exposure"] = float(exposure.mean())
    return out


def window_mask(days: list[date], lo: date | None, hi: date | None) -> np.ndarray:
    return np.array([(lo is None or d >= lo) and (hi is None or d <= hi) for d in days])


def split_metrics(t: Tape, res: Result, first: int) -> dict:
    days = t.days
    eval_mask = np.arange(len(days)) >= first
    out = {}
    for name, lo, hi in (("full", None, None), ("is", None, IS_END), ("oos", OOS_START, None)):
        m = eval_mask & window_mask(days, lo, hi)
        idx = set(np.nonzero(m)[0])
        tr = [x for x in res.trades if x["i"] in idx]
        out[name] = metrics(res.daily[m], tr, res.exposure[m])
        sel = [days[i] for i in sorted(idx)]
        out[name]["start"], out[name]["end"] = sel[0].isoformat(), sel[-1].isoformat()
    return out


def first_evaluable(t: Tape) -> int:
    return WARMUP


# --------------------------------------------------------------------------
# Checks
# --------------------------------------------------------------------------


def placebo(t: Tape, res: Result, first: int, draws: int = 2000) -> dict:
    rng = np.random.default_rng(RNG_SEED)
    tr = sorted((x for x in res.trades if x["i"] >= first), key=lambda x: x["i"])
    gross = np.array([x["gross"] for x in tr])
    cost = np.array([x["gross"] - x["net"] for x in tr])
    day_idx = np.array([x["i"] for x in tr])
    n = len(t.days) - first
    starts = np.r_[0, np.nonzero(np.diff(day_idx))[0] + 1]
    days_with = day_idx[starts] - first
    signs = rng.choice([-1.0, 1.0], size=(draws, len(tr)))
    per_trade = signs * gross[None, :] - cost[None, :]
    per_day = np.add.reduceat(per_trade, starts, axis=1)
    full = np.zeros((draws, n))
    full[:, days_with] = per_day
    sds = full.std(axis=1, ddof=1)
    sh = full.mean(axis=1) / sds * math.sqrt(ANNUAL)
    actual = sharpe(res.daily[first:])
    return {
        "actual_sharpe": actual,
        "placebo_mean": float(sh.mean()),
        "placebo_p95": float(np.percentile(sh, 95)),
        "placebo_p99": float(np.percentile(sh, 99)),
        "p_value": float((sh >= actual).mean()),
        "draws": draws,
        "hist": np.histogram(sh, bins=40)[0].tolist(),
        "hist_edges": np.histogram(sh, bins=40)[1].tolist(),
    }


def block_bootstrap_sharpe(r: np.ndarray, block: int = 20, draws: int = 2000) -> dict:
    rng = np.random.default_rng(RNG_SEED + 1)
    n = len(r)
    nblocks = math.ceil(n / block)
    out = np.empty(draws)
    for k in range(draws):
        starts = rng.integers(0, n, nblocks)
        idx = (starts[:, None] + np.arange(block)[None, :]).ravel()[:n] % n
        out[k] = sharpe(r[idx])
    return {"lo95": float(np.percentile(out, 2.5)), "hi95": float(np.percentile(out, 97.5)),
            "p_sharpe_le_0": float((out <= 0).mean())}


def by_year(t: Tape, daily: np.ndarray, trades: list[dict], first: int, bench: dict[str, np.ndarray]) -> list[dict]:
    rows = []
    years = sorted({d.year for d in t.days[first:]})
    for y in years:
        m = np.array([d.year == y for d in t.days]) & (np.arange(len(t.days)) >= first)
        idx = set(np.nonzero(m)[0])
        tr = [x for x in trades if x["i"] in idx]
        row = {"year": y, **{k: v for k, v in metrics(daily[m], tr).items()
                             if k in ("sessions", "total_return", "sharpe", "max_drawdown", "trades",
                                      "profit_factor", "win_rate")}}
        for name, br in bench.items():
            row[f"{name}_return"] = float(np.prod(1 + np.nan_to_num(br[m])) - 1)
        rows.append(row)
    return rows


def by_side(trades: list[dict], first: int) -> dict:
    out = {}
    for side in ("long", "short"):
        nets = np.array([x["net"] for x in trades if x["side"] == side and x["i"] >= first])
        out[side] = {
            "trades": int(len(nets)),
            "sum_net": float(nets.sum()),
            "avg_bps": float(nets.mean() * 1e4) if len(nets) else float("nan"),
            "win_rate": float((nets > 0).mean()) if len(nets) else float("nan"),
            "profit_factor": float(nets[nets > 0].sum() / -nets[nets < 0].sum()) if (nets < 0).any() else float("inf"),
        }
    return out


def by_entry(trades: list[dict], first: int) -> list[dict]:
    groups: dict[int, list[float]] = {}
    for x in trades:
        if x["i"] >= first:
            groups.setdefault(x["entry_min"], []).append(x["net"])
    rows = []
    for k in sorted(groups):
        a = np.array(groups[k])
        hh, mm = divmod(9 * 60 + 30 + k, 60)
        rows.append({"entry": f"{hh:02d}:{mm:02d}", "trades": len(a), "sum_net": float(a.sum()),
                     "avg_bps": float(a.mean() * 1e4), "win_rate": float((a > 0).mean())})
    return rows


def by_move_quintile(t: Tape, daily: np.ndarray, first: int) -> list[dict]:
    oc = t.day_close / t.day_open - 1.0
    idx = np.arange(first, len(t.days))
    ok = idx[~np.isnan(oc[idx])]
    absmove = np.abs(oc[ok])
    edges = np.percentile(absmove, [20, 40, 60, 80])
    q = np.searchsorted(edges, absmove, side="right")
    rows = []
    for k in range(5):
        sel = ok[q == k]
        rows.append({"quintile": k + 1, "sessions": int(len(sel)),
                     "abs_open_to_close_bps_lo": float(np.abs(oc[sel]).min() * 1e4),
                     "abs_open_to_close_bps_hi": float(np.abs(oc[sel]).max() * 1e4),
                     "avg_strategy_bps": float(daily[sel].mean() * 1e4),
                     "hit_rate": float((daily[sel] > 0).sum() / max((daily[sel] != 0).sum(), 1)),
                     "share_of_pnl": float(daily[sel].sum() / daily[ok].sum()) if daily[ok].sum() else float("nan")})
    return rows


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------


def main() -> None:
    with MarketData() as md:
        tapes = {s: load_tape(md, s) for s in ("QQQ", "SPY", "IGV")}
        coverage = md.coverage_summary("QQQ", "1m")
    q = tapes["QQQ"]
    first = first_evaluable(q)
    sig14 = noise_sigma(q, 14)
    vw = session_vwap(q)
    lev = vol_target_leverage(q)

    p1 = run_noise(q, sigma=sig14, vwap=vw)
    p2 = run_noise(q, sigma=sig14, vwap=vw, leverage=lev)
    s = run_gao(q)

    cc = close_to_close(q)
    oc = q.day_close / q.day_open - 1.0
    bench_hold = np.nan_to_num(cc)
    bench_oc = np.nan_to_num(oc)
    bench_hold[:first] = 0
    bench_oc[:first] = 0

    ev = slice(first, None)
    res: dict = {
        "rules": "RULES.md",
        "data": {
            "symbol": "QQQ",
            "sessions_loaded": len(q.days),
            "first_session": q.days[0].isoformat(),
            "evaluation_start": q.days[first].isoformat(),
            "evaluation_end": q.days[-1].isoformat(),
            "early_close_sessions": int((q.nbars < MINUTES).sum()),
            "untradeable_no_prev_close": [q.days[i].isoformat() for i in range(1, len(q.days))
                                           if math.isnan(q.prev_close[i])],
            "coverage_needs_attention": coverage["needs_attention"],
            "median_leverage_p2": float(np.nanmedian(lev[first:])),
            "leverage_at_cap_share": float(np.nanmean(lev[first:] >= 2.0 - 1e-12)),
        },
        "P1": split_metrics(q, p1, first),
        "P2": split_metrics(q, p2, first),
        "S": split_metrics(q, s, first),
        "benchmarks": {
            "hold": {k: metrics(bench_hold[m], []) for k, m in _masks(q, first).items()},
            "open_to_close_long": {k: metrics(bench_oc[m], []) for k, m in _masks(q, first).items()},
        },
    }
    res["correlations_full"] = {
        "P1_vs_hold": float(np.corrcoef(p1.daily[ev], bench_hold[ev])[0, 1]),
        "P1_vs_open_to_close": float(np.corrcoef(p1.daily[ev], bench_oc[ev])[0, 1]),
        "P1_vs_S": float(np.corrcoef(p1.daily[ev], s.daily[ev])[0, 1]),
    }
    res["P1_by_year"] = by_year(q, p1.daily, p1.trades, first, {"hold": bench_hold, "oc": bench_oc})
    res["P2_by_year"] = by_year(q, p2.daily, p2.trades, first, {"hold": bench_hold})
    res["S_by_year"] = by_year(q, s.daily, s.trades, first, {})
    res["P1_by_side"] = {"full": by_side(p1.trades, first),
                         "oos": by_side([x for x in p1.trades if q.days[x["i"]] >= OOS_START], first)}
    res["P1_by_entry"] = by_entry(p1.trades, first)
    res["P1_by_move_quintile"] = by_move_quintile(q, p1.daily, first)
    res["P1_hold_minutes"] = {
        "median": float(np.median([x["exit_min"] - x["entry_min"] for x in p1.trades])),
        "mean": float(np.mean([x["exit_min"] - x["entry_min"] for x in p1.trades])),
        "held_to_close_share": float(np.mean([x["exit_min"] == q.nbars[x["i"]] for x in p1.trades])),
    }
    res["P1_placebo"] = placebo(q, p1, first)
    res["P1_bootstrap"] = block_bootstrap_sharpe(p1.daily[ev])
    res["P2_bootstrap"] = block_bootstrap_sharpe(p2.daily[ev])

    # Costs and latency.
    res["P1_costs"] = []
    for bps in (0.0, 0.5, 1.0, 2.0, 3.0):
        r = run_noise(q, sigma=sig14, vwap=vw, cost_bps=bps)
        sm = split_metrics(q, r, first)
        res["P1_costs"].append({"bps": bps, "full_sharpe": sm["full"]["sharpe"],
                                "full_total": sm["full"]["total_return"],
                                "oos_sharpe": sm["oos"]["sharpe"], "oos_total": sm["oos"]["total_return"]})
    r = run_noise(q, sigma=sig14, vwap=vw, delay=1)
    res["P1_delay1"] = split_metrics(q, r, first)
    r = run_noise(q, sigma=sig14, vwap=vw, leverage=lev, cost_bps=2.0)
    res["P2_cost2"] = split_metrics(q, r, first)

    # Grid, on every sample (only IS is used to judge the plateau).
    grid = []
    for spacing in (15, 30, 60):
        for lb in (7, 10, 14, 20, 30):
            sg = sig14 if lb == 14 else noise_sigma(q, lb)
            for band in (0.5, 0.75, 1.0, 1.25, 1.5):
                r = run_noise(q, lookback=lb, band=band, spacing=spacing, sigma=sg, vwap=vw)
                sm = split_metrics(q, r, WARMUP + 16)  # longest lookback + 1
                grid.append({"spacing": spacing, "lookback": lb, "band": band,
                             "is_sharpe": sm["is"]["sharpe"], "oos_sharpe": sm["oos"]["sharpe"],
                             "is_trades": sm["is"]["trades"], "oos_trades": sm["oos"]["trades"],
                             "is_total": sm["is"]["total_return"], "oos_total": sm["oos"]["total_return"]})
    res["grid"] = grid
    best_is = max(grid, key=lambda g: g["is_sharpe"])
    res["grid_summary"] = {
        "cells": len(grid),
        "is_positive_share": float(np.mean([g["is_sharpe"] > 0 for g in grid])),
        "oos_positive_share": float(np.mean([g["oos_sharpe"] > 0 for g in grid])),
        "is_best": best_is,
        "default_rank_is": 1 + sorted((g["is_sharpe"] for g in grid), reverse=True).index(
            next(g["is_sharpe"] for g in grid if g["spacing"] == 30 and g["lookback"] == 14 and g["band"] == 1.0)),
        "is_oos_rank_corr": _spearman([g["is_sharpe"] for g in grid], [g["oos_sharpe"] for g in grid]),
    }

    # Other markets, identical rules.
    res["cross_market"] = {}
    for sym in ("SPY", "IGV"):
        t = tapes[sym]
        r = run_noise(t)
        res["cross_market"][sym] = split_metrics(t, r, WARMUP)
        hold = np.nan_to_num(close_to_close(t))
        res["cross_market"][sym]["hold_full_sharpe"] = sharpe(hold[WARMUP:])

    # Acceptance, exactly as written in RULES.md.
    grid_default_like = res["grid_summary"]["is_positive_share"]
    cost2 = next(c for c in res["P1_costs"] if c["bps"] == 2.0)
    res["acceptance"] = {
        "oos_sharpe_ge_0.5": res["P1"]["oos"]["sharpe"] >= 0.5,
        "oos_profit_factor_ge_1.10": res["P1"]["oos"]["profit_factor"] >= 1.10,
        "placebo_p_le_0.05": res["P1_placebo"]["p_value"] <= 0.05,
        "is_sharpe_gt_0": res["P1"]["is"]["sharpe"] > 0,
        "is_grid_60pct_positive": grid_default_like >= 0.60,
        "positive_at_2bps_full": cost2["full_total"] > 0,
    }
    res["acceptance"]["all"] = all(res["acceptance"].values())

    (HERE / "results.json").write_text(json.dumps(res, indent=2, default=_json_default), encoding="utf-8")
    _write_trades(HERE / "trades_p1.csv", q, p1.trades)
    with open(HERE / "daily.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["session", "p1", "p2", "s", "hold", "open_to_close", "p2_leverage", "p1_exposure"])
        for i in range(first, len(q.days)):
            w.writerow([q.days[i].isoformat(), f"{p1.daily[i]:.8f}", f"{p2.daily[i]:.8f}", f"{s.daily[i]:.8f}",
                        f"{bench_hold[i]:.8f}", f"{bench_oc[i]:.8f}", f"{lev[i]:.4f}", f"{p1.exposure[i]:.4f}"])
    _print_summary(res)


def _masks(t: Tape, first: int) -> dict[str, np.ndarray]:
    ev = np.arange(len(t.days)) >= first
    return {"full": ev, "is": ev & window_mask(t.days, None, IS_END), "oos": ev & window_mask(t.days, OOS_START, None)}


def _spearman(a: list[float], b: list[float]) -> float:
    ra = np.argsort(np.argsort(a))
    rb = np.argsort(np.argsort(b))
    return float(np.corrcoef(ra, rb)[0, 1])


def _json_default(x):
    if isinstance(x, (np.floating, np.integer)):
        return x.item()
    if isinstance(x, np.bool_):
        return bool(x)
    raise TypeError(type(x))


def _write_trades(path: Path, t: Tape, trades: list[dict]) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["session", "side", "entry_ny", "exit_ny", "entry", "exit", "gross_bps", "net_bps"])
        for x in trades:
            if x["i"] < WARMUP:
                continue
            eh, em = divmod(570 + x["entry_min"], 60)
            xh, xm = divmod(570 + x["exit_min"], 60)
            w.writerow([x["day"], x["side"], f"{eh:02d}:{em:02d}", f"{xh:02d}:{xm:02d}",
                        f"{x['entry']:.4f}", f"{x['exit']:.4f}", f"{x['gross'] * 1e4:.2f}", f"{x['net'] * 1e4:.2f}"])


def _print_summary(res: dict) -> None:
    def line(name: str, m: dict) -> str:
        return (f"{name:<22} sh={m['sharpe']:+.2f} tot={m['total_return']:+.1%} cagr={m['cagr']:+.1%} "
                f"mdd={m['max_drawdown']:.1%} trades={m['trades']} pf={m['profit_factor']:.2f} "
                f"avg={m['avg_trade_bps']:.1f}bps")
    for key in ("P1", "P2", "S"):
        for part in ("full", "is", "oos"):
            print(line(f"{key} {part}", res[key][part]))
    for key in ("hold", "open_to_close_long"):
        for part in ("full", "is", "oos"):
            m = res["benchmarks"][key][part]
            print(f"{key + ' ' + part:<22} sh={m['sharpe']:+.2f} tot={m['total_return']:+.1%} mdd={m['max_drawdown']:.1%}")
    print("placebo", {k: v for k, v in res["P1_placebo"].items() if not k.startswith("hist")})
    print("bootstrap P1", res["P1_bootstrap"], "P2", res["P2_bootstrap"])
    print("grid", res["grid_summary"])
    print("costs", res["P1_costs"])
    print("delay1 full/oos sharpe", res["P1_delay1"]["full"]["sharpe"], res["P1_delay1"]["oos"]["sharpe"])
    for sym, m in res["cross_market"].items():
        print(line(f"{sym} full", m["full"]), "oos sh", round(m["oos"]["sharpe"], 2), "hold sh", round(m["hold_full_sharpe"], 2))
    print("acceptance", res["acceptance"])


if __name__ == "__main__":
    main()
