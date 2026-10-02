# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""IGV small-account minute fade, measured against RULES.md.

Run from the repo root:

    python research/igv-small-account-fade/research/backtest.py
    python research/igv-small-account-fade/research/backtest.py self-test

The store is not opened until the synthetic self-test passes. A store run
appends one entry to RUNLOG.md.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import statistics
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
ANNUAL = 252
SEED_DIRECTION = 20260926
SEED_TIMING = 20260927
SEED_BOOTSTRAP = 20260928


@dataclass(frozen=True)
class Params:
    d_min: float = 250_000.0
    d_max: float = 1_000_000.0
    rv_min: float = 2.0
    mag_mult: float = 2.0
    mag_floor: float = 0.001
    lookback: int = 20
    hold: int = 15
    k_min: int = 15
    k_max: int = 360
    k_max_early: int = 150
    flatten_k: int = 385
    flatten_k_early: int = 205
    retrace: float = 0.5
    stop_mult: float = 1.0
    cost_bps: float = 2.0
    notional: float = 5_000.0
    account: float = 25_000.0
    delay_bars: int = 0
    fill_on_close: bool = False


P = Params()


@dataclass(frozen=True)
class Signal:
    day: date
    k: int
    side: int
    r: float
    dollar: float
    p_prev: float
    p_sig: float


# --------------------------------------------------------------------------
# Rules lock
# --------------------------------------------------------------------------


def rules_digest() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(raw).hexdigest()


def assert_lock() -> str:
    digest = rules_digest()
    lines = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()
    locked = lines[0].split()[1]
    if locked != digest:
        raise SystemExit(f"RULES.md hash {digest} does not match lock {locked}")
    return digest


def git_state() -> tuple[str, bool]:
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = bool(subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip())
    return head, dirty


# --------------------------------------------------------------------------
# Features and signals
# --------------------------------------------------------------------------


def build_features(days: list[date], bars: dict, lookback: int) -> list[dict]:
    """Per session, per minute: (r, dollar, med_abs, med_d, p_prev, p_sig).

    Medians use the prior `lookback` occurrences only. The current session is
    appended after its own decisions are fixed.
    """
    hist_abs: list[list[float]] = [[] for _ in range(390)]
    hist_dol: list[list[float]] = [[] for _ in range(390)]
    out: list[dict] = []
    for day in days:
        feat: dict[int, tuple] = {}
        session = bars[day]
        for k, (_o, _h, _l, c, v) in session.items():
            prev = session.get(k - 1)
            if prev is None or prev[3] <= 0 or c <= 0:
                continue
            r = math.log(c / prev[3])
            dollar = c * v
            med_a = med_d = None
            if len(hist_abs[k]) >= lookback:
                window_a = hist_abs[k][-lookback:]
                window_d = hist_dol[k][-lookback:]
                med_a = statistics.median(window_a)
                med_d = statistics.median(window_d)
            feat[k] = (r, dollar, med_a, med_d, prev[3], c)
        out.append(feat)
        for k, row in feat.items():
            hist_abs[k].append(abs(row[0]))
            hist_dol[k].append(row[1])
    return out


def k_max_for(day: date, p: Params) -> int:
    return p.k_max_early if day in EARLY_CLOSES else p.k_max


def signals_on_day(day: date, feat: dict, p: Params) -> list[Signal]:
    k_max = k_max_for(day, p)
    found: list[Signal] = []
    for k in sorted(feat):
        r, dollar, med_a, med_d, p_prev, p_sig = feat[k]
        if med_a is None or med_d is None or med_d <= 0 or r == 0:
            continue
        if not (p.k_min <= k <= k_max):
            continue
        if not (p.d_min <= dollar < p.d_max):
            continue
        if dollar / med_d < p.rv_min:
            continue
        if abs(r) < max(p.mag_floor, p.mag_mult * med_a):
            continue
        side = -1 if r > 0 else 1
        found.append(Signal(day, k, side, r, dollar, p_prev, p_sig))
    return found


def all_signals(days: list[date], features: list[dict], p: Params) -> dict[date, list[Signal]]:
    return {day: signals_on_day(day, feat, p) for day, feat in zip(days, features)}


# --------------------------------------------------------------------------
# Simulation
# --------------------------------------------------------------------------


def later_minutes(existing: list[int], decision: int) -> list[int]:
    return [k for k in existing if k > decision]


def resolve_fill(session: dict, existing: list[int], decision: int, p: Params, is_entry: bool):
    """Return (minute, price, on_close) or None when an entry is cancelled.

    Delay reading, chosen before any result: the fill is the open of later
    bar number `delay_bars` (0 is the first later bar). An entry that does
    not have that bar is cancelled. An exit with no later bar fills at the
    decision close. An exit with exactly one later bar, when a later bar was
    required, fills at that bar's open.
    """
    if p.fill_on_close:
        return None
    later = later_minutes(existing, decision)
    if p.delay_bars < len(later):
        k = later[p.delay_bars]
        return k, session[k][0], False
    if is_entry:
        return None
    if len(later) == 1:
        k = later[0]
        return k, session[k][0], False
    return decision, session[decision][3], True


def exit_reason(pos: dict, m: int, close: float, last_m: int, flatten_at: int, hold: int):
    side = pos["side"]
    if side == 1 and close <= pos["stop_px"]:
        return "stop"
    if side == -1 and close >= pos["stop_px"]:
        return "stop"
    if side == 1 and close >= pos["target_px"]:
        return "target"
    if side == -1 and close <= pos["target_px"]:
        return "target"
    if m >= flatten_at or m == last_m:
        return "flatten"
    if m >= pos["signal_k"] + hold:
        return "time"
    return None


def _finish(pos: dict, exit_k: int, exit_px: float, on_close: bool, reason: str, p: Params) -> dict:
    shares = p.notional / pos["entry_px"]
    gross = pos["side"] * shares * (exit_px - pos["entry_px"])
    cost = p.cost_bps / 10_000.0 * p.notional
    net = gross - 2.0 * cost
    hold_min = (exit_k - pos["entry_k"] + 1) if on_close else (exit_k - pos["entry_k"])
    return {
        "day": pos["day"],
        "side": "long" if pos["side"] == 1 else "short",
        "side_sign": pos["side"],
        "signal_k": pos["signal_k"],
        "entry_k": pos["entry_k"],
        "entry_px": pos["entry_px"],
        "exit_k": exit_k,
        "exit_px": exit_px,
        "exit_on_close": on_close,
        "reason": reason,
        "dollar": pos["dollar"],
        "r": pos["r"],
        "p_sig": pos["p_sig"],
        "p_prev": pos["p_prev"],
        "gross": gross,
        "net": net,
        "gross_bps": gross / p.notional * 10_000.0,
        "net_bps": net / p.notional * 10_000.0,
        "hold_min": hold_min,
    }


def simulate(days: list[date], bars: dict, signals: dict[date, list[Signal]], p: Params) -> list[dict]:
    trades: list[dict] = []
    for day in days:
        session = bars[day]
        existing = sorted(session)
        if not existing:
            continue
        last_m = existing[-1]
        flatten_at = p.flatten_k_early if day in EARLY_CLOSES else p.flatten_k
        queued = list(signals.get(day, []))
        q_i = 0
        position = None
        pending_entry = None
        pending_exit = None
        blocked = False

        def take_entry(sig: Signal, entry_k: int, entry_px: float, first_check: int):
            move = sig.p_sig - sig.p_prev
            return {
                "day": day,
                "side": sig.side,
                "signal_k": sig.k,
                "entry_k": entry_k,
                "entry_px": entry_px,
                "first_check": first_check,
                "dollar": sig.dollar,
                "r": sig.r,
                "p_sig": sig.p_sig,
                "p_prev": sig.p_prev,
                "target_px": sig.p_sig - p.retrace * move,
                "stop_px": sig.p_sig + p.stop_mult * move,
            }

        for m in existing:
            if pending_exit is not None and m == pending_exit[0]:
                trades.append(_finish(position, pending_exit[0], pending_exit[1], False, pending_exit[2], p))
                if pending_exit[2] == "stop":
                    blocked = True
                position = None
                pending_exit = None
            if pending_entry is not None and position is None and m == pending_entry[0]:
                position = pending_entry[1]
                pending_entry = None

            if position is not None and pending_exit is None and m >= position["first_check"]:
                reason = exit_reason(position, m, session[m][3], last_m, flatten_at, p.hold)
                if reason:
                    if p.fill_on_close:
                        trades.append(_finish(position, m, session[m][3], True, reason, p))
                        if reason == "stop":
                            blocked = True
                        position = None
                    else:
                        filled = resolve_fill(session, existing, m, p, False)
                        if filled[2]:
                            trades.append(_finish(position, filled[0], filled[1], True, reason, p))
                            if reason == "stop":
                                blocked = True
                            position = None
                        else:
                            pending_exit = (filled[0], filled[1], reason)
                    continue

            if position is not None or pending_entry is not None or blocked:
                continue
            while q_i < len(queued) and queued[q_i].k < m:
                q_i += 1
            if q_i >= len(queued) or queued[q_i].k != m:
                continue
            sig = queued[q_i]
            q_i += 1
            if p.fill_on_close:
                nxt = later_minutes(existing, m)
                if not nxt:
                    continue
                position = take_entry(sig, m, sig.p_sig, nxt[0])
                continue
            filled = resolve_fill(session, existing, m, p, True)
            if filled is None:
                continue
            entry_k, entry_px, _on_close = filled
            position_now = take_entry(sig, entry_k, entry_px, entry_k)
            if entry_k == m:
                position = position_now
            else:
                pending_entry = (entry_k, position_now)
    return trades


# --------------------------------------------------------------------------
# Statistics
# --------------------------------------------------------------------------


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
    return float((eq / peak - 1.0).min())


def profit_factor(nets: np.ndarray) -> float:
    if len(nets) == 0:
        return float("nan")
    wins = float(nets[nets > 0].sum())
    losses = float(-nets[nets < 0].sum())
    if losses == 0:
        return float("inf") if wins > 0 else float("nan")
    return wins / losses


def pack_stats(daily: np.ndarray, days: list[date], trades: list[dict]) -> dict:
    nets = np.array([t["net"] for t in trades], dtype=float)
    gross = np.array([t["gross_bps"] for t in trades], dtype=float)
    total = float(np.prod(1.0 + daily) - 1.0) if len(daily) else 0.0
    n = len(daily)
    return {
        "first": days[0].isoformat() if days else None,
        "last": days[-1].isoformat() if days else None,
        "sessions": n,
        "total_return": total,
        "cagr": float((1.0 + total) ** (ANNUAL / n) - 1.0) if n and total > -1 else float("nan"),
        "vol": float(daily.std(ddof=1) * math.sqrt(ANNUAL)) if n > 1 else float("nan"),
        "sharpe": sharpe(daily),
        "max_dd": max_drawdown(daily),
        "t_stat": float(daily.mean() / (daily.std(ddof=1) / math.sqrt(n))) if n > 1 and daily.std(ddof=1) > 0 else float("nan"),
        "trades": len(trades),
        "sessions_traded": len({t["day"] for t in trades}),
        "win_rate": float((nets > 0).mean()) if len(nets) else float("nan"),
        "profit_factor": profit_factor(nets),
        "avg_net_bps": float(np.mean([t["net_bps"] for t in trades])) if trades else float("nan"),
        "median_net_bps": float(np.median([t["net_bps"] for t in trades])) if trades else float("nan"),
        "avg_gross_bps": float(gross.mean()) if len(gross) else float("nan"),
        "avg_winner_bps": float(np.mean([t["net_bps"] for t in trades if t["net"] > 0])) if any(t["net"] > 0 for t in trades) else float("nan"),
        "avg_loser_bps": float(np.mean([t["net_bps"] for t in trades if t["net"] < 0])) if any(t["net"] < 0 for t in trades) else float("nan"),
    }


def daily_from_trades(calendar: list[date], trades: list[dict], account: float, field: str) -> np.ndarray:
    pnl = defaultdict(float)
    for t in trades:
        pnl[t["day"]] += t[field]
    return np.array([pnl.get(day, 0.0) / account for day in calendar], dtype=float)


def slice_mask(calendar: list[date], which: str) -> np.ndarray:
    if which == "is":
        return np.array([d <= IS_END for d in calendar])
    if which == "oos":
        return np.array([d >= OOS_START for d in calendar])
    return np.ones(len(calendar), dtype=bool)


def trades_in(trades: list[dict], calendar_set: set[date]) -> list[dict]:
    return [t for t in trades if t["day"] in calendar_set]


def window_stats(calendar: list[date], trades: list[dict], account: float, field: str = "net") -> dict:
    out = {}
    daily = daily_from_trades(calendar, trades, account, field)
    for name in ("full", "is", "oos"):
        mask = slice_mask(calendar, name)
        days = [d for d, keep in zip(calendar, mask) if keep]
        chosen = set(days)
        out[name] = pack_stats(daily[mask], days, trades_in(trades, chosen))
    return out


def gross_sharpe(calendar: list[date], trades: list[dict], account: float, signs: np.ndarray | None = None) -> float:
    pnl = defaultdict(float)
    for i, t in enumerate(trades):
        sign = 1.0 if signs is None else float(signs[i])
        pnl[t["day"]] += sign * t["gross"]
    daily = np.array([pnl.get(day, 0.0) / account for day in calendar], dtype=float)
    return sharpe(daily)


def direction_placebo(calendar: list[date], trades: list[dict], account: float) -> dict:
    actual = gross_sharpe(calendar, trades, account)
    rng = np.random.default_rng(SEED_DIRECTION)
    n = len(trades)
    draws = np.empty(2000)
    for i in range(2000):
        signs = rng.choice(np.array([-1.0, 1.0]), size=n)
        draws[i] = gross_sharpe(calendar, trades, account, signs)
    p = (1 + int(np.sum(draws >= actual))) / 2001
    return {
        "actual_gross_sharpe": actual,
        "null_mean": float(np.nanmean(draws)),
        "null_p95": float(np.nanpercentile(draws, 95)),
        "p": p,
        "draws": 2000,
        "seed": SEED_DIRECTION,
    }


def bootstrap_sharpe(daily: np.ndarray) -> dict:
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    n = len(daily)
    block = 20
    n_blocks = math.ceil(n / block)
    draws = np.empty(2000)
    for i in range(2000):
        starts = rng.integers(0, n, size=n_blocks)
        pieces = []
        for start in starts:
            for j in range(block):
                pieces.append(daily[(int(start) + j) % n])
                if len(pieces) == n:
                    break
            if len(pieces) == n:
                break
        draws[i] = sharpe(np.array(pieces))
    return {
        "p2_5": float(np.nanpercentile(draws, 2.5)),
        "p97_5": float(np.nanpercentile(draws, 97.5)),
        "draws": 2000,
        "seed": SEED_BOOTSTRAP,
        "block": block,
    }


def eligible_pool(day: date, feat: dict, session: dict, p: Params) -> list[int]:
    existing = sorted(session)
    k_max = k_max_for(day, p)
    pool = []
    for k, row in feat.items():
        _r, dollar, _ma, _md, _pp, _ps = row
        if not (p.k_min <= k <= k_max):
            continue
        if not (p.d_min <= dollar < p.d_max):
            continue
        if not later_minutes(existing, k):
            continue
        pool.append(k)
    pool.sort()
    return pool


def timing_placebo(days, bars, features, trades, p: Params, calendar) -> dict:
    """Same entry count per session, random clock inside the capacity window."""
    by_day_n = defaultdict(int)
    for t in trades:
        by_day_n[t["day"]] += 1
    pools = {}
    feat_of = {}
    for day, feat in zip(days, features):
        if by_day_n[day]:
            pools[day] = eligible_pool(day, feat, bars[day], p)
            feat_of[day] = feat
    rng = np.random.default_rng(SEED_TIMING)
    actual = gross_sharpe(calendar, trades, p.account)
    draws = np.empty(500)
    shortfall = 0
    for i in range(500):
        synthetic: dict[date, list[Signal]] = {}
        for day, n in by_day_n.items():
            pool = pools.get(day, [])
            if len(pool) < n:
                shortfall += 1
                chosen = list(pool)
            else:
                chosen = list(rng.choice(pool, size=n, replace=False))
            chosen.sort()
            sigs = []
            for k in chosen:
                r, dollar, _ma, _md, p_prev, p_sig = feat_of[day][k]
                side = int(rng.choice(np.array([-1, 1])))
                if r == 0:
                    r = 1e-12
                sigs.append(Signal(day, k, side, r if side == -1 else -abs(r), dollar, p_prev, p_sig))
                # Side is random. The minute's actual move still sets target and stop,
                # so the stored r must have the sign the side is fading? No: the exit
                # geometry uses p_sig - p_prev, not r. Keep the real prices.
                sigs[-1] = Signal(day, k, side, r, dollar, p_prev, p_sig)
            synthetic[day] = sigs
        sim = simulate(days, bars, synthetic, p)
        draws[i] = gross_sharpe(calendar, sim, p.account)
    return {
        "actual_gross_sharpe": actual,
        "null_mean": float(np.nanmean(draws)),
        "p": (1 + int(np.sum(draws >= actual))) / 501,
        "draws": 500,
        "seed": SEED_TIMING,
        "shortfall_session_draws": shortfall,
    }


def event_study(days, bars, signals: dict[date, list[Signal]]) -> dict:
    horizons = (5, 15, 30)
    buckets = {h: [] for h in horizons}
    for day in days:
        session = bars[day]
        for sig in signals.get(day, []):
            later = later_minutes(sorted(session), sig.k)
            if not later:
                continue
            fill = session[later[0]][0]
            if fill <= 0:
                continue
            for h in horizons:
                bar = session.get(sig.k + h)
                if bar is None or bar[3] <= 0:
                    continue
                buckets[h].append(sig.side * math.log(bar[3] / fill))
    out = {}
    for h, values in buckets.items():
        arr = np.array(values, dtype=float)
        out[str(h)] = {
            "n": int(len(arr)),
            "mean": float(arr.mean()) if len(arr) else float("nan"),
        }
    return out


def quintiles(pairs: list[tuple[float, object]]) -> list[list[object]]:
    ordered = sorted(pairs, key=lambda item: (item[0], str(item[1])))
    n = len(ordered)
    groups = []
    for i in range(5):
        chunk = ordered[i * n // 5:(i + 1) * n // 5]
        groups.append([item[1] for item in chunk])
    return groups


def group_trade_stats(trades: list[dict]) -> dict:
    if not trades:
        return {"trades": 0, "avg_net_bps": None, "avg_gross_bps": None, "win_rate": None, "net": 0.0}
    nets = [t["net_bps"] for t in trades]
    return {
        "trades": len(trades),
        "avg_net_bps": float(np.mean(nets)),
        "avg_gross_bps": float(np.mean([t["gross_bps"] for t in trades])),
        "win_rate": float(np.mean([t["net"] > 0 for t in trades])),
        "net": float(sum(t["net"] for t in trades)),
        "profit_factor": profit_factor(np.array([t["net"] for t in trades])),
    }


def breakdowns(calendar, trades, oc_ret: dict[date, float], session_open: dict[date, float]) -> dict:
    by_year = {}
    years = sorted({d.year for d in calendar})
    daily = daily_from_trades(calendar, trades, P.account, "net")
    bench = np.array([oc_ret.get(d, 0.0) for d in calendar])
    # close-to-close is passed separately by the caller via a parallel array? We
    # recompute year totals from the trade list and from oc_ret for the intraday
    # benchmark. The close-to-close series is attached by the caller through
    # the calendar order in `year_cc` if present on the function via closure.
    for year in years:
        mask = np.array([d.year == year for d in calendar])
        days = [d for d in calendar if d.year == year]
        chosen = set(days)
        stats = pack_stats(daily[mask], days, trades_in(trades, chosen))
        stats["bench_oc_return"] = float(np.prod(1.0 + bench[mask]) - 1.0)
        by_year[str(year)] = stats

    by_side = {}
    for name, sign in (("long", 1), ("short", -1)):
        by_side[name] = group_trade_stats([t for t in trades if t["side_sign"] == sign])
    by_reason = {}
    for reason in ("target", "stop", "time", "flatten"):
        by_reason[reason] = group_trade_stats([t for t in trades if t["reason"] == reason])

    def bucket(k: int) -> str:
        if k < 90:
            return "09:45-11:00"
        if k < 210:
            return "11:00-13:00"
        if k < 330:
            return "13:00-15:00"
        return "15:00-and-later"

    by_entry = {}
    for name in ("09:45-11:00", "11:00-13:00", "13:00-15:00", "15:00-and-later"):
        by_entry[name] = group_trade_stats([t for t in trades if bucket(t["entry_k"]) == name])

    oc_pairs = [(oc_ret[d], d) for d in calendar if d in oc_ret and d in session_open]
    # Sessions with no bars are absent from session_open. Quintiles use sessions
    # that have an open-to-close return, including exact zeros.
    q_days = quintiles([(oc_ret[d], d) for d in calendar if d in session_open])
    by_oc = []
    for i, days in enumerate(q_days, start=1):
        chosen = set(days)
        row = group_trade_stats(trades_in(trades, chosen))
        vals = [oc_ret[d] for d in days]
        row["quintile"] = i
        row["sessions"] = len(days)
        row["oc_min"] = float(min(vals)) if vals else None
        row["oc_max"] = float(max(vals)) if vals else None
        by_oc.append(row)

    sig_pairs = []
    for t in trades:
        opened = session_open.get(t["day"])
        if opened and opened > 0:
            sig_pairs.append((t["p_sig"] / opened - 1.0, t))
    by_sig = []
    for i, group in enumerate(quintiles(sig_pairs), start=1):
        row = group_trade_stats(group)
        row["quintile"] = i
        by_sig.append(row)
    return {
        "by_year": by_year,
        "by_side": by_side,
        "by_reason": by_reason,
        "by_entry": by_entry,
        "by_open_close_quintile": by_oc,
        "by_open_to_signal_quintile": by_sig,
    }


def thin_thick(trades: list[dict]) -> dict:
    thin = [t for t in trades if t["dollar"] < 500_000]
    thick = [t for t in trades if t["dollar"] >= 500_000]
    return {"under_500k": group_trade_stats(thin), "from_500k": group_trade_stats(thick)}


def spearman(xs: list[float], ys: list[float]) -> float:
    def ranks(values: list[float]) -> np.ndarray:
        order = np.argsort(values)
        out = np.empty(len(values))
        out[order] = np.arange(len(values))
        return out
    if len(xs) < 3:
        return float("nan")
    rx, ry = ranks(xs), ranks(ys)
    rx = rx - rx.mean()
    ry = ry - ry.mean()
    denom = math.sqrt(float((rx ** 2).sum() * (ry ** 2).sum()))
    if denom == 0:
        return float("nan")
    return float((rx * ry).sum() / denom)


# --------------------------------------------------------------------------
# Self-test
# --------------------------------------------------------------------------


def _flat(minutes, price=100.0, volume=4000.0) -> dict:
    return {k: (price, price, price, price, volume) for k in minutes}


def _tape(days: list[date], sessions: list[dict]):
    return days, {day: session for day, session in zip(days, sessions)}


def _warmup(n: int, minutes, start: date = date(2022, 1, 10)):
    days = [start + timedelta(days=i) for i in range(n)]
    sessions = [_flat(minutes) for _ in days]
    return days, sessions


def _assert_trade(trades, **expect):
    if len(trades) != 1:
        raise AssertionError(f"expected 1 trade, got {len(trades)}: {trades}")
    t = trades[0]
    for key, value in expect.items():
        got = t[key]
        if isinstance(value, float):
            if abs(got - value) > 1e-9:
                raise AssertionError(f"{key}: {got} != {value}")
        elif got != value:
            raise AssertionError(f"{key}: {got} != {value}")


def self_test() -> None:
    minutes = range(0, 45)

    def run(extra_days, extra_sessions, params=P, early_extra=()):
        days, sessions = _warmup(20, minutes)
        days = days + extra_days
        sessions = sessions + extra_sessions
        # Mark synthetic early closes by temporarily adding them.
        added = []
        for day in early_extra:
            if day not in EARLY_CLOSES:
                EARLY_CLOSES_MUTABLE_NOTE = day
                added.append(day)
        # EARLY_CLOSES is a frozenset. Tests that need an early close use a
        # real member of that set rather than mutating it.
        del added
        bars_days, bars = _tape(days, sessions)
        feat = build_features(bars_days, bars, params.lookback)
        sigs = all_signals(bars_days, feat, params)
        return sigs, simulate(bars_days, bars, sigs, params)

    # 1. Long target. Down move, half retrace, fill at the next opens.
    down = 100.0 * math.exp(-0.002)
    day = date(2022, 2, 1)
    session = _flat(minutes)
    session[20] = (100.0, 100.0, down, down, 9000.0)
    session[21] = (99.80, 99.80, 99.80, 99.80, 4000.0)
    session[22] = (99.80, 99.91, 99.80, 99.91, 4000.0)
    session[23] = (99.92, 99.92, 99.92, 99.92, 4000.0)
    sigs, trades = run([day], [session])
    _assert_trade(trades, side="long", signal_k=20, entry_k=21, entry_px=99.80,
                  exit_k=23, exit_px=99.92, reason="target")
    gross = (5000.0 / 99.80) * (99.92 - 99.80)
    if abs(trades[0]["net"] - (gross - 2.0)) > 1e-9:
        raise AssertionError("cost arithmetic")
    if len(sigs[day]) != 1:
        raise AssertionError("raw signal count")

    # 2. Short stop, then a later valid signal is blocked.
    up = 100.0 * math.exp(0.002)
    session = _flat(minutes)
    session[30] = (100.0, up, 100.0, up, 9000.0)
    session[31] = (100.20, 100.20, 100.20, 100.20, 4000.0)
    session[32] = (100.20, 100.70, 100.20, 100.70, 4000.0)
    session[33] = (100.71, 100.71, 100.71, 100.71, 4000.0)
    session[35] = (100.71, 100.71, 100.71, 100.71, 4000.0)
    session[36] = (100.71, 100.71 * math.exp(0.003), 100.71, 100.71 * math.exp(0.003), 9000.0)
    session[37] = (100.71, 100.71, 100.71, 100.71, 4000.0)
    sigs, trades = run([day], [session])
    _assert_trade(trades, side="short", signal_k=30, reason="stop", entry_k=31, entry_px=100.20)
    if trades[0]["exit_k"] != 33:
        raise AssertionError(f"stop fill minute {trades[0]['exit_k']}")
    if len(sigs[day]) < 2:
        raise AssertionError("expected a second raw signal after the stop")

    # 3. Time exit 15 minutes after the signal, no target or stop.
    session = _flat(minutes)
    session[20] = (100.0, 100.0, down, down, 9000.0)
    for k in range(21, 37):
        session[k] = (99.80, 99.80, 99.80, 99.80, 4000.0)
    _sigs, trades = run([day], [session])
    _assert_trade(trades, reason="time", signal_k=20, entry_k=21, exit_k=36, exit_px=99.80)

    # 4. Flatten at 15:55 when the hold is long enough not to fire first.
    wide = range(350, 390)
    days, sessions = _warmup(20, wide)
    session = _flat(wide)
    session[360] = (100.0, 100.0, down, down, 9000.0)
    for k in range(361, 390):
        session[k] = (99.80, 99.80, 99.80, 99.80, 4000.0)
    flat_day = days[-1] + timedelta(days=1)
    days = days + [flat_day]
    sessions = sessions + [session]
    bars_days, bars = _tape(days, sessions)
    params = Params(hold=100)
    feat = build_features(bars_days, bars, params.lookback)
    sigs = all_signals(bars_days, feat, params)
    trades = simulate(bars_days, bars, sigs, params)
    _assert_trade(trades, reason="flatten", signal_k=360, entry_k=361, exit_k=386, exit_px=99.80)

    # 5. Early close: 12:00 is allowed, 12:10 is not, flatten is 12:55.
    early = date(2024, 7, 3)
    if early not in EARLY_CLOSES:
        raise AssertionError("test date is not an early close")
    wide = range(140, 211)
    days, sessions = _warmup(20, wide, start=date(2024, 6, 3))
    session = _flat(wide)
    session[150] = (100.0, 100.0, down, down, 9000.0)
    session[160] = (100.0, 100.0, down, down, 9000.0)
    for k in range(151, 211):
        if k == 160:
            continue
        session[k] = (99.80, 99.80, 99.80, 99.80, 4000.0)
    # Minute 149 must stay 100 so the signal at 150 has p_prev = 100.
    # The loop above starts at 151. Good.
    days = days + [early]
    sessions = sessions + [session]
    bars_days, bars = _tape(days, sessions)
    params = Params(hold=100)
    feat = build_features(bars_days, bars, params.lookback)
    sigs = all_signals(bars_days, feat, params)
    ks = [s.k for s in sigs[early]]
    if ks != [150]:
        raise AssertionError(f"early-close signals {ks}")
    trades = simulate(bars_days, bars, sigs, params)
    _assert_trade(trades, reason="flatten", signal_k=150, entry_k=151, exit_k=206)

    # 6. Missing previous minute: no signal.
    session = _flat(minutes)
    del session[19]
    session[20] = (100.0, 100.0, down, down, 9000.0)
    sigs, trades = run([day], [session])
    if sigs[day] or trades:
        raise AssertionError("gap before the signal minute must not trade")

    # 7. Dollar volume outside the window.
    session = _flat(minutes)
    session[20] = (100.0, 100.0, down, down, 2000.0)  # about $200k
    sigs, trades = run([day], [session])
    if sigs[day] or trades:
        raise AssertionError("dollar volume below the floor traded")
    session[20] = (100.0, 100.0, down, down, 12000.0)  # about $1.2M
    sigs, trades = run([day], [session])
    if sigs[day] or trades:
        raise AssertionError("dollar volume above the cap traded")

    # 8. Relative volume too low. Dollar volume is inside the window.
    session = _flat(minutes)
    session[20] = (100.0, 100.0, down, down, 4500.0)
    sigs, trades = run([day], [session])
    if sigs[day] or trades:
        raise AssertionError("low relative volume traded")

    # 9. Magnitude below the 10 bp floor.
    small = 100.0 * math.exp(-0.0005)
    session = _flat(minutes)
    session[20] = (100.0, 100.0, small, small, 9000.0)
    sigs, trades = run([day], [session])
    if sigs[day] or trades:
        raise AssertionError("small move traded")

    # 10. Gap fill uses the next existing open.
    session = _flat(minutes)
    session[20] = (100.0, 100.0, down, down, 9000.0)
    del session[21]
    session[22] = (99.50, 99.50, 99.50, 99.50, 4000.0)
    for k in range(23, 40):
        session[k] = (99.50, 99.50, 99.50, 99.50, 4000.0)
    _sigs, trades = run([day], [session])
    if not trades or trades[0]["entry_k"] != 22 or abs(trades[0]["entry_px"] - 99.50) > 1e-9:
        raise AssertionError(f"gap fill {trades[:1]}")

    # 11. Last bar fills an exit at that bar's close, and the reason is flatten.
    short = range(0, 36)
    days, sessions = _warmup(20, short)
    session = _flat(short)
    session[20] = (100.0, 100.0, down, down, 9000.0)
    for k in range(21, 36):
        session[k] = (99.70, 99.70, 99.70, 99.70, 4000.0)
    last_day = days[-1] + timedelta(days=1)
    days = days + [last_day]
    sessions = sessions + [session]
    bars_days, bars = _tape(days, sessions)
    feat = build_features(bars_days, bars, P.lookback)
    sigs = all_signals(bars_days, feat, P)
    trades = simulate(bars_days, bars, sigs, P)
    _assert_trade(trades, reason="flatten", exit_k=35, exit_px=99.70, exit_on_close=True)

    # 12. A signal while a trade is open is ignored. The second move stays
    # inside the open trade's stop and target, so only the position rule rejects it.
    session = _flat(range(0, 45))
    session[20] = (100.0, 100.0, down, down, 9000.0)
    for k in list(range(21, 25)) + list(range(26, 45)):
        session[k] = (99.75, 99.75, 99.75, 99.75, 4000.0)
    second = 99.75 * math.exp(-0.0012)
    session[25] = (99.75, 99.75, second, second, 9000.0)
    sigs, trades = run([day], [session])
    if len(sigs[day]) < 2:
        raise AssertionError("expected two raw signals")
    _assert_trade(trades, reason="time", signal_k=20)

    # 13. After a target, a later signal can enter.
    session = _flat(range(0, 45))
    session[20] = (100.0, 100.0, down, down, 9000.0)
    session[21] = (99.80, 99.80, 99.80, 99.80, 4000.0)
    session[22] = (99.80, 99.91, 99.80, 99.91, 4000.0)
    session[23] = (99.92, 99.92, 99.92, 99.92, 4000.0)
    for k in range(24, 30):
        session[k] = (99.92, 99.92, 99.92, 99.92, 4000.0)
    session[30] = (99.92, 99.92, 99.92 * math.exp(-0.003), 99.92 * math.exp(-0.003), 9000.0)
    session[31] = (99.50, 99.50, 99.50, 99.50, 4000.0)
    session[32] = (99.50, 99.80, 99.50, 99.80, 4000.0)
    session[33] = (99.81, 99.81, 99.81, 99.81, 4000.0)
    sigs, trades = run([day], [session])
    if len(trades) != 2 or trades[0]["reason"] != "target" or trades[1]["signal_k"] != 30:
        raise AssertionError(f"re-entry after target failed: {[(t['signal_k'], t['reason']) for t in trades]}")

    # 14. One extra bar of delay fills on the second later open.
    session = _flat(minutes)
    session[20] = (100.0, 100.0, down, down, 9000.0)
    session[21] = (99.10, 99.10, 99.10, 99.10, 4000.0)
    session[22] = (99.40, 99.40, 99.40, 99.40, 4000.0)
    for k in range(23, 45):
        session[k] = (99.40, 99.40, 99.40, 99.40, 4000.0)
    sigs, trades = run([day], [session], params=Params(delay_bars=1))
    if not trades or trades[0]["entry_k"] != 22 or abs(trades[0]["entry_px"] - 99.40) > 1e-9:
        raise AssertionError(f"delay fill {trades[:1]}")

    print("self-test passed")


# --------------------------------------------------------------------------
# Store run
# --------------------------------------------------------------------------


def load_symbol(md: MarketData, symbol: str):
    rows = md.bars(symbol, "1m")
    if not rows:
        raise SystemExit(f"{symbol} has no 1-minute bars")
    bars: dict[date, dict] = defaultdict(dict)
    for b in rows:
        k = b.time.hour * 60 + b.time.minute - (9 * 60 + 30)
        if not 0 <= k <= 389:
            raise SystemExit(f"{symbol} bar outside regular hours: {b.time}")
        if b.close <= 0 or b.open <= 0:
            raise SystemExit(f"{symbol} nonpositive price at {b.time}")
        if k in bars[b.session]:
            raise SystemExit(f"{symbol} duplicate minute {b.session} {k}")
        bars[b.session][k] = (b.open, b.high, b.low, b.close, b.volume)
    if date(2021, 12, 31) in bars:
        raise SystemExit(f"{symbol} unexpectedly has bars on 2021-12-31")
    days = sorted(bars)
    if len(days) < 1000 and symbol in ("IGV", "QQQ", "SPY"):
        raise SystemExit(f"{symbol} has only {len(days)} sessions")
    return days, dict(bars)


def clock(day: date, k: int) -> str:
    stamp = datetime(day.year, day.month, day.day, 9, 30) + timedelta(minutes=k)
    return stamp.strftime("%Y-%m-%d %H:%M")


def json_ready(value):
    if isinstance(value, dict):
        return {str(k): json_ready(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [json_ready(v) for v in value]
    if isinstance(value, (date, datetime)):
        return value.isoformat()
    if isinstance(value, (np.floating, float)):
        value = float(value)
        if math.isnan(value) or math.isinf(value):
            return None
        return value
    if isinstance(value, (np.integer,)):
        return int(value)
    if isinstance(value, np.bool_):
        return bool(value)
    return value


def benchmarks(calendar: list[date], days: list[date], bars: dict):
    last_close = {}
    first_open = {}
    for day in days:
        session = bars[day]
        ks = sorted(session)
        first_open[day] = session[ks[0]][0]
        last_close[day] = session[ks[-1]][3]
    prev = None
    prev_map = {}
    for day in days:
        prev_map[day] = prev
        prev = last_close[day]
    cc = {}
    oc = {}
    for day in calendar:
        if day not in last_close or prev_map.get(day) is None:
            cc[day] = 0.0
        else:
            cc[day] = last_close[day] / prev_map[day] - 1.0
        if day not in last_close:
            oc[day] = 0.0
        else:
            oc[day] = last_close[day] / first_open[day] - 1.0
    return cc, oc, first_open


def cost_and_delay(days, bars, features, calendar) -> dict:
    out = {}
    for bps in (0.0, 1.0, 2.0, 4.0, 6.0):
        params = Params(cost_bps=bps)
        sigs = all_signals(days, features, params)
        trades = simulate(days, bars, sigs, params)
        stats = window_stats(calendar, trades, params.account)
        out[str(bps)] = {
            "full_sharpe": stats["full"]["sharpe"],
            "full_return": stats["full"]["total_return"],
            "oos_sharpe": stats["oos"]["sharpe"],
            "oos_return": stats["oos"]["total_return"],
            "is_sharpe": stats["is"]["sharpe"],
        }
    for name, params in (
        ("delay", Params(delay_bars=1)),
        ("close_fill", Params(fill_on_close=True)),
    ):
        sigs = all_signals(days, features, params)
        trades = simulate(days, bars, sigs, params)
        stats = window_stats(calendar, trades, params.account)
        out[name] = {
            "full_sharpe": stats["full"]["sharpe"],
            "full_return": stats["full"]["total_return"],
            "oos_sharpe": stats["oos"]["sharpe"],
            "oos_return": stats["oos"]["total_return"],
            "trades": stats["full"]["trades"],
        }
    return out


def grid(days, bars, features, calendar) -> dict:
    cells = []
    is_pos = 0
    finite_is = []
    finite_oos = []
    for rv in (1.5, 2.0, 3.0):
        for mag in (1.5, 2.0, 3.0):
            for d_max in (500_000.0, 1_000_000.0, 2_000_000.0):
                for hold in (5, 15, 30):
                    params = Params(rv_min=rv, mag_mult=mag, d_max=d_max, hold=hold)
                    sigs = all_signals(days, features, params)
                    trades = simulate(days, bars, sigs, params)
                    stats = window_stats(calendar, trades, params.account)
                    is_s = stats["is"]["sharpe"]
                    oos_s = stats["oos"]["sharpe"]
                    if is_s is not None and is_s == is_s and is_s > 0:
                        is_pos += 1
                    cell = {
                        "rv": rv, "mag": mag, "d_max": d_max, "hold": hold,
                        "is_sharpe": is_s, "oos_sharpe": oos_s,
                        "is_trades": stats["is"]["trades"],
                        "oos_trades": stats["oos"]["trades"],
                        "primary": rv == 2 and mag == 2 and d_max == 1_000_000 and hold == 15,
                    }
                    cells.append(cell)
                    if is_s == is_s and oos_s == oos_s:
                        finite_is.append(is_s)
                        finite_oos.append(oos_s)
    ranked = sorted(
        [c for c in cells if c["is_sharpe"] == c["is_sharpe"]],
        key=lambda c: c["is_sharpe"],
        reverse=True,
    )
    primary_rank = next(i for i, c in enumerate(ranked, start=1) if c["primary"])
    return {
        "cells": cells,
        "is_positive": is_pos,
        "n": len(cells),
        "is_positive_share": is_pos / len(cells),
        "primary_is_rank": primary_rank,
        "is_oos_spearman": spearman(finite_is, finite_oos),
    }


def run_symbol(days, bars, params: Params):
    features = build_features(days, bars, params.lookback)
    signals = all_signals(days, features, params)
    trades = simulate(days, bars, signals, params)
    raw = sum(len(v) for v in signals.values())
    return features, signals, trades, raw


def exposure(calendar, days, bars, trades) -> dict:
    eval_days = set(calendar)
    minutes = 0
    for day in days:
        if day in eval_days:
            minutes += len(bars[day])
    held = sum(t["hold_min"] for t in trades if t["day"] in eval_days)
    holds = [t["hold_min"] for t in trades if t["day"] in eval_days]
    return {
        "minutes": minutes,
        "minutes_in_market": held,
        "time_in_market": (held / minutes) if minutes else None,
        "hold_mean": float(np.mean(holds)) if holds else None,
        "hold_median": float(np.median(holds)) if holds else None,
    }


def run_store(digest: str) -> dict:
    with MarketData() as md:
        igv_days, igv_bars = load_symbol(md, "IGV")
        qqq_days, qqq_bars = load_symbol(md, "QQQ")
        spy_days, spy_bars = load_symbol(md, "SPY")
        splits = [a for a in md.corporate_actions("IGV") if a["type"] == "split"]
        if not any(a["split_ratio"] == 5 for a in splits):
            raise SystemExit("IGV 5-for-1 split is not in the store")

    if igv_days[0] != date(2021, 9, 27) or igv_days[-1] != date(2026, 9, 25):
        raise SystemExit(f"unexpected IGV range {igv_days[0]} {igv_days[-1]}")

    features, signals, trades, raw = run_symbol(igv_days, igv_bars, P)
    first_eval = igv_days[20]
    calendar = nyse_sessions(first_eval, igv_days[-1])
    eval_trades = [t for t in trades if t["day"] in set(calendar)]
    stats = window_stats(calendar, eval_trades, P.account)
    gross_stats = window_stats(calendar, eval_trades, P.account, "gross")
    cc, oc, session_open = benchmarks(calendar, igv_days, igv_bars)
    cc_arr = np.array([cc[d] for d in calendar])
    oc_arr = np.array([oc[d] for d in calendar])

    def bench_pack(arr):
        packed = {}
        for name in ("full", "is", "oos"):
            mask = slice_mask(calendar, name)
            days = [d for d, keep in zip(calendar, mask) if keep]
            packed[name] = pack_stats(arr[mask], days, [])
        return packed

    print("running placebos, grid, and controls")
    direction = direction_placebo(calendar, eval_trades, P.account)
    timing = timing_placebo(igv_days, igv_bars, features, eval_trades, P, calendar)
    daily_net = daily_from_trades(calendar, eval_trades, P.account, "net")
    boot = bootstrap_sharpe(daily_net)
    costs = cost_and_delay(igv_days, igv_bars, features, calendar)
    cells = grid(igv_days, igv_bars, features, calendar)
    parts = breakdowns(calendar, eval_trades, oc, session_open)
    # Attach close-to-close year returns. Open-to-close is already in parts.
    for year, row in parts["by_year"].items():
        mask = np.array([d.year == int(year) for d in calendar])
        row["bench_cc_return"] = float(np.prod(1.0 + cc_arr[mask]) - 1.0)
        row["bench_cc_sharpe"] = sharpe(cc_arr[mask])

    halves = thin_thick(eval_trades)
    events = event_study(igv_days, igv_bars, signals)
    uncapped_params = Params(d_max=float("inf"))
    _f, unc_sigs, unc_trades, unc_raw = run_symbol(igv_days, igv_bars, uncapped_params)
    unc_eval = [t for t in unc_trades if t["day"] in set(calendar)]
    unc_stats = window_stats(calendar, unc_eval, P.account)

    controls = {}
    for symbol, s_days, s_bars in (("QQQ", qqq_days, qqq_bars), ("SPY", spy_days, spy_bars)):
        _sf, s_sigs, s_trades, s_raw = run_symbol(s_days, s_bars, P)
        controls[symbol] = {
            "raw_signals": s_raw,
            "trades": len(s_trades),
            "sessions": len(s_days),
        }

    head, dirty = git_state()
    result = {
        "rules_sha256": digest,
        "git_head": head,
        "git_dirty": dirty,
        "seeds": {
            "direction": SEED_DIRECTION,
            "timing": SEED_TIMING,
            "bootstrap": SEED_BOOTSTRAP,
        },
        "primary": stats,
        "gross": {
            "full_sharpe": gross_stats["full"]["sharpe"],
            "is_sharpe": gross_stats["is"]["sharpe"],
            "oos_sharpe": gross_stats["oos"]["sharpe"],
            "full_return": gross_stats["full"]["total_return"],
            "oos_return": gross_stats["oos"]["total_return"],
        },
        "benchmark_close_to_close": bench_pack(cc_arr),
        "benchmark_open_to_close": bench_pack(oc_arr),
        "raw_signals": {
            "IGV": raw,
            "IGV_entries": len(eval_trades),
            "QQQ": controls["QQQ"]["raw_signals"],
            "SPY": controls["SPY"]["raw_signals"],
        },
        "controls": controls,
        "exposure": exposure(calendar, igv_days, igv_bars, eval_trades),
        "direction_placebo": direction,
        "timing_placebo": timing,
        "bootstrap": boot,
        "costs": costs,
        "grid": cells,
        "breakdowns": parts,
        "thin_thick": halves,
        "event_study": events,
        "uncapped": {
            "raw_signals": unc_raw,
            "avg_gross_bps": unc_stats["full"]["avg_gross_bps"],
            "trades": unc_stats["full"]["trades"],
            "full_sharpe": unc_stats["full"]["sharpe"],
            "oos_sharpe": unc_stats["oos"]["sharpe"],
            "full_return": unc_stats["full"]["total_return"],
        },
        "warmup_sessions": 20,
        "first_evaluation_session": first_eval.isoformat(),
        "calendar_sessions": len(calendar),
    }
    return result, calendar, eval_trades, daily_net, cc_arr, oc_arr


def write_outputs(result, calendar, trades, daily, cc, oc) -> None:
    payload = json_ready(result)
    (HERE / "results.json").write_text(json.dumps(payload, indent=2), encoding="utf-8")
    with (HERE / "daily.csv").open("w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(["session", "strategy", "bench_cc", "bench_oc", "equity"])
        equity = 1.0
        for i, day in enumerate(calendar):
            equity *= 1.0 + float(daily[i])
            writer.writerow([day.isoformat(), f"{daily[i]:.10f}", f"{cc[i]:.10f}", f"{oc[i]:.10f}", f"{equity:.10f}"])
    with (HERE / "trades.csv").open("w", newline="", encoding="utf-8") as fh:
        fields = [
            "session", "side", "signal_time", "entry_time", "entry_price",
            "exit_time", "exit_price", "exit_on_close", "exit_reason",
            "signal_dollar", "signal_logret", "gross_dollars", "net_dollars",
            "gross_bps", "net_bps", "hold_min",
        ]
        writer = csv.DictWriter(fh, fieldnames=fields)
        writer.writeheader()
        for t in trades:
            writer.writerow({
                "session": t["day"].isoformat(),
                "side": t["side"],
                "signal_time": clock(t["day"], t["signal_k"]),
                "entry_time": clock(t["day"], t["entry_k"]),
                "entry_price": f"{t['entry_px']:.6f}",
                "exit_time": clock(t["day"], t["exit_k"]),
                "exit_price": f"{t['exit_px']:.6f}",
                "exit_on_close": int(t["exit_on_close"]),
                "exit_reason": t["reason"],
                "signal_dollar": f"{t['dollar']:.4f}",
                "signal_logret": f"{t['r']:.8f}",
                "gross_dollars": f"{t['gross']:.6f}",
                "net_dollars": f"{t['net']:.6f}",
                "gross_bps": f"{t['gross_bps']:.6f}",
                "net_bps": f"{t['net_bps']:.6f}",
                "hold_min": t["hold_min"],
            })


def append_runlog(result, reason: str) -> None:
    head, dirty = git_state()
    primary = result["primary"]
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    lines = [
        f"## {stamp}",
        f"- reason: {reason}",
        f"- rules_sha256: {result['rules_sha256']}",
        f"- git_head: {head}",
        f"- git_dirty: {str(dirty).lower()}",
        f"- full: sharpe {primary['full']['sharpe']:.4f}, return {primary['full']['total_return']:.4%}",
        f"- IS: sharpe {primary['is']['sharpe']:.4f}, return {primary['is']['total_return']:.4%}",
        f"- OOS: sharpe {primary['oos']['sharpe']:.4f}, return {primary['oos']['total_return']:.4%}",
        "",
    ]
    path = HERE / "RUNLOG.md"
    previous = path.read_text(encoding="utf-8") if path.exists() else "# Run log\n\nAppend-only. One entry per store run.\n\n"
    path.write_text(previous + "\n".join(lines), encoding="utf-8")


def main() -> None:
    digest = assert_lock()
    self_test()
    if len(sys.argv) > 1 and sys.argv[1] == "self-test":
        return
    reason = sys.argv[1] if len(sys.argv) > 1 else "initial"
    result, calendar, trades, daily, cc, oc = run_store(digest)
    write_outputs(result, calendar, trades, daily, cc, oc)
    append_runlog(result, reason)
    p = result["primary"]
    print(
        f"OOS sharpe {p['oos']['sharpe']:.3f} return {p['oos']['total_return']:.2%} "
        f"trades {p['oos']['trades']}"
    )
    print(
        f"full sharpe {p['full']['sharpe']:.3f} return {p['full']['total_return']:.2%} "
        f"IS sharpe {p['is']['sharpe']:.3f}"
    )


if __name__ == "__main__":
    main()
