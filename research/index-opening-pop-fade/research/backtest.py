# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Index opening-pop fade: the pre-registered backtest.

Runs the self-test on synthetic sessions, refuses to run if RULES.md no longer
matches RULES.lock, checks the data, then runs every pre-registered check and
writes results.json, daily.csv, trades.csv, and a RUNLOG.md entry.

    python research/index-opening-pop-fade/research/backtest.py --reason "initial run"
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
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, ny_datetime, nyse_sessions  # noqa: E402

# ---- Constants: one for one with RULES.md -----------------------------------
T_DECISION = 10 * 60             # 10:00, minute of day
Z = 1.0                          # pop threshold in trailing RMS units
L = 60                           # RMS lookback, sessions with a pop
COST_BPS = {"SPY": 1.0, "QQQ": 1.0, "IGV": 2.0}
PRIMARY, CROSS, REPORTED = "SPY", "QQQ", "IGV"
DATA_START = date(2021, 9, 27)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
END = date(2026, 9, 25)
EXPECTED_EVAL_START = {"open": date(2021, 12, 21), "close": date(2021, 12, 22)}
COST_SWEEP = (0.0, 0.5, 1.0, 2.0, 3.0)      # multiples of base cost
GRID_Z = (0.5, 0.75, 1.0, 1.5, 2.0)
GRID_T = (9 * 60 + 45, 10 * 60, 10 * 60 + 30)
N_DIR, N_TIME, N_BOOT, BLOCK = 2000, 2000, 2000, 20
SEED_DIR, SEED_TIME, SEED_BOOT = 20260926, 20260927, 20260928
MIN_OOS_TRADES = 60
OPEN_MIN = 9 * 60 + 30
NOON = 12 * 60
LAST_HALF = 15 * 60 + 30
REGIME_SMA = 50


# ---- Rules lock -------------------------------------------------------------
def rules_hash() -> str:
    b = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(b).hexdigest()


def check_lock() -> str:
    lock = dict(line.split(" ", 1) for line in (HERE / "RULES.lock").read_text().splitlines() if line)
    h = rules_hash()
    if lock["sha256"].strip() != h:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {lock['sha256']}. Refusing to run.")
    return h


# ---- Sessions ---------------------------------------------------------------
@dataclass
class Sess:
    day: date
    m: list[int]        # minute of day of each bar's open (New York)
    o: list[float]
    c: list[float]

    def close_before(self, t: int) -> tuple[int, float] | None:
        """(minute, close) of the last bar with minute < t."""
        out = None
        for mm, cc in zip(self.m, self.c):
            if mm < t:
                out = (mm, cc)
            else:
                break
        return out

    def open_at_or_after(self, t: int) -> tuple[int, float] | None:
        for mm, oo in zip(self.m, self.o):
            if mm >= t:
                return mm, oo
        return None


def load_sessions(md: MarketData, sym: str) -> list[Sess]:
    by_day: dict[date, list] = {}
    for b in md.bars(sym, "1m", DATA_START, END):
        by_day.setdefault(b.session, []).append(b)
    out = []
    for d in sorted(by_day):
        bars = sorted(by_day[d], key=lambda b: b.ts)
        ms = []
        for b in bars:
            t = ny_datetime(b.ts)
            ms.append(t.hour * 60 + t.minute)
        out.append(Sess(d, ms, [b.open for b in bars], [b.close for b in bars]))
    return out


def data_checks(sym: str, sessions: list[Sess], calendar: list[date]) -> dict:
    have = {s.day for s in sessions}
    missing = [d for d in calendar if d not in have]
    assert missing == [date(2021, 12, 31)], f"{sym}: unexpected sessions without bars {missing}"
    for s in sessions:
        assert s.m[0] == OPEN_MIN, f"{sym} {s.day}: first bar at minute {s.m[0]}"
        want = 13 * 60 if s.day in EARLY_CLOSES else 15 * 60 + 59
        assert s.m[-1] == want, f"{sym} {s.day}: last bar at minute {s.m[-1]}, expected {want}"
    return {"sessions_with_bars": len(sessions), "sessions_without_bars": [str(d) for d in missing]}


# ---- Engine -----------------------------------------------------------------
@dataclass
class Trade:
    day: date
    side: str
    pop: float
    rms: float
    entry_min: int
    entry_px: float
    exit_min: int
    exit_px: float
    gross: float
    net: float
    gap_up: bool | None
    seg_noon: float | None      # fade-signed entry -> last close before 12:00
    seg_last: float | None      # fade-signed last close before 15:30 -> session close
    reason: str = "session_close"

    @property
    def z(self) -> float:
        return self.pop / self.rms


@dataclass
class Result:
    eval_start: date
    days: list                  # evaluation calendar
    net: dict                   # date -> net daily return
    gross: dict
    trades: list = field(default_factory=list)


def pops_for(sessions: list[Sess], t: int, kind: str) -> dict[date, float]:
    """kind 'open': P/O - 1 (O = the 09:30 bar open). kind 'close': P/prior last close - 1."""
    out: dict[date, float] = {}
    prev_c = None
    for s in sessions:
        pb = s.close_before(t)
        if pb is not None:
            if kind == "open":
                if s.m[0] == OPEN_MIN:
                    out[s.day] = pb[1] / s.o[0] - 1
            elif prev_c is not None:
                out[s.day] = pb[1] / prev_c - 1
        prev_c = s.c[-1]
    return out


def simulate(sessions: list[Sess], calendar: list[date], *, t: int = T_DECISION, z: float = Z,
             lookback: int = L, cost_bps: float = 1.0, kind: str = "open", fill: str = "base",
             side: str = "short") -> Result:
    """fill: 'base' (open of first bar >= t), 'delay' (first bar >= t+1), 'upper' (at P).
    side: 'short' on pops >= z*R, or 'long' (mirror diagnostic) on pops <= -z*R."""
    c = cost_bps / 1e4
    pops = pops_for(sessions, t, kind)
    hist: list[float] = []
    prev_close: dict[date, float] = {}
    pc = None
    for s in sessions:
        if pc is not None:
            prev_close[s.day] = pc
        pc = s.c[-1]
    trades: list[Trade] = []
    eval_start = None
    for s in sessions:
        d = s.day
        if d not in pops:
            continue
        p = pops[d]
        if len(hist) >= lookback:
            if eval_start is None:
                eval_start = d
            window = hist[-lookback:]
            rms = math.sqrt(sum(x * x for x in window) / lookback)
            hit = p >= z * rms if side == "short" else p <= -z * rms
            if hit and rms > 0:
                pb = s.close_before(t)
                if fill == "upper":
                    ent = pb
                else:
                    ent = s.open_at_or_after(t if fill == "base" else t + 1)
                if ent is not None:
                    e_min, e_px = ent
                    x_px, x_min = s.c[-1], s.m[-1]
                    sign = 1.0 if side == "short" else -1.0
                    gross = sign * (1 - x_px / e_px)
                    net = gross - c * (1 + x_px / e_px)
                    nb = s.close_before(NOON)
                    lb = s.close_before(LAST_HALF)
                    seg_noon = sign * (1 - nb[1] / e_px) if nb is not None and nb[0] >= e_min else None
                    seg_last = sign * (1 - x_px / lb[1]) if lb is not None else None
                    gap = None
                    if d in prev_close and s.m[0] == OPEN_MIN:
                        gap = s.o[0] > prev_close[d]
                    trades.append(Trade(d, side, p, rms, e_min, e_px, x_min, x_px, gross, net,
                                        gap, seg_noon, seg_last))
        hist.append(p)
    if eval_start is None:
        return Result(date.max, [], {}, {}, [])
    days = [d for d in calendar if d >= eval_start]
    net = {d: 0.0 for d in days}
    gross = {d: 0.0 for d in days}
    for tr in trades:
        net[tr.day] = tr.net
        gross[tr.day] = tr.gross
    return Result(eval_start, days, net, gross, trades)


# ---- Metrics ----------------------------------------------------------------
def sharpe(r) -> float:
    r = np.asarray(r, dtype=float)
    if len(r) < 2:
        return float("nan")
    sd = r.std(ddof=1)
    return float(r.mean() / sd * math.sqrt(252)) if sd > 0 else float("nan")


def max_dd(r) -> float:
    eq = np.cumprod(1 + np.asarray(r, dtype=float))
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return float((eq / peak - 1).min()) if len(eq) else 0.0


def in_window(d: date, w: str) -> bool:
    return w == "full" or (w == "IS" and d <= IS_END) or (w == "OOS" and d >= OOS_START)


def metrics(res: Result, w: str, bench: dict | None = None) -> dict:
    days = [d for d in res.days if in_window(d, w)]
    r = np.array([res.net[d] for d in days])
    trs = [t for t in res.trades if in_window(t.day, w)]
    nets = np.array([t.net for t in trs])
    n = len(r)
    total = float(np.prod(1 + r) - 1) if n else 0.0
    sd = r.std(ddof=1) if n > 1 else float("nan")
    wins, losses = nets[nets > 0].sum() if len(nets) else 0.0, nets[nets < 0].sum() if len(nets) else 0.0
    out = {
        "first": str(days[0]) if days else None, "last": str(days[-1]) if days else None,
        "sessions": n,
        "total_return": total,
        "cagr": float((1 + total) ** (252 / n) - 1) if n else float("nan"),
        "ann_vol": float(sd * math.sqrt(252)) if n > 1 else float("nan"),
        "sharpe": sharpe(r),
        "max_dd": max_dd(r),
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if n > 1 and sd > 0 else float("nan"),
        "trades": len(trs),
        "win_rate": float((nets > 0).mean()) if len(nets) else float("nan"),
        "profit_factor": float(wins / abs(losses)) if losses < 0 else float("inf"),
        "avg_net_bp": float(nets.mean() * 1e4) if len(nets) else float("nan"),
        "avg_gross_bp": float(np.mean([t.gross for t in trs]) * 1e4) if trs else float("nan"),
        "exposure_sessions": float(len(trs) / n) if n else float("nan"),
        "exposure_time": float(sum(t.exit_min + 1 - t.entry_min for t in trs) / sum(
            390 if d not in EARLY_CLOSES else 211 for d in days)) if n else float("nan"),
        "gross_sharpe": sharpe([res.gross[d] for d in days]),
    }
    if bench is not None:
        b = np.array([bench[d] for d in days])
        out["bench_sharpe"] = sharpe(b)
        out["bench_max_dd"] = max_dd(b)
        out["bench_total_return"] = float(np.prod(1 + b) - 1)
    return out


def benchmarks(sessions: list[Sess], days: list[date]) -> tuple[dict, dict]:
    """Buy and hold close to close; unconditional short 10:00 open -> last close (gross)."""
    by_day = {s.day: s for s in sessions}
    bh, us = {}, {}
    prev_c = None
    first = days[0]
    for s in sessions:
        if s.day >= first:
            bh[s.day] = s.c[-1] / prev_c - 1 if prev_c is not None else 0.0
            e = s.open_at_or_after(T_DECISION)
            us[s.day] = (1 - s.c[-1] / e[1]) if e is not None else 0.0
        prev_c = s.c[-1]
    for d in days:
        bh.setdefault(d, 0.0)
        us.setdefault(d, 0.0)
    del by_day
    return bh, us


# ---- Self-test --------------------------------------------------------------
def _synthetic(day: date, o: float, p: float, x: float, *, first: int = OPEN_MIN, last: int = 15 * 60 + 59,
               drop: tuple = (), e_open: float | None = None, noon: float | None = None,
               pre_last: float | None = None) -> Sess:
    """Flat bars at o until 09:59 close = p; 10:00 open = e_open (default p); last close = x."""
    ms = [m for m in range(first, last + 1) if m not in drop]
    op, cl = [], []
    for m in ms:
        if m < T_DECISION:
            op.append(o)
            cl.append(o if m < T_DECISION - 1 else p)
        elif m == ms[-1]:
            op.append(e_open if (e_open is not None and m == min(k for k in ms if k >= T_DECISION)) else p)
            cl.append(x)
        else:
            first_after = min(k for k in ms if k >= T_DECISION)
            op.append(e_open if (e_open is not None and m == first_after) else p)
            if noon is not None and m == 11 * 60 + 59:
                cl.append(noon)
            elif pre_last is not None and m == LAST_HALF - 1:
                cl.append(pre_last)
            else:
                cl.append(p)
    # Make the 09:59 close exactly p even if 09:59 is dropped: the last pre-10:00 bar closes at p.
    pre = [i for i, m in enumerate(ms) if m < T_DECISION]
    if pre:
        cl[pre[-1]] = p
    return Sess(day, ms, op, cl)


def self_test() -> None:
    cal = nyse_sessions(date(2023, 1, 3), date(2023, 2, 28))
    ss: list[Sess] = []
    lb = 3
    # Warm-up: pops of +0.5% and -0.5% -> RMS 0.5%.
    ss.append(_synthetic(cal[0], 100, 100.5, 100))
    ss.append(_synthetic(cal[1], 100, 99.5, 100))
    ss.append(_synthetic(cal[2], 100, 100.5, 100))
    # cal[3]: pop +0.6% >= 1.0 * RMS(0.5%) -> short at 10:00 open 100.7, exit 100.2.
    ss.append(_synthetic(cal[3], 100, 100.6, 100.2, e_open=100.7, noon=100.4, pre_last=100.3))
    # cal[4]: pop +0.3% -> no trade.
    ss.append(_synthetic(cal[4], 100, 100.3, 101))
    # cal[5]: first bar at 09:31 -> no pop, no trade, not in the RMS window.
    ss.append(_synthetic(cal[5], 100, 105, 100, first=OPEN_MIN + 1))
    # cal[6]: 09:59 and 10:00 missing: P = 09:58 close, entry = 10:01 open. Early-close style last bar 13:00.
    ss.append(_synthetic(cal[6], 100, 101, 100.5, drop=(599, 600), last=13 * 60))
    # cal[7]: session with no bar at or after 10:00 -> pop exists, no trade.
    ss.append(_synthetic(cal[7], 100, 102, 102, last=599))
    # cal[8] has no bars at all (a zero day). cal[9]: RMS uses prior pops only.
    ss.append(_synthetic(cal[9], 100, 97.0, 100))       # mirror-long signal day (-3%)
    cal_used = cal[:10]
    r = simulate(ss, cal_used, lookback=lb, cost_bps=1.0)
    assert r.eval_start == cal[3], r.eval_start
    assert [t.day for t in r.trades] == [cal[3], cal[6], ], [t.day for t in r.trades]
    t0 = r.trades[0]
    assert (t0.entry_min, t0.entry_px, t0.exit_min, t0.exit_px) == (600, 100.7, 959, 100.2)
    assert abs(t0.gross - (1 - 100.2 / 100.7)) < 1e-12
    assert abs(t0.net - (t0.gross - 1e-4 * (1 + 100.2 / 100.7))) < 1e-12
    assert abs(t0.seg_noon - (1 - 100.4 / 100.7)) < 1e-12
    assert abs(t0.seg_last - (1 - 100.2 / 100.3)) < 1e-12
    # cal[6]: window = pops of cal[3], cal[4], cal[2]... last 3 pops before cal[6] = [0.005, 0.006, 0.003].
    rms6 = math.sqrt((0.005 ** 2 + 0.006 ** 2 + 0.003 ** 2) / 3)
    t1 = r.trades[1]
    assert abs(t1.rms - rms6) < 1e-12 and abs(t1.pop - 0.01) < 1e-12
    assert (t1.entry_min, t1.exit_min) == (601, 13 * 60), (t1.entry_min, t1.exit_min)
    assert t1.seg_last is not None and abs(t1.seg_last) < 1e-12   # 15:30 is after an early close
    assert r.net[cal[8]] == 0.0 and r.net[cal[4]] == 0.0 and cal[8] in r.net
    # RMS excludes today's pop: cal[3] pop 0.6% vs RMS 0.5%. Including it would give RMS 0.53% (still
    # a trade), so check directly that the stored rms is the prior-only value.
    assert abs(t0.rms - 0.005) < 1e-12
    # Delay fill: cal[3] enters at the 10:01 open (= p = 100.6 in the synthetic).
    rd = simulate(ss, cal_used, lookback=lb, fill="delay")
    assert (rd.trades[0].entry_min, rd.trades[0].entry_px) == (601, 100.6)
    # Upper-bound fill: at P, the 09:59 close.
    ru = simulate(ss, cal_used, lookback=lb, fill="upper")
    assert (ru.trades[0].entry_min, ru.trades[0].entry_px) == (599, 100.6)
    assert (ru.trades[1].entry_min, ru.trades[1].entry_px) == (598, 101)
    # Mirror long: cal[9] pop -1% <= -1.0 * RMS -> long.
    rl = simulate(ss, cal_used, lookback=lb, side="long")
    assert [t.day for t in rl.trades] == [cal[9]]
    assert abs(rl.trades[0].gross - (100 / 97.0 - 1)) < 1e-12
    # Kind 'close': cal[3] pop = 100.6 / prior close 100 - 1 = 0.6%; cal[5] prior close is cal[4]'s 101.
    pc = pops_for(ss, T_DECISION, "close")
    assert cal[0] not in pc and abs(pc[cal[3]] - 0.006) < 1e-12 and abs(pc[cal[5]] - (105 / 101 - 1)) < 1e-12
    assert abs(pc[cal[9]] - (97.0 / 102 - 1)) < 1e-12        # prior session with bars is cal[7]
    # Cost sweep: zero cost -> net == gross.
    r0 = simulate(ss, cal_used, lookback=lb, cost_bps=0.0)
    assert all(t.net == t.gross for t in r0.trades)
    # Grid decision time 09:45: pop measured to the 09:44 close (= o in the synthetic) -> no trades.
    rg = simulate(ss, cal_used, lookback=lb, t=9 * 60 + 45)
    assert rg.trades == []
    print("self-test: all cases passed")


# ---- Placebos and bootstrap -------------------------------------------------
def direction_placebo(res: Result) -> dict:
    days = res.days
    g = np.array([res.gross[d] for d in days])
    idx = np.array([days.index(t.day) for t in res.trades])
    actual = sharpe(g)
    rng = np.random.default_rng(SEED_DIR)
    draws = np.empty(N_DIR)
    for k in range(N_DIR):
        x = g.copy()
        x[idx] *= rng.choice((-1.0, 1.0), size=len(idx))
        draws[k] = sharpe(x)
    return {"actual_gross_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)),
            "p": float((1 + (draws >= actual).sum()) / (N_DIR + 1)), "draws": draws.tolist()}


def timing_placebo(res: Result, uncond: dict, eligible: list[date]) -> dict:
    days = res.days
    pos = {d: i for i, d in enumerate(days)}
    elig_idx = np.array([pos[d] for d in eligible])
    vals = np.array([uncond[d] for d in eligible])
    n = len(res.trades)
    actual = sharpe([res.gross[d] for d in days])
    rng = np.random.default_rng(SEED_TIME)
    draws = np.empty(N_TIME)
    for k in range(N_TIME):
        pick = rng.choice(len(eligible), size=n, replace=False)
        x = np.zeros(len(days))
        x[elig_idx[pick]] = vals[pick]
        draws[k] = sharpe(x)
    return {"actual_gross_sharpe": actual, "n_trades": n, "n_eligible": len(eligible),
            "null_mean": float(draws.mean()), "null_p95": float(np.percentile(draws, 95)),
            "p": float((1 + (draws >= actual).sum()) / (N_TIME + 1)), "draws": draws.tolist()}


def bootstrap(r: list[float]) -> dict:
    r = np.asarray(r)
    n = len(r)
    rng = np.random.default_rng(SEED_BOOT)
    out = np.empty(N_BOOT)
    nb = math.ceil(n / BLOCK)
    for k in range(N_BOOT):
        starts = rng.integers(0, n, size=nb)
        idx = (starts[:, None] + np.arange(BLOCK)[None, :]).ravel()[:n] % n
        out[k] = sharpe(r[idx])
    return {"lo": float(np.percentile(out, 2.5)), "hi": float(np.percentile(out, 97.5)),
            "median": float(np.median(out))}


# ---- Breakdowns -------------------------------------------------------------
def breakdowns(res: Result, sessions: list[Sess], bh: dict) -> dict:
    out: dict = {}
    by_year: dict = {}
    for y in sorted({d.year for d in res.days}):
        days = [d for d in res.days if d.year == y]
        r = np.array([res.net[d] for d in days])
        b = np.array([bh[d] for d in days])
        trs = [t for t in res.trades if t.day.year == y]
        by_year[y] = {"sessions": len(days), "trades": len(trs), "return": float(np.prod(1 + r) - 1),
                      "sharpe": sharpe(r), "max_dd": max_dd(r), "bench_return": float(np.prod(1 + b) - 1),
                      "avg_net_bp": float(np.mean([t.net for t in trs]) * 1e4) if trs else None}
    out["by_year"] = by_year

    def grp(trs):
        if not trs:
            return {"trades": 0}
        nets = np.array([t.net for t in trs])
        pos, neg = nets[nets > 0].sum(), nets[nets < 0].sum()
        return {"trades": len(trs), "avg_net_bp": float(nets.mean() * 1e4),
                "avg_gross_bp": float(np.mean([t.gross for t in trs]) * 1e4),
                "win_rate": float((nets > 0).mean()), "sum_net": float(nets.sum()),
                "profit_factor": float(pos / abs(neg)) if neg < 0 else None}

    tr = res.trades
    out["by_pop_bucket"] = {"1.0-1.5": grp([t for t in tr if t.z < 1.5]),
                            "1.5-2.0": grp([t for t in tr if 1.5 <= t.z < 2.0]),
                            ">=2.0": grp([t for t in tr if t.z >= 2.0])}
    out["by_gap"] = {"gap_up": grp([t for t in tr if t.gap_up is True]),
                     "gap_down_or_flat": grp([t for t in tr if t.gap_up is False])}
    # Regime: prior close vs mean of the 50 prior closes (sessions with bars).
    closes = [(s.day, s.c[-1]) for s in sessions]
    regime = {}
    for i, (d, _) in enumerate(closes):
        if i >= REGIME_SMA + 1:
            prior = [c for _, c in closes[i - REGIME_SMA:i]]
            regime[d] = "above" if closes[i - 1][1] > sum(prior) / REGIME_SMA else "below"
    out["by_regime_sma50"] = {k: grp([t for t in tr if regime.get(t.day) == k]) for k in ("above", "below")}
    out["by_side"] = {"short": grp(tr)}
    out["by_exit_reason"] = {"session_close": grp(tr)}
    # Quintiles of the open-to-close move over all evaluable sessions with bars.
    sd = {s.day: s for s in sessions}
    oc = {d: sd[d].c[-1] / sd[d].o[0] - 1 for d in res.days if d in sd}
    ds = sorted(oc, key=lambda d: oc[d])
    q = {}
    for k in range(5):
        chunk = ds[k * len(ds) // 5:(k + 1) * len(ds) // 5]
        cs = set(chunk)
        q[f"Q{k + 1}"] = {"sessions": len(chunk), "mean_open_close_bp": float(np.mean([oc[d] for d in chunk]) * 1e4),
                          "range_bp": [float(oc[chunk[0]] * 1e4), float(oc[chunk[-1]] * 1e4)],
                          "trades": sum(1 for t in tr if t.day in cs),
                          "mean_strategy_day_bp": float(np.mean([res.net[d] for d in chunk]) * 1e4),
                          "sum_net": float(sum(res.net[d] for d in chunk))}
    out["by_open_close_quintile"] = q
    out["segments"] = {
        "entry_to_1159_mean_bp": float(np.mean([t.seg_noon for t in tr if t.seg_noon is not None]) * 1e4),
        "entry_to_1159_n": sum(1 for t in tr if t.seg_noon is not None),
        "last_half_hour_mean_bp": float(np.mean([t.seg_last for t in tr if t.seg_last is not None]) * 1e4),
        "last_half_hour_n": sum(1 for t in tr if t.seg_last is not None),
    }
    return out


# ---- Output -----------------------------------------------------------------
def git_state() -> tuple[str, bool]:
    h = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True,
                                cwd=ROOT).stdout.strip())
    return h, dirty


def clean(x):
    if isinstance(x, float):
        return None if math.isnan(x) else ("inf" if math.isinf(x) else x)
    if isinstance(x, dict):
        return {str(k): clean(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [clean(v) for v in x]
    return x


def book(sessions, calendar, sym, bh, **kw) -> dict:
    res = simulate(sessions, calendar, cost_bps=COST_BPS[sym], **kw)
    return {"res": res, "m": {w: metrics(res, w, bh) for w in ("full", "IS", "OOS")}}


def evaluate(name: str, kind: str, data: dict, calendar: list[date], bhs: dict, unconds: dict) -> dict:
    """Every pre-registered check for one candidate (primary: kind 'open'; S1: kind 'close')."""
    sessions = data[PRIMARY]
    bh = bhs[PRIMARY]
    base = book(sessions, calendar, PRIMARY, bh, kind=kind)
    res = base["res"]
    assert res.eval_start == EXPECTED_EVAL_START[kind], (name, res.eval_start)
    out: dict = {"eval_start": str(res.eval_start), "metrics": base["m"]}
    out["costs"] = {}
    for mult in COST_SWEEP:
        r = simulate(sessions, calendar, cost_bps=COST_BPS[PRIMARY] * mult, kind=kind)
        out["costs"][str(mult)] = {w: {k: metrics(r, w)[k] for k in ("sharpe", "total_return", "profit_factor")}
                                   for w in ("full", "IS", "OOS")}
    out["fills"] = {}
    for f in ("delay", "upper"):
        r = simulate(sessions, calendar, cost_bps=COST_BPS[PRIMARY], kind=kind, fill=f)
        out["fills"][f] = {w: metrics(r, w) for w in ("full", "IS", "OOS")}
    out["direction_placebo"] = direction_placebo(res)
    eligible = [s.day for s in sessions if s.day >= res.eval_start and s.open_at_or_after(T_DECISION) is not None]
    out["timing_placebo"] = timing_placebo(res, unconds[PRIMARY], eligible)
    out["bootstrap"] = bootstrap([res.net[d] for d in res.days])
    grid = []
    for t in GRID_T:
        for z in GRID_Z:
            r = simulate(sessions, calendar, t=t, z=z, cost_bps=COST_BPS[PRIMARY], kind=kind)
            grid.append({"t": f"{t // 60:02d}:{t % 60:02d}", "z": z,
                         "is_sharpe": metrics(r, "IS")["sharpe"], "oos_sharpe": metrics(r, "OOS")["sharpe"],
                         "is_trades": metrics(r, "IS")["trades"], "oos_trades": metrics(r, "OOS")["trades"],
                         "primary": t == T_DECISION and z == Z})
    out["grid"] = grid
    out["cross"] = {}
    for sym in (CROSS, REPORTED):
        b = book(data[sym], calendar, sym, bhs[sym], kind=kind)
        out["cross"][sym] = {"metrics": b["m"],
                             "corr_with_primary": float(np.corrcoef(
                                 [res.net[d] for d in res.days],
                                 [b["res"].net.get(d, 0.0) for d in res.days])[0, 1]),
                             "res": b["res"]}
    mirror = simulate(sessions, calendar, cost_bps=COST_BPS[PRIMARY], kind=kind, side="long")
    out["mirror_long"] = {"metrics": {w: metrics(mirror, w) for w in ("full", "IS", "OOS")},
                          "mean_gross_bp": float(np.mean([t.gross for t in mirror.trades]) * 1e4)}
    out["breakdowns"] = breakdowns(res, sessions, bh)
    uc = unconds[PRIMARY]
    out["unconditional_short"] = {w: {"sharpe": sharpe([uc[d] for d in res.days if in_window(d, w)]),
                                      "total_return": float(np.prod([1 + uc[d] for d in res.days
                                                                     if in_window(d, w)]) - 1),
                                      "max_dd": max_dd([uc[d] for d in res.days if in_window(d, w)])}
                                  for w in ("full", "IS", "OOS")}
    m = out["metrics"]
    is_pos = sum(1 for g in grid if g["is_sharpe"] is not None and g["is_sharpe"] > 0)
    acc = [
        {"line": 1, "criterion": "OOS Sharpe >= 0.5 and OOS profit factor >= 1.10",
         "actual": f"Sharpe {m['OOS']['sharpe']:.2f}, PF {m['OOS']['profit_factor']:.2f}",
         "pass": m["OOS"]["sharpe"] >= 0.5 and m["OOS"]["profit_factor"] >= 1.10},
        {"line": 2, "criterion": "Direction placebo p <= 0.05 (full)",
         "actual": f"p = {out['direction_placebo']['p']:.4f}", "pass": out["direction_placebo"]["p"] <= 0.05},
        {"line": 3, "criterion": "IS Sharpe > 0 and >= 9 of 15 IS grid cells > 0",
         "actual": f"IS Sharpe {m['IS']['sharpe']:.2f}; {is_pos} of 15 cells > 0",
         "pass": m["IS"]["sharpe"] > 0 and is_pos >= 9},
        {"line": 4, "criterion": "Full-sample total return > 0 at 2 bp per side",
         "actual": f"{out['costs']['2.0']['full']['total_return'] * 100:.2f}%",
         "pass": out["costs"]["2.0"]["full"]["total_return"] > 0},
        {"line": 5, "criterion": "QQQ OOS Sharpe > 0, identical rules",
         "actual": f"{out['cross'][CROSS]['metrics']['OOS']['sharpe']:.2f}",
         "pass": out["cross"][CROSS]["metrics"]["OOS"]["sharpe"] > 0},
        {"line": 6, "criterion": "Timing placebo p <= 0.05 (full)",
         "actual": f"p = {out['timing_placebo']['p']:.4f}", "pass": out["timing_placebo"]["p"] <= 0.05},
        {"line": 7, "criterion": "At least 60 OOS trades (else Inconclusive)",
         "actual": f"{m['OOS']['trades']}", "pass": m["OOS"]["trades"] >= MIN_OOS_TRADES},
    ]
    out["acceptance"] = acc
    if not acc[6]["pass"]:
        status = "Inconclusive"
    elif all(a["pass"] for a in acc):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    out["status"] = status
    out["failed_lines"] = [a["line"] for a in acc if not a["pass"]]
    out["_res"] = res
    return out


def predictions(p: dict) -> list[dict]:
    tr = p["_res"].trades
    big = [t.gross for t in tr if t.z >= 1.5]
    small = [t.gross for t in tr if t.z < 1.5]
    seg = p["breakdowns"]["segments"]
    short_mean = float(np.mean([t.gross for t in tr]) * 1e4)
    return [
        {"n": 1, "prediction": "Mean gross trade for z >= 1.5 > for 1.0 <= z < 1.5",
         "evidence": {"z_ge_1.5_bp": float(np.mean(big) * 1e4), "n_big": len(big),
                      "z_lt_1.5_bp": float(np.mean(small) * 1e4), "n_small": len(small)},
         "consistent": bool(np.mean(big) > np.mean(small))},
        {"n": 2, "prediction": "Mean gross fade-signed entry -> 11:59 close > 0",
         "evidence": {"mean_bp": seg["entry_to_1159_mean_bp"], "n": seg["entry_to_1159_n"]},
         "consistent": seg["entry_to_1159_mean_bp"] > 0},
        {"n": 3, "prediction": "Short-on-pop mean gross > mirror-long-on-drop mean gross",
         "evidence": {"short_bp": short_mean, "mirror_long_bp": p["mirror_long"]["mean_gross_bp"]},
         "consistent": short_mean > p["mirror_long"]["mean_gross_bp"]},
        {"n": 4, "prediction": "Timing placebo p <= 0.05",
         "evidence": {"p": p["timing_placebo"]["p"]}, "consistent": p["timing_placebo"]["p"] <= 0.05},
        {"n": 5, "prediction": "Mean gross fade-signed last half hour >= 0",
         "evidence": {"mean_bp": seg["last_half_hour_mean_bp"], "n": seg["last_half_hour_n"]},
         "consistent": seg["last_half_hour_mean_bp"] >= 0},
    ]


def write_trades(path: Path, books: list[tuple[str, str, Result]]) -> None:
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["book", "symbol", "session", "side", "pop", "rms", "z", "entry_time", "entry_px",
                    "exit_time", "exit_px", "gross", "net", "gross_bp", "net_bp", "exit_reason",
                    "gap_up", "seg_entry_1159", "seg_last_half", "window"])
        for name, sym, res in books:
            for t in res.trades:
                w.writerow([name, sym, t.day, t.side, f"{t.pop:.8f}", f"{t.rms:.8f}", f"{t.z:.4f}",
                            f"{t.entry_min // 60:02d}:{t.entry_min % 60:02d}", f"{t.entry_px:.6f}",
                            f"{t.exit_min // 60:02d}:{t.exit_min % 60:02d}", f"{t.exit_px:.6f}",
                            f"{t.gross:.8f}", f"{t.net:.8f}", f"{t.gross * 1e4:.3f}", f"{t.net * 1e4:.3f}",
                            t.reason, t.gap_up,
                            "" if t.seg_noon is None else f"{t.seg_noon:.8f}",
                            "" if t.seg_last is None else f"{t.seg_last:.8f}",
                            "IS" if t.day <= IS_END else "OOS"])


def append_runlog(reason: str, h: str, head: str, dirty: bool, p: dict, s1: dict) -> None:
    m = p["metrics"]
    s = s1["metrics"]
    entry = (
        f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n\n"
        f"- Reason: {reason}\n- Rules sha256: `{h}`\n- git HEAD: `{head}` (dirty: {dirty})\n"
        f"- Primary SPY: full Sharpe {m['full']['sharpe']:.2f}, return {m['full']['total_return'] * 100:.2f}%; "
        f"IS Sharpe {m['IS']['sharpe']:.2f}, return {m['IS']['total_return'] * 100:.2f}%; "
        f"OOS Sharpe {m['OOS']['sharpe']:.2f}, return {m['OOS']['total_return'] * 100:.2f}%; "
        f"OOS trades {m['OOS']['trades']}. Status: {p['status']} (failed lines {p['failed_lines']})\n"
        f"- S1 SPY: full Sharpe {s['full']['sharpe']:.2f}, IS {s['IS']['sharpe']:.2f}, "
        f"OOS {s['OOS']['sharpe']:.2f}, OOS return {s['OOS']['total_return'] * 100:.2f}%. "
        f"Status: {s1['status']} (failed lines {s1['failed_lines']})\n"
    )
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nAppend-only. One entry per run that computes returns from the store.\n")
    with path.open("a") as f:
        f.write(entry)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reason", required=True)
    args = ap.parse_args()
    self_test()
    h = check_lock()
    calendar = nyse_sessions(DATA_START, END)
    data, checks = {}, {}
    with MarketData() as md:
        for sym in (PRIMARY, CROSS, REPORTED):
            data[sym] = load_sessions(md, sym)
            if sym != REPORTED:
                checks[sym] = data_checks(sym, data[sym], calendar)
    bhs, unconds = {}, {}
    for sym in data:
        bhs[sym], unconds[sym] = benchmarks(data[sym], [d for d in calendar if d >= date(2021, 12, 21)])
    primary = evaluate("primary", "open", data, calendar, bhs, unconds)
    s1 = evaluate("S1", "close", data, calendar, bhs, unconds)
    primary["predictions"] = predictions(primary)
    s1["predictions"] = predictions(s1)
    head, dirty = git_state()
    res = primary["_res"]

    def strip(x):
        y = {k: v for k, v in x.items() if k != "_res"}
        y["cross"] = {s: {k: v for k, v in c.items() if k != "res"} for s, c in x["cross"].items()}
        return y

    results = {
        "rules_sha256": h, "git_head": head, "git_dirty": dirty,
        "run_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"), "reason": args.reason,
        "seeds": {"direction": SEED_DIR, "timing": SEED_TIME, "bootstrap": SEED_BOOT},
        "params": {"T": "10:00", "Z": Z, "L": L, "cost_bps": COST_BPS, "is_end": str(IS_END),
                   "oos_start": str(OOS_START), "end": str(END)},
        "data_checks": checks,
        "primary": strip(primary),
        "s1": strip(s1),
    }
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=1))
    # daily.csv
    s1r = s1["_res"]
    q = primary["cross"][CROSS]["res"]
    ig = primary["cross"][REPORTED]["res"]
    with (HERE / "daily.csv").open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["session", "window", "primary_net", "primary_gross", "s1_net", "qqq_net", "igv_net",
                    "spy_buy_hold", "spy_uncond_short_gross", "qqq_buy_hold", "trade"])
        for d in res.days:
            w.writerow([d, "IS" if d <= IS_END else "OOS", f"{res.net[d]:.10f}", f"{res.gross[d]:.10f}",
                        f"{s1r.net.get(d, 0.0):.10f}", f"{q.net.get(d, 0.0):.10f}", f"{ig.net.get(d, 0.0):.10f}",
                        f"{bhs[PRIMARY][d]:.10f}", f"{unconds[PRIMARY][d]:.10f}", f"{bhs[CROSS][d]:.10f}",
                        int(any(t.day == d for t in res.trades))])
    write_trades(HERE / "trades.csv", [("primary", PRIMARY, res), ("primary", CROSS, q),
                                       ("primary", REPORTED, ig), ("S1", PRIMARY, s1r),
                                       ("S1", CROSS, s1["cross"][CROSS]["res"])])
    append_runlog(args.reason, h, head, dirty, primary, s1)
    m = primary["metrics"]
    print(f"Primary: {primary['status']}  failed lines {primary['failed_lines']}")
    for wdw in ("full", "IS", "OOS"):
        print(f"  {wdw:4s} Sharpe {m[wdw]['sharpe']:.2f} return {m[wdw]['total_return'] * 100:.2f}% "
              f"trades {m[wdw]['trades']} PF {m[wdw]['profit_factor']:.2f}")
    print(f"S1: {s1['status']}  failed lines {s1['failed_lines']}")


if __name__ == "__main__":
    main()
