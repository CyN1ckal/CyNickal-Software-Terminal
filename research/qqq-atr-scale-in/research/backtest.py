# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered QQQ ATR scale-in backtest.

    python research/qqq-atr-scale-in/research/backtest.py
    python research/qqq-atr-scale-in/research/backtest.py --self-test

The self-test runs before the store is opened. A store run refuses to start
unless RULES.md still matches RULES.lock, and it appends one RUNLOG.md entry.
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
import subprocess
import sys
from dataclasses import dataclass
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    MarketData,
    ny_datetime,
    nyse_sessions,
    resample,
)

HERE = Path(__file__).resolve().parent
BAR_S = 300
ATR_N = 14
MAX_UNITS = 3
CUTOFF_MIN = 30
PRIMARY_SPACING = 0.5
PRIMARY_TARGET = 0.25
COST_BPS = 1.0
DELAY = 1
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
LAST_DAY = date(2026, 9, 25)
QQQ_FIRST = date(2021, 10, 7)
ANNUAL = 252
SEED_DIRECTION = 20260926
SEED_TIMING = 20260927
SEED_BOOT = 20260928
GRID_SPACING = (0.25, 0.375, 0.50, 0.625, 0.75)
GRID_TARGET = (0.125, 0.25, 0.375)
COST_SWEEP = (0.0, 0.5, 1.0, 2.0, 3.0)


@dataclass(frozen=True)
class Params:
    spacing: float = PRIMARY_SPACING
    target: float = PRIMARY_TARGET
    max_units: int = MAX_UNITS
    cost_bps: float = COST_BPS
    delay: int = DELAY


class Px:
    __slots__ = ("ts", "open", "close", "minute")

    def __init__(self, ts: int, open_: float, close: float, minute: int) -> None:
        self.ts = ts
        self.open = open_
        self.close = close
        self.minute = minute


def end_minute(day: date) -> int:
    return 13 * 60 if day in EARLY_CLOSES else 16 * 60


def minute_of(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def clock(ts: int) -> str:
    return ny_datetime(ts).strftime("%Y-%m-%d %H:%M")


# ---- ATR (same construction as counts.py; prior closes only) ----------------
def atr_at_close(daily) -> list[tuple[date, float]]:
    if len(daily) < ATR_N + 1:
        return []
    trs: list[float] = []
    days: list[date] = []
    for i in range(1, len(daily)):
        prev_c = daily[i - 1].close
        h, lo = daily[i].high, daily[i].low
        trs.append(max(h - lo, abs(h - prev_c), abs(lo - prev_c)))
        days.append(daily[i].session)
    atr = sum(trs[:ATR_N]) / ATR_N
    out = [(days[ATR_N - 1], atr)]
    for j in range(ATR_N, len(trs)):
        atr = ((ATR_N - 1) * atr + trs[j]) / ATR_N
        if atr > 0:
            out.append((days[j], atr))
    return out


def atr_for_day(closes: list[tuple[date, float]], day: date) -> tuple[float, date] | None:
    j = -1
    for i, (d, _) in enumerate(closes):
        if d < day:
            j = i
        else:
            break
    if j < 0:
        return None
    return closes[j][1], closes[j][0]


# ---- rule -------------------------------------------------------------------
def _decide(close: float, side: int, legs: list[tuple[int, float]], S: float, A: float, p: Params):
    """Return ('entry', side), ('add', None), ('exit', None), or None.

    Target is tested before an add. High and low are not arguments.
    """
    step = p.spacing * A
    if side == 0:
        if close <= S - step:
            return ("entry", 1)
        if close >= S + step:
            return ("entry", -1)
        return None
    prices = [px for _, px in legs]
    avg = sum(prices) / len(prices)
    n = len(prices)
    if side == 1:
        if close >= avg + p.target * A:
            return ("exit", None)
        if n < p.max_units and close <= S - (n + 1) * step and close < min(prices):
            return ("add", None)
        return None
    if close <= avg - p.target * A:
        return ("exit", None)
    if n < p.max_units and close >= S + (n + 1) * step and close > max(prices):
        return ("add", None)
    return None


def _pending(signal_ts: int, kind: str, side: int, bars_by_ts: dict[int, Px], end_m: int, p: Params):
    if p.delay < 1:
        raise RuntimeError("pending is only for a positive delay")
    for step in range(1, p.delay + 1):
        if signal_ts + step * BAR_S not in bars_by_ts:
            return None
    fill = bars_by_ts[signal_ts + p.delay * BAR_S]
    if fill.minute >= end_m - CUTOFF_MIN:
        return None
    return {"fill_ts": fill.ts, "kind": kind, "side": side}


def run_session(bars: list[Px], A: float, end_m: int, p: Params) -> tuple[list[dict], int]:
    """Campaigns and the count of bars during which a unit was on."""
    if not bars or A <= 0:
        return [], 0
    S = bars[0].open
    by_ts = {b.ts: b for b in bars}
    index = {b.ts: i for i, b in enumerate(bars)}
    side = 0
    legs: list[tuple[int, float]] = []
    pending = None
    campaigns: list[dict] = []
    exposed: set[int] = set()

    def entry_index(leg_ts: int, bar_i: int) -> int:
        if leg_ts in index:
            return index[leg_ts]
        return index[leg_ts - BAR_S] if leg_ts - BAR_S in index else bar_i

    def close_campaign(price: float, ts: int, reason: str, bar_i: int) -> None:
        nonlocal side, legs
        if price <= 0 or not legs:
            raise RuntimeError("exit requires a positive price and an open campaign")
        entry_i = entry_index(legs[0][0], bar_i)
        campaigns.append({
            "side": side,
            "legs": list(legs),
            "exit_ts": ts,
            "exit_price": price,
            "reason": reason,
            "entry_bar": entry_i,
            "exit_bar": bar_i,
        })
        side = 0
        legs = []

    def execute(kind: str, fill_side: int, price: float, ts: int, bar_i: int, reason: str) -> None:
        nonlocal side
        if price <= 0:
            raise RuntimeError("fill price is not positive")
        exposed.add(bar_i)
        if kind == "entry":
            if side != 0:
                raise RuntimeError("entry while in a campaign")
            side = fill_side
            legs.append((ts, price))
        elif kind == "add":
            if side == 0 or len(legs) >= p.max_units:
                raise RuntimeError("add without room")
            legs.append((ts, price))
        else:
            close_campaign(price, ts, reason, bar_i)

    for i, b in enumerate(bars):
        last = i == len(bars) - 1
        if pending is not None and pending["fill_ts"] == b.ts:
            kind = pending["kind"]
            execute(kind, pending["side"], b.open, b.ts, i, "target" if kind == "exit" else "")
            pending = None
        if side != 0:
            exposed.add(i)
        if last:
            if side != 0:
                reason = "session"
                if p.delay == 0:
                    action = _decide(b.close, side, legs, S, A, p)
                    if action is not None and action[0] == "exit":
                        reason = "target"
                execute("exit", 0, b.close, b.ts + BAR_S, i, reason)
            pending = None
            break
        if pending is not None:
            continue
        action = _decide(b.close, side, legs, S, A, p)
        if action is None:
            continue
        kind, fill_side = action
        if p.delay == 0:
            # The last bar never reaches here. Fill at this close and do not judge again.
            reason = "target" if kind == "exit" else ""
            execute(kind, side if kind != "entry" else fill_side, b.close, b.ts + BAR_S, i, reason)
            continue
        made = _pending(b.ts, kind, fill_side if fill_side is not None else side, by_ts, end_m, p)
        if made is not None:
            pending = made
    return campaigns, len(exposed)


def replay_campaign(bars: list[Px], start_i: int, forced_side: int, n_cap: int,
                    A: float, end_m: int, p: Params) -> dict | None:
    """One timing-placebo campaign. The first unit is forced; later units use the add rule."""
    local = Params(p.spacing, p.target, n_cap, p.cost_bps, p.delay)
    S = bars[0].open
    by_ts = {b.ts: b for b in bars}
    b0 = bars[start_i]
    side = forced_side
    legs = [(b0.ts, b0.open)]
    pending = None
    # Judge the start bar's close, then walk forward. The start fill already happened.
    i = start_i
    while i < len(bars):
        b = bars[i]
        last = i == len(bars) - 1
        if i != start_i and pending is not None and pending["fill_ts"] == b.ts:
            if pending["kind"] == "exit":
                return _pack(side, legs, b.open, b.ts, "target", start_i, i)
            legs.append((b.ts, b.open))
            pending = None
        if last:
            return _pack(side, legs, b.close, b.ts + BAR_S, "session", start_i, i)
        if pending is None:
            action = _decide(b.close, side, legs, S, A, local)
            if action is not None:
                kind, _ = action
                if local.delay == 0:
                    if kind == "exit":
                        return _pack(side, legs, b.close, b.ts + BAR_S, "target", start_i, i)
                    if kind == "add":
                        legs.append((b.ts + BAR_S, b.close))
                else:
                    made = _pending(b.ts, kind, side, by_ts, end_m, local)
                    if made is not None:
                        pending = made
        i += 1
    return _pack(side, legs, bars[-1].close, bars[-1].ts + BAR_S, "session", start_i, len(bars) - 1)


def _pack(side, legs, price, ts, reason, entry_bar, exit_bar) -> dict:
    return {
        "side": side, "legs": list(legs), "exit_ts": ts, "exit_price": price,
        "reason": reason, "entry_bar": entry_bar, "exit_bar": exit_bar,
    }


def per_unit(side: int, legs, exit_price: float, cost_bps: float) -> tuple[float, float]:
    rate = cost_bps / 10_000.0
    gross = [side * (exit_price / px - 1.0) for _, px in legs]
    g = sum(gross) / len(gross)
    n = g - 2.0 * rate
    return g, n


def day_return(campaigns: list[dict], cost_bps: float) -> float:
    """Session return on start-of-day equity. Each unit is one third of equity."""
    total = 0.0
    for c in campaigns:
        _, net = per_unit(c["side"], c["legs"], c["exit_price"], cost_bps)
        total += net * len(c["legs"])
    return total / MAX_UNITS


def fixed_gross(campaigns: list[dict]) -> float:
    total = 0.0
    for c in campaigns:
        for _, px in c["legs"]:
            total += c["side"] * (c["exit_price"] / px - 1.0) / MAX_UNITS
    return total


# ---- self-test --------------------------------------------------------------
def _bars(rows: list[tuple[int, float, float]], origin: int = 570) -> list[Px]:
    """rows are (minute offset from origin, open, close). Origin 570 is 09:30."""
    out = []
    for off, o, c in rows:
        minute = origin + off
        out.append(Px(minute * 60, o, c, minute))
    return out


def _one(camps, n, side, reason, entry, exit_):
    assert len(camps) == 1, camps
    c = camps[0]
    assert c["side"] == side
    assert len(c["legs"]) == n
    assert c["reason"] == reason
    assert abs(c["legs"][0][1] - entry) < 1e-12
    assert abs(c["exit_price"] - exit_) < 1e-12
    return c


def self_test() -> None:
    p = Params()
    A = 10.0
    # 1. One long, target, exit at the next open. High above the target is ignored
    #    on the fill bar: only the close counts, and that close is still short of it.
    bars = _bars([(0, 100, 94), (5, 96, 95), (10, 96, 99), (15, 99, 99), (20, 99, 99)])
    # Rewrite test 1 more carefully below; this first shape is replaced.
    # Bar 0 close 94 <= 95 schedules a buy. Bar +5 fills at 96.
    # avg 96, target 98.5. Close 95 does not exit and is not a new low past 90.
    # Bar +10 close 99 >= 98.5 schedules the exit. Bar +15 fills at 99.
    camps, _ = run_session(bars, A, 16 * 60, p)
    c = _one(camps, 1, 1, "target", 96, 99)
    assert c["legs"][0][0] == (570 + 5) * 60
    assert c["exit_ts"] == (570 + 15) * 60

    # 2. Three adds, never back at the target, flatten at the last close.
    bars = _bars([
        (0, 100, 94), (5, 94, 89), (10, 89, 84), (15, 84, 80), (20, 80, 80),
    ])
    camps, exp = run_session(bars, A, 16 * 60, p)
    c = _one(camps, 3, 1, "session", 94, 80)
    assert [px for _, px in c["legs"]] == [94, 89, 84]
    assert c["exit_ts"] == (570 + 20) * 60 + BAR_S
    # Fill bars and the flatten bar are in the market. The signal bar is not.
    assert exp == 4
    g1 = 80 / 94 - 1
    g2 = 80 / 89 - 1
    g3 = 80 / 84 - 1
    net = (1 / 3) * ((g1 - 0.0002) + (g2 - 0.0002) + (g3 - 0.0002))
    assert abs(day_return(camps, 1.0) - net) < 1e-12

    # 3. Short target.
    bars = _bars([(0, 100, 106), (5, 106, 100), (10, 101, 101), (15, 101, 101)])
    camps, _ = run_session(bars, A, 16 * 60, p)
    _one(camps, 1, -1, "target", 106, 101)

    # 4. Gap fill through the next level. A later close that is not a new low does not add.
    bars = _bars([(0, 100, 94), (5, 88, 89), (10, 89, 89.5), (15, 89.5, 89)])
    camps, _ = run_session(bars, A, 16 * 60, p)
    _one(camps, 1, 1, "session", 88, 89)

    # 5. First close beyond level 3 buys one unit. Later bars add once each, on new lows.
    bars = _bars([(0, 100, 80), (5, 80, 79), (10, 79, 78), (15, 78, 77), (20, 77, 77)])
    camps, _ = run_session(bars, A, 16 * 60, p)
    c = _one(camps, 3, 1, "session", 80, 77)
    assert [px for _, px in c["legs"]] == [80, 79, 78]

    # 6. No fill in the last 30 minutes. End is 10:00 so the cutoff is 09:30,
    #    and the only possible fill (09:35) is not strictly earlier than 09:30.
    bars = _bars([(0, 100, 94), (5, 94, 90), (10, 90, 90)])
    camps, _ = run_session(bars, A, 600, p)
    assert camps == []

    # 7. Missing next bucket. No fill across the hole.
    bars = _bars([(0, 100, 94), (10, 90, 90)])
    camps, _ = run_session(bars, A, 16 * 60, p)
    assert camps == []

    # 8. Early-close cutoff. End 13:00, so a fill at 12:30 or later is refused.
    #    Entry scheduled at 12:20 fills at 12:25. The target on that bar would
    #    fill at 12:30 and is refused. The position flattens at the last close.
    bars = _bars([(170, 100, 94), (175, 96, 99), (180, 99, 99), (185, 97, 97)], origin=570)
    # 570+170 = 740 = 12:20. End minute 780. Cutoff is minute < 750.
    camps, _ = run_session(bars, A, 13 * 60, p)
    c = _one(camps, 1, 1, "session", 96, 97)
    assert c["legs"][0][0] == 745 * 60

    # 9. Exit, then a new campaign on a later close. Not a same-fill flip.
    bars = _bars([
        (0, 100, 94), (5, 94, 99), (10, 100, 106), (15, 106, 100), (20, 101, 101), (25, 101, 101),
    ])
    camps, _ = run_session(bars, A, 16 * 60, p)
    assert len(camps) == 2
    assert camps[0]["side"] == 1 and camps[0]["reason"] == "target"
    assert abs(camps[0]["exit_price"] - 100) < 1e-12
    assert camps[1]["side"] == -1 and camps[1]["reason"] == "target"
    assert camps[1]["legs"][0][0] == (570 + 15) * 60
    assert abs(camps[1]["legs"][0][1] - 106) < 1e-12

    # 10. A signal on the last bar does not open.
    bars = _bars([(0, 100, 100), (5, 90, 90)])
    assert run_session(bars, A, 16 * 60, p)[0] == []

    # 11. No signal.
    bars = _bars([(0, 100, 100), (5, 100, 101), (10, 101, 100)])
    assert run_session(bars, A, 16 * 60, p)[0] == []

    # 12. A wick through the target does not exit. Close is still below it.
    bars = _bars([(0, 100, 94), (5, 94, 95), (10, 95, 95)])
    camps, _ = run_session(bars, A, 16 * 60, p)
    _one(camps, 1, 1, "session", 94, 95)

    # 13. DELAY 0 fills at the signal close and does not judge that bar again.
    bars = _bars([(0, 100, 94), (5, 97, 99), (10, 99, 99)])
    camps, _ = run_session(bars, A, 16 * 60, Params(delay=0))
    c = _one(camps, 1, 1, "target", 94, 99)
    assert c["legs"][0][0] == 570 * 60 + BAR_S
    assert c["exit_ts"] == (570 + 5) * 60 + BAR_S

    # 14. DELAY 0 on the last bar: target exits as target; otherwise session. No entry.
    bars = _bars([(0, 100, 94), (5, 94, 99)])
    camps, _ = run_session(bars, A, 16 * 60, Params(delay=0))
    _one(camps, 1, 1, "target", 94, 99)
    bars = _bars([(0, 100, 94), (5, 94, 94.5)])
    camps, _ = run_session(bars, A, 16 * 60, Params(delay=0))
    _one(camps, 1, 1, "session", 94, 94.5)
    bars = _bars([(0, 100, 100), (5, 90, 90)])
    assert run_session(bars, A, 16 * 60, Params(delay=0))[0] == []

    # 15. DELAY 2 fills two bars later and does not decide while the order is pending.
    bars = _bars([(0, 100, 94), (5, 90, 90), (10, 96, 99), (15, 99, 99), (20, 99, 99)])
    camps, _ = run_session(bars, A, 16 * 60, Params(delay=2))
    c = _one(camps, 1, 1, "target", 96, 99)
    assert c["legs"][0][0] == (570 + 10) * 60

    # 16. Replay: forced long, price keeps falling, adds stop at the cap, session flatten.
    bars = _bars([(0, 100, 100), (5, 96, 89), (10, 89, 84), (15, 84, 80), (20, 80, 80)])
    camp = replay_campaign(bars, 1, 1, 3, A, 16 * 60, p)
    assert camp is not None and camp["side"] == 1 and len(camp["legs"]) == 3
    assert camp["reason"] == "session"
    # Forced long that is immediately through the target exits with one unit.
    bars = _bars([(0, 100, 100), (5, 100, 103), (10, 103, 103)])
    camp = replay_campaign(bars, 1, 1, 3, A, 16 * 60, p)
    assert camp is not None and len(camp["legs"]) == 1 and camp["reason"] == "target"

    # 17. A short gap does not add unless the close is above every fill.
    bars = _bars([(0, 100, 106), (5, 112, 111), (10, 111, 111)])
    camps, _ = run_session(bars, A, 16 * 60, p)
    # Level 2 is 110. Close 111 is above it, but 111 is not above the fill at 112.
    _one(camps, 1, -1, "session", 112, 111)


def rules_hash() -> str:
    return hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def check_lock() -> str:
    h = rules_hash()
    locked = {}
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        if " " in line:
            k, v = line.split(" ", 1)
            locked[k] = v.strip()
    if locked.get("sha256") != h:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {locked.get('sha256')}. Refusing to run.")
    return h


# ---- statistics -------------------------------------------------------------
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


def profit_factor(dollars: np.ndarray) -> float:
    if len(dollars) == 0:
        return float("nan")
    wins = dollars[dollars > 0].sum()
    losses = dollars[dollars < 0].sum()
    if losses < 0:
        return float(wins / abs(losses))
    return float("nan")


def skew_g1(x: np.ndarray) -> float:
    if len(x) < 3:
        return float("nan")
    m = float(x.mean())
    d = x - m
    m2 = float(np.mean(d ** 2))
    if m2 <= 0:
        return float("nan")
    m3 = float(np.mean(d ** 3))
    return m3 / (m2 ** 1.5)


def metrics(r: np.ndarray, trades: list[dict]) -> dict:
    total = float(np.prod(1.0 + r) - 1.0) if len(r) else float("nan")
    n = len(r)
    sd = float(r.std(ddof=1)) if n > 1 else float("nan")
    dollars = np.array([t["net_dollars"] for t in trades]) if trades else np.zeros(0)
    nets = np.array([t["net_ret"] for t in trades]) if trades else np.zeros(0)
    gross = np.array([t["gross_ret"] for t in trades]) if trades else np.zeros(0)
    out = {
        "sessions": n,
        "sharpe": sharpe(r),
        "total_return": total,
        "cagr": float((1.0 + total) ** (ANNUAL / n) - 1.0) if n and total > -1 else float("nan"),
        "ann_vol": float(sd * math.sqrt(ANNUAL)) if sd == sd else float("nan"),
        "max_dd": max_drawdown(r),
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if sd == sd and sd > 0 else float("nan"),
        "trades": len(trades),
        "win_rate": float((dollars > 0).mean()) if len(dollars) else float("nan"),
        "profit_factor": profit_factor(dollars),
        "avg_net_bp": float(nets.mean() * 1e4) if len(nets) else float("nan"),
        "avg_gross_bp": float(gross.mean() * 1e4) if len(gross) else float("nan"),
        "avg_winner_bp": float(nets[dollars > 0].mean() * 1e4) if np.any(dollars > 0) else float("nan"),
        "avg_loser_bp": float(nets[dollars < 0].mean() * 1e4) if np.any(dollars < 0) else float("nan"),
        "net_dollars_per_start_equity": float(dollars.sum()) if len(dollars) else 0.0,
    }
    if trades:
        held = np.array([t["bars_held"] for t in trades], dtype=float)
        out["median_bars_held"] = float(np.median(held))
        out["mean_bars_held"] = float(held.mean())
        out["trades_per_year"] = float(len(trades) / (n / ANNUAL)) if n else float("nan")
    return out


def slice_metrics(r: np.ndarray, mask: np.ndarray, trades: list[dict], sessions: list[date]) -> dict:
    keep = {sessions[i] for i in range(len(sessions)) if mask[i]}
    sub = [t for t in trades if t["session"] in keep]
    return metrics(r[mask], sub)


# ---- market -----------------------------------------------------------------
def prepare(md: MarketData, symbol: str):
    daily = md.bars(symbol, "1d")
    bars_5m = resample(md.bars(symbol, "1m"), BAR_S)
    grouped: dict[date, list[Px]] = {}
    for b in bars_5m:
        grouped.setdefault(b.session, []).append(Px(b.ts, b.open, b.close, minute_of(b.ts)))
    closes = atr_at_close(daily)
    daily_close = {b.session: b.close for b in daily}
    # First session that has a strictly prior ATR and an intraday bar.
    first = None
    for day in sorted(grouped):
        if atr_for_day(closes, day) is not None:
            first = day
            break
    if first is None:
        raise RuntimeError(f"{symbol} has no usable ATR session")
    calendar = [d for d in nyse_sessions(first, LAST_DAY)]
    sessions = []
    for day in calendar:
        got = atr_for_day(closes, day)
        bars = grouped.get(day, [])
        tradable = False
        atr = None
        atr_day = None
        if got is not None and bars:
            atr, atr_day = got
            last_m = bars[-1].minute
            ok_last = last_m >= (12 * 60 + 55 if day in EARLY_CLOSES else 15 * 60 + 55)
            tradable = atr > 0 and atr_day < day and bars[0].minute == 9 * 60 + 30 and ok_last
        sessions.append({
            "day": day,
            "bars": bars if tradable else [],
            "atr": atr if tradable else None,
            "atr_day": atr_day,
            "tradable": tradable,
            "open": bars[0].open if tradable else None,
            "last": bars[-1].close if bars else None,
            "daily_close": daily_close.get(day),
        })
    return sessions, daily_close


def check_prepared(symbol: str, sessions: list[dict]) -> None:
    if symbol == "QQQ":
        if sessions[0]["day"] != QQQ_FIRST:
            raise RuntimeError(f"QQQ first session is {sessions[0]['day']}, rules require {QQQ_FIRST}")
        gap = date(2021, 12, 31)
        if gap not in {s["day"] for s in sessions}:
            raise RuntimeError("2021-12-31 is missing from the QQQ calendar")
        row = next(s for s in sessions if s["day"] == gap)
        if row["tradable"] or row["bars"]:
            raise RuntimeError("2021-12-31 should have no five-minute tape")
    for s in sessions:
        if s["tradable"]:
            if not (s["atr"] and s["atr"] > 0 and s["atr_day"] < s["day"]):
                raise RuntimeError(f"ATR look-ahead or empty ATR on {s['day']}")
            if s["bars"][0].minute != 9 * 60 + 30:
                raise RuntimeError(f"missing 09:30 bar on {s['day']}")


def simulate(sessions: list[dict], p: Params) -> tuple[np.ndarray, list[dict], list[int]]:
    """Net daily returns at p.cost_bps, trade rows without dollars, exposed counts."""
    rets = np.zeros(len(sessions))
    trades: list[dict] = []
    exposed = []
    equity = 1.0
    for i, s in enumerate(sessions):
        if not s["tradable"]:
            exposed.append(0)
            continue
        camps, n_exp = run_session(s["bars"], s["atr"], end_minute(s["day"]), p)
        exposed.append(n_exp)
        rets[i] = day_return(camps, p.cost_bps)
        unit = equity / MAX_UNITS
        for c in camps:
            g, n = per_unit(c["side"], c["legs"], c["exit_price"], p.cost_bps)
            trades.append({
                "session": s["day"],
                "side": c["side"],
                "n_units": len(c["legs"]),
                "entry_ts": c["legs"][0][0],
                "exit_ts": c["exit_ts"],
                "entry_price": sum(px for _, px in c["legs"]) / len(c["legs"]),
                "exit_price": c["exit_price"],
                "reason": c["reason"],
                "legs": c["legs"],
                "gross_ret": g,
                "net_ret": n,
                "net_dollars": n * len(c["legs"]) * unit,
                "bars_held": c["exit_bar"] - c["entry_bar"] + 1,
                "gross_fixed": fixed_gross([c]),
            })
        equity *= 1.0 + rets[i]
    return rets, trades, exposed


def reprice(sessions: list[dict], trades_by_day: list[list[dict]], cost_bps: float) -> np.ndarray:
    """Path does not depend on cost. Rebuild the daily return at another cost."""
    out = np.zeros(len(sessions))
    for i, camps in enumerate(trades_by_day):
        if not camps:
            continue
        total = 0.0
        for c in camps:
            _, n = per_unit(c["side"], c["legs"], c["exit_price"], cost_bps)
            total += n * len(c["legs"])
        out[i] = total / MAX_UNITS
    return out


def gross_series(n: int, trades: list[dict], sessions: list[date], signs: np.ndarray | None = None) -> np.ndarray:
    idx = {d: i for i, d in enumerate(sessions)}
    g = np.zeros(n)
    for k, t in enumerate(trades):
        sign = 1.0 if signs is None else float(signs[k])
        g[idx[t["session"]]] += sign * t["gross_fixed"]
    return g


def legal_fill_indexes(bars: list[Px], end_m: int) -> list[int]:
    return [i for i, b in enumerate(bars) if i > 0 and b.minute < end_m - CUTOFF_MIN]


def has_run(bars: list[Px], legal: set[int], i: int, n: int) -> bool:
    if i not in legal:
        return False
    idx = i
    for _ in range(1, n):
        nxt = idx + 1
        if nxt >= len(bars) or nxt not in legal or bars[nxt].ts != bars[idx].ts + BAR_S:
            return False
        idx = nxt
    return True


def quintiles(values: np.ndarray) -> np.ndarray:
    n = len(values)
    order = np.argsort(values, kind="mergesort")
    q = np.empty(n, dtype=int)
    q[order] = np.minimum(np.arange(n) * 5 // n, 4)
    return q


def block_bootstrap(r: np.ndarray) -> dict:
    rng = np.random.default_rng(SEED_BOOT)
    n = len(r)
    block = 20
    n_blocks = math.ceil(n / block)
    starts = rng.integers(0, n, size=(2000, n_blocks))
    idx = np.arange(block)
    out = np.empty(2000)
    for d in range(2000):
        sample = np.concatenate([r[(int(s) + idx) % n] for s in starts[d]])[:n]
        out[d] = sharpe(sample)
    return {
        "p2_5": float(np.quantile(out, 0.025)),
        "p97_5": float(np.quantile(out, 0.975)),
        "share_le_0": float(np.mean(out <= 0)),
        "draws": 2000,
        "block": block,
        "seed": SEED_BOOT,
    }


def clean(obj):
    if isinstance(obj, dict):
        return {str(k): clean(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [clean(v) for v in obj]
    if isinstance(obj, np.ndarray):
        return clean(obj.tolist())
    if isinstance(obj, (np.floating,)):
        obj = float(obj)
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, float):
        if math.isnan(obj) or math.isinf(obj):
            return None
        return obj
    if isinstance(obj, (date, datetime)):
        return obj.isoformat()
    return obj


def git_head() -> str:
    r = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True)
    return r.stdout.strip()


def git_dirty() -> bool:
    r = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True)
    return bool(r.stdout.strip())


def write_log(rules_sha: str, reason: str, full: dict, ins: dict, oos: dict) -> None:
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nAppend-only. One entry per store run.\n\n", encoding="utf-8", newline="\n")
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    text = (
        f"## {stamp}\n"
        f"- reason: {reason}\n"
        f"- rules_sha256: {rules_sha}\n"
        f"- git_head: {git_head()}\n"
        f"- git_dirty: {str(git_dirty()).lower()}\n"
        f"- full: sharpe {full['sharpe']:.4f}, return {full['total_return']:.4%}\n"
        f"- IS: sharpe {ins['sharpe']:.4f}, return {ins['total_return']:.4%}\n"
        f"- OOS: sharpe {oos['sharpe']:.4f}, return {oos['total_return']:.4%}\n\n"
    )
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)


def group_rows(trades: list[dict], sessions: list[dict]) -> list[list[dict]]:
    """Raw campaigns keyed like `trades`, but the simulator's dicts are flat.

    Cost repricing needs the legs. They are still on each trade.
    """
    by = {s["day"]: [] for s in sessions}
    for t in trades:
        by[t["session"]].append(t)
    return [by[s["day"]] for s in sessions]


def breakdown_side(trades: list[dict]) -> list[dict]:
    rows = []
    for side, name in ((1, "long"), (-1, "short")):
        sub = [t for t in trades if t["side"] == side]
        dollars = np.array([t["net_dollars"] for t in sub]) if sub else np.zeros(0)
        nets = np.array([t["net_ret"] for t in sub]) if sub else np.zeros(0)
        rows.append({
            "side": name,
            "trades": len(sub),
            "win_rate": float((dollars > 0).mean()) if len(sub) else None,
            "profit_factor": profit_factor(dollars),
            "avg_net_bp": float(nets.mean() * 1e4) if len(sub) else None,
            "net_dollars": float(dollars.sum()) if len(sub) else 0.0,
        })
    return rows


def breakdown_reason(trades: list[dict]) -> list[dict]:
    rows = []
    for reason in ("target", "session"):
        sub = [t for t in trades if t["reason"] == reason]
        dollars = np.array([t["net_dollars"] for t in sub]) if sub else np.zeros(0)
        nets = np.array([t["net_ret"] for t in sub]) if sub else np.zeros(0)
        rows.append({
            "reason": reason,
            "trades": len(sub),
            "win_rate": float((dollars > 0).mean()) if len(sub) else None,
            "profit_factor": profit_factor(dollars),
            "avg_net_bp": float(nets.mean() * 1e4) if len(sub) else None,
            "net_dollars": float(dollars.sum()) if len(sub) else 0.0,
        })
    return rows


def breakdown_units(trades: list[dict]) -> list[dict]:
    rows = []
    for n in (1, 2, 3):
        sub = [t for t in trades if t["n_units"] == n]
        nets = np.array([t["net_ret"] for t in sub]) if sub else np.zeros(0)
        dollars = np.array([t["net_dollars"] for t in sub]) if sub else np.zeros(0)
        rows.append({
            "units": n,
            "trades": len(sub),
            "win_rate": float((dollars > 0).mean()) if len(sub) else None,
            "mean_net_ret": float(nets.mean()) if len(sub) else None,
            "avg_net_bp": float(nets.mean() * 1e4) if len(sub) else None,
            "net_dollars": float(dollars.sum()) if len(sub) else 0.0,
        })
    return rows


def breakdown_hour(trades: list[dict]) -> list[dict]:
    rows = []
    for hour in range(9, 16):
        sub = [t for t in trades if ny_datetime(t["entry_ts"]).hour == hour]
        dollars = np.array([t["net_dollars"] for t in sub]) if sub else np.zeros(0)
        nets = np.array([t["net_ret"] for t in sub]) if sub else np.zeros(0)
        rows.append({
            "hour": hour,
            "trades": len(sub),
            "win_rate": float((dollars > 0).mean()) if len(sub) else None,
            "avg_net_bp": float(nets.mean() * 1e4) if len(sub) else None,
            "net_dollars": float(dollars.sum()) if len(sub) else 0.0,
        })
    return rows


def year_table(sessions: list[date], r: np.ndarray, bench: np.ndarray) -> list[dict]:
    rows = []
    years = sorted({d.year for d in sessions})
    for y in years:
        mask = np.array([d.year == y for d in sessions])
        rr = r[mask]
        rows.append({
            "year": y,
            "sessions": int(mask.sum()),
            "return": float(np.prod(1.0 + rr) - 1.0),
            "sharpe": sharpe(rr),
            "max_dd": max_drawdown(rr),
            "benchmark_return": float(np.prod(1.0 + bench[mask]) - 1.0),
        })
    return rows


def bench_marks(sessions: list[dict], daily_close: dict[date, float]) -> tuple[np.ndarray, np.ndarray, int]:
    """Close-to-close and open-to-close. Returns (cc, oc, n_5m_marks).

    The first session's previous mark is the latest daily close before the
    evaluation calendar (the warm-up close). A session with no daily bar uses
    its last five-minute close. A session with neither keeps a zero return.
    """
    prior = [d for d in daily_close if d < sessions[0]["day"]]
    prev = daily_close[max(prior)] if prior else None
    cc = np.zeros(len(sessions))
    oc = np.zeros(len(sessions))
    n_5m = 0
    for i, s in enumerate(sessions):
        if s["daily_close"] is not None:
            mark = s["daily_close"]
        elif s["last"] is not None:
            mark = s["last"]
            n_5m += 1
        else:
            mark = None
        if mark is not None and prev is not None and prev > 0:
            cc[i] = mark / prev - 1.0
        if mark is not None:
            prev = mark
        if s["tradable"] and s["open"]:
            oc[i] = s["last"] / s["open"] - 1.0
    return cc, oc, n_5m


def run_symbol(sessions: list[dict], p: Params):
    rets, trades, exposed = simulate(sessions, p)
    days = [s["day"] for s in sessions]
    return rets, trades, exposed, days


def acceptance(primary_is, primary_oos, placebo_p, grid_pos, grid_n, ret_2x, spy_oos, n_oos) -> tuple[list[dict], str]:
    lines = [
        {
            "id": 1,
            "name": "OOS Sharpe and profit factor",
            "required": "Sharpe >= 0.5 and profit factor >= 1.10",
            "actual": {"sharpe": primary_oos["sharpe"], "profit_factor": primary_oos["profit_factor"]},
            "passed": _ge(primary_oos["sharpe"], 0.5) and _ge(primary_oos["profit_factor"], 1.10),
        },
        {
            "id": 2,
            "name": "Direction placebo",
            "required": "p <= 0.05",
            "actual": placebo_p,
            "passed": _ge(0.05, placebo_p) and placebo_p == placebo_p,
        },
        {
            "id": 3,
            "name": "In-sample Sharpe and plateau",
            "required": "IS Sharpe > 0 and at least 9 of 15 grid cells > 0",
            "actual": {"is_sharpe": primary_is["sharpe"], "grid_positive": grid_pos, "grid_n": grid_n},
            "passed": _gt(primary_is["sharpe"], 0.0) and grid_pos >= 9,
        },
        {
            "id": 4,
            "name": "Full-sample return at 2 bp",
            "required": "total return > 0",
            "actual": ret_2x,
            "passed": _gt(ret_2x, 0.0),
        },
        {
            "id": 5,
            "name": "SPY out-of-sample Sharpe",
            "required": "> 0",
            "actual": spy_oos,
            "passed": _gt(spy_oos, 0.0),
        },
        {
            "id": 6,
            "name": "Out-of-sample campaigns",
            "required": ">= 100",
            "actual": n_oos,
            "passed": n_oos >= 100,
        },
    ]
    if not lines[5]["passed"]:
        status = "Inconclusive"
    elif all(line["passed"] for line in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    return lines, status


def _ge(a, b) -> bool:
    return isinstance(a, (int, float)) and a == a and isinstance(b, (int, float)) and a >= b


def _gt(a, b) -> bool:
    return isinstance(a, (int, float)) and a == a and a > b


def write_trades(path: Path, rows: list[dict]) -> None:
    fields = ["symbol", "session", "side", "n_units", "entry_ts", "exit_ts", "entry_time",
              "exit_time", "entry_price", "exit_price", "exit_reason", "leg_prices",
              "gross_ret", "net_ret", "net_dollars", "bars_held"]
    with path.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for t in rows:
            w.writerow({
                "symbol": t["symbol"],
                "session": t["session"].isoformat(),
                "side": t["side"],
                "n_units": t["n_units"],
                "entry_ts": t["entry_ts"],
                "exit_ts": t["exit_ts"],
                "entry_time": clock(t["entry_ts"]),
                "exit_time": clock(t["exit_ts"]),
                "entry_price": f"{t['entry_price']:.10f}",
                "exit_price": f"{t['exit_price']:.10f}",
                "exit_reason": t["reason"],
                "leg_prices": ";".join(f"{px:.10f}" for _, px in t["legs"]),
                "gross_ret": f"{t['gross_ret']:.12f}",
                "net_ret": f"{t['net_ret']:.12f}",
                "net_dollars": f"{t['net_dollars']:.12f}",
                "bars_held": t["bars_held"],
            })


def write_daily(path: Path, qqq, spy_r, igv_r, spy_days, igv_days) -> None:
    sessions = qqq["days"]
    spy_map = {d: spy_r[i] for i, d in enumerate(spy_days)}
    igv_map = {d: igv_r[i] for i, d in enumerate(igv_days)}
    fields = ["session", "in_sample", "qqq_ret", "bench_cc", "bench_oc", "equity",
              "excursion", "quintile", "spy_ret", "igv_ret"]
    equity = 1.0
    with path.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for i, day in enumerate(sessions):
            equity *= 1.0 + qqq["rets"][i]
            w.writerow({
                "session": day.isoformat(),
                "in_sample": int(day <= IS_END),
                "qqq_ret": f"{qqq['rets'][i]:.12f}",
                "bench_cc": f"{qqq['bench_cc'][i]:.12f}",
                "bench_oc": f"{qqq['bench_oc'][i]:.12f}",
                "equity": f"{equity:.12f}",
                "excursion": "" if qqq["excursion"][i] is None else f"{qqq['excursion'][i]:.8f}",
                "quintile": "" if qqq["quintile"][i] is None else int(qqq["quintile"][i]) + 1,
                "spy_ret": "" if day not in spy_map else f"{spy_map[day]:.12f}",
                "igv_ret": "" if day not in igv_map else f"{igv_map[day]:.12f}",
            })


def corr(a_days, a_r, b_days, b_r) -> float:
    mb = {d: b_r[i] for i, d in enumerate(b_days)}
    xs, ys = [], []
    for i, d in enumerate(a_days):
        if d in mb:
            xs.append(a_r[i])
            ys.append(mb[d])
    if len(xs) < 3:
        return float("nan")
    return float(np.corrcoef(xs, ys)[0, 1])


def main() -> None:
    self_test()
    if "--self-test" in sys.argv:
        print("self-test passed")
        return
    rules_sha = check_lock()
    reason = "initial"
    if "--reason" in sys.argv:
        reason = sys.argv[sys.argv.index("--reason") + 1]
    p = Params()
    prepared = {}
    dailies = {}
    with MarketData() as md:
        for symbol in ("QQQ", "SPY", "IGV"):
            sessions, daily_close = prepare(md, symbol)
            prepared[symbol] = sessions
            dailies[symbol] = daily_close
    for symbol, sessions in prepared.items():
        check_prepared(symbol, sessions)

    qqq_s = prepared["QQQ"]
    rets, trades, exposed, days = run_symbol(qqq_s, p)
    mask_is = np.array([d <= IS_END for d in days])
    mask_oos = np.array([d >= OOS_START for d in days])
    full_m = metrics(rets, trades)
    is_m = slice_metrics(rets, mask_is, trades, days)
    oos_m = slice_metrics(rets, mask_oos, trades, days)
    full_m["exposure"] = float(sum(exposed) / sum(len(s["bars"]) for s in qqq_s if s["tradable"]))

    bench_cc, bench_oc, n_5m = bench_marks(qqq_s, dailies["QQQ"])
    bench = {
        "close_to_close": {
            "full": metrics(bench_cc, []),
            "is": metrics(bench_cc[mask_is], []),
            "oos": metrics(bench_cc[mask_oos], []),
            "marks_from_5m_close": n_5m,
        },
        "open_to_close": {
            "full": metrics(bench_oc, []),
            "is": metrics(bench_oc[mask_is], []),
            "oos": metrics(bench_oc[mask_oos], []),
        },
    }

    # Cost sweep from the stored legs. The path does not depend on cost.
    by_day = group_rows(trades, qqq_s)
    costs = {}
    for bps in COST_SWEEP:
        rr = reprice(qqq_s, by_day, bps)
        costs[str(bps)] = {
            "full_sharpe": sharpe(rr),
            "oos_sharpe": sharpe(rr[mask_oos]),
            "is_sharpe": sharpe(rr[mask_is]),
            "full_return": float(np.prod(1.0 + rr) - 1.0),
            "oos_return": float(np.prod(1.0 + rr[mask_oos]) - 1.0),
            "is_return": float(np.prod(1.0 + rr[mask_is]) - 1.0),
        }

    delays = {"1": {"full_sharpe": full_m["sharpe"], "oos_sharpe": oos_m["sharpe"],
                     "full_return": full_m["total_return"], "oos_return": oos_m["total_return"]}}
    for delay in (2, 0):
        rr, _, _, _ = run_symbol(qqq_s, Params(delay=delay))
        delays[str(delay)] = {
            "full_sharpe": sharpe(rr),
            "oos_sharpe": sharpe(rr[mask_oos]),
            "full_return": float(np.prod(1.0 + rr) - 1.0),
            "oos_return": float(np.prod(1.0 + rr[mask_oos]) - 1.0),
        }

    # Direction placebo.
    actual_g = gross_series(len(days), trades, days)
    actual_g_sharpe = sharpe(actual_g)
    rng = np.random.default_rng(SEED_DIRECTION)
    signs = rng.choice(np.array([-1.0, 1.0]), size=(2000, len(trades)))
    null = np.empty(2000)
    for i in range(2000):
        null[i] = sharpe(gross_series(len(days), trades, days, signs[i]))
    n_ge = int(np.sum(null >= actual_g_sharpe - 1e-15))
    direction = {
        "actual_gross_sharpe": actual_g_sharpe,
        "null_mean": float(null.mean()),
        "null_p95": float(np.quantile(null, 0.95)),
        "p": (1 + n_ge) / 2001,
        "draws": 2000,
        "seed": SEED_DIRECTION,
        "null": null.tolist(),
    }

    # Timing placebo. One generator, sessions in calendar order, campaigns in entry order.
    bundle = []
    by_sess: dict[date, list[dict]] = {}
    for t in trades:
        by_sess.setdefault(t["session"], []).append(t)
    for s in qqq_s:
        camps = by_sess.get(s["day"], [])
        bundle.append({
            "bars": s["bars"], "atr": s["atr"], "end_m": end_minute(s["day"]),
            "camps": camps if s["tradable"] else [],
        })
    rng_t = np.random.default_rng(SEED_TIMING)
    timing_null = np.empty(500)
    camp_counts = np.empty(500)
    for i in range(500):
        g, n_camps = timing_draw_counted(bundle, rng_t, p)
        timing_null[i] = sharpe(g)
        camp_counts[i] = n_camps
    n_ge_t = int(np.sum(timing_null >= actual_g_sharpe - 1e-15))
    timing = {
        "actual_gross_sharpe": actual_g_sharpe,
        "null_mean": float(timing_null.mean()),
        "null_p95": float(np.quantile(timing_null, 0.95)),
        "p": (1 + n_ge_t) / 501,
        "draws": 500,
        "seed": SEED_TIMING,
        "mean_sessions_with_a_contribution": float(camp_counts.mean()),
        "primary_campaigns": len(trades),
        "null": timing_null.tolist(),
    }

    boot = block_bootstrap(rets)

    # Grid.
    grid = []
    for spacing in GRID_SPACING:
        for target in GRID_TARGET:
            rr, tr, _, _ = run_symbol(qqq_s, Params(spacing=spacing, target=target))
            grid.append({
                "spacing": spacing,
                "target": target,
                "is_sharpe": sharpe(rr[mask_is]),
                "oos_sharpe": sharpe(rr[mask_oos]),
                "is_return": float(np.prod(1.0 + rr[mask_is]) - 1.0),
                "oos_return": float(np.prod(1.0 + rr[mask_oos]) - 1.0),
                "is_trades": len([t for t in tr if t["session"] <= IS_END]),
            })
    def _finite_gt(cell):
        v = cell["is_sharpe"]
        return isinstance(v, float) and v == v and v > 0
    n_pos = sum(1 for cell in grid if _finite_gt(cell))
    better = 0
    primary_is_sharpe = None
    for cell in grid:
        if cell["spacing"] == PRIMARY_SPACING and cell["target"] == PRIMARY_TARGET:
            primary_is_sharpe = cell["is_sharpe"]
    for cell in grid:
        v = cell["is_sharpe"]
        if isinstance(v, float) and v == v and isinstance(primary_is_sharpe, float) and v > primary_is_sharpe:
            better += 1
    is_best = max(grid, key=lambda cell: (cell["is_sharpe"] if cell["is_sharpe"] == cell["is_sharpe"] else -1e9,
                                           -GRID_SPACING.index(cell["spacing"]),
                                           -GRID_TARGET.index(cell["target"])))
    grid_summary = {
        "n_cells": 15,
        "n_is_positive": n_pos,
        "primary_is_rank": better + 1,
        "is_best": {k: is_best[k] for k in ("spacing", "target", "is_sharpe", "oos_sharpe")},
        "tie_break": "If in-sample Sharpes tie, the listed spacing order then the listed target order wins. Nothing is selected.",
    }

    cross = {}
    cross_rets = {}
    all_trades = []
    for t in trades:
        t["symbol"] = "QQQ"
        all_trades.append(t)
    for symbol in ("SPY", "IGV"):
        rr, tr, exp, dd = run_symbol(prepared[symbol], p)
        m_is = np.array([d <= IS_END for d in dd])
        m_oos = np.array([d >= OOS_START for d in dd])
        for t in tr:
            t["symbol"] = symbol
            all_trades.append(t)
        cross[symbol] = {
            "full": metrics(rr, tr),
            "is": slice_metrics(rr, m_is, tr, dd),
            "oos": slice_metrics(rr, m_oos, tr, dd),
            "first": dd[0],
            "last": dd[-1],
            "sessions": len(dd),
        }
        cross[symbol]["full"]["exposure"] = float(
            sum(exp) / sum(len(s["bars"]) for s in prepared[symbol] if s["tradable"]))
        cross_rets[symbol] = (dd, rr)

    # Predictions and quintiles on QQQ.
    tradable_i = [i for i, s in enumerate(qqq_s) if s["tradable"]]
    exc = np.array([abs(qqq_s[i]["last"] - qqq_s[i]["open"]) / qqq_s[i]["atr"] for i in tradable_i])
    q = quintiles(exc)
    excursion = [None] * len(qqq_s)
    quintile = [None] * len(qqq_s)
    for k, i in enumerate(tradable_i):
        excursion[i] = float(exc[k])
        quintile[i] = int(q[k])
    q_means = []
    for qi in range(5):
        idx = [tradable_i[k] for k in range(len(tradable_i)) if q[k] == qi]
        q_means.append({
            "quintile": qi + 1,
            "sessions": len(idx),
            "mean_strategy_return": float(rets[idx].mean()) if idx else None,
            "mean_excursion": float(exc[q == qi].mean()) if np.any(q == qi) else None,
        })
    idx23 = [tradable_i[k] for k in range(len(tradable_i)) if q[k] in (1, 2)]
    idx5 = [tradable_i[k] for k in range(len(tradable_i)) if q[k] == 4]
    mean23 = float(rets[idx23].mean()) if idx23 else float("nan")
    mean5 = float(rets[idx5].mean()) if idx5 else float("nan")

    units = breakdown_units(trades)
    u1 = next(r for r in units if r["units"] == 1)
    u3 = next(r for r in units if r["units"] == 3)
    g1 = skew_g1(np.array([t["net_ret"] for t in trades])) if trades else float("nan")
    pred1 = "consistent" if _gt(full_m["win_rate"], 0.55) and _gt(0.0, g1) else "not consistent"
    pred2 = "consistent" if _gt(mean23, 0.0) and _gt(0.0, mean5) else "not consistent"
    if u1["trades"] < 5 or u3["trades"] < 5:
        pred3 = "not testable"
    elif u1["mean_net_ret"] > u3["mean_net_ret"]:
        pred3 = "consistent"
    else:
        pred3 = "not consistent"

    lines, status = acceptance(
        is_m, oos_m, direction["p"], n_pos, 15,
        costs["2.0"]["full_return"], cross["SPY"]["oos"]["sharpe"], oos_m["trades"],
    )

    results = {
        "rules_sha256": rules_sha,
        "reason": reason,
        "seeds": {"direction": SEED_DIRECTION, "timing": SEED_TIMING, "bootstrap": SEED_BOOT},
        "status": status,
        "n_passed": sum(1 for line in lines if line["passed"]),
        "n_failed": sum(1 for line in lines if not line["passed"]),
        "acceptance": lines,
        "primary": {"full": full_m, "is": is_m, "oos": oos_m},
        "benchmark": bench,
        "costs": costs,
        "delay": delays,
        "placebo": {"direction": {k: direction[k] for k in direction if k != "null"},
                    "timing": {k: timing[k] for k in timing if k != "null"}},
        "placebo_null": {"direction": direction["null"], "timing": timing["null"]},
        "bootstrap": boot,
        "grid": grid,
        "grid_summary": grid_summary,
        "cross": cross,
        "correlation": {
            "qqq_spy": corr(days, rets, cross_rets["SPY"][0], cross_rets["SPY"][1]),
            "qqq_igv": corr(days, rets, cross_rets["IGV"][0], cross_rets["IGV"][1]),
            "spy_igv": corr(cross_rets["SPY"][0], cross_rets["SPY"][1],
                            cross_rets["IGV"][0], cross_rets["IGV"][1]),
        },
        "predictions": {
            "1": {"win_rate": full_m["win_rate"], "skew_g1": g1, "score": pred1},
            "2": {"mean_quintiles_2_and_3": mean23, "mean_quintile_5": mean5,
                  "quintiles": q_means, "score": pred2},
            "3": {"mean_1_unit": u1["mean_net_ret"], "n_1": u1["trades"],
                  "mean_3_unit": u3["mean_net_ret"], "n_3": u3["trades"], "score": pred3},
        },
        "breakdowns": {
            "year": year_table(days, rets, bench_cc),
            "side": breakdown_side(trades),
            "exit": breakdown_reason(trades),
            "hour": breakdown_hour(trades),
            "units": units,
        },
        "coverage": {
            "first": days[0], "last": days[-1],
            "calendar_sessions": len(days),
            "tradable_sessions": sum(1 for s in qqq_s if s["tradable"]),
            "is_sessions": int(mask_is.sum()),
            "oos_sessions": int(mask_oos.sum()),
            "sessions_with_a_campaign": len({t["session"] for t in trades}),
        },
    }
    # Drop the bulky nulls from the human results file's twin: they stay, charts need them.
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8", newline="\n")
    qqq_pack = {"days": days, "rets": rets, "bench_cc": bench_cc, "bench_oc": bench_oc,
                "excursion": excursion, "quintile": quintile}
    write_daily(HERE / "daily.csv", qqq_pack, cross_rets["SPY"][1], cross_rets["IGV"][1],
                cross_rets["SPY"][0], cross_rets["IGV"][0])
    write_trades(HERE / "trades.csv", all_trades)
    write_log(rules_sha, reason, full_m, is_m, oos_m)
    print(f"status {status}  OOS sharpe {oos_m['sharpe']:.4f}  return {oos_m['total_return']:.4%}  "
          f"trades {oos_m['trades']}  full sharpe {full_m['sharpe']:.4f}")


def timing_draw_counted(bundle, rng, p):
    g = np.zeros(len(bundle))
    n_camps = 0
    for i, s in enumerate(bundle):
        camps = s["camps"]
        if not camps or not s["bars"]:
            continue
        bars = s["bars"]
        legal = legal_fill_indexes(bars, s["end_m"])
        legal_set = set(legal)
        cursor = -1
        for c in camps:
            cands = [j for j in legal if j > cursor and has_run(bars, legal_set, j, c["n_units"])]
            if not cands:
                continue
            start = int(cands[int(rng.integers(0, len(cands)))])
            played = replay_campaign(bars, start, c["side"], c["n_units"], s["atr"], s["end_m"], p)
            if played is None:
                continue
            g[i] += fixed_gross([played])
            cursor = played["exit_bar"]
            n_camps += 1
    return g, n_camps


if __name__ == "__main__":
    main()
