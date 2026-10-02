# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Opening-gap fade on a Finviz sample. The constants mirror RULES.md.

The self-test runs before the store is opened. A store run refuses to start if
RULES.md no longer matches RULES.lock, and appends one entry to RUNLOG.md.

    python research/finviz-gap-up-fade/research/backtest.py
    python research/finviz-gap-up-fade/research/backtest.py self-test
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import statistics
import subprocess
import sys
from dataclasses import dataclass
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent

CONTROLS = ("QQQ", "SPY", "IGV", "AAPL", "AMZN", "META", "GOOGL", "ADBE", "AAL")


def load_books() -> tuple[tuple[str, ...], tuple[str, ...], dict[str, int]]:
    universe = json.loads((HERE / "universe.json").read_text(encoding="utf-8"))
    primary = tuple(universe["primary_kept"])
    cross = tuple(universe["cross_kept"])
    prelock = {k: int(v) for k, v in universe["prelock_signals"].items()}
    return primary, cross, prelock

GAP_MIN = 0.05
GAP_MAX = 1.00
COST_BPS = 20
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
MIN_BARS = 252
ANNUAL = 252
START = date(2016, 1, 4)
EVAL_START = date(2016, 1, 5)
END = date(2026, 9, 25)
OOS_START = date(2024, 1, 2)
DV_WINDOW = 21
DV_MIN_OBS = 15
PRELOCK_SIGNALS: dict[str, int] = {}

SEED_DIRECTION = 20260926
SEED_BOOTSTRAP = 20260927
SEED_TIMING = 20260928
PLACEBO_DRAWS = 2000
TIMING_DRAWS = 500
BOOT_DRAWS = 2000
BOOT_BLOCK = 20
GRID_GAP = (0.03, 0.04, 0.05, 0.07, 0.10)


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


def usable(o: float, h: float, l: float, c: float) -> bool:
    return o > 0 and h > 0 and l > 0 and c > 0 and l <= o <= h and l <= c <= h


def is_gap(open_: float, prior: float, gap_min: float, gap_max: float) -> bool:
    if prior <= 0 or open_ <= 0:
        return False
    ratio = open_ / prior
    return ratio >= 1.0 + gap_min and ratio < 1.0 + gap_max


def close_time(day: date) -> str:
    return "13:00" if day in EARLY_CLOSES else "16:00"


def median(xs: list[float]) -> float:
    return float(statistics.median(xs))


@dataclass
class Book:
    symbols: tuple[str, ...]
    bars: dict[str, dict[date, tuple[float, float, float, float, float]]]


@dataclass
class Sim:
    sessions: list[date]
    ret: np.ndarray
    exposure: np.ndarray
    trades: list[dict]
    ruined: bool


def load_book(md: MarketData, symbols: tuple[str, ...]) -> Book:
    bars: dict[str, dict[date, tuple[float, float, float, float, float]]] = {}
    for sym in symbols:
        series: dict[date, tuple[float, float, float, float, float]] = {}
        for bar in md.bars(sym, "1d", start=START.isoformat(), end=END.isoformat()):
            series[bar.session] = (bar.open, bar.high, bar.low, bar.close, bar.volume)
        bars[sym] = series
    return Book(symbols, bars)


def dollar_volume(book: Book, sym: str, sessions: list[date], i: int) -> float | None:
    lo = max(0, i - DV_WINDOW)
    obs: list[float] = []
    series = book.bars[sym]
    for day in sessions[lo:i]:
        bar = series.get(day)
        if bar is None:
            continue
        c, v = bar[3], bar[4]
        if c > 0 and v is not None and v >= 0:
            obs.append(c * v)
    if len(obs) < DV_MIN_OBS:
        return None
    return median(obs)


def spy_gap_on(spy: dict[date, tuple], day: date, prev: dict[date, date]) -> float | None:
    p = prev.get(day)
    if p is None or day not in spy or p not in spy:
        return None
    prior = spy[p][3]
    open_ = spy[day][0]
    if prior <= 0 or open_ <= 0:
        return None
    return open_ / prior - 1.0


def build_signals(book: Book, sessions: list[date], prev: dict[date, date], index: dict[date, int],
                  gap_min: float, spy: dict[date, tuple] | None) -> dict[date, list[dict]]:
    out: dict[date, list[dict]] = {}
    for day in sessions:
        if day < EVAL_START or day not in prev:
            continue
        p = prev[day]
        spy_g = spy_gap_on(spy, day, prev) if spy else None
        rows = []
        for sym in book.symbols:
            today = book.bars[sym].get(day)
            yday = book.bars[sym].get(p)
            if today is None or yday is None or yday[3] <= 0 or not usable(*today[:4]):
                continue
            if not is_gap(today[0], yday[3], gap_min, GAP_MAX):
                continue
            gap = today[0] / yday[3] - 1.0
            rows.append({
                "symbol": sym,
                "session": day,
                "open": today[0],
                "high": today[1],
                "low": today[2],
                "close": today[3],
                "prior_close": yday[3],
                "gap": gap,
                "dollar_volume": dollar_volume(book, sym, sessions, index[day]),
                "spy_gap": spy_g,
            })
        if rows:
            out[day] = rows
    return out


def signal_count(signals: dict[date, list[dict]]) -> int:
    return sum(len(v) for v in signals.values())


def _finish_day(equity: float, pnl: float) -> tuple[float, float, bool]:
    if equity + pnl <= 0:
        return -1.0, 0.0, True
    return pnl / equity, equity + pnl, False


def simulate(signals: dict[date, list[dict]], eval_sessions: list[date], cost_mult: float,
             mode: str) -> Sim:
    """mode is 'close' or 'gap_fill'. One session, open to that session's exit."""
    equity = 1.0
    ruined = False
    rets = np.zeros(len(eval_sessions))
    exposure = np.zeros(len(eval_sessions))
    trades: list[dict] = []
    rate = cost_mult * COST_BPS / 10000.0
    for i, day in enumerate(eval_sessions):
        if ruined or equity <= 0:
            ruined = True
            continue
        rows = signals.get(day) or []
        if not rows:
            continue
        n = len(rows)
        start_eq = equity
        pnl = 0.0
        staged = []
        for row in rows:
            if mode == "gap_fill" and row["low"] <= row["prior_close"]:
                exit_px, reason = row["prior_close"], "gap_fill"
            else:
                exit_px, reason = row["close"], "session_close"
            notional = start_eq / n
            shares = notional / row["open"]
            entry_cost = rate * notional
            exit_cost = rate * shares * exit_px
            gross = shares * (row["open"] - exit_px)
            net = gross - entry_cost - exit_cost
            pnl += net
            excess = None if row["spy_gap"] is None else row["gap"] - row["spy_gap"]
            staged.append({
                "symbol": row["symbol"],
                "side": "short",
                "entry_session": day.isoformat(),
                "entry_time": "09:30",
                "entry_price": row["open"],
                "exit_session": day.isoformat(),
                "exit_time": close_time(day),
                "exit_price": exit_px,
                "reason": reason,
                "gap": row["gap"],
                "prior_close": row["prior_close"],
                "low": row["low"],
                "gross_ret": gross / notional,
                "net_ret": net / notional,
                "gross_pnl": gross,
                "net_pnl": net,
                "cost": entry_cost + exit_cost,
                "equity_at_entry": start_eq,
                "entry_notional": notional,
                "weight": 1.0 / n,
                "dollar_volume": row["dollar_volume"],
                "spy_gap": row["spy_gap"],
                "excess_gap": excess,
            })
        day_ret, equity, ruined = _finish_day(start_eq, pnl)
        rets[i] = day_ret
        exposure[i] = 1.0
        trades.extend(staged)
    return Sim(eval_sessions, rets, exposure, trades, ruined)


def simulate_delay(signals: dict[date, list[dict]], eval_sessions: list[date], bars_open: dict[str, dict[date, float]],
                   cost_mult: float) -> Sim:
    """Short the signal close, cover the next session's open. P&L lands on the exit session."""
    equity = 1.0
    ruined = False
    rets = np.zeros(len(eval_sessions))
    exposure = np.zeros(len(eval_sessions))
    trades: list[dict] = []
    rate = cost_mult * COST_BPS / 10000.0
    nxt = {eval_sessions[i]: eval_sessions[i + 1] for i in range(len(eval_sessions) - 1)}
    pending: list[dict] = []
    for i, day in enumerate(eval_sessions):
        if ruined or equity <= 0:
            ruined = True
            pending = []
            continue
        pnl = 0.0
        closing = pending
        pending = []
        for t in closing:
            exit_px = bars_open[t["symbol"]][day]
            gross = t["shares"] * (t["entry_price"] - exit_px)
            exit_cost = rate * t["shares"] * exit_px
            net = gross - t["entry_cost"] - exit_cost
            pnl += net
            t["exit_session"] = day.isoformat()
            t["exit_time"] = "09:30"
            t["exit_price"] = exit_px
            t["reason"] = "next_open"
            t["gross_pnl"] = gross
            t["net_pnl"] = net
            t["cost"] = t["entry_cost"] + exit_cost
            t["gross_ret"] = gross / t["entry_notional"]
            t["net_ret"] = net / t["entry_notional"]
            trades.append(t)
        if closing:
            day_ret, equity, ruined = _finish_day(equity, pnl)
            rets[i] = day_ret
            exposure[i] = 1.0
            if ruined:
                continue
        rows = signals.get(day) or []
        nday = nxt.get(day)
        if not rows or nday is None or equity <= 0:
            continue
        alive = []
        for row in rows:
            op = bars_open.get(row["symbol"], {}).get(nday)
            if op is None or op <= 0:
                continue
            alive.append((row, op))
        if not alive:
            continue
        n = len(alive)
        for row, _op in alive:
            notional = equity / n
            shares = notional / row["close"]
            pending.append({
                "symbol": row["symbol"],
                "side": "short",
                "entry_session": day.isoformat(),
                "entry_time": close_time(day),
                "entry_price": row["close"],
                "shares": shares,
                "entry_notional": notional,
                "entry_cost": rate * notional,
                "weight": 1.0 / n,
                "gap": row["gap"],
                "equity_at_entry": equity,
                "prior_close": row["prior_close"],
                "low": row["low"],
                "dollar_volume": row["dollar_volume"],
                "spy_gap": row["spy_gap"],
                "excess_gap": None if row["spy_gap"] is None else row["gap"] - row["spy_gap"],
            })
    return Sim(eval_sessions, rets, exposure, trades, ruined)


def sharpe(r: np.ndarray) -> float:
    if len(r) < 2:
        return float("nan")
    sd = float(r.std(ddof=1))
    if sd <= 0:
        return float("nan")
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def max_drawdown(r: np.ndarray) -> float:
    if len(r) == 0:
        return 0.0
    eq = np.cumprod(1.0 + r)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    with np.errstate(divide="ignore", invalid="ignore"):
        dd = eq / peak - 1.0
    return float(np.nanmin(dd))


def metrics(r: np.ndarray, trades: list[dict]) -> dict:
    total = float(np.prod(1.0 + r) - 1.0) if len(r) else float("nan")
    n = len(r)
    nets = np.array([t["net_ret"] for t in trades], float) if trades else np.zeros(0)
    wins = float(nets[nets > 0].sum()) if len(nets) else 0.0
    losses = float(nets[nets < 0].sum()) if len(nets) else 0.0
    sd = float(r.std(ddof=1)) if n > 1 else float("nan")
    gross = np.array([t["gross_ret"] for t in trades], float) if trades else np.zeros(0)
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
        "avg_gross_bp": float(gross.mean() * 1e4) if len(gross) else float("nan"),
        "avg_winner_bp": float(nets[nets > 0].mean() * 1e4) if np.any(nets > 0) else float("nan"),
        "avg_loser_bp": float(nets[nets < 0].mean() * 1e4) if np.any(nets < 0) else float("nan"),
        "final_equity": float(np.prod(1.0 + r)) if len(r) else float("nan"),
    }


def split_mask(sessions: list[date], which: str) -> np.ndarray:
    if which == "IS":
        return np.array([d < OOS_START for d in sessions])
    if which == "OOS":
        return np.array([d >= OOS_START for d in sessions])
    return np.ones(len(sessions), bool)


def slice_metrics(sim: Sim, which: str) -> dict:
    mask = split_mask(sim.sessions, which)
    trades = []
    for t in sim.trades:
        entry = date.fromisoformat(t["entry_session"])
        keep = which == "full" or (which == "IS" and entry < OOS_START) or (which == "OOS" and entry >= OOS_START)
        if keep:
            trades.append(t)
    out = metrics(sim.ret[mask], trades)
    out["exposure"] = float(sim.exposure[mask].mean()) if mask.any() else float("nan")
    out["trades_per_year"] = float(len(trades) / (out["sessions"] / ANNUAL)) if out["sessions"] else float("nan")
    out["ruined"] = bool(sim.ruined)
    return out


def summarize(sim: Sim) -> dict:
    return {which: slice_metrics(sim, which) for which in ("full", "IS", "OOS")}


def compound_floor(raw: np.ndarray) -> tuple[np.ndarray, bool]:
    equity = 1.0
    out = np.zeros(len(raw))
    ruined = False
    for i, r in enumerate(raw):
        if ruined or equity <= 0:
            ruined = True
            continue
        if equity * (1.0 + r) <= 0:
            out[i] = -1.0
            equity = 0.0
            ruined = True
        else:
            out[i] = r
            equity *= 1.0 + r
    return out, ruined


def bench_intraday(book: Book, eval_sessions: list[date]) -> np.ndarray:
    raw = np.zeros(len(eval_sessions))
    for i, day in enumerate(eval_sessions):
        rs = []
        for sym in book.symbols:
            bar = book.bars[sym].get(day)
            if bar is None or not usable(*bar[:4]):
                continue
            rs.append((bar[0] - bar[3]) / bar[0])
        if rs:
            raw[i] = float(np.mean(rs))
    return compound_floor(raw)[0]


def bench_ew(book: Book, eval_sessions: list[date], prev: dict[date, date]) -> np.ndarray:
    raw = np.zeros(len(eval_sessions))
    for i, day in enumerate(eval_sessions):
        p = prev[day]
        rs = []
        for sym in book.symbols:
            today = book.bars[sym].get(day)
            yday = book.bars[sym].get(p)
            if today is None or yday is None or yday[3] <= 0 or not usable(*today[:4]):
                continue
            rs.append(today[3] / yday[3] - 1.0)
        if rs:
            raw[i] = float(np.mean(rs))
    return compound_floor(raw)[0]


def bench_spy(spy: dict[date, tuple], eval_sessions: list[date], prev: dict[date, date]) -> tuple[np.ndarray, np.ndarray]:
    raw = np.zeros(len(eval_sessions))
    defined = np.zeros(len(eval_sessions))
    for i, day in enumerate(eval_sessions):
        p = prev[day]
        today = spy.get(day)
        yday = spy.get(p)
        if today is None or yday is None or yday[3] <= 0 or not usable(*today[:4]):
            continue
        raw[i] = today[3] / yday[3] - 1.0
        defined[i] = 1.0
    floored, _ = compound_floor(raw)
    # A zero raw return on a day SPY is undefined must stay 0, and the floor
    # only bites if a defined return wipes the benchmark. compound_floor does that.
    # Days before the first SPY print stay 0 because raw is 0 there.
    return floored, defined


def compound_gross(pnls: np.ndarray) -> float:
    equity = 1.0
    rets = np.zeros(len(pnls))
    for i, pnl in enumerate(pnls):
        if equity <= 0:
            break
        rets[i] = pnl / equity
        equity += pnl
    return sharpe(rets)


def direction_placebo(sim: Sim) -> dict:
    n = len(sim.sessions)
    if not sim.trades:
        return {"actual_gross_sharpe": float("nan"), "p": float("nan"), "null_mean": float("nan"), "null": []}
    day_index = {d: i for i, d in enumerate(sim.sessions)}
    base = np.zeros(n)
    per_trade = []
    for t in sim.trades:
        i = day_index[date.fromisoformat(t["entry_session"])]
        per_trade.append((i, t["gross_pnl"]))
        base[i] += t["gross_pnl"]
    actual = compound_gross(base)
    rng = np.random.default_rng(SEED_DIRECTION)
    null = np.empty(PLACEBO_DRAWS)
    beats = 0
    for draw in range(PLACEBO_DRAWS):
        signs = rng.choice(np.array([-1.0, 1.0]), size=len(per_trade))
        pnls = np.zeros(n)
        for (i, pnl), sgn in zip(per_trade, signs):
            pnls[i] += sgn * pnl
        null[draw] = compound_gross(pnls)
        if null[draw] >= actual:
            beats += 1
    return {
        "actual_gross_sharpe": actual,
        "p": (1 + beats) / (PLACEBO_DRAWS + 1),
        "null_mean": float(np.nanmean(null)),
        "null": null.tolist(),
    }


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


def timing_placebo(book: Book, sessions: list[date], prev: dict[date, date], eval_sessions: list[date],
                   signals: dict[date, list[dict]], actual: float) -> dict:
    pools: dict[date, list[str]] = {}
    for day in eval_sessions:
        p = prev[day]
        pool = []
        for sym in book.symbols:
            today = book.bars[sym].get(day)
            yday = book.bars[sym].get(p)
            if today is None or yday is None or yday[3] <= 0 or not usable(*today[:4]):
                continue
            pool.append(sym)
        pools[day] = pool
    rng = np.random.default_rng(SEED_TIMING)
    null = np.empty(TIMING_DRAWS)
    beats = 0
    for draw in range(TIMING_DRAWS):
        forced: dict[date, list[dict]] = {}
        for day in eval_sessions:
            rows = signals.get(day) or []
            k = len(rows)
            if k == 0:
                continue
            pool = pools[day]
            if k > len(pool):
                raise RuntimeError(f"timing pool smaller than the signal count on {day}")
            pick = rng.choice(len(pool), size=k, replace=False)
            chosen = []
            p = prev[day]
            for j in pick:
                sym = pool[int(j)]
                today = book.bars[sym][day]
                yday = book.bars[sym][p]
                chosen.append({
                    "symbol": sym, "session": day, "open": today[0], "high": today[1],
                    "low": today[2], "close": today[3], "prior_close": yday[3],
                    "gap": today[0] / yday[3] - 1.0, "dollar_volume": None, "spy_gap": None,
                })
            forced[day] = chosen
        sim = simulate(forced, eval_sessions, 1.0, "close")
        null[draw] = sharpe(sim.ret)
        if null[draw] >= actual:
            beats += 1
    return {
        "actual_net_sharpe": actual,
        "p": (1 + beats) / (TIMING_DRAWS + 1),
        "null_mean": float(np.nanmean(null)),
        "null": null.tolist(),
    }


def _half_score(hi: list[float], lo: list[float], higher_is_good: bool) -> dict:
    if len(hi) < 30 or len(lo) < 30:
        return {"n_hi": len(hi), "n_lo": len(lo), "mean_hi": _mean(hi), "mean_lo": _mean(lo),
                "score": "not testable"}
    mh, ml = float(np.mean(hi)), float(np.mean(lo))
    ok = mh > ml if higher_is_good else mh < ml
    return {"n_hi": len(hi), "n_lo": len(lo), "mean_hi": mh, "mean_lo": ml,
            "score": "consistent" if ok else "not consistent"}


def _mean(xs: list[float]) -> float:
    return float(np.mean(xs)) if xs else float("nan")


def predictions(book: Book, sessions: list[date], prev: dict[date, date], eval_sessions: list[date],
                signals: dict[date, list[dict]], trades: list[dict], control_trades: list[dict]) -> dict:
    sig_oc: list[float] = []
    oth_oc: list[float] = []
    signal_keys = {(row["symbol"], day) for day, rows in signals.items() for row in rows}
    for day in eval_sessions:
        p = prev[day]
        for sym in book.symbols:
            today = book.bars[sym].get(day)
            yday = book.bars[sym].get(p)
            if today is None or yday is None or yday[3] <= 0 or not usable(*today[:4]):
                continue
            ratio = today[0] / yday[3]
            if ratio >= 1.0 + GAP_MAX:
                continue
            oc = today[3] / today[0] - 1.0
            if (sym, day) in signal_keys:
                sig_oc.append(oc)
            else:
                oth_oc.append(oc)
    if sig_oc and oth_oc:
        p1 = "consistent" if float(np.mean(sig_oc)) < float(np.mean(oth_oc)) else "not consistent"
    else:
        p1 = "not testable"

    gaps = [t["gap"] for t in trades]
    gmed = median(gaps) if gaps else float("nan")
    hi = [t["gross_ret"] for t in trades if t["gap"] >= gmed] if trades else []
    lo = [t["gross_ret"] for t in trades if t["gap"] < gmed] if trades else []
    if hi and lo:
        p2 = "consistent" if float(np.mean(hi)) > float(np.mean(lo)) else "not consistent"
    else:
        p2 = "not testable"

    with_dv = [t for t in trades if t["dollar_volume"] is not None]
    if with_dv:
        dmed = median([t["dollar_volume"] for t in with_dv])
        thin = [t["gross_ret"] for t in with_dv if t["dollar_volume"] <= dmed]
        thick = [t["gross_ret"] for t in with_dv if t["dollar_volume"] > dmed]
    else:
        dmed = float("nan")
        thin, thick = [], []
    p3 = _half_score(thin, thick, True)
    p3_out = {
        "score": p3["score"], "median_dollar_volume": dmed,
        "n_thin": p3["n_hi"], "n_thick": p3["n_lo"],
        "mean_thin": p3["mean_hi"], "mean_thick": p3["mean_lo"],
    }

    if len(control_trades) < 30 or not trades:
        p4_score = "not testable"
    else:
        p4_score = "consistent" if float(np.mean([t["gross_ret"] for t in trades])) > float(
            np.mean([t["gross_ret"] for t in control_trades])) else "not consistent"

    with_spy = [t for t in trades if t["excess_gap"] is not None]
    idio = [t["gross_ret"] for t in with_spy if t["excess_gap"] >= 0.05]
    beta = [t["gross_ret"] for t in with_spy if t["excess_gap"] < 0.05]
    p5 = _half_score(idio, beta, True)
    p5_out = {
        "score": p5["score"], "n_with_spy": len(with_spy),
        "n_idio": p5["n_hi"], "n_market": p5["n_lo"],
        "mean_idio": p5["mean_hi"], "mean_market": p5["mean_lo"],
    }

    filled = sum(1 for t in trades if t["low"] <= t["prior_close"])
    share = filled / len(trades) if trades else float("nan")
    p6 = "consistent" if trades and share > 0.5 else ("not testable" if not trades else "not consistent")

    return {
        "1_gap_filter": {"score": p1, "signal_mean_oc": _mean(sig_oc), "other_mean_oc": _mean(oth_oc),
                         "n_signal": len(sig_oc), "n_other": len(oth_oc)},
        "2_larger_gaps": {"score": p2, "median_gap": gmed, "n_hi": len(hi), "n_lo": len(lo),
                          "mean_hi": _mean(hi), "mean_lo": _mean(lo)},
        "3_thinner_names": p3_out,
        "4_versus_controls": {
            "score": p4_score,
            "primary_mean_gross": _mean([t["gross_ret"] for t in trades]),
            "control_mean_gross": _mean([t["gross_ret"] for t in control_trades]),
            "n_primary": len(trades), "n_control": len(control_trades),
        },
        "5_idiosyncratic": p5_out,
        "6_trades_through_prior_close": {"score": p6, "share": share, "n": len(trades), "n_filled": filled},
    }


def breakdowns(sim: Sim, spy_ret: np.ndarray, spy_defined: np.ndarray) -> dict:
    by_year = []
    years = sorted({d.year for d in sim.sessions})
    for year in years:
        mask = np.array([d.year == year for d in sim.sessions])
        trades = [t for t in sim.trades if date.fromisoformat(t["entry_session"]).year == year]
        m = metrics(sim.ret[mask], trades)
        by_year.append({"year": year, "return": m["total_return"], "sharpe": m["sharpe"],
                        "max_dd": m["max_dd"], "trades": m["trades"], "avg_net_bp": m["avg_net_bp"]})
    reasons: dict[str, list[dict]] = {}
    for t in sim.trades:
        reasons.setdefault(t["reason"], []).append(t)
    by_reason = []
    for reason, rows in sorted(reasons.items()):
        m = metrics(np.zeros(0), rows)
        by_reason.append({"reason": reason, "trades": len(rows), "avg_net_bp": m["avg_net_bp"],
                          "avg_gross_bp": m["avg_gross_bp"], "profit_factor": m["profit_factor"],
                          "win_rate": m["win_rate"]})
    buckets = [("05_10", 0.05, 0.10), ("10_20", 0.10, 0.20), ("20_50", 0.20, 0.50), ("50_100", 0.50, 1.00)]
    by_gap = []
    for name, lo, hi in buckets:
        rows = [t for t in sim.trades if lo <= t["gap"] < hi]
        m = metrics(np.zeros(0), rows)
        by_gap.append({"bucket": name, "trades": len(rows), "avg_gross_bp": m["avg_gross_bp"],
                       "avg_net_bp": m["avg_net_bp"], "win_rate": m["win_rate"]})
    defined_idx = [i for i, flag in enumerate(spy_defined) if flag == 1.0]
    quintiles = []
    if defined_idx:
        order = sorted(defined_idx, key=lambda i: (spy_ret[i], i))
        n = len(order)
        cuts = [round(k * n / 5) for k in range(6)]
        for q in range(5):
            members = order[cuts[q]:cuts[q + 1]]
            days = {sim.sessions[i] for i in members}
            trades = [t for t in sim.trades if date.fromisoformat(t["entry_session"]) in days]
            chunk = sim.ret[np.array(members)]
            quintiles.append({
                "quintile": q + 1,
                "label": "lowest SPY return" if q == 0 else ("highest SPY return" if q == 4 else ""),
                "sessions": len(members),
                "spy_mean": float(np.mean(spy_ret[np.array(members)])),
                "strategy_mean": float(np.mean(chunk)) if len(chunk) else float("nan"),
                "strategy_return": float(np.prod(1.0 + chunk) - 1.0) if len(chunk) else float("nan"),
                "trades": len(trades),
            })
    no_spy_idx = [i for i, flag in enumerate(spy_defined) if flag == 0.0]
    no_spy_trades = [t for t in sim.trades if spy_defined[sim.sessions.index(date.fromisoformat(t["entry_session"]))] == 0.0]
    return {
        "by_year": by_year,
        "by_reason": by_reason,
        "by_gap": by_gap,
        "spy_quintiles": quintiles,
        "no_spy": {"sessions": len(no_spy_idx), "trades": len(no_spy_trades)},
        "short_trades": len(sim.trades),
        "long_trades": 0,
    }


def clean(obj):
    if isinstance(obj, dict):
        return {k: clean(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [clean(v) for v in obj]
    if isinstance(obj, float):
        if math.isnan(obj) or math.isinf(obj):
            return None
        return obj
    if isinstance(obj, (np.floating,)):
        v = float(obj)
        return None if math.isnan(v) or math.isinf(v) else v
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


def require_history(md: MarketData, symbols: tuple[str, ...]) -> None:
    for sym in symbols:
        md.resolve(sym)
        bars = md.bars(sym, "1d", start=START.isoformat(), end=END.isoformat())
        if len(bars) < MIN_BARS:
            sys.exit(f"{sym} has {len(bars)} daily bars, under the {MIN_BARS} floor. Refusing to write results.")
        for bar in bars:
            if bar.open <= 0 or bar.close <= 0:
                sys.exit(f"nonpositive price {sym} {bar.session}. Refusing to write results.")


def opens_of(book: Book) -> dict[str, dict[date, float]]:
    return {sym: {day: bar[0] for day, bar in series.items()} for sym, series in book.bars.items()}


def store_run(reason: str, rules_sha: str) -> None:
    global PRIMARY, CROSS, PRELOCK_SIGNALS
    PRIMARY, CROSS, PRELOCK_SIGNALS = load_books()
    sessions = nyse_sessions(START, END)
    if sessions[0] != START or sessions[-1] != END or date(2016, 1, 5) not in sessions:
        sys.exit("NYSE session bounds do not match the locked window. Refusing to write results.")
    prev = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    index = {day: i for i, day in enumerate(sessions)}
    eval_sessions = [d for d in sessions if d >= EVAL_START]
    with MarketData() as md:
        require_history(md, PRIMARY + CROSS)
        md.resolve("SPY")
        primary = load_book(md, PRIMARY)
        cross = load_book(md, CROSS)
        controls = load_book(md, CONTROLS)
        spy = controls.bars["SPY"]
    books = {"primary": primary, "cross": cross, "control": controls}
    built = {name: build_signals(book, sessions, prev, index, GAP_MIN, spy) for name, book in books.items()}
    for name, expected in PRELOCK_SIGNALS.items():
        got = signal_count(built[name])
        if got != expected:
            sys.exit(f"{name} signal count {got} != pre-lock {expected}. Refusing to write results.")

    base = simulate(built["primary"], eval_sessions, 1.0, "close")
    summary = summarize(base)
    ew = bench_ew(primary, eval_sessions, prev)
    intra = bench_intraday(primary, eval_sessions)
    spy_ret, spy_defined = bench_spy(spy, eval_sessions, prev)
    bench = {
        "intraday_short": {which: metrics(intra[split_mask(eval_sessions, which)], []) for which in ("full", "IS", "OOS")},
        "ew": {which: metrics(ew[split_mask(eval_sessions, which)], []) for which in ("full", "IS", "OOS")},
        "spy": {which: metrics(spy_ret[split_mask(eval_sessions, which)], []) for which in ("full", "IS", "OOS")},
    }
    first_spy = next((i for i, flag in enumerate(spy_defined) if flag == 1.0), None)
    if first_spy is None:
        overlap = None
    else:
        overlap = {
            "first_session": eval_sessions[first_spy].isoformat(),
            "strategy": metrics(base.ret[first_spy:], []),
            "spy": metrics(spy_ret[first_spy:], []),
        }
        overlap["strategy"]["trades"] = sum(
            1 for t in base.trades if date.fromisoformat(t["entry_session"]) >= eval_sessions[first_spy])

    sweep = {}
    sims_by_mult = {}
    for mult in COST_MULTS:
        sim = base if mult == 1.0 else simulate(built["primary"], eval_sessions, mult, "close")
        sims_by_mult[mult] = sim
        summ = summary if mult == 1.0 else summarize(sim)
        sweep[str(mult)] = {
            "full_sharpe": summ["full"]["sharpe"], "full_return": summ["full"]["total_return"],
            "oos_sharpe": summ["OOS"]["sharpe"], "oos_return": summ["OOS"]["total_return"],
            "oos_pf": summ["OOS"]["profit_factor"], "is_sharpe": summ["IS"]["sharpe"],
            "avg_gross_bp": summ["full"]["avg_gross_bp"], "avg_net_bp": summ["full"]["avg_net_bp"],
        }
    delay = simulate_delay(built["primary"], eval_sessions, opens_of(primary), 1.0)
    delay0 = simulate_delay(built["primary"], eval_sessions, opens_of(primary), 0.0)
    secondary = simulate(built["primary"], eval_sessions, 1.0, "gap_fill")
    sec_sum = summarize(secondary)
    cross_sim = simulate(built["cross"], eval_sessions, 1.0, "close")
    cross_sum = summarize(cross_sim)
    control_sim = simulate(built["control"], eval_sessions, 1.0, "close")
    control_sum = summarize(control_sim)

    grid = []
    positive = 0
    for gap in GRID_GAP:
        sigs = built["primary"] if gap == GAP_MIN else build_signals(primary, sessions, prev, index, gap, spy)
        sim = base if gap == GAP_MIN else simulate(sigs, eval_sessions, 1.0, "close")
        cell = summary if gap == GAP_MIN else summarize(sim)
        grid.append({"gap_min": gap, "is_sharpe": cell["IS"]["sharpe"], "oos_sharpe": cell["OOS"]["sharpe"],
                     "is_return": cell["IS"]["total_return"], "oos_return": cell["OOS"]["total_return"],
                     "is_trades": cell["IS"]["trades"], "oos_trades": cell["OOS"]["trades"]})
        if cell["IS"]["sharpe"] is not None and cell["IS"]["sharpe"] > 0 and not math.isnan(cell["IS"]["sharpe"]):
            positive += 1
    grid_share = positive / len(grid)
    primary_cell = next(c for c in grid if c["gap_min"] == GAP_MIN)
    if abs(primary_cell["is_sharpe"] - summary["IS"]["sharpe"]) > 1e-12:
        sys.exit("grid primary cell does not match the primary in-sample Sharpe. Refusing to write results.")

    placebo = direction_placebo(base)
    boot = block_bootstrap(base.ret)
    timing = timing_placebo(primary, sessions, prev, eval_sessions, built["primary"], summary["full"]["sharpe"])
    pred = predictions(primary, sessions, prev, eval_sessions, built["primary"], base.trades, control_sim.trades)
    parts = breakdowns(base, spy_ret, spy_defined)
    # Year rows also carry the benchmarks, aligned to the same sessions.
    for row in parts["by_year"]:
        mask = np.array([d.year == row["year"] for d in eval_sessions])
        row["bench_ew_return"] = float(np.prod(1.0 + ew[mask]) - 1.0)
        row["bench_intraday_return"] = float(np.prod(1.0 + intra[mask]) - 1.0)
        row["bench_spy_return"] = float(np.prod(1.0 + spy_ret[mask]) - 1.0)

    oos, is_ = summary["OOS"], summary["IS"]
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
        "rules_sha256": rules_sha,
        "reason": reason,
        "seeds": {"direction": SEED_DIRECTION, "bootstrap": SEED_BOOTSTRAP, "timing": SEED_TIMING},
        "eval_start": eval_sessions[0].isoformat(),
        "eval_end": eval_sessions[-1].isoformat(),
        "signal_counts": {name: signal_count(sigs) for name, sigs in built.items()},
        "primary": summary,
        "benchmarks": bench,
        "spy_overlap": overlap,
        "cost_sweep": sweep,
        "delay": {"cost_1": summarize(delay), "cost_0": summarize(delay0)},
        "secondary_gap_fill": sec_sum,
        "secondary_passes_its_line": sec_pass,
        "cross": cross_sum,
        "control": control_sum,
        "grid": grid,
        "grid_is_positive_share": grid_share,
        "placebo": {k: v for k, v in placebo.items() if k != "null"},
        "placebo_null": placebo["null"],
        "timing_placebo": {k: v for k, v in timing.items() if k != "null"},
        "timing_null": timing["null"],
        "bootstrap": boot,
        "predictions": pred,
        "breakdowns": parts,
        "acceptance": {"lines": lines, "status": status,
                       "passed": sum(bool(line["pass"]) for line in lines),
                       "failed": sum(not bool(line["pass"]) for line in lines)},
        "symbols": {"primary": list(PRIMARY), "cross": list(CROSS), "controls": list(CONTROLS)},
    }
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8")
    equity = np.cumprod(1.0 + base.ret)
    with (HERE / "daily.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(["session", "sample", "ret", "equity", "bench_intraday_short_ret", "bench_ew_ret",
                         "bench_spy_ret", "spy_defined", "exposure"])
        for k, day in enumerate(eval_sessions):
            writer.writerow([
                day.isoformat(), "IS" if day < OOS_START else "OOS",
                f"{base.ret[k]:.10f}", f"{equity[k]:.10f}", f"{intra[k]:.10f}", f"{ew[k]:.10f}",
                f"{spy_ret[k]:.10f}", f"{int(spy_defined[k])}", f"{base.exposure[k]:.0f}",
            ])
    fields = ["symbol", "side", "entry_session", "entry_time", "entry_price", "exit_session", "exit_time",
              "exit_price", "reason", "gap", "prior_close", "low", "gross_ret", "net_ret", "gross_pnl",
              "net_pnl", "cost", "equity_at_entry", "entry_notional", "weight", "dollar_volume",
              "spy_gap", "excess_gap", "sample"]
    with (HERE / "trades.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=fields)
        writer.writeheader()
        for t in base.trades:
            entry = date.fromisoformat(t["entry_session"])
            row = {k: t[k] for k in fields if k != "sample"}
            for key in ("entry_price", "exit_price", "gap", "prior_close", "low", "gross_ret", "net_ret",
                        "gross_pnl", "net_pnl", "cost", "equity_at_entry", "entry_notional", "weight"):
                row[key] = f"{t[key]:.10f}"
            for key in ("dollar_volume", "spy_gap", "excess_gap"):
                row[key] = "" if t[key] is None else f"{t[key]:.10f}"
            row["sample"] = "IS" if entry < OOS_START else "OOS"
            writer.writerow(row)
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


def _bars(rows: dict[date, tuple]) -> dict[date, tuple]:
    return rows


def self_test() -> None:
    failures: list[str] = []

    def check(name: str, cond: bool, detail: str = "") -> None:
        if not cond:
            failures.append(f"{name}: {detail}")

    d0, d1, d2, d3 = date(2020, 1, 6), date(2020, 1, 7), date(2020, 1, 8), date(2020, 1, 9)
    sessions = [d0, d1, d2, d3]
    prev = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    index = {d: i for i, d in enumerate(sessions)}
    eval_sessions = [d for d in sessions if d >= d1]

    def book_of(symbols: tuple[str, ...], data: dict[str, dict[date, tuple]]) -> Book:
        return Book(symbols, data)

    # Exact 5% boundary and a hand-computed winner. prior 100, open 105, close 102.
    a = {
        d0: (100, 101, 99, 100, 1000),
        d1: (105, 106, 99, 102, 1000),
        d2: (100, 101, 99, 100, 1000),
        d3: (100, 101, 99, 100, 1000),
    }
    b = book_of(("A",), {"A": a})
    sig = build_signals(b, sessions, prev, index, GAP_MIN, None)
    check("boundary fires", len(sig.get(d1, [])) == 1, str(len(sig.get(d1, []))))
    check("flat days do not fire", d2 not in sig and d3 not in sig, str(sorted(sig)))
    sim = simulate(sig, eval_sessions, 1.0, "close")
    gross = 3 / 105
    net = gross - 0.002 - 0.002 * 102 / 105
    check("one trade", len(sim.trades) == 1, str(len(sim.trades)))
    check("winner gross", abs(sim.trades[0]["gross_ret"] - gross) < 1e-12, str(sim.trades[0]["gross_ret"]))
    check("winner net", abs(sim.trades[0]["net_ret"] - net) < 1e-12, str(sim.trades[0]["net_ret"]))
    check("winner return", abs(sim.ret[0] - net) < 1e-12 and sim.ret[1] == 0 and sim.ret[2] == 0, str(sim.ret))
    check("session close", sim.trades[0]["reason"] == "session_close" and sim.trades[0]["exit_time"] == "16:00",
          sim.trades[0]["exit_time"])
    sec = simulate(sig, eval_sessions, 1.0, "gap_fill")
    sec_gross = 5 / 105
    sec_net = sec_gross - 0.002 - 0.002 * 100 / 105
    check("gap fill exit", sec.trades[0]["reason"] == "gap_fill" and abs(sec.trades[0]["exit_price"] - 100) < 1e-12,
          str(sec.trades[0]["exit_price"]))
    check("gap fill net", abs(sec.trades[0]["net_ret"] - sec_net) < 1e-12, str(sec.trades[0]["net_ret"]))

    # Below the limit, and a double, which is excluded. Just inside the cap is kept.
    below = {d0: (10, 11, 9, 10, 1), d1: (10.4, 11, 10, 10.4, 1)}
    cap = {d0: (10, 11, 9, 10, 1), d1: (20, 21, 19, 20, 1)}
    inside = {d0: (10, 11, 9, 10, 1), d1: (19, 19.5, 18, 18.5, 1)}
    check("below silent", not build_signals(book_of(("A",), {"A": below}), sessions, prev, index, GAP_MIN, None))
    check("double silent", not build_signals(book_of(("A",), {"A": cap}), sessions, prev, index, GAP_MIN, None))
    check("inside cap", len(build_signals(book_of(("A",), {"A": inside}), sessions, prev, index, GAP_MIN, None)[d1]) == 1)

    # Missing previous session does not reach back to an older close.
    gap = {d0: (10, 11, 9, 10, 1), d2: (20, 21, 19, 18, 1)}
    missed = build_signals(book_of(("A",), {"A": gap}), sessions, prev, index, GAP_MIN, None)
    check("no stale close", not missed, str(missed))

    # Unusable bar.
    bad = {d0: (10, 11, 9, 10, 1), d1: (12, 13, 12.5, 11, 1)}
    check("unusable silent", not build_signals(book_of(("A",), {"A": bad}), sessions, prev, index, GAP_MIN, None))

    # Two names, equal weight, hand net.
    left = {d0: (10, 11, 9, 10, 1), d1: (12, 13, 11, 11, 1)}
    right = {d0: (10, 11, 9, 10, 1), d1: (12, 13, 11, 13, 1)}
    two = book_of(("A", "B"), {"A": left, "B": right})
    tsig = build_signals(two, sessions, prev, index, GAP_MIN, None)
    tsim = simulate(tsig, [d1], 1.0, "close")
    # notional 0.5, shares 0.5/12
    a_net = (0.5 / 12) * (12 - 11) - 0.001 - 0.002 * (0.5 / 12) * 11
    b_net = (0.5 / 12) * (12 - 13) - 0.001 - 0.002 * (0.5 / 12) * 13
    check("two weights", abs(tsim.trades[0]["weight"] - 0.5) < 1e-12 and abs(tsim.trades[1]["weight"] - 0.5) < 1e-12, "")
    check("two pnl", abs(tsim.ret[0] - (a_net + b_net)) < 1e-12, str(tsim.ret[0]))

    # Compounding: the second trade is sized off the equity the first trade left.
    chain = {
        d0: (100, 101, 99, 100, 1),
        d1: (105, 106, 99, 100, 1),
        d2: (110, 112, 108, 110, 1),
    }
    cbook = book_of(("A",), {"A": chain})
    csig = build_signals(cbook, sessions, prev, index, GAP_MIN, None)
    csim = simulate(csig, [d1, d2], 1.0, "close")
    e1 = 1.0 + (5 / 105 - 0.002 - 0.002 * 100 / 105)
    check("compound notional", abs(csim.trades[1]["entry_notional"] - e1) < 1e-12, str(csim.trades[1]["entry_notional"]))

    # Ruin stops later signals. prior 8, open 10, close 40.
    ruin_bars = {
        d0: (8, 9, 7, 8, 1),
        d1: (10, 41, 9, 40, 1),
        d2: (50, 51, 49, 50, 1),
    }
    rsig = build_signals(book_of(("A",), {"A": ruin_bars}), sessions, prev, index, GAP_MIN, None)
    rsim = simulate(rsig, [d1, d2], 1.0, "close")
    check("ruin return", rsim.ret[0] == -1.0 and rsim.ret[1] == 0.0, str(rsim.ret))
    check("ruin stops", len(rsim.trades) == 1 and rsim.ruined, str(len(rsim.trades)))

    # Zero cost: net equals gross.
    z = simulate(sig, eval_sessions, 0.0, "close")
    check("zero cost", abs(z.trades[0]["net_pnl"] - z.trades[0]["gross_pnl"]) < 1e-12, "")

    # No low through the prior close: secondary exits at the close.
    hold = {d0: (10, 11, 9, 10, 1), d1: (12, 13, 10.5, 11, 1)}
    hsig = build_signals(book_of(("A",), {"A": hold}), sessions, prev, index, GAP_MIN, None)
    hsim = simulate(hsig, [d1], 1.0, "gap_fill")
    check("no fill keeps close", hsim.trades[0]["reason"] == "session_close" and abs(hsim.trades[0]["exit_price"] - 11) < 1e-12,
          hsim.trades[0]["reason"])

    # Delay: short d1's close, cover d2's open. Whole P&L on d2. A missing next open skips.
    delay_bars = {
        d0: (100, 101, 99, 100, 1),
        d1: (120, 121, 108, 110, 1),
        d2: (90, 92, 89, 91, 1),
    }
    db = book_of(("A",), {"A": delay_bars})
    dsig = build_signals(db, sessions, prev, index, GAP_MIN, None)
    dsim = simulate_delay(dsig, [d1, d2], {"A": {d1: 120, d2: 90}}, 1.0)
    dnet = 20 / 110 - 0.002 - 0.002 * (1 / 110) * 90
    check("delay timing", abs(dsim.ret[0]) < 1e-15 and abs(dsim.ret[1] - dnet) < 1e-12, str(dsim.ret))
    check("delay price", len(dsim.trades) == 1 and abs(dsim.trades[0]["entry_price"] - 110) < 1e-12
          and abs(dsim.trades[0]["exit_price"] - 90) < 1e-12, str(dsim.trades))
    skipped = simulate_delay(dsig, [d1], {"A": {d1: 120}}, 1.0)
    check("delay skip", len(skipped.trades) == 0 and skipped.ret[0] == 0.0, str(skipped.trades))

    # Early close labels the exit at 13:00. 2024-07-03 is on the calendar.
    e0, e1 = date(2024, 7, 2), date(2024, 7, 3)
    esessions = [e0, e1]
    eprev = {e1: e0}
    eindex = {e0: 0, e1: 1}
    ebar = {e0: (10, 11, 9, 10, 1), e1: (12, 13, 11, 11.5, 1)}
    esig = build_signals(book_of(("A",), {"A": ebar}), esessions, eprev, eindex, GAP_MIN, None)
    esim = simulate(esig, [e1], 1.0, "close")
    check("early close", esim.trades[0]["exit_time"] == "13:00" and esim.trades[0]["entry_time"] == "09:30",
          esim.trades[0]["exit_time"])

    if failures:
        raise SystemExit("self-test failed:\n" + "\n".join(failures))
    print(f"self-test passed ({13} paths)")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", nargs="?", default="run", choices=["run", "self-test"])
    parser.add_argument("--reason", default="initial")
    args = parser.parse_args()
    self_test()
    if args.mode == "self-test":
        return
    rules_sha = check_lock()
    store_run(args.reason, rules_sha)


if __name__ == "__main__":
    main()
