# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Intraday channel trend, as locked in RULES.md.

Run from the repo root or from anywhere:

    python research/intraday-channel-trend/research/backtest.py

The self-test runs first and does not read the store. The store run writes
REPORT inputs next to this file. It does not write the report, and it does
not change RULES.md.
"""

from __future__ import annotations

import csv
import json
import math
import sys
from collections import defaultdict
from dataclasses import asdict, dataclass
from datetime import date, datetime
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import (  # noqa: E402
    EARLY_CLOSES,
    Bar,
    MarketData,
    _ny_local_to_utc,
    ny_datetime,
    resample,
    rth_minutes,
)

OUT = Path(__file__).resolve().parent
BAR_SECONDS = 15 * 60
PRIMARY_N = 8
PRIMARY_K = 2.5
COST_BP = 1.0
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
GRID_N = (4, 6, 8, 12)
GRID_K = (1.5, 2.0, 2.5, 3.0, 4.0)
COST_GRID_BP = (0.0, 0.5, 1.0, 2.0, 3.0)
PLACEBO_DRAWS = 2000
BOOTSTRAP_DRAWS = 2000
PLACEBO_SEED = 20260926
BOOTSTRAP_SEED = 20260927
BLOCK = 20
NAMES = ("QQQ", "SPY", "IGV")


@dataclass(frozen=True)
class Trade:
    symbol: str
    session: str
    side: int
    reason: str
    entry_ts: int
    exit_ts: int
    entry_px: float
    exit_px: float
    gross: float

    @property
    def hold_s(self) -> int:
        return self.exit_ts - self.entry_ts


def bar_minutes(bar: Bar) -> int:
    t = bar.time
    return t.hour * 60 + t.minute


def cutoff_minutes(day: date) -> int:
    if day in EARLY_CLOSES:
        return 12 * 60 + 30
    return 15 * 60 + 30


def session_cap_ts(day: date) -> int:
    if day in EARLY_CLOSES:
        return _ny_local_to_utc(day, 13 * 3600 + 60)
    return _ny_local_to_utc(day, 16 * 3600)


def flatten_ts(day: date, last: Bar, bar_seconds: int) -> int:
    return min(last.ts + bar_seconds, session_cap_ts(day))


def _finite_bar(bar: Bar) -> bool:
    prices = (bar.open, bar.high, bar.low, bar.close)
    if not all(math.isfinite(p) and p > 0 for p in prices):
        return False
    return bar.high + 1e-9 >= max(bar.open, bar.close) and bar.low - 1e-9 <= min(bar.open, bar.close)


def prepare_sessions(bars: list[Bar]) -> dict[date, list[Bar]]:
    grouped: dict[date, list[Bar]] = defaultdict(list)
    for bar in bars:
        if _finite_bar(bar):
            grouped[bar.session].append(bar)
    out: dict[date, list[Bar]] = {}
    for day, rows in grouped.items():
        rows.sort(key=lambda b: b.ts)
        deduped: list[Bar] = []
        for bar in rows:
            if deduped and deduped[-1].ts == bar.ts:
                deduped[-1] = bar
            else:
                deduped.append(bar)
        out[day] = deduped
    return out


def _next_fill(bars: list[Bar], signal_i: int, lag: int, bar_seconds: int) -> int | None:
    target = bars[signal_i].ts + lag * bar_seconds
    for j in range(signal_i + 1, len(bars)):
        if bars[j].ts >= target:
            return j
    return None


def simulate_session(
    bars: list[Bar],
    symbol: str,
    n: int,
    k: float,
    *,
    fill_mode: str = "next",
    use_stop: bool = True,
    bar_seconds: int = BAR_SECONDS,
) -> list[Trade]:
    """One session. `fill_mode` is next, skip, or close. See RULES.md."""
    m = len(bars)
    if m == 0 or n < 1:
        return []
    day = bars[0].session
    cutoff = cutoff_minutes(day)
    lag = {"next": 1, "skip": 2, "close": 0}[fill_mode]

    high = [b.high for b in bars]
    low = [b.low for b in bars]
    close = [b.close for b in bars]
    tr = [high[0] - low[0]]
    for i in range(1, m):
        tr.append(max(high[i] - low[i], abs(high[i] - close[i - 1]), abs(low[i] - close[i - 1])))

    def atr_at(i: int) -> float | None:
        if i < n - 1:
            return None
        window = tr[i - n + 1 : i + 1]
        return sum(window) / n

    trades: list[Trade] = []
    pos = 0
    entry_px = 0.0
    entry_ts = 0
    extreme = 0.0
    pending: tuple[int, str, int] | None = None  # fill index, action, side

    def close_position(exit_px: float, exit_ts: int, reason: str) -> None:
        nonlocal pos
        if pos == 0 or entry_px <= 0 or exit_px <= 0:
            pos = 0
            return
        gross = pos * (exit_px / entry_px - 1.0)
        trades.append(
            Trade(symbol, day.isoformat(), pos, reason, entry_ts, exit_ts, entry_px, exit_px, gross)
        )
        pos = 0

    def open_position(side: int, px: float, ts: int) -> None:
        nonlocal pos, entry_px, entry_ts, extreme
        pos = side
        entry_px = px
        entry_ts = ts
        extreme = px

    def stop_hit(i: int, atr: float | None) -> bool:
        if not use_stop or pos == 0 or atr is None or atr <= 0:
            return False
        if pos > 0:
            return close[i] < extreme - k * atr
        return close[i] > extreme + k * atr

    for i in range(m):
        if pending is not None and fill_mode != "close" and pending[0] == i:
            action, side = pending[1], pending[2]
            px = bars[i].open
            ts = bars[i].ts
            pending = None
            if action == "close":
                close_position(px, ts, "stop")
            elif action == "flip":
                close_position(px, ts, "flip")
                open_position(side, px, ts)
            elif action == "open":
                open_position(side, px, ts)

        if pos != 0:
            if pos > 0:
                extreme = max(extreme, high[i])
            else:
                extreme = min(extreme, low[i])

        atr = atr_at(i) if i >= n - 1 else None
        is_last = i == m - 1
        if is_last:
            if pos != 0:
                reason = "stop" if stop_hit(i, atr) else "eod"
                close_position(close[i], flatten_ts(day, bars[i], bar_seconds), reason)
            break

        if pending is not None or i < n:
            continue

        hh = max(high[i - n : i])
        ll = min(low[i - n : i])
        want = 0
        if atr is not None and atr > 0:
            if close[i] > hh:
                want = 1
            elif close[i] < ll:
                want = -1
        hit = stop_hit(i, atr)

        if fill_mode == "close":
            fill_clock = bar_minutes(bars[i]) + bar_seconds // 60
            enter_ok = fill_clock < cutoff
            px = close[i]
            ts = bars[i].ts + bar_seconds
            if pos != 0 and hit:
                close_position(px, ts, "stop")
            elif pos != 0 and want == -pos and enter_ok:
                close_position(px, ts, "flip")
                open_position(want, px, ts)
            elif pos == 0 and want != 0 and enter_ok:
                open_position(want, px, ts)
            continue

        fill_i = _next_fill(bars, i, lag, bar_seconds)
        # The final bar can host a stop fill. It cannot host a new entry or the open leg of a flip.
        enter_ok = fill_i is not None and fill_i < m - 1 and bar_minutes(bars[fill_i]) < cutoff
        if pos != 0 and hit:
            if fill_i is not None:
                pending = (fill_i, "close", 0)
        elif pos != 0 and want == -pos and enter_ok and fill_i is not None:
            pending = (fill_i, "flip", want)
        elif pos == 0 and want != 0 and enter_ok and fill_i is not None:
            pending = (fill_i, "open", want)

    return trades


def simulate_symbol(
    sessions: dict[date, list[Bar]],
    symbol: str,
    n: int,
    k: float,
    *,
    fill_mode: str = "next",
    use_stop: bool = True,
    bar_seconds: int = BAR_SECONDS,
) -> dict[date, list[Trade]]:
    return {
        day: simulate_session(
            rows, symbol, n, k, fill_mode=fill_mode, use_stop=use_stop, bar_seconds=bar_seconds
        )
        for day, rows in sessions.items()
    }


def day_return(trades: list[Trade], cost_bp: float) -> float:
    drag = 2.0 * cost_bp * 1e-4
    return sum(t.gross - drag for t in trades)


def aligned_returns(
    by_symbol: dict[str, dict[date, list[Trade]]],
    calendar: list[date],
    cost_bp: float,
    drop: dict[str, set[date]] | None = None,
) -> dict[str, np.ndarray]:
    """Daily fractional returns. A dropped name-session contributes 0."""
    out: dict[str, np.ndarray] = {}
    for name in NAMES:
        series = np.zeros(len(calendar))
        booked = by_symbol[name]
        skipped = drop.get(name, set()) if drop else set()
        for i, day in enumerate(calendar):
            if day in skipped:
                continue
            series[i] = day_return(booked.get(day, []), cost_bp)
        out[name] = series
    out["book"] = (out["QQQ"] + out["SPY"] + out["IGV"]) / 3.0
    return out


def finite_only(series: np.ndarray, mask: np.ndarray) -> np.ndarray:
    values = series[mask]
    return values[np.isfinite(values)]


def compound(series: np.ndarray) -> float:
    level = 1.0
    for value in series:
        if math.isfinite(float(value)):
            level *= 1.0 + float(value)
    return float(level - 1.0)


def equity_curve(returns: np.ndarray) -> np.ndarray:
    curve = np.empty(len(returns) + 1)
    curve[0] = 1.0
    for i, r in enumerate(returns):
        curve[i + 1] = curve[i] * (1.0 + r)
    return curve


def sharpe(returns: np.ndarray) -> float:
    if len(returns) < 2:
        return 0.0
    sd = float(np.std(returns, ddof=1))
    if sd == 0.0 or not math.isfinite(sd):
        return 0.0
    return float(np.mean(returns) / sd * math.sqrt(252))


def max_drawdown(curve: np.ndarray) -> tuple[float, int, int]:
    peak = curve[0]
    peak_i = 0
    worst = 0.0
    worst_peak = 0
    worst_trough = 0
    for i, level in enumerate(curve):
        if level > peak:
            peak = level
            peak_i = i
        dd = level / peak - 1.0 if peak > 0 else 0.0
        if dd < worst:
            worst = dd
            worst_peak = peak_i
            worst_trough = i
    return float(worst), worst_peak, worst_trough


def profit_factor(trades: list[Trade], cost_bp: float) -> float | None:
    drag = 2.0 * cost_bp * 1e-4
    gains = 0.0
    losses = 0.0
    for trade in trades:
        net = trade.gross - drag
        if net > 0:
            gains += net
        elif net < 0:
            losses += net
    if losses == 0.0:
        return None if gains == 0.0 else math.inf
    return gains / abs(losses)


def t_stat(returns: np.ndarray) -> float:
    if len(returns) < 2:
        return 0.0
    sd = float(np.std(returns, ddof=1))
    if sd == 0.0:
        return 0.0
    return float(np.mean(returns) / (sd / math.sqrt(len(returns))))


def collect_trades(
    by_symbol: dict[str, dict[date, list[Trade]]],
    calendar: list[date],
    drop: dict[str, set[date]] | None = None,
) -> list[Trade]:
    kept: list[Trade] = []
    days = set(calendar)
    for name in NAMES:
        skipped = drop.get(name, set()) if drop else set()
        for day, trades in by_symbol[name].items():
            if day not in days or day in skipped:
                continue
            kept.extend(trades)
    return kept


def exposure_fraction(
    by_symbol: dict[str, dict[date, list[Trade]]],
    calendar: list[date],
    sessions_15: dict[str, dict[date, list[Bar]]],
) -> dict[str, float]:
    """Share of regular-hours seconds with a position on, flat sessions included."""
    out: dict[str, float] = {}
    for name in NAMES:
        held = 0.0
        span = 0.0
        booked = by_symbol[name]
        for day in calendar:
            span += rth_minutes(day) * 60
            for trade in booked.get(day, []):
                held += max(0, trade.hold_s)
        out[name] = held / span if span else 0.0
    # Book exposure is the average of the three names, matching the 1/3 weights.
    out["book"] = (out["QQQ"] + out["SPY"] + out["IGV"]) / 3.0
    _ = sessions_15
    return out


def summarize_window(
    returns: np.ndarray,
    trades: list[Trade],
    cost_bp: float,
    exposure: float,
) -> dict[str, float | int | None]:
    curve = equity_curve(returns)
    dd, _, _ = max_drawdown(curve)
    drag = 2.0 * cost_bp * 1e-4
    nets = [t.gross - drag for t in trades]
    wins = [x for x in nets if x > 0]
    losses = [x for x in nets if x < 0]
    n = len(returns)
    vol = float(np.std(returns, ddof=1) * math.sqrt(252)) if n > 1 else 0.0
    total = float(curve[-1] - 1.0) if n else 0.0
    cagr = float(curve[-1] ** (252 / n) - 1.0) if n and curve[-1] > 0 else None
    return {
        "sessions": n,
        "sharpe": sharpe(returns),
        "total_return": total,
        "cagr": cagr,
        "volatility": vol,
        "max_drawdown": dd,
        "t_stat": t_stat(returns),
        "trades": len(trades),
        "trades_per_session": (len(trades) / n) if n else 0.0,
        "win_rate": (len(wins) / len(trades)) if trades else None,
        "profit_factor": profit_factor(trades, cost_bp),
        "avg_trade_bps": (float(np.mean(nets)) * 1e4) if nets else None,
        "avg_winner_bps": (float(np.mean(wins)) * 1e4) if wins else None,
        "avg_loser_bps": (float(np.mean(losses)) * 1e4) if losses else None,
        "exposure": exposure,
    }


def window_mask(calendar: list[date], start: date | None, end: date | None) -> np.ndarray:
    mask = np.ones(len(calendar), dtype=bool)
    for i, day in enumerate(calendar):
        if start is not None and day < start:
            mask[i] = False
        if end is not None and day > end:
            mask[i] = False
    return mask


def slice_trades(trades: list[Trade], start: date | None, end: date | None) -> list[Trade]:
    kept = []
    for trade in trades:
        day = date.fromisoformat(trade.session)
        if start is not None and day < start:
            continue
        if end is not None and day > end:
            continue
        kept.append(trade)
    return kept


def placebo_p(by_symbol: dict[str, dict[date, list[Trade]]], calendar: list[date], actual: float) -> dict:
    rng = np.random.default_rng(PLACEBO_SEED)
    # Trades in calendar order so a sign flip rebuilds the same days.
    per_day: list[list[tuple[float, str]]] = []
    lookup = {name: by_symbol[name] for name in NAMES}
    for day in calendar:
        row: list[tuple[float, str]] = []
        for name in NAMES:
            for trade in lookup[name].get(day, []):
                row.append((trade.gross, name))
        per_day.append(row)
    drag = 2.0 * COST_BP * 1e-4
    null = np.empty(PLACEBO_DRAWS)
    beat = 0
    for draw in range(PLACEBO_DRAWS):
        series = np.zeros(len(calendar))
        for i, row in enumerate(per_day):
            if not row:
                continue
            acc = {name: 0.0 for name in NAMES}
            signs = rng.choice((-1.0, 1.0), size=len(row))
            for (gross, name), sign in zip(row, signs):
                acc[name] += sign * gross - drag
            series[i] = (acc["QQQ"] + acc["SPY"] + acc["IGV"]) / 3.0
        value = sharpe(series)
        null[draw] = value
        if value >= actual - 1e-15:
            beat += 1
    counts, edges = np.histogram(null, bins=40)
    return {
        "draws": PLACEBO_DRAWS,
        "seed": PLACEBO_SEED,
        "p": (1 + beat) / (PLACEBO_DRAWS + 1),
        "null_mean": float(np.mean(null)),
        "null_p95": float(np.quantile(null, 0.95)),
        "actual_sharpe": actual,
        "hist": counts.astype(int).tolist(),
        "hist_edges": [float(x) for x in edges],
    }


def bootstrap_ci(returns: np.ndarray) -> dict:
    rng = np.random.default_rng(BOOTSTRAP_SEED)
    n = len(returns)
    n_blocks = int(math.ceil(n / BLOCK))
    samples = np.empty(BOOTSTRAP_DRAWS)
    for draw in range(BOOTSTRAP_DRAWS):
        starts = rng.integers(0, n, size=n_blocks)
        pieces = [returns[np.arange(s, s + BLOCK) % n] for s in starts]
        sample = np.concatenate(pieces)[:n]
        samples[draw] = sharpe(sample)
    lo, hi = np.quantile(samples, [0.025, 0.975])
    return {
        "draws": BOOTSTRAP_DRAWS,
        "seed": BOOTSTRAP_SEED,
        "block": BLOCK,
        "p2_5": float(lo),
        "p97_5": float(hi),
    }


def benchmark_returns(
    prints: dict[str, dict[date, tuple[float, float]]],
    calendar: list[date],
) -> dict[str, np.ndarray]:
    """Open-to-close and close-to-close from the first and last 1-minute prints."""
    otc = {name: np.full(len(calendar), np.nan) for name in NAMES}
    cc = {name: np.full(len(calendar), np.nan) for name in NAMES}
    prev_close = {name: None for name in NAMES}
    # Walk every stored session so a book gap still chains the prior close.
    all_days = sorted(set(calendar) | set().union(*[set(prints[name]) for name in NAMES]))
    cal_index = {day: i for i, day in enumerate(calendar)}
    for day in all_days:
        for name in NAMES:
            row = prints[name].get(day)
            if row is None:
                continue
            first_open, last_close = row
            if day in cal_index and first_open > 0:
                otc[name][cal_index[day]] = last_close / first_open - 1.0
            if day in cal_index and prev_close[name] not in (None, 0):
                cc[name][cal_index[day]] = last_close / prev_close[name] - 1.0
            prev_close[name] = last_close

    def average(columns: dict[str, np.ndarray]) -> np.ndarray:
        stacked = np.vstack([columns[name] for name in NAMES])
        finite = np.isfinite(stacked)
        count = finite.sum(axis=0)
        total = np.where(finite, stacked, 0.0).sum(axis=0)
        out = np.full(stacked.shape[1], np.nan)
        good = count > 0
        out[good] = total[good] / count[good]
        return out

    otc["book"] = average(otc)
    cc["book"] = average(cc)
    return {"otc": otc, "cc": cc}


def pearson(a: np.ndarray, b: np.ndarray) -> float | None:
    mask = np.isfinite(a) & np.isfinite(b)
    if int(mask.sum()) < 3:
        return None
    x = a[mask]
    y = b[mask]
    if float(np.std(x)) == 0.0 or float(np.std(y)) == 0.0:
        return None
    return float(np.corrcoef(x, y)[0, 1])


def quintiles(strategy: np.ndarray, otc: np.ndarray) -> list[dict]:
    mask = np.isfinite(otc)
    values = otc[mask]
    strat = strategy[mask]
    if len(values) < 5:
        return []
    edges = np.quantile(values, [0.2, 0.4, 0.6, 0.8])
    bins = np.digitize(values, edges)
    rows = []
    for q in range(5):
        take = bins == q
        rows.append(
            {
                "quintile": q + 1,
                "sessions": int(take.sum()),
                "mean_strategy": float(np.mean(strat[take])) if take.any() else None,
                "mean_open_to_close": float(np.mean(values[take])) if take.any() else None,
            }
        )
    return rows


def grouped_trade_stats(trades: list[Trade], key_fn, cost_bp: float) -> list[dict]:
    buckets: dict[str, list[Trade]] = defaultdict(list)
    for trade in trades:
        buckets[str(key_fn(trade))].append(trade)
    drag = 2.0 * cost_bp * 1e-4
    rows = []
    for key in sorted(buckets):
        group = buckets[key]
        nets = [t.gross - drag for t in group]
        wins = sum(1 for x in nets if x > 0)
        rows.append(
            {
                "key": key,
                "trades": len(group),
                "win_rate": wins / len(group),
                "avg_bps": float(np.mean(nets)) * 1e4,
                "sum_gross": float(sum(t.gross for t in group)),
                "sum_net": float(sum(nets)),
            }
        )
    return rows


def monthly_returns(calendar: list[date], returns: np.ndarray) -> list[dict]:
    buckets: dict[str, list[float]] = defaultdict(list)
    for day, ret in zip(calendar, returns):
        buckets[f"{day.year:04d}-{day.month:02d}"].append(float(ret))
    rows = []
    for key in sorted(buckets):
        rs = np.array(buckets[key])
        # Compound inside the month.
        level = 1.0
        for r in rs:
            level *= 1.0 + r
        rows.append({"month": key, "sessions": len(rs), "return": float(level - 1.0)})
    return rows


def jsonable(value):
    if isinstance(value, dict):
        return {str(k): jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [jsonable(v) for v in value]
    if isinstance(value, (np.floating,)):
        value = float(value)
    if isinstance(value, (np.integer,)):
        value = int(value)
    if isinstance(value, float):
        if math.isnan(value):
            return None
        if math.isinf(value):
            return "inf"
        return value
    return value


def write_equity_svg(path: Path, calendar: list[date], curves: dict[str, np.ndarray], drawdown: np.ndarray) -> None:
    width, height = 960, 520
    left, right, top = 64, 20, 28
    gap = 28
    eq_h = 300
    dd_h = 120
    plot_w = width - left - right

    def x_of(i: int, n: int) -> float:
        if n <= 1:
            return left
        return left + plot_w * i / (n - 1)

    def y_of(value: float, lo: float, hi: float, y0: float, h: float) -> float:
        if hi == lo:
            return y0 + h / 2
        return y0 + h * (1 - (value - lo) / (hi - lo))

    n = len(calendar)
    eq_lo = min(float(np.min(c)) for c in curves.values())
    eq_hi = max(float(np.max(c)) for c in curves.values())
    pad = 0.04 * (eq_hi - eq_lo or 1)
    eq_lo -= pad
    eq_hi += pad
    dd_lo = min(float(np.min(drawdown)), -0.01)
    dd_hi = 0.0

    colors = {"Strategy": "#1f4b99", "Close to close": "#8a8175", "Open to close": "#b7a99a"}
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<text x="64" y="18" font-family="Segoe UI, Helvetica, sans-serif" font-size="13" fill="#1c1915">Intraday channel trend, equal-weight QQQ / SPY / IGV</text>',
    ]

    def polyline(series: np.ndarray, lo: float, hi: float, y0: float, h: float, color: str) -> str:
        pts = []
        for i, value in enumerate(series):
            if not math.isfinite(float(value)):
                continue
            pts.append(f"{x_of(i, len(series)):.1f},{y_of(float(value), lo, hi, y0, h):.1f}")
        return f'<polyline fill="none" stroke="{color}" stroke-width="1.4" points="{" ".join(pts)}"/>'

    y0 = top
    # One scale for every equity line, with labels so the flat strategy is readable against the tape.
    span = eq_hi - eq_lo
    step = 0.25 if span > 0.8 else 0.1
    tick = math.floor(eq_lo / step) * step
    while tick <= eq_hi + 1e-9:
        if tick >= eq_lo - 1e-9:
            y = y_of(tick, eq_lo, eq_hi, y0, eq_h)
            parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left + plot_w}" y2="{y:.1f}" stroke="#e6e2da"/>')
            parts.append(
                f'<text x="{left - 8}" y="{y + 3:.1f}" text-anchor="end" font-family="Segoe UI, Helvetica, sans-serif" font-size="10" fill="#6b6560">{tick:.2f}</text>'
            )
        tick += step
    for name, series in curves.items():
        parts.append(polyline(series, eq_lo, eq_hi, y0, eq_h, colors[name]))
    parts.append(f'<text x="{left}" y="{top + eq_h + 14}" font-family="Segoe UI, Helvetica, sans-serif" font-size="11" fill="#1f4b99">Strategy</text>')
    parts.append(f'<text x="{left + 70}" y="{top + eq_h + 14}" font-family="Segoe UI, Helvetica, sans-serif" font-size="11" fill="#8a8175">Close to close</text>')
    parts.append(f'<text x="{left + 170}" y="{top + eq_h + 14}" font-family="Segoe UI, Helvetica, sans-serif" font-size="11" fill="#b7a99a">Open to close</text>')

    dd_y = top + eq_h + gap
    for level, label in ((0.0, "0%"), (dd_lo, f"{dd_lo * 100:.0f}%")):
        y = y_of(level, dd_lo, dd_hi, dd_y, dd_h)
        parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left + plot_w}" y2="{y:.1f}" stroke="#e6e2da"/>')
        parts.append(
            f'<text x="{left - 8}" y="{y + 3:.1f}" text-anchor="end" font-family="Segoe UI, Helvetica, sans-serif" font-size="10" fill="#6b6560">{label}</text>'
        )
    parts.append(polyline(drawdown, dd_lo, dd_hi, dd_y, dd_h, "#8c2f39"))
    parts.append(f'<text x="{left}" y="{dd_y + dd_h + 16}" font-family="Segoe UI, Helvetica, sans-serif" font-size="11" fill="#8c2f39">Strategy drawdown</text>')

    # Year labels along the equity panel.
    seen_year = set()
    for i, day in enumerate(calendar):
        if day.year in seen_year:
            continue
        seen_year.add(day.year)
        x = x_of(i, n)
        parts.append(f'<text x="{x:.1f}" y="{height - 8}" font-family="Segoe UI, Helvetica, sans-serif" font-size="11" fill="#6b6560">{day.year}</text>')

    parts.append("</svg>")
    path.write_text("\n".join(parts), encoding="utf-8")


def load_symbol(md: MarketData, symbol: str) -> tuple[dict[date, list[Bar]], dict[date, tuple[float, float]], dict[date, int], dict]:
    raw = md.bars(symbol, "1m")
    minutes = [bar for bar in raw if _finite_bar(bar)]
    fifteens = prepare_sessions(resample(minutes, BAR_SECONDS))
    prints: dict[date, tuple[float, float]] = {}
    counts: dict[date, int] = defaultdict(int)
    for bar in minutes:
        day = bar.session
        counts[day] += 1
        if day not in prints:
            prints[day] = (bar.open, bar.close)
        else:
            prints[day] = (prints[day][0], bar.close)
    coverage = {date.fromisoformat(row["session"]): int(row["bar_count"]) for row in md.coverage(symbol, "1m")}
    meta = {
        "symbol": symbol,
        "minute_bars_kept": len(minutes),
        "minute_bars_dropped": len(raw) - len(minutes),
        "sessions_15m": len(fifteens),
        "first_session": min(prints).isoformat() if prints else None,
        "last_session": max(prints).isoformat() if prints else None,
        "corporate_actions": md.corporate_actions(symbol),
        "coverage_sessions": len(coverage),
        "first_bucket_not_0930": sum(
            1 for rows in fifteens.values() if rows and bar_minutes(rows[0]) != 9 * 60 + 30
        ),
        "last_bar_before_expected_close": sum(
            1
            for day, rows in fifteens.items()
            if rows
            and bar_minutes(rows[-1]) < (13 * 60 if day in EARLY_CLOSES else 15 * 60 + 45)
        ),
        "minute_count_vs_coverage_mismatches": sum(
            1 for day, count in counts.items() if coverage.get(day, -1) != count
        ),
    }
    return fifteens, prints, coverage, meta


def thin_days(coverage: dict[date, int]) -> set[date]:
    out = set()
    for day, count in coverage.items():
        expected = rth_minutes(day)
        if expected <= 0:
            continue
        need = math.ceil(0.90 * expected - 1e-12)
        if 0 < count < need:
            out.add(day)
    return out


def assert_qqq_calendar(coverage: dict[date, int]) -> None:
    bad = []
    for day, count in sorted(coverage.items()):
        if count <= 0:
            continue
        if day in EARLY_CLOSES:
            if count != 211:
                bad.append((day.isoformat(), count, "early"))
        elif count != 390:
            bad.append((day.isoformat(), count, "full"))
    if bad:
        raise SystemExit(f"QQQ minute counts do not match the early-close calendar: {bad[:12]}")


def run_store() -> dict:
    with MarketData() as md:
        loaded = [load_symbol(md, name) for name in NAMES]
    sessions = {row[3]["symbol"]: row[0] for row in loaded}
    prints = {row[3]["symbol"]: row[1] for row in loaded}
    coverage = {row[3]["symbol"]: row[2] for row in loaded}
    meta = [row[3] for row in loaded]

    assert_qqq_calendar(coverage["QQQ"])
    calendar = sorted(day for day, rows in sessions["QQQ"].items() if rows)
    if IS_END not in set(calendar) or OOS_START not in set(calendar):
        raise SystemExit("The in-sample end or the out-of-sample start is not a QQQ session.")
    between = [day for day in calendar if IS_END < day < OOS_START]
    if between:
        raise SystemExit(f"Sessions fall between the sample cut: {between}")

    primary = {
        name: simulate_symbol(sessions[name], name, PRIMARY_N, PRIMARY_K, fill_mode="next", use_stop=True)
        for name in NAMES
    }
    skip = {
        name: simulate_symbol(sessions[name], name, PRIMARY_N, PRIMARY_K, fill_mode="skip", use_stop=True)
        for name in NAMES
    }
    at_close = {
        name: simulate_symbol(sessions[name], name, PRIMARY_N, PRIMARY_K, fill_mode="close", use_stop=True)
        for name in NAMES
    }
    hold = {
        name: simulate_symbol(sessions[name], name, PRIMARY_N, PRIMARY_K, fill_mode="next", use_stop=False)
        for name in NAMES
    }

    returns = aligned_returns(primary, calendar, COST_BP)
    trades = collect_trades(primary, calendar)
    exposure = exposure_fraction(primary, calendar, sessions)
    benches = benchmark_returns(prints, calendar)

    windows = {
        "full": (None, None),
        "is": (None, IS_END),
        "oos": (OOS_START, None),
    }

    def exposure_for(trade_map, days: list[date]) -> dict[str, float]:
        return exposure_fraction(trade_map, days, sessions)

    def pack_clean(trade_map, cost_bp: float, drop: dict[str, set[date]] | None = None) -> dict:
        ret_map = aligned_returns(trade_map, calendar, cost_bp, drop)
        packed = {}
        for label, (start, end) in windows.items():
            days = [day for day in calendar if (start is None or day >= start) and (end is None or day <= end)]
            mask = window_mask(calendar, start, end)
            exp_map = exposure_for(trade_map, days)
            # Dropped sessions contribute no hold time. Rebuild exposure with those trades removed.
            if drop:
                filtered = {}
                for name in NAMES:
                    filtered[name] = {
                        day: ([] if day in drop.get(name, set()) else trades_on)
                        for day, trades_on in trade_map[name].items()
                    }
                exp_map = exposure_for(filtered, days)
                trade_source = filtered
            else:
                trade_source = trade_map
            packed[label] = {}
            window_trades = collect_trades(trade_source, days)
            by_name: dict[str, list[Trade]] = {name: [] for name in NAMES}
            for trade in window_trades:
                by_name[trade.symbol].append(trade)
            for key in ("book",) + NAMES:
                subset = window_trades if key == "book" else by_name[key]
                packed[label][key] = summarize_window(ret_map[key][mask], subset, cost_bp, exp_map[key])
        return packed, ret_map

    primary_stats, primary_returns = pack_clean(primary, COST_BP)
    costs = {f"{bp:g}bp": pack_clean(primary, bp)[0] for bp in COST_GRID_BP}
    delays = {
        "next_open": primary_stats,
        "skip_one_bar": pack_clean(skip, COST_BP)[0],
        "signal_close_upper_bound": pack_clean(at_close, COST_BP)[0],
    }
    hold_stats, _ = pack_clean(hold, COST_BP)
    tape_drop = {name: thin_days(coverage[name]) for name in NAMES}
    tape_stats, _ = pack_clean(primary, COST_BP, tape_drop)

    grid = []
    for n in GRID_N:
        for k in GRID_K:
            ran = {name: simulate_symbol(sessions[name], name, n, k) for name in NAMES}
            stats, _ = pack_clean(ran, COST_BP)
            if n == PRIMARY_N and k == PRIMARY_K:
                if abs(stats["full"]["book"]["sharpe"] - primary_stats["full"]["book"]["sharpe"]) > 1e-9:
                    raise SystemExit("Grid primary cell does not match the primary run.")
            grid.append(
                {
                    "n": n,
                    "k": k,
                    "primary": n == PRIMARY_N and k == PRIMARY_K,
                    "is_sharpe": stats["is"]["book"]["sharpe"],
                    "oos_sharpe": stats["oos"]["book"]["sharpe"],
                    "full_sharpe": stats["full"]["book"]["sharpe"],
                    "is_profit_factor": stats["is"]["book"]["profit_factor"],
                    "oos_profit_factor": stats["oos"]["book"]["profit_factor"],
                    "full_total_return": stats["full"]["book"]["total_return"],
                }
            )

    full_curve = equity_curve(primary_returns["book"])
    dd_level, dd_peak, dd_trough = max_drawdown(full_curve)
    # Drawdown index is on the equity curve, which has a leading 1.0.
    def curve_date(index: int) -> str | None:
        if index <= 0:
            return calendar[0].isoformat()
        if index - 1 >= len(calendar):
            return calendar[-1].isoformat()
        return calendar[index - 1].isoformat()

    underwater = np.empty_like(full_curve)
    peak = -1e300
    for i, level in enumerate(full_curve):
        peak = max(peak, level)
        underwater[i] = level / peak - 1.0

    bh_curve = equity_curve(np.nan_to_num(benches["cc"]["book"], nan=0.0))
    otc_curve = equity_curve(np.nan_to_num(benches["otc"]["book"], nan=0.0))

    drag = 2.0 * COST_BP * 1e-4
    ranked = sorted(trades, key=lambda t: t.gross - drag)
    def trade_row(trade: Trade) -> dict:
        return {
            "symbol": trade.symbol,
            "session": trade.session,
            "side": "long" if trade.side > 0 else "short",
            "reason": trade.reason,
            "entry": ny_datetime(trade.entry_ts).strftime("%Y-%m-%d %H:%M"),
            "exit": ny_datetime(trade.exit_ts).strftime("%Y-%m-%d %H:%M"),
            "entry_px": trade.entry_px,
            "exit_px": trade.exit_px,
            "net_bps": (trade.gross - drag) * 1e4,
            "hold_minutes": trade.hold_s / 60,
        }

    is_grid_positive = sum(1 for cell in grid if cell["is_sharpe"] > 0)
    full_at_2bp = costs["2bp"]["full"]["book"]["total_return"]
    oos_names_positive = sum(1 for name in NAMES if primary_stats["oos"][name]["sharpe"] > 0)
    acceptance = {
        "oos_sharpe_and_profit_factor": bool(
            primary_stats["oos"]["book"]["sharpe"] >= 0.5
            and (primary_stats["oos"]["book"]["profit_factor"] or 0) >= 1.10
        ),
        "placebo": None,  # filled below
        "is_sharpe_and_grid": bool(primary_stats["is"]["book"]["sharpe"] > 0 and is_grid_positive >= 0.60 * len(grid)),
        "full_sample_positive_at_2bp": bool(full_at_2bp > 0),
        "two_names_oos_positive": bool(oos_names_positive >= 2),
        "is_grid_positive_cells": is_grid_positive,
        "is_grid_cells": len(grid),
        "oos_names_positive": oos_names_positive,
    }

    placebo = placebo_p(primary, calendar, primary_stats["full"]["book"]["sharpe"])
    acceptance["placebo"] = bool(placebo["p"] <= 0.05)
    acceptance["pass"] = all(
        acceptance[key]
        for key in (
            "oos_sharpe_and_profit_factor",
            "placebo",
            "is_sharpe_and_grid",
            "full_sample_positive_at_2bp",
            "two_names_oos_positive",
        )
    )

    session_sets = {name: set(prints[name]) for name in NAMES}
    summary = {
        "rules": "research/intraday-channel-trend/research/RULES.md",
        "primary": {"n": PRIMARY_N, "k": PRIMARY_K, "bar_minutes": 15, "cost_bp": COST_BP, "fill": "next_open"},
        "calendar": {
            "first": calendar[0].isoformat(),
            "last": calendar[-1].isoformat(),
            "sessions": len(calendar),
            "is_sessions": int(window_mask(calendar, None, IS_END).sum()),
            "oos_sessions": int(window_mask(calendar, OOS_START, None).sum()),
            "is_end": IS_END.isoformat(),
            "oos_start": OOS_START.isoformat(),
            "session_set_equal": session_sets["QQQ"] == session_sets["SPY"] == session_sets["IGV"],
            "spy_only": sorted(d.isoformat() for d in session_sets["SPY"] - session_sets["QQQ"])[:20],
            "igv_only": sorted(d.isoformat() for d in session_sets["IGV"] - session_sets["QQQ"])[:20],
            "qqq_missing_spy": sorted(d.isoformat() for d in session_sets["QQQ"] - session_sets["SPY"])[:20],
            "qqq_missing_igv": sorted(d.isoformat() for d in session_sets["QQQ"] - session_sets["IGV"])[:20],
        },
        "data": meta,
        "thin_sessions": {name: len(tape_drop[name]) for name in NAMES},
        "acceptance": acceptance,
        "primary_stats": primary_stats,
        "costs": costs,
        "delays": delays,
        "hold_to_close": hold_stats,
        "tape_filter": tape_stats,
        "grid": grid,
        "placebo": placebo,
        "bootstrap": bootstrap_ci(primary_returns["book"]),
        "correlation": {
            "book_vs_open_to_close": pearson(primary_returns["book"], benches["otc"]["book"]),
            "book_vs_close_to_close": pearson(primary_returns["book"], benches["cc"]["book"]),
            "qqq_spy": pearson(primary_returns["QQQ"], primary_returns["SPY"]),
            "qqq_igv": pearson(primary_returns["QQQ"], primary_returns["IGV"]),
            "spy_igv": pearson(primary_returns["SPY"], primary_returns["IGV"]),
        },
        "benchmarks": {
            "close_to_close": {
                label: summarize_window(finite_only(benches["cc"]["book"], window_mask(calendar, start, end)), [], 0.0, 1.0)
                for label, (start, end) in windows.items()
            },
            "open_to_close": {
                label: summarize_window(finite_only(benches["otc"]["book"], window_mask(calendar, start, end)), [], 0.0, 1.0)
                for label, (start, end) in windows.items()
            },
        },
        "years": [],
        "sides": grouped_trade_stats(trades, lambda t: "long" if t.side > 0 else "short", COST_BP),
        "reasons": grouped_trade_stats(trades, lambda t: t.reason, COST_BP),
        "hours": grouped_trade_stats(
            trades, lambda t: f"{ny_datetime(t.entry_ts).hour:02d}", COST_BP
        ),
        "quintiles": quintiles(primary_returns["book"], benches["otc"]["book"]),
        "months": monthly_returns(calendar, primary_returns["book"]),
        "worst_trades": [trade_row(t) for t in ranked[:10]],
        "best_trades": [trade_row(t) for t in ranked[-10:][::-1]],
        "drawdown": {
            "depth": dd_level,
            "peak_session": curve_date(dd_peak),
            "trough_session": curve_date(dd_trough),
        },
    }

    # Year rows from the book path, with per-year Sharpe.
    by_year: dict[int, list[int]] = defaultdict(list)
    for i, day in enumerate(calendar):
        by_year[day.year].append(i)
    for year in sorted(by_year):
        idx = np.array(by_year[year])
        year_days = [calendar[i] for i in idx]
        year_trades = [t for t in trades if t.session.startswith(str(year))]
        year_exposure = exposure_fraction(primary, year_days, sessions)["book"]
        summary["years"].append(
            {"year": year, **summarize_window(primary_returns["book"][idx], year_trades, COST_BP, year_exposure)}
        )

    for row in summary["years"]:
        idx = np.array(by_year[row["year"]])
        row["close_to_close"] = compound(benches["cc"]["book"][idx])
        row["open_to_close"] = compound(benches["otc"]["book"][idx])
        row["strategy"] = row["total_return"]

    write_outputs(summary, calendar, primary, primary_returns, benches, full_curve, bh_curve, otc_curve, underwater)
    return summary


def write_outputs(
    summary: dict,
    calendar: list[date],
    primary: dict[str, dict[date, list[Trade]]],
    primary_returns: dict[str, np.ndarray],
    benches: dict,
    full_curve: np.ndarray,
    bh_curve: np.ndarray,
    otc_curve: np.ndarray,
    underwater: np.ndarray,
) -> None:
    (OUT / "summary.json").write_text(json.dumps(jsonable(summary), indent=2), encoding="utf-8")

    drag = 2.0 * COST_BP * 1e-4
    with (OUT / "trades.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            ["symbol", "session", "side", "reason", "entry", "exit", "entry_px", "exit_px", "gross_bps", "net_bps_1bp", "hold_minutes"]
        )
        rows = []
        for name in NAMES:
            for day, trades in primary[name].items():
                for trade in trades:
                    rows.append(trade)
        rows.sort(key=lambda t: (t.entry_ts, t.symbol))
        for trade in rows:
            writer.writerow(
                [
                    trade.symbol,
                    trade.session,
                    "long" if trade.side > 0 else "short",
                    trade.reason,
                    ny_datetime(trade.entry_ts).strftime("%Y-%m-%d %H:%M"),
                    ny_datetime(trade.exit_ts).strftime("%Y-%m-%d %H:%M"),
                    f"{trade.entry_px:.6f}",
                    f"{trade.exit_px:.6f}",
                    f"{trade.gross * 1e4:.4f}",
                    f"{(trade.gross - drag) * 1e4:.4f}",
                    f"{trade.hold_s / 60:.2f}",
                ]
            )

    with (OUT / "daily.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            ["session", "r_qqq", "r_spy", "r_igv", "r_book", "equity", "cc_book", "otc_book", "cc_qqq", "cc_spy", "cc_igv", "otc_qqq", "otc_spy", "otc_igv"]
        )
        for i, day in enumerate(calendar):
            writer.writerow(
                [
                    day.isoformat(),
                    f"{primary_returns['QQQ'][i]:.8f}",
                    f"{primary_returns['SPY'][i]:.8f}",
                    f"{primary_returns['IGV'][i]:.8f}",
                    f"{primary_returns['book'][i]:.8f}",
                    f"{full_curve[i + 1]:.8f}",
                    _fmt(benches["cc"]["book"][i]),
                    _fmt(benches["otc"]["book"][i]),
                    _fmt(benches["cc"]["QQQ"][i]),
                    _fmt(benches["cc"]["SPY"][i]),
                    _fmt(benches["cc"]["IGV"][i]),
                    _fmt(benches["otc"]["QQQ"][i]),
                    _fmt(benches["otc"]["SPY"][i]),
                    _fmt(benches["otc"]["IGV"][i]),
                ]
            )

    write_equity_svg(
        OUT / "equity.svg",
        calendar,
        {
            "Strategy": full_curve[1:],
            "Close to close": bh_curve[1:],
            "Open to close": otc_curve[1:],
        },
        underwater[1:],
    )


def _fmt(value: float) -> str:
    if value is None or (isinstance(value, float) and not math.isfinite(value)):
        return ""
    return f"{value:.8f}"


def _bar(day: date, hour: int, minute: int, o: float, h: float, l: float, c: float) -> Bar:
    return Bar(_ny_local_to_utc(day, hour * 3600 + minute * 60), o, h, l, c, 1.0)


def run_self_tests() -> None:
    day = date(2024, 1, 2)
    assert day not in EARLY_CLOSES

    probe = _bar(day, 9, 30, 1, 1, 1, 1)
    assert probe.time.hour == 9 and probe.time.minute == 30, probe.time

    # 1. Stop exits at the next bar's open. Hand-computed in the study notes.
    bars = [
        _bar(day, 9, 30, 100, 102, 99, 101),
        _bar(day, 9, 35, 101, 103, 100, 102),
        _bar(day, 9, 40, 102, 106, 101, 105),
        _bar(day, 9, 45, 105, 107, 104, 106),
        _bar(day, 9, 50, 106, 108, 105, 107),
        _bar(day, 15, 25, 107, 108, 100, 101),
        _bar(day, 15, 30, 101, 102, 99, 100),
        _bar(day, 15, 55, 100, 101, 98, 99),
    ]
    trades = simulate_session(bars, "QQQ", 2, 1.0, fill_mode="next", bar_seconds=300)
    assert len(trades) == 1, trades
    trade = trades[0]
    assert trade.side == 1 and trade.reason == "stop"
    assert trade.entry_px == 105 and trade.exit_px == 101
    assert abs(trade.gross - (101 / 105 - 1)) < 1e-12
    assert ny_datetime(trade.entry_ts).strftime("%H:%M") == "09:45"
    assert ny_datetime(trade.exit_ts).strftime("%H:%M") == "15:30"

    # 2. No entry once the fill would land at or after 15:30.
    blocked = [
        _bar(day, 15, 15, 100, 101, 99, 100),
        _bar(day, 15, 20, 100, 101, 99, 100),
        _bar(day, 15, 25, 100, 105, 99, 104),
        _bar(day, 15, 30, 104, 106, 103, 105),
        _bar(day, 15, 55, 105, 106, 104, 105),
    ]
    assert simulate_session(blocked, "QQQ", 2, 1.0, fill_mode="next", bar_seconds=300) == []

    # 3. A flip closes and opens at the same open. The stop is too wide to fire.
    flip_bars = [
        _bar(day, 10, 0, 100, 102, 100, 101),
        _bar(day, 10, 5, 101, 103, 101, 102),
        _bar(day, 10, 10, 102, 106, 102, 105),
        _bar(day, 10, 15, 105, 106, 100, 101),
        _bar(day, 10, 20, 101, 102, 96, 97),
        _bar(day, 10, 25, 97, 98, 95, 96),
        _bar(day, 15, 55, 96, 97, 94, 95),
    ]
    flipped = simulate_session(flip_bars, "QQQ", 2, 100.0, fill_mode="next", bar_seconds=300)
    assert len(flipped) == 2, flipped
    assert flipped[0].side == 1 and flipped[0].reason == "flip"
    assert flipped[0].entry_px == 105 and flipped[0].exit_px == 97
    assert flipped[1].side == -1 and flipped[1].reason == "eod"
    assert flipped[1].entry_px == 97 and flipped[1].exit_px == 95
    assert abs(flipped[1].gross - (2 / 97)) < 1e-12

    # 4. A missing bucket fills on the next real bar. No price is invented.
    gapped = [
        _bar(day, 10, 0, 100, 102, 99, 101),
        _bar(day, 10, 5, 101, 103, 100, 102),
        _bar(day, 10, 10, 102, 106, 101, 105),
        _bar(day, 10, 20, 110, 112, 109, 111),
        _bar(day, 15, 55, 111, 112, 110, 111),
    ]
    gap_trades = simulate_session(gapped, "QQQ", 2, 100.0, fill_mode="next", bar_seconds=300)
    assert len(gap_trades) == 1, gap_trades
    assert gap_trades[0].entry_px == 110
    assert ny_datetime(gap_trades[0].entry_ts).strftime("%H:%M") == "10:20"

    # 5. The upper bound fills at the signal close, which here is not the next open.
    closed = [
        _bar(day, 10, 0, 100, 102, 99, 101),
        _bar(day, 10, 5, 101, 103, 100, 102),
        _bar(day, 10, 10, 102, 106, 101, 105),
        _bar(day, 10, 15, 110, 112, 109, 111),
        _bar(day, 15, 55, 111, 114, 110, 113),
    ]
    close_trades = simulate_session(closed, "QQQ", 2, 100.0, fill_mode="close", bar_seconds=300)
    assert len(close_trades) == 1, close_trades
    assert close_trades[0].entry_px == 105 and close_trades[0].exit_px == 113

    # 6. A two-bar delay keeps the original order across the skipped bar and across a gap.
    delayed = [
        _bar(day, 10, 0, 100, 110, 100, 105),
        _bar(day, 10, 5, 105, 112, 104, 108),
        _bar(day, 10, 10, 108, 114, 90, 113),
        _bar(day, 10, 15, 113, 114, 80, 85),
        _bar(day, 10, 20, 70, 72, 68, 71),
        _bar(day, 15, 55, 71, 80, 70, 75),
    ]
    delay_trades = simulate_session(delayed, "QQQ", 2, 100.0, fill_mode="skip", bar_seconds=300)
    assert len(delay_trades) == 1, delay_trades
    assert delay_trades[0].side == 1 and delay_trades[0].entry_px == 70
    assert ny_datetime(delay_trades[0].entry_ts).strftime("%H:%M") == "10:20"
    assert delay_trades[0].exit_px == 75

    gapped_delay = [
        _bar(day, 10, 0, 100, 102, 99, 101),
        _bar(day, 10, 5, 101, 103, 100, 102),
        _bar(day, 10, 10, 102, 106, 101, 105),
        _bar(day, 10, 25, 130, 131, 129, 130),
        _bar(day, 15, 55, 130, 132, 129, 131),
    ]
    gap_delay = simulate_session(gapped_delay, "QQQ", 2, 100.0, fill_mode="skip", bar_seconds=300)
    assert len(gap_delay) == 1 and gap_delay[0].entry_px == 130, gap_delay
    assert ny_datetime(gap_delay[0].entry_ts).strftime("%H:%M") == "10:25"

    # Book math: three names, one trade, equal weight, cost applied once per name.
    sample = Trade("QQQ", day.isoformat(), 1, "eod", 0, 60, 100, 101, 0.01)
    booked = {
        "QQQ": {day: [sample]},
        "SPY": {day: []},
        "IGV": {day: []},
    }
    returns = aligned_returns(booked, [day], 1.0)
    assert abs(returns["QQQ"][0] - (0.01 - 0.0002)) < 1e-12
    assert returns["SPY"][0] == 0.0 and returns["IGV"][0] == 0.0
    assert abs(returns["book"][0] - returns["QQQ"][0] / 3) < 1e-12

    # Early-close cutoff rejects a 12:30 fill and still allows a stop after it.
    early = date(2024, 7, 3)
    assert early in EARLY_CLOSES
    early_bars = [
        _bar(early, 11, 45, 100, 101, 99, 100),
        _bar(early, 12, 0, 100, 101, 99, 100),
        _bar(early, 12, 15, 100, 105, 99, 104),
        _bar(early, 12, 30, 104, 106, 103, 105),
        _bar(early, 12, 45, 105, 106, 104, 105),
    ]
    assert simulate_session(early_bars, "QQQ", 2, 1.0, fill_mode="next", bar_seconds=900) == []

    print("self-test ok")


def print_summary(summary: dict) -> None:
    acc = summary["acceptance"]
    book = summary["primary_stats"]
    print(f"calendar {summary['calendar']['first']} .. {summary['calendar']['last']}  sessions {summary['calendar']['sessions']}")
    print(f"pass {acc['pass']}")
    for label in ("is", "oos", "full"):
        row = book[label]["book"]
        print(
            f"{label:4} sharpe {row['sharpe']:.3f}  total {row['total_return']:.4f}  "
            f"pf {row['profit_factor']}  trades {row['trades']}  exposure {row['exposure']:.3f}"
        )
    for name in NAMES:
        row = book["oos"][name]
        print(f"oos {name} sharpe {row['sharpe']:.3f} total {row['total_return']:.4f} trades {row['trades']}")
    print(f"placebo p {summary['placebo']['p']:.4f}  bootstrap {summary['bootstrap']['p2_5']:.3f} .. {summary['bootstrap']['p97_5']:.3f}")
    print(f"2bp full total {summary['costs']['2bp']['full']['book']['total_return']:.4f}")
    print(f"grid IS positive {acc['is_grid_positive_cells']}/{acc['is_grid_cells']}")


def main() -> None:
    run_self_tests()
    summary = run_store()
    print_summary(summary)


if __name__ == "__main__":
    main()
