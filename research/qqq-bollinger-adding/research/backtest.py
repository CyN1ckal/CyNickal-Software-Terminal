# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""QQQ intraday Bollinger reversion with adding: the pre-registered backtest.

Refuses to run unless RULES.md matches RULES.lock. Runs a synthetic self-test
before it opens the store, then writes results.json, daily.csv, and
trades.csv next to this file and appends an entry to RUNLOG.md.

    python research/qqq-bollinger-adding/research/backtest.py [--reason TEXT]
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    Bar,
    MarketData,
    ny_datetime,
    nyse_sessions,
    resample,
    rth_window,
)

# ---- Constants, one for one with RULES.md ---------------------------------
BAR_S = 300
N = 20
K = 2.0
STEP = 1.0
MAX_UNITS = 3
COST_BPS = 1.0
CUTOFF_MIN = 30
DELAY = 1

START = date(2021, 9, 27)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
END = date(2026, 9, 25)

PRIMARY = "QQQ"
CROSS = ("SPY", "IGV")
COSTS = (0.0, 0.5, 1.0, 2.0, 3.0)
GRID_N = (10, 20, 30)
GRID_K = (1.5, 2.0, 2.5)
GRID_STEP = (0.5, 1.0, 1.5)

SEED_DIRECTION = 20260926
SEED_TIMING = 20260927
SEED_BOOTSTRAP = 20260928
N_DIRECTION = 2000
N_TIMING = 500
N_BOOT = 2000
BOOT_BLOCK = 20


# ---- Rules hash -----------------------------------------------------------
def rules_hash() -> str:
    b = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(b).hexdigest()


def check_lock() -> str:
    h = rules_hash()
    lock = (HERE / "RULES.lock").read_text(encoding="utf-8").split()
    locked = lock[lock.index("sha256") + 1]
    if h != locked:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {locked}. Refusing to run.")
    return h


# ---- Session helpers ------------------------------------------------------
def session_end_ts(day: date) -> int:
    start, end = rth_window(day)
    return start + 12600 if day in EARLY_CLOSES else end


def tradable(bars: list, day: date) -> bool:
    if not bars:
        return False
    start, _ = rth_window(day)
    if bars[0].ts != start:
        return False
    return bars[-1].ts >= session_end_ts(day) - BAR_S


def session_bands(closes: list[float], n: int) -> tuple[list, list]:
    """Mean and population SD of the last n closes ending at each index."""
    m: list = [None] * len(closes)
    s: list = [None] * len(closes)
    for i in range(n - 1, len(closes)):
        w = closes[i - n + 1: i + 1]
        mu = sum(w) / n
        m[i] = mu
        s[i] = math.sqrt(sum((x - mu) ** 2 for x in w) / n)
    return m, s


# ---- Engine ---------------------------------------------------------------
def simulate(bars: list, day: date, M: list, S: list, k: float = K, step: float = STEP,
             max_units: int = MAX_UNITS, delay: int = DELAY) -> list[dict]:
    """Campaigns for one tradable session. M/S are the band inputs per bar (None = no band).

    A campaign is {side, legs: [(idx, price, at)], exit_idx, exit_price, exit_at, reason}.
    """
    cutoff = session_end_ts(day) - CUTOFF_MIN * 60
    last = len(bars) - 1
    out: list[dict] = []
    side = 0
    legs: list = []
    pending = None  # (fill_idx, kind)

    def close_campaign(idx, price, at, reason):
        nonlocal side, legs
        out.append({"side": side, "legs": list(legs), "exit_idx": idx,
                    "exit_price": price, "exit_at": at, "reason": reason})
        side = 0
        legs = []

    def decide(i):
        c = bars[i].close
        m, s = M[i], S[i]
        if side == 1:
            if c >= m:
                return "exit"
            if len(legs) < max_units and c < m - k * s and c <= min(p for _, p, _ in legs) - step * s:
                return "add"
        elif side == -1:
            if c <= m:
                return "exit"
            if len(legs) < max_units and c > m + k * s and c >= max(p for _, p, _ in legs) + step * s:
                return "add"
        else:
            if c < m - k * s:
                return "long"
            if c > m + k * s:
                return "short"
        return None

    def apply(kind, idx, price, at):
        nonlocal side, legs
        if kind == "exit":
            close_campaign(idx, price, at, "middle")
        elif kind == "add":
            legs.append((idx, price, at))
        else:
            side = 1 if kind == "long" else -1
            legs = [(idx, price, at)]

    for i, bar in enumerate(bars):
        if pending is not None and pending[0] == i:
            apply(pending[1], i, bar.open, "open")
            pending = None
        if i == last:
            if side != 0:
                reason = "session"
                if delay == 0 and M[i] is not None:
                    c = bar.close
                    if (side == 1 and c >= M[i]) or (side == -1 and c <= M[i]):
                        reason = "middle"
                close_campaign(i, bar.close, "close", reason)
            break
        if pending is not None or M[i] is None:
            continue
        kind = decide(i)
        if kind is None:
            continue
        is_open = kind != "exit"
        if delay == 0:
            if is_open and not (bar.ts + BAR_S < cutoff):
                continue
            apply(kind, i, bar.close, "close")
            continue
        j = i + delay
        if j > last or any(bars[i + d].ts != bar.ts + d * BAR_S for d in range(1, delay + 1)):
            continue
        if is_open and not (bars[j].ts < cutoff):
            continue
        pending = (j, kind)
    return out


# ---- Self-test ------------------------------------------------------------
def make_session(day: date, closes: list[float], opens: dict | None = None,
                 drop: tuple = ()) -> list:
    start, _ = rth_window(day)
    opens = opens or {}
    bars = []
    prev = closes[0]
    for i, c in enumerate(closes):
        o = opens.get(i, prev)
        prev = c
        if i in drop:
            continue
        bars.append(Bar(start + i * BAR_S, o, max(o, c), min(o, c), c, 1.0))
    return bars


def summarize_campaigns(camps):
    return [(c["side"], [(l[0], round(l[1], 6)) for l in c["legs"]], c["exit_idx"],
             round(c["exit_price"], 6), c["reason"]) for c in camps]


def self_test() -> int:
    reg = date(2024, 3, 5)
    early = date(2024, 11, 29)
    n78 = 78
    cases = []

    def run(bars, day, n=4, k=1.0, step=1.0, units=3, delay=1):
        m, s = session_bands([b.close for b in bars], n)
        return summarize_campaigns(simulate(bars, day, m, s, k, step, units, delay))

    # 1. One-unit long, exit at the middle.
    c1 = [100.0] * n78
    c1[30], c1[31], c1[32] = 99.0, 99.0, 100.0
    b1 = make_session(reg, c1, {31: 99.05, 33: 100.02})
    cases.append(("long to middle", run(b1, reg), [(1, [(31, 99.05)], 33, 100.02, "middle")]))

    # 2. One-unit short, exit at the middle.
    c2 = [100.0] * n78
    c2[30], c2[31], c2[32] = 101.0, 101.0, 100.0
    b2 = make_session(reg, c2, {31: 100.95, 33: 99.98})
    cases.append(("short to middle", run(b2, reg), [(-1, [(31, 100.95)], 33, 99.98, "middle")]))

    # 3. Long adds twice, refused at the cap, flattened at the session close.
    c3 = [100.0] * 30 + [99.0, 97.0, 94.0, 90.0] + [90.0 - 0.1 * (i - 33) for i in range(34, n78)]
    b3 = make_session(reg, c3, {31: 99.0, 32: 97.0, 33: 94.0})
    exp3_exit = round(90.0 - 0.1 * 44, 6)
    cases.append(("two adds, cap, session", run(b3, reg),
                  [(1, [(31, 99.0), (32, 97.0), (33, 94.0)], 77, exp3_exit, "session")]))

    # 4. Close below the band but not a full STEP below the worst fill: no add.
    c4 = [100.0] * n78
    c4[30], c4[31], c4[32] = 99.0, 98.5, 100.0
    b4 = make_session(reg, c4, {31: 99.0, 33: 100.0})
    cases.append(("add refused by step", run(b4, reg), [(1, [(31, 99.0)], 33, 100.0, "middle")]))

    # 5. Entry filling 15:25 allowed, exit at 15:35 allowed, entry at 15:50 refused.
    c5 = [100.0] * n78
    c5[70], c5[71], c5[72] = 99.0, 99.0, 100.0
    c5[75], c5[76], c5[77] = 99.0, 99.0, 99.0
    b5 = make_session(reg, c5, {71: 99.0, 73: 100.0})
    cases.append(("cutoff", run(b5, reg), [(1, [(71, 99.0)], 73, 100.0, "middle")]))
    # 5b. A signal at the 15:25 bar (fill 15:30) is refused.
    c5b = [100.0] * n78
    c5b[71] = 99.0
    cases.append(("cutoff at 15:30", run(make_session(reg, c5b), reg), []))

    # 6. Early close: fill at 12:25 allowed, adds filling from 12:30 refused, flatten 13:00.
    c6 = [100.0] * 34 + [99.0] + [99.0 - 0.1 * (i - 34) for i in range(35, 43)]
    b6 = make_session(early, c6, {35: 99.0})
    cases.append(("early close", run(b6, early), [(1, [(35, 99.0)], 42, round(99.0 - 0.8, 6), "session")]))

    # 7. The next bucket is missing: no order from that close.
    c7 = [100.0] * n78
    c7[30], c7[31], c7[32] = 99.0, 99.0, 99.0
    b7 = make_session(reg, c7, drop=(31,))
    cases.append(("missing next bucket", run(b7, reg), []))

    # 8. No decision before bar N-1 (N = 20, K = 2).
    c8 = [100.0] * n78
    c8[5] = 90.0
    cases.append(("no early decision", run(make_session(reg, c8), reg, n=20, k=2.0), []))

    # 9. A middle-band close on the last bar exits as 'session' under DELAY = 1.
    c9 = [100.0] * 70 + [99.0, 98.9, 98.8, 98.7, 98.6, 98.5, 98.4, 100.0]
    b9 = make_session(reg, c9, {71: 99.0})
    cases.append(("last-bar middle is session", run(b9, reg), [(1, [(71, 99.0)], 77, 100.0, "session")]))

    # 10. Exit, then re-entry on the exit bar's close.
    c10 = [100.0] * n78
    c10[30], c10[31], c10[32], c10[33] = 99.0, 100.0, 98.0, 100.0
    b10 = make_session(reg, c10, {31: 99.0, 32: 100.0, 33: 98.0, 34: 100.0})
    cases.append(("exit and re-entry", run(b10, reg),
                  [(1, [(31, 99.0)], 32, 100.0, "middle"), (1, [(33, 98.0)], 34, 100.0, "middle")]))

    # 11. DELAY = 2 on case 1's data.
    cases.append(("delay 2", run(b1, reg, delay=2), [(1, [(32, 99.0)], 34, 100.0, "middle")]))

    # 12. DELAY = 0 on case 1's data.
    cases.append(("delay 0", run(b1, reg, delay=0), [(1, [(30, 99.0)], 32, 100.0, "middle")]))

    # 13. DELAY = 0: middle close on the last bar exits as 'middle'.
    cases.append(("delay 0 last bar", run(b9, reg, delay=0), [(1, [(70, 99.0)], 77, 100.0, "middle")]))

    # 14. No adds when MAX_UNITS = 1.
    cases.append(("cap 1", run(b3, reg, units=1), [(1, [(31, 99.0)], 77, exp3_exit, "session")]))

    failed = 0
    for name, got, want in cases:
        ok = got == want
        failed += not ok
        print(f"  self-test {'ok  ' if ok else 'FAIL'} {name}" + ("" if ok else f"\n    got  {got}\n    want {want}"))

    # 15. Costs on three units (case 3): session return = sum(g - 2c) / 3.
    m3, s3 = session_bands([b.close for b in b3], 4)
    camps = simulate(b3, reg, m3, s3, 1.0, 1.0, 3, 1)
    r = session_return(camps, 1.0)
    want = ((exp3_exit / 99.0 - 1 - 2e-4) + (exp3_exit / 97.0 - 1 - 2e-4) + (exp3_exit / 94.0 - 1 - 2e-4)) / 3
    ok = abs(r - want) < 1e-12
    failed += not ok
    print(f"  self-test {'ok  ' if ok else 'FAIL'} costs on three units ({r:.8f} vs {want:.8f})")
    return failed


# ---- Accounting -----------------------------------------------------------
def leg_gross(c: dict) -> list[float]:
    return [c["side"] * (c["exit_price"] / p - 1.0) for _, p, _ in c["legs"]]


def session_return(camps: list[dict], cost_bps: float) -> float:
    cost = 2.0 * cost_bps / 1e4
    return sum(sum(g - cost for g in leg_gross(c)) for c in camps) / 3.0


def sharpe(x) -> float:
    x = np.asarray(x, dtype=float)
    if len(x) < 2:
        return float("nan")
    sd = x.std(ddof=1)
    return float(x.mean() / sd * math.sqrt(252)) if sd > 0 else float("nan")


def max_dd(r) -> float:
    eq = np.cumprod(1.0 + np.asarray(r, dtype=float))
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return float((eq / peak - 1.0).min()) if len(eq) else 0.0


def window_of(day: date) -> str:
    return "is" if day <= IS_END else "oos"


# ---- Running a symbol -----------------------------------------------------
class SymbolData:
    def __init__(self, md: MarketData, sym: str, calendar: list[date]):
        self.sym = sym
        b5 = resample(md.bars(sym, "1m", start=START, end=END), BAR_S)
        self.by_day: dict[date, list] = {}
        for b in b5:
            self.by_day.setdefault(b.session, []).append(b)
        self.calendar = calendar
        self.tradable = {d: tradable(self.by_day.get(d, []), d) for d in calendar}
        self.all_bars = b5
        self._bands: dict = {}

    def bands(self, n: int, continuous: bool = False) -> dict:
        key = (n, continuous)
        if key not in self._bands:
            out = {}
            if not continuous:
                for d, bars in self.by_day.items():
                    out[d] = session_bands([b.close for b in bars], n)
            else:
                m, s = session_bands([b.close for b in self.all_bars], n)
                pos = 0
                for d in sorted(self.by_day):
                    k = len(self.by_day[d])
                    out[d] = (m[pos:pos + k], s[pos:pos + k])
                    pos += k
            self._bands[key] = out
        return self._bands[key]

    def run(self, n=N, k=K, step=STEP, units=MAX_UNITS, delay=DELAY, continuous=False,
            days=None) -> dict[date, list[dict]]:
        bands = self.bands(n, continuous)
        out = {}
        for d in (days or self.calendar):
            if not self.tradable[d]:
                out[d] = []
                continue
            m, s = bands[d]
            out[d] = simulate(self.by_day[d], d, m, s, k, step, units, delay)
        return out


def daily_returns(camps_by_day: dict, calendar: list[date], cost_bps: float) -> np.ndarray:
    return np.array([session_return(camps_by_day.get(d, []), cost_bps) for d in calendar])


def gross_daily(camps_by_day: dict, calendar: list[date]) -> np.ndarray:
    return daily_returns(camps_by_day, calendar, 0.0)


def campaign_rows(sd: SymbolData, camps_by_day: dict, cost_bps: float) -> list[dict]:
    """Flat campaign records with dollars on the compounded path starting at equity 1."""
    rows = []
    eq = 1.0
    cost = 2.0 * cost_bps / 1e4
    for d in sd.calendar:
        camps = camps_by_day.get(d, [])
        unit = eq / 3.0
        bars = sd.by_day.get(d, [])
        for c in camps:
            g = leg_gross(c)
            net = [x - cost for x in g]
            units = len(c["legs"])
            first = c["legs"][0]
            rows.append({
                "session": d, "window": window_of(d), "side": c["side"], "units": units,
                "legs": c["legs"], "bars": bars,
                "entry_idx": first[0], "entry_at": first[2],
                "avg_entry": sum(p for _, p, _ in c["legs"]) / units,
                "exit_idx": c["exit_idx"], "exit_price": c["exit_price"], "exit_at": c["exit_at"],
                "reason": c["reason"], "hold_bars": c["exit_idx"] - first[0] + 1,
                "leg_gross": g,
                "gross_bp_unit": sum(g) / units * 1e4, "net_bp_unit": sum(net) / units * 1e4,
                "net_dollars": sum(net) * unit, "gross_dollars": sum(g) * unit,
                "unit_notional": unit,
            })
        eq *= 1.0 + session_return(camps, cost_bps)
    return rows


def window_metrics(sd: SymbolData, r: np.ndarray, rows: list[dict], window: str) -> dict:
    cal = sd.calendar
    mask = np.array([window == "full" or window_of(d) == window for d in cal])
    x = r[mask]
    days = [d for d, m in zip(cal, mask) if m]
    rs = [t for t in rows if window == "full" or t["window"] == window]
    n = len(x)
    tr = float(np.prod(1 + x) - 1)
    wins = sum(t["net_dollars"] for t in rs if t["net_dollars"] > 0)
    losses = sum(t["net_dollars"] for t in rs if t["net_dollars"] < 0)
    tbars = sum(len(sd.by_day.get(d, [])) for d in days if sd.tradable[d])
    held = sum(t["hold_bars"] for t in rs)
    sdv = float(x.std(ddof=1)) if n > 1 else float("nan")
    return {
        "first": days[0].isoformat(), "last": days[-1].isoformat(), "sessions": n,
        "total_return": tr,
        "cagr": float((1 + tr) ** (252 / n) - 1) if n else float("nan"),
        "ann_vol": sdv * math.sqrt(252),
        "sharpe": sharpe(x),
        "max_dd": max_dd(x),
        "t_stat": float(x.mean() / (sdv / math.sqrt(n))) if sdv > 0 else float("nan"),
        "trades": len(rs),
        "win_rate": (sum(1 for t in rs if t["net_dollars"] > 0) / len(rs)) if rs else float("nan"),
        "profit_factor": (wins / -losses) if losses < 0 else float("inf"),
        "avg_net_bp_unit": float(np.mean([t["net_bp_unit"] for t in rs])) if rs else float("nan"),
        "avg_gross_bp_unit": float(np.mean([t["gross_bp_unit"] for t in rs])) if rs else float("nan"),
        "exposure": held / tbars if tbars else float("nan"),
        "sessions_with_trade": len({t["session"] for t in rs}),
        "median_hold_bars": float(np.median([t["hold_bars"] for t in rs])) if rs else float("nan"),
        "mean_hold_bars": float(np.mean([t["hold_bars"] for t in rs])) if rs else float("nan"),
        "mean_units": float(np.mean([t["units"] for t in rs])) if rs else float("nan"),
    }


def three_windows(sd, r, rows) -> dict:
    return {w: window_metrics(sd, r, rows, w) for w in ("full", "is", "oos")}


def group_stats(rows: list[dict], key) -> dict:
    groups: dict = {}
    for t in rows:
        groups.setdefault(key(t), []).append(t)
    out = {}
    for g, ts in sorted(groups.items(), key=lambda kv: str(kv[0])):
        wins = sum(t["net_dollars"] for t in ts if t["net_dollars"] > 0)
        losses = sum(t["net_dollars"] for t in ts if t["net_dollars"] < 0)
        out[str(g)] = {
            "trades": len(ts),
            "net_dollars": sum(t["net_dollars"] for t in ts),
            "profit_factor": (wins / -losses) if losses < 0 else float("inf"),
            "win_rate": sum(1 for t in ts if t["net_dollars"] > 0) / len(ts),
            "avg_net_bp_unit": float(np.mean([t["net_bp_unit"] for t in ts])),
            "avg_gross_bp_unit": float(np.mean([t["gross_bp_unit"] for t in ts])),
        }
    return out


# ---- Benchmarks -----------------------------------------------------------
def benchmarks(md: MarketData, sd: SymbolData) -> tuple[np.ndarray, np.ndarray, int]:
    daily = {b.session: b for b in md.bars(sd.sym, "1d", start=START, end=END)}
    bh, o2c = [], []
    prev_mark = None
    from_5m = 0
    for d in sd.calendar:
        bars = sd.by_day.get(d, [])
        if d in daily:
            mark = daily[d].close
        elif bars:
            mark = bars[-1].close
            from_5m += 1
        else:
            mark = None
        if mark is None:
            bh.append(0.0)
        elif prev_mark is None:
            bh.append(mark / bars[0].open - 1.0 if bars else 0.0)
        else:
            bh.append(mark / prev_mark - 1.0)
        if mark is not None:
            prev_mark = mark
        o2c.append(bars[-1].close / bars[0].open - 1.0 if bars else 0.0)
    return np.array(bh), np.array(o2c), from_5m


# ---- Placebos and bootstrap -----------------------------------------------
def direction_placebo(camps_by_day: dict, calendar: list[date]) -> dict:
    day_idx, contrib = [], []
    for i, d in enumerate(calendar):
        for c in camps_by_day.get(d, []):
            day_idx.append(i)
            contrib.append(sum(leg_gross(c)) / 3.0)
    day_idx = np.array(day_idx)
    contrib = np.array(contrib)
    actual = sharpe(np.bincount(day_idx, weights=contrib, minlength=len(calendar)))
    rng = np.random.default_rng(SEED_DIRECTION)
    draws = np.empty(N_DIRECTION)
    for j in range(N_DIRECTION):
        signs = rng.choice((-1.0, 1.0), size=len(contrib))
        draws[j] = sharpe(np.bincount(day_idx, weights=contrib * signs, minlength=len(calendar)))
    return {"actual_gross_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)),
            "p": float((1 + (draws >= actual).sum()) / (N_DIRECTION + 1)),
            "draws": draws.tolist()}


def timing_placebo(sd: SymbolData, camps_by_day: dict, actual: float) -> dict:
    from bisect import bisect_right
    rng = np.random.default_rng(SEED_TIMING)
    cal = sd.calendar
    legal_starts = {}
    for d in cal:
        if camps_by_day.get(d):
            bars = sd.by_day[d]
            cutoff = session_end_ts(d) - CUTOFF_MIN * 60
            legal_starts[d] = [b for b in range(N, len(bars)) if bars[b].ts < cutoff]
    draws = np.empty(N_TIMING)
    for j in range(N_TIMING):
        daily = np.zeros(len(cal))
        for i, d in enumerate(cal):
            camps = camps_by_day.get(d, [])
            if not camps:
                continue
            bars = sd.by_day[d]
            last = len(bars) - 1
            cutoff = session_end_ts(d) - CUTOFF_MIN * 60
            legal = legal_starts[d]
            prev_exit = -1
            total = 0.0
            for c in camps:
                first = c["legs"][0][0]
                offsets = [l[0] - first for l in c["legs"]]
                hold = c["exit_idx"] - first
                starts = legal[bisect_right(legal, prev_exit):]
                if not starts:
                    continue
                s = starts[int(rng.integers(len(starts)))]
                if c["reason"] == "session":
                    ex_idx, ex_px = last, bars[last].close
                elif s + hold <= last:
                    ex_idx, ex_px = s + hold, bars[s + hold].open
                else:
                    ex_idx, ex_px = last, bars[last].close
                for off in offsets:
                    li = s + off
                    if off > 0 and (li > last or bars[li].ts >= cutoff or li >= ex_idx):
                        continue
                    total += c["side"] * (ex_px / bars[li].open - 1.0) / 3.0
                prev_exit = ex_idx
            daily[i] = total
        draws[j] = sharpe(daily)
    return {"actual_gross_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)),
            "p": float((1 + (draws >= actual).sum()) / (N_TIMING + 1)),
            "draws": draws.tolist()}


def bootstrap(r: np.ndarray) -> dict:
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    n = len(r)
    nb = math.ceil(n / BOOT_BLOCK)
    out = np.empty(N_BOOT)
    for j in range(N_BOOT):
        starts = rng.integers(0, n, size=nb)
        idx = (starts[:, None] + np.arange(BOOT_BLOCK)[None, :]).ravel()[:n] % n
        out[j] = sharpe(r[idx])
    return {"lo": float(np.percentile(out, 2.5)), "hi": float(np.percentile(out, 97.5)),
            "share_le_0": float((out <= 0).mean())}


# ---- Main -----------------------------------------------------------------
def git_state() -> tuple[str, bool]:
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT).stdout.strip())
    return head, dirty


def ny(ts: int) -> str:
    return ny_datetime(ts).strftime("%Y-%m-%d %H:%M")


def clean(o):
    if isinstance(o, float):
        return None if (math.isnan(o) or math.isinf(o)) else round(o, 10)
    if isinstance(o, dict):
        return {str(k): clean(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [clean(v) for v in o]
    if isinstance(o, (np.floating,)):
        return clean(float(o))
    if isinstance(o, (np.integer,)):
        return int(o)
    if isinstance(o, date):
        return o.isoformat()
    return o


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reason", default="initial")
    args = ap.parse_args()

    h = check_lock()
    print("self-test")
    if self_test():
        sys.exit("Self-test failed. Not opening the store.")

    calendar = nyse_sessions(START, END)
    res: dict = {"rules_sha256": h, "seeds": {"direction": SEED_DIRECTION, "timing": SEED_TIMING,
                                              "bootstrap": SEED_BOOTSTRAP}}
    with MarketData() as md:
        data = {s: SymbolData(md, s, calendar) for s in (PRIMARY,) + CROSS}
        q = data[PRIMARY]

        # Data checks (RULES.md, Data).
        assert len(calendar) == 1255, len(calendar)
        assert len(q.by_day) == 1254, len(q.by_day)
        assert date(2021, 12, 31) in calendar and date(2021, 12, 31) not in q.by_day
        for d, bars in q.by_day.items():
            assert q.tradable[d], d
            assert len(bars) == (43 if d in EARLY_CLOSES else 78), (d, len(bars))
        bh, o2c, bh_from_5m = benchmarks(md, q)

    # Primary.
    camps = q.run()
    r = daily_returns(camps, calendar, COST_BPS)
    g = gross_daily(camps, calendar)
    rows = campaign_rows(q, camps, COST_BPS)
    res["primary"] = three_windows(q, r, rows)
    res["primary_gross_sharpe"] = {w: sharpe(g[[w == "full" or window_of(d) == w for d in calendar]])
                                   for w in ("full", "is", "oos")}
    res["primary_gross_return"] = {w: float(np.prod(1 + g[[w == "full" or window_of(d) == w for d in calendar]]) - 1)
                                   for w in ("full", "is", "oos")}
    res["benchmarks"] = {}
    for name, x in (("buy_and_hold", bh), ("open_to_close", o2c)):
        res["benchmarks"][name] = {w: {"total_return": float(np.prod(1 + x[m]) - 1), "sharpe": sharpe(x[m]),
                                       "max_dd": max_dd(x[m])}
                                   for w, m in (("full", np.ones(len(calendar), bool)),
                                                ("is", np.array([window_of(d) == "is" for d in calendar])),
                                                ("oos", np.array([window_of(d) == "oos" for d in calendar])))}
    res["benchmarks"]["buy_and_hold_marks_from_5m"] = bh_from_5m
    res["corr_with_buy_and_hold"] = float(np.corrcoef(r, bh)[0, 1])

    # Breakdowns.
    years = sorted({d.year for d in calendar})
    res["by_year"] = {}
    for y in years:
        m = np.array([d.year == y for d in calendar])
        res["by_year"][y] = {"sessions": int(m.sum()), "return": float(np.prod(1 + r[m]) - 1),
                             "sharpe": sharpe(r[m]), "max_dd": max_dd(r[m]),
                             "buy_and_hold": float(np.prod(1 + bh[m]) - 1),
                             "trades": sum(1 for t in rows if t["session"].year == y)}
    res["by_side"] = {w: group_stats([t for t in rows if w == "full" or t["window"] == w],
                                     lambda t: "long" if t["side"] == 1 else "short") for w in ("full", "oos")}
    res["by_reason"] = group_stats(rows, lambda t: t["reason"])
    res["by_hour"] = group_stats(rows, lambda t: ny_datetime(t["bars"][t["entry_idx"]].ts).hour)
    res["by_units"] = {w: group_stats([t for t in rows if w == "full" or t["window"] == w], lambda t: t["units"])
                       for w in ("full", "oos")}

    # Legs: first vs added.
    first_legs = [t["leg_gross"][0] for t in rows]
    added_legs = [x for t in rows for x in t["leg_gross"][1:]]
    res["legs"] = {"first": {"count": len(first_legs), "mean_gross_bp": float(np.mean(first_legs)) * 1e4},
                   "added": {"count": len(added_legs),
                             "mean_gross_bp": float(np.mean(added_legs)) * 1e4 if added_legs else None},
                   "second": {"count": sum(1 for t in rows if t["units"] >= 2),
                              "mean_gross_bp": float(np.mean([t["leg_gross"][1] for t in rows if t["units"] >= 2])) * 1e4
                              if any(t["units"] >= 2 for t in rows) else None},
                   "third": {"count": sum(1 for t in rows if t["units"] >= 3),
                             "mean_gross_bp": float(np.mean([t["leg_gross"][2] for t in rows if t["units"] >= 3])) * 1e4
                             if any(t["units"] >= 3 for t in rows) else None}}

    # Quintiles of the day's move.
    trad = [(i, d) for i, d in enumerate(calendar) if q.tradable[d]]
    moves = [abs(q.by_day[d][-1].close / q.by_day[d][0].open - 1.0) for _, d in trad]
    order = sorted(range(len(trad)), key=lambda j: moves[j])
    T = len(trad)
    quint = {}
    for p, j in enumerate(order):
        quint[trad[j][1]] = min(5 * p // T, 4) + 1
    res["by_move_quintile"] = {}
    for qn in range(1, 6):
        idx = [i for i, d in trad if quint[d] == qn]
        res["by_move_quintile"][qn] = {
            "sessions": len(idx), "mean_net_bp": float(np.mean(r[idx])) * 1e4,
            "sum_net": float(np.sum(r[idx])), "mean_gross_bp": float(np.mean(g[idx])) * 1e4,
            "median_abs_move_bp": float(np.median([moves[j] for j in range(T) if quint[trad[j][1]] == qn])) * 1e4,
            "trades": sum(1 for t in rows if quint.get(t["session"]) == qn)}

    # Predictions.
    q12 = [i for i, d in trad if quint[d] in (1, 2)]
    q5 = [i for i, d in trad if quint[d] == 5]
    unit_net = np.array([t["net_bp_unit"] for t in rows]) / 1e4
    mu = unit_net.mean()
    m2 = ((unit_net - mu) ** 2).mean()
    m3 = ((unit_net - mu) ** 3).mean()
    skew = float(m3 / m2 ** 1.5)
    winrate = res["primary"]["full"]["win_rate"]
    p1 = float(np.mean(r[q12])) > 0 and float(np.mean(r[q5])) < 0
    if len(added_legs) < 30:
        p2 = "not testable"
    else:
        p2 = "consistent" if np.mean(added_legs) > np.mean(first_legs) else "not consistent"
    res["predictions"] = {
        "1_range_days": {"q12_mean_net_bp": float(np.mean(r[q12])) * 1e4, "q5_mean_net_bp": float(np.mean(r[q5])) * 1e4,
                         "score": "consistent" if p1 else "not consistent"},
        "2_adding_paid": {"first_mean_gross_bp": float(np.mean(first_legs)) * 1e4,
                          "added_mean_gross_bp": float(np.mean(added_legs)) * 1e4 if added_legs else None,
                          "added_count": len(added_legs), "score": p2},
        "3_shape": {"win_rate": winrate, "skew": skew,
                    "score": "consistent" if (winrate > 0.55 and skew < 0) else "not consistent"},
    }

    # Costs.
    res["costs"] = {}
    for c in COSTS:
        rc = daily_returns(camps, calendar, c)
        res["costs"][c] = {w: {"sharpe": sharpe(rc[m]), "total_return": float(np.prod(1 + rc[m]) - 1)}
                           for w, m in (("full", np.ones(len(calendar), bool)),
                                        ("is", np.array([window_of(d) == "is" for d in calendar])),
                                        ("oos", np.array([window_of(d) == "oos" for d in calendar])))}

    # Fill delay.
    res["delay"] = {}
    for dl in (0, 2):
        cd = q.run(delay=dl)
        rd = daily_returns(cd, calendar, COST_BPS)
        res["delay"][dl] = three_windows(q, rd, campaign_rows(q, cd, COST_BPS))

    # Placebos and bootstrap.
    dp = direction_placebo(camps, calendar)
    res["direction_placebo"] = {k: v for k, v in dp.items() if k != "draws"}
    tp = timing_placebo(q, camps, dp["actual_gross_sharpe"])
    res["timing_placebo"] = {k: v for k, v in tp.items() if k != "draws"}
    res["bootstrap"] = bootstrap(r)
    np.save(HERE / "placebo_draws.npy", np.array([dp["draws"], tp["draws"] + [np.nan] * (N_DIRECTION - N_TIMING)]))

    # Grid.
    is_m = np.array([window_of(d) == "is" for d in calendar])
    grid = []
    for n in GRID_N:
        for k in GRID_K:
            for st in GRID_STEP:
                cg = q.run(n=n, k=k, step=st)
                rg = daily_returns(cg, calendar, COST_BPS)
                grid.append({"n": n, "k": k, "step": st, "is_sharpe": sharpe(rg[is_m]),
                             "oos_sharpe": sharpe(rg[~is_m]),
                             "is_trades": sum(len(v) for d, v in cg.items() if window_of(d) == "is"),
                             "primary": (n, k, st) == (N, K, STEP)})
    ranked = sorted(grid, key=lambda c: -(c["is_sharpe"] if not math.isnan(c["is_sharpe"]) else -1e9))
    res["grid"] = {"cells": grid,
                   "is_positive": sum(1 for c in grid if not math.isnan(c["is_sharpe"]) and c["is_sharpe"] > 0),
                   "primary_is_rank": 1 + next(i for i, c in enumerate(ranked) if c["primary"]),
                   "is_best": ranked[0],
                   "rank_corr_is_oos": float(np.corrcoef(
                       np.argsort(np.argsort([c["is_sharpe"] for c in grid])),
                       np.argsort(np.argsort([c["oos_sharpe"] for c in grid])))[0, 1])}

    # Diagnostics D1 (no adding) and D2 (continuous bands).
    c1 = q.run(units=1)
    r1 = daily_returns(c1, calendar, COST_BPS)
    res["d1_no_adding"] = three_windows(q, r1, campaign_rows(q, c1, COST_BPS))
    res["d1_no_adding"]["gross_sharpe_full"] = sharpe(gross_daily(c1, calendar))
    c2 = q.run(continuous=True)
    r2 = daily_returns(c2, calendar, COST_BPS)
    res["d2_continuous"] = three_windows(q, r2, campaign_rows(q, c2, COST_BPS))
    res["d2_continuous"]["gross_sharpe_full"] = sharpe(gross_daily(c2, calendar))

    # Cross-market.
    cross_r = {}
    res["cross"] = {}
    for s in CROSS:
        sd = data[s]
        cs = sd.run()
        rs = daily_returns(cs, calendar, COST_BPS)
        cross_r[s] = rs
        res["cross"][s] = three_windows(sd, rs, campaign_rows(sd, cs, COST_BPS))
        res["cross"][s]["gross_sharpe"] = {w: sharpe(gross_daily(cs, calendar)[m]) for w, m in
                                           (("full", np.ones(len(calendar), bool)), ("oos", ~is_m))}
    res["correlations"] = {"QQQ_SPY": float(np.corrcoef(r, cross_r["SPY"])[0, 1]),
                           "QQQ_IGV": float(np.corrcoef(r, cross_r["IGV"])[0, 1]),
                           "SPY_IGV": float(np.corrcoef(cross_r["SPY"], cross_r["IGV"])[0, 1])}

    # Acceptance.
    P = res["primary"]
    acc = [
        {"line": 1, "criterion": "OOS Sharpe >= 0.5 and OOS PF >= 1.10",
         "actual": f"Sharpe {P['oos']['sharpe']:.2f}, PF {P['oos']['profit_factor']:.2f}",
         "pass": P["oos"]["sharpe"] >= 0.5 and P["oos"]["profit_factor"] >= 1.10},
        {"line": 2, "criterion": "Direction placebo p <= 0.05", "actual": f"p = {dp['p']:.3f}",
         "pass": dp["p"] <= 0.05},
        {"line": 3, "criterion": "IS Sharpe > 0 and >= 17 of 27 IS grid cells > 0",
         "actual": f"IS Sharpe {P['is']['sharpe']:.2f}, {res['grid']['is_positive']} of 27",
         "pass": P["is"]["sharpe"] > 0 and res["grid"]["is_positive"] >= 17},
        {"line": 4, "criterion": "Full-sample return > 0 at 2 bp",
         "actual": f"{res['costs'][2.0]['full']['total_return'] * 100:.2f}%",
         "pass": res["costs"][2.0]["full"]["total_return"] > 0},
        {"line": 5, "criterion": "SPY OOS Sharpe > 0", "actual": f"{res['cross']['SPY']['oos']['sharpe']:.2f}",
         "pass": res["cross"]["SPY"]["oos"]["sharpe"] > 0},
        {"line": 6, "criterion": ">= 100 OOS QQQ campaigns", "actual": str(P["oos"]["trades"]),
         "pass": P["oos"]["trades"] >= 100},
    ]
    res["acceptance"] = acc
    if not acc[5]["pass"]:
        status = "Inconclusive"
    elif all(a["pass"] for a in acc):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    res["status"] = status
    res["failed_lines"] = [a["line"] for a in acc if not a["pass"]]

    # Outputs.
    (HERE / "results.json").write_text(json.dumps(clean(res), indent=2) + "\n", encoding="utf-8")
    with open(HERE / "daily.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["session", "window", "tradable", "qqq_net", "qqq_gross", "buy_and_hold", "open_to_close",
                    "spy_net", "igv_net", "d1_no_adding_net", "d2_continuous_net", "move_quintile"])
        for i, d in enumerate(calendar):
            w.writerow([d.isoformat(), window_of(d), int(q.tradable[d]), f"{r[i]:.10f}", f"{g[i]:.10f}",
                        f"{bh[i]:.10f}", f"{o2c[i]:.10f}", f"{cross_r['SPY'][i]:.10f}",
                        f"{cross_r['IGV'][i]:.10f}", f"{r1[i]:.10f}", f"{r2[i]:.10f}", quint.get(d, "")])
    with open(HERE / "trades.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["id", "session", "window", "side", "units", "entry_time", "entry_at", "legs", "avg_entry",
                    "exit_time", "exit_at", "exit_price", "reason", "hold_bars", "gross_bp_unit", "net_bp_unit",
                    "net_dollars", "unit_notional"])
        for n_, t in enumerate(rows, 1):
            bars = t["bars"]
            legs = "|".join(f"{ny(bars[i].ts)[11:]}{'c' if at == 'close' else ''}@{p:.4f}" for i, p, at in t["legs"])
            w.writerow([n_, t["session"].isoformat(), t["window"], "long" if t["side"] == 1 else "short", t["units"],
                        ny(bars[t["entry_idx"]].ts), t["entry_at"], legs, f"{t['avg_entry']:.6f}",
                        ny(bars[t["exit_idx"]].ts), t["exit_at"], f"{t['exit_price']:.6f}", t["reason"],
                        t["hold_bars"], f"{t['gross_bp_unit']:.4f}", f"{t['net_bp_unit']:.4f}",
                        f"{t['net_dollars']:.10f}", f"{t['unit_notional']:.10f}"])

    # Run log.
    head, dirty = git_state()
    entry = (f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n"
             f"- reason: {args.reason}\n- rules_sha256: {h}\n- git_head: {head}\n- git_dirty: {str(dirty).lower()}\n"
             f"- full: sharpe {P['full']['sharpe']:.4f}, return {P['full']['total_return'] * 100:.4f}%\n"
             f"- IS: sharpe {P['is']['sharpe']:.4f}, return {P['is']['total_return'] * 100:.4f}%\n"
             f"- OOS: sharpe {P['oos']['sharpe']:.4f}, return {P['oos']['total_return'] * 100:.4f}%\n"
             f"- status: {status}\n")
    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nAppend-only. One entry per store run.\n", encoding="utf-8")
    with open(log, "a", encoding="utf-8") as f:
        f.write(entry)
    print(entry)


if __name__ == "__main__":
    main()
