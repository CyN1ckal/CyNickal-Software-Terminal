# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""SPY RSI(2) dip-buy: the pre-registered backtest.

Runs the self-test on synthetic sessions, refuses to run if RULES.md no longer
matches RULES.lock, then runs every pre-registered check and writes
results.json, daily.csv, trades.csv, and a RUNLOG.md entry.

    python research/spy-rsi2-dip-buy/research/backtest.py --reason "initial run"
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
from mdq import EARLY_CLOSES, MarketData, ny_datetime, nyse_sessions, rth_window  # noqa: E402

# ---- Constants: one for one with RULES.md -----------------------------------
RSI_N = 2
ENTRY_RSI = 10.0
EXIT_SMA = 5
TREND_SMA_S1 = 200
WARMUP = 20                      # sessions with bars before the first decision
COST_BPS = {"SPY": 1.0, "QQQ": 1.0, "IGV": 2.0}
PRIMARY, CROSS, REPORTED = "SPY", "QQQ", "IGV"
DATA_START = date(2021, 9, 27)
EVAL_START = date(2021, 10, 25)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
END = date(2026, 9, 25)
COST_SWEEP = (0.0, 0.5, 1.0, 2.0, 3.0)      # multiples of base cost
GRID_RSI = (5, 10, 15, 20, 25)
GRID_SMA = (3, 5, 10)
N_DIR, N_TIME, N_BOOT, BLOCK = 2000, 2000, 2000, 20
SEED_DIR, SEED_TIME, SEED_BOOT = 20260926, 20260927, 20260928
MIN_OOS_TRADES, MIN_OOS_TRADES_S1 = 25, 20
MIN_WIN_RATE = 0.60


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
class Session:
    day: date
    o: float          # first regular-hours minute open
    p: float          # decision price: 15:49 (12:49 early close) minute close
    c: float          # last regular-hours minute close


def decision_minute(day: date) -> int:
    return (12 if day in EARLY_CLOSES else 15) * 60 + 49


def sessions_from_minutes(minutes) -> list[Session]:
    """Group regular-hours 1-minute bars by session; build O, P, C."""
    by_day: dict[date, list] = {}
    for b in minutes:
        by_day.setdefault(b.session, []).append(b)
    out = []
    for d in sorted(by_day):
        bars = sorted(by_day[d], key=lambda b: b.ts)
        dm = decision_minute(d)
        p = None
        for b in bars:
            t = ny_datetime(b.ts)
            if t.hour * 60 + t.minute <= dm:
                p = b.close
            else:
                break
        if p is None:
            continue  # no decision possible; rules check asserts this never happens
        out.append(Session(d, bars[0].open, p, bars[-1].close))
    return out


# ---- Indicators -------------------------------------------------------------
def wilder_states(closes: list[float], n: int = RSI_N) -> list[tuple[float, float] | None]:
    """(AG, AL) after close i; None until seeded at index n."""
    st: list[tuple[float, float] | None] = [None] * len(closes)
    if len(closes) <= n:
        return st
    ch = [closes[k] - closes[k - 1] for k in range(1, n + 1)]
    ag = sum(max(x, 0.0) for x in ch) / n
    al = sum(max(-x, 0.0) for x in ch) / n
    st[n] = (ag, al)
    for i in range(n + 1, len(closes)):
        d = closes[i] - closes[i - 1]
        ag = (ag * (n - 1) + max(d, 0.0)) / n
        al = (al * (n - 1) + max(-d, 0.0)) / n
        st[i] = (ag, al)
    return st


def rsi_value(ag: float, al: float) -> float:
    if al == 0:
        return 50.0 if ag == 0 else 100.0
    return 100.0 - 100.0 / (1.0 + ag / al)


def proxy_rsi(state_prev: tuple[float, float], change: float, n: int = RSI_N) -> float:
    ag, al = state_prev
    return rsi_value((ag * (n - 1) + max(change, 0.0)) / n, (al * (n - 1) + max(-change, 0.0)) / n)


def proxy_sma(closes: list[float], i: int, x: float, n: int) -> float | None:
    if i - (n - 1) < 0:
        return None
    return (sum(closes[i - n + 1:i]) + x) / n


# ---- Engine -----------------------------------------------------------------
@dataclass
class Trade:
    entry_day: date
    entry_px: float
    exit_day: date
    exit_px: float
    reason: str
    entry_rsi: float
    entry_idx: int            # index into sessions-with-bars
    exit_idx: int
    gross: float = 0.0
    net: float = 0.0
    pieces: list = field(default_factory=list)   # (calendar date, gross daily return)


@dataclass
class Result:
    net: dict                 # calendar date -> net daily return
    gross: dict               # calendar date -> gross daily return
    trades: list
    held: dict                # calendar date -> 1 if a position is held at that close


def simulate(sessions: list[Session], calendar: list[date], *, entry_rsi: float = ENTRY_RSI,
             exit_sma: int = EXIT_SMA, cost_bps: float = 1.0, trend_sma: int | None = None,
             mode: str = "primary", first_day: date = EVAL_START) -> Result:
    """mode: 'primary' (decide on P, fill at C), 'U' (decide on C, fill at C),
    'D1' (decide on C, fill at the next session's open)."""
    c = cost_bps / 1e4
    closes = [s.c for s in sessions]
    states = wilder_states(closes)
    idx_of = {s.day: k for k, s in enumerate(sessions)}
    f = next(k for k, s in enumerate(sessions) if s.day >= first_day)
    last = len(sessions) - 1
    factor_net = {s.day: 1.0 for s in sessions}
    factor_gross = {s.day: 1.0 for s in sessions}
    held = {d: 0 for d in calendar}
    trades: list[Trade] = []
    pos: Trade | None = None
    pending: str | None = None     # D1 only: 'enter' or 'exit' at the next open
    pending_rsi = 0.0

    def mul(day, g, fee):
        factor_gross[day] *= g
        factor_net[day] *= g * fee

    for i in range(f, last + 1):
        s = sessions[i]
        prev_c = closes[i - 1]
        # 1. The session's P&L: D1 fills at this open, otherwise a held position.
        if mode == "D1" and pending is not None:
            if pending == "exit":
                mul(s.day, s.o / prev_c, 1 - c)
                pos.pieces.append((s.day, s.o / prev_c - 1))
                pos.exit_day, pos.exit_px, pos.exit_idx, pos.reason = s.day, s.o, i, "sma"
                trades.append(pos)
                pos = None
            else:
                pos = Trade(s.day, s.o, s.day, s.o, "", pending_rsi, i, i)
                mul(s.day, s.c / s.o, 1 - c)
                pos.pieces.append((s.day, s.c / s.o - 1))
            pending = None
        elif pos is not None:
            mul(s.day, s.c / prev_c, 1.0)
            pos.pieces.append((s.day, s.c / prev_c - 1))
        # 2. The decision, on P (primary) or on C (U, D1).
        x = s.c if mode in ("U", "D1") else s.p
        if pos is not None:
            sma = proxy_sma(closes, i, x, exit_sma)
            if sma is not None and x > sma:
                if mode == "D1":
                    if i < last:
                        pending = "exit"
                else:
                    mul(s.day, 1.0, 1 - c)
                    pos.exit_day, pos.exit_px, pos.exit_idx, pos.reason = s.day, s.c, i, "sma"
                    trades.append(pos)
                    pos = None
        else:  # flat at the decision; a session that exited above never reaches here
            r = proxy_rsi(states[i - 1], x - prev_c)
            ok = r < entry_rsi
            if ok and trend_sma is not None:
                t = proxy_sma(closes, i, x, trend_sma)
                ok = t is not None and x > t
            if ok:
                if mode == "D1":
                    if i < last:
                        pending, pending_rsi = "enter", r
                else:
                    pos = Trade(s.day, s.c, s.day, s.c, "", r, i, i)
                    mul(s.day, 1.0, 1 - c)
                    pos.pieces.append((s.day, 0.0))
        if pos is not None:
            held[s.day] = 1
    if pos is not None:
        s = sessions[last]
        mul(s.day, 1.0, 1 - c)
        pos.exit_day, pos.exit_px, pos.exit_idx, pos.reason = s.day, s.c, last, "end"
        trades.append(pos)
    for t in trades:
        t.gross = t.exit_px / t.entry_px - 1
        t.net = (1 - c) ** 2 * t.exit_px / t.entry_px - 1
    cal = [d for d in calendar if d >= first_day]
    net = {d: (factor_net[d] - 1 if d in idx_of else 0.0) for d in cal}
    gross = {d: (factor_gross[d] - 1 if d in idx_of else 0.0) for d in cal}
    held_cal, last_held = {}, 0
    for d in cal:
        if d in idx_of:
            last_held = held[d]
        held_cal[d] = last_held   # a session without bars carries the previous close's position
    return Result(net, gross, trades, held_cal)


# ---- Metrics ----------------------------------------------------------------
def sharpe(r) -> float | None:
    r = np.asarray(r, float)
    if len(r) < 2 or r.std(ddof=1) == 0:
        return None
    return float(r.mean() / r.std(ddof=1) * math.sqrt(252))


def skew(x) -> float | None:
    x = np.asarray(x, float)
    if len(x) < 3:
        return None
    m = x - x.mean()
    m2 = (m ** 2).mean()
    return None if m2 == 0 else float((m ** 3).mean() / m2 ** 1.5)


def window_days(res: Result, lo: date, hi: date) -> list[date]:
    return [d for d in res.net if lo <= d <= hi]


def metrics(res: Result, lo: date, hi: date) -> dict:
    days = window_days(res, lo, hi)
    r = np.array([res.net[d] for d in days])
    eq = np.cumprod(1 + r)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    tr = [t for t in res.trades if lo <= t.entry_day <= hi]
    nets = np.array([t.net for t in tr])
    wins, losses = nets[nets > 0], nets[nets <= 0]
    n = len(r)
    sd = r.std(ddof=1) if n > 1 else 0.0
    exit_days = {t.exit_day for t in tr}
    in_pos = [res.net[d] for d in days if res.held[d] or d in exit_days]
    return {
        "window": [str(days[0]), str(days[-1])] if days else None,
        "sessions": n,
        "total_return": float(eq[-1] - 1) if n else 0.0,
        "cagr": float(eq[-1] ** (252 / n) - 1) if n and eq[-1] > 0 else None,
        "ann_vol": float(sd * math.sqrt(252)),
        "sharpe": sharpe(r),
        "max_dd": float((eq / peak - 1).min()) if n else 0.0,
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if sd > 0 else None,
        "trades": len(tr),
        "win_rate": float((nets > 0).mean()) if len(tr) else None,
        "profit_factor": float(wins.sum() / abs(losses.sum())) if losses.sum() < 0 else None,
        "avg_net_bp": float(nets.mean() * 1e4) if len(tr) else None,
        "avg_win_bp": float(wins.mean() * 1e4) if len(wins) else None,
        "avg_loss_bp": float(losses.mean() * 1e4) if len(losses) else None,
        "trade_skew": skew(nets),
        "exposure": float(np.mean([res.held[d] for d in days])) if n else 0.0,
        "hold_sessions_median": float(np.median([t.exit_idx - t.entry_idx for t in tr])) if tr else None,
        "hold_sessions_mean": float(np.mean([t.exit_idx - t.entry_idx for t in tr])) if tr else None,
        "daily_skew_in_position": skew(in_pos),
    }


def bh_returns(sessions: list[Session], calendar: list[date], first_day: date = EVAL_START) -> dict:
    idx = {s.day: k for k, s in enumerate(sessions)}
    out = {}
    for d in calendar:
        if d < first_day:
            continue
        k = idx.get(d)
        out[d] = sessions[k].c / sessions[k - 1].c - 1 if k is not None else 0.0
    return out


def bh_metrics(bh: dict, lo: date, hi: date) -> dict:
    r = np.array([v for d, v in bh.items() if lo <= d <= hi])
    eq = np.cumprod(1 + r)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return {"total_return": float(eq[-1] - 1), "sharpe": sharpe(r), "max_dd": float((eq / peak - 1).min()),
            "ann_vol": float(r.std(ddof=1) * math.sqrt(252))}


# ---- Placebos and bootstrap -------------------------------------------------
def direction_placebo(res: Result, seed: int) -> dict:
    days = list(res.gross)
    pos = {d: k for k, d in enumerate(days)}
    base = np.array([res.gross[d] for d in days])
    owner = np.full(len(days), -1)
    for j, t in enumerate(res.trades):
        for d, _ in t.pieces:
            owner[pos[d]] = j
    actual = sharpe(base)
    rng = np.random.default_rng(seed)
    signs = rng.choice([-1.0, 1.0], size=(N_DIR, len(res.trades)))
    draws = []
    for k in range(N_DIR):
        s = np.where(owner >= 0, signs[k][np.maximum(owner, 0)], 1.0)
        draws.append(sharpe(base * s) or 0.0)
    draws = np.array(draws)
    return {"actual_gross_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)),
            "p": float((1 + (draws >= actual).sum()) / (N_DIR + 1)), "draws": draws.tolist()}


def timing_placebo(res: Result, bh: dict, seed: int) -> dict:
    days = list(res.gross)
    pos = {d: k for k, d in enumerate(days)}
    bhv = np.array([bh[d] for d in days])
    S = len(days)
    occ = [pos[t.exit_day] - pos[t.entry_day] + 1 for t in res.trades]
    n, K = len(occ), sum(occ)
    F = S - K
    rng = np.random.default_rng(seed)
    actual = sharpe([res.gross[d] for d in days])
    draws = []
    for _ in range(N_TIME):
        order = rng.permutation(n)
        bars = np.sort(rng.choice(F + n, size=n, replace=False))
        gaps = np.diff(np.concatenate([[-1], bars, [F + n]])) - 1   # n+1 gaps summing to F
        mask = np.zeros(S)
        at = gaps[0]
        for j, ti in enumerate(order):
            k = occ[ti]
            mask[at + 1: at + k] = 1.0        # entry session earns 0
            at += k + gaps[j + 1]
        draws.append(sharpe(bhv * mask) or 0.0)
    draws = np.array(draws)
    return {"actual_gross_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)),
            "p": float((1 + (draws >= actual).sum()) / (N_TIME + 1)), "draws": draws.tolist()}


def block_bootstrap(r, seed: int) -> dict:
    r = np.asarray(r, float)
    n = len(r)
    rng = np.random.default_rng(seed)
    out = []
    for _ in range(N_BOOT):
        starts = rng.integers(0, n, size=math.ceil(n / BLOCK))
        idx = (starts[:, None] + np.arange(BLOCK)[None, :]).ravel()[:n] % n
        out.append(sharpe(r[idx]) or 0.0)
    out = np.array(out)
    return {"sharpe_lo": float(np.percentile(out, 2.5)), "sharpe_hi": float(np.percentile(out, 97.5)),
            "share_le_0": float((out <= 0).mean())}


# ---- Self-test --------------------------------------------------------------
def selftest() -> None:
    from mdq import Bar
    c = 1e-4
    fails = []

    def check(name, cond):
        if not cond:
            fails.append(name)

    # 1. Proxy extraction: 15:49, 12:49 on an early close, and fallback.
    def minute_bars(day, closes_by_minute):
        start, _ = rth_window(day)
        return [Bar(start + (m - 570) * 60, v, v, v, v, 1.0) for m, v in closes_by_minute]
    d1, d2, d3 = date(2024, 7, 2), date(2024, 7, 3), date(2024, 7, 5)   # 07-03 is an early close
    mins = (minute_bars(d1, [(570, 10.0), (948, 11.0), (949, 12.0), (950, 13.0), (959, 14.0)])
            + minute_bars(d2, [(570, 20.0), (769, 21.0), (779, 22.0)])
            + minute_bars(d3, [(570, 30.0), (947, 31.0), (955, 32.0)]))
    ss = sessions_from_minutes(mins)
    check("proxy 15:49", ss[0].p == 12.0 and ss[0].c == 14.0 and ss[0].o == 10.0)
    check("proxy 12:49 early close", ss[1].p == 21.0 and ss[1].c == 22.0)
    check("proxy fallback", ss[2].p == 31.0 and ss[2].c == 32.0)

    # 2. RSI with AL = 0 and AG = AL = 0.
    check("rsi al=0", rsi_value(1.0, 0.0) == 100.0 and rsi_value(0.0, 0.0) == 50.0)
    check("rsi value", abs(proxy_rsi((1.0, 0.0), -20.0) - (100 - 100 / 1.05)) < 1e-12)

    # 3. Entry, hold, SMA exit. Closes 100..120 rise by 1 (indices 0..20).
    base = [date(2023, 1, 2)]
    cal = nyse_sessions(date(2023, 1, 3), date(2023, 3, 31))
    closes = [100.0 + k for k in range(21)] + [100.0, 101.0, 115.0, 116.0]
    sess = [Session(d, cl, cl, cl) for d, cl in zip(cal, closes)]
    first = cal[20]
    res = simulate(sess, cal[:len(sess)], cost_bps=1.0, first_day=first)
    t = res.trades
    check("one trade + no entry at idx20", len(t) == 1 and t[0].entry_day == cal[21])
    check("exit at idx23 sma", t[0].exit_day == cal[23] and t[0].exit_px == 115.0 and t[0].reason == "sma")
    check("trade net", abs(t[0].net - ((1 - c) ** 2 * 1.15 - 1)) < 1e-12)
    check("daily entry", abs(res.net[cal[21]] + c) < 1e-15)
    check("daily hold", abs(res.net[cal[22]] - 0.01) < 1e-12)
    check("daily exit", abs(res.net[cal[23]] - (115 / 101 * (1 - c) - 1)) < 1e-12)
    check("daily idx20 flat", res.net[cal[20]] == 0.0 and res.net[cal[24]] == 0.0)
    check("held flags", res.held[cal[21]] == 1 and res.held[cal[22]] == 1 and res.held[cal[23]] == 0)

    # 4. No re-entry on an exit session (threshold 101 = always oversold).
    res = simulate(sess, cal[:len(sess)], entry_rsi=101.0, first_day=first)
    ents = [tr.entry_day for tr in res.trades]
    exs = [tr.exit_day for tr in res.trades if tr.reason == "sma"]
    check("no re-entry on exit session", all(e not in exs for e in ents) and len(ents) >= 2)
    # entered idx20, exit when P > SMA5: idx21 P=100 < sma; idx22 101 < sma; idx23 115 > sma exit; idx24 re-enter
    check("re-entry next session", ents[:2] == [cal[20], cal[24]] and res.trades[1].reason == "end")

    # 5. End-of-sample close, and a session without bars while holding.
    closes5 = [100.0 + k for k in range(21)] + [100.0, 99.0, 98.0]
    days5 = cal[:24]
    sess5 = [Session(d, cl, cl, cl) for d, cl in zip(days5, closes5) if d != days5[22]]
    res = simulate(sess5, days5, first_day=first)
    tr = res.trades
    check("end exit", len(tr) == 1 and tr[0].reason == "end" and tr[0].exit_day == days5[23])
    check("no-bar session zero", res.net[days5[22]] == 0.0 and res.held[days5[22]] == 1)
    check("gap bridged", abs(res.net[days5[23]] - (98 / 100 * (1 - c) - 1)) < 1e-12)

    # 6. S1 trend filter: a falling series with a crash is below its 200-average.
    cal6 = nyse_sessions(date(2022, 1, 3), date(2023, 12, 29))
    cl6 = [300.0 - 0.5 * k for k in range(230)] + [150.0]
    s6 = [Session(d, v, v, v) for d, v in zip(cal6, cl6)]
    f6 = cal6[210]
    r_no = simulate(s6, cal6[:len(s6)], first_day=f6)
    r_s1 = simulate(s6, cal6[:len(s6)], trend_sma=TREND_SMA_S1, first_day=f6)
    check("S1 blocks below 200", len(r_s1.trades) == 0 and len(r_no.trades) >= 1)
    cl6b = [100.0 + 0.5 * k for k in range(230)] + [200.0]
    s6b = [Session(d, v, v, v) for d, v in zip(cal6, cl6b)]
    r_s1b = simulate(s6b, cal6[:len(s6b)], trend_sma=TREND_SMA_S1, first_day=f6)
    check("S1 allows above 200", len(r_s1b.trades) == 1 and r_s1b.trades[0].entry_day == cal6[230])

    # 7. D1: decide on close, fill at the next open.
    closes7 = [100.0 + k for k in range(21)] + [100.0, 101.0, 115.0, 116.0, 117.0]
    opens7 = [cl - 0.5 for cl in closes7]
    s7 = [Session(d, o, cl, cl) for d, o, cl in zip(cal, opens7, closes7)]
    r7 = simulate(s7, cal[:len(s7)], mode="D1", first_day=first)
    t7 = r7.trades
    check("D1 entry next open", t7[0].entry_day == cal[22] and t7[0].entry_px == 100.5)
    check("D1 exit next open", t7[0].exit_day == cal[24] and t7[0].exit_px == 115.5 and t7[0].reason == "sma")
    check("D1 entry return", abs(r7.net[cal[22]] - ((1 - c) * 101 / 100.5 - 1)) < 1e-12)
    check("D1 exit return", abs(r7.net[cal[24]] - (115.5 / 115 * (1 - c) - 1)) < 1e-12)
    check("D1 hold return", abs(r7.net[cal[23]] - (115 / 101 - 1)) < 1e-12)

    # 8. U: decide and fill on the close.
    rU = simulate(sess, cal[:len(sess)], mode="U", first_day=first)
    check("U entry idx21", rU.trades[0].entry_day == cal[21] and rU.trades[0].exit_day == cal[23])

    if fails:
        sys.exit("SELF-TEST FAILED: " + ", ".join(fails))
    print("self-test: all cases passed")


# ---- Run --------------------------------------------------------------------
def git_state() -> tuple[str, bool]:
    h = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT).stdout.strip())
    return h, dirty


def load(md: MarketData, sym: str) -> list[Session]:
    return sessions_from_minutes(md.bars(sym, "1m", DATA_START, END))


def trade_row(sym: str, t: Trade) -> dict:
    return {"symbol": sym, "side": "long", "entry_day": str(t.entry_day), "entry_px": t.entry_px,
            "exit_day": str(t.exit_day), "exit_px": t.exit_px, "entry_rsi": round(t.entry_rsi, 4),
            "hold_sessions": t.exit_idx - t.entry_idx, "gross": t.gross, "net": t.net, "exit_reason": t.reason,
            "window": "IS" if t.entry_day <= IS_END else "OOS"}


def three(res: Result) -> dict:
    return {"full": metrics(res, EVAL_START, END), "is": metrics(res, EVAL_START, IS_END),
            "oos": metrics(res, OOS_START, END)}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reason", required=True)
    args = ap.parse_args()
    selftest()
    rh = check_lock()
    head, dirty = git_state()
    calendar = nyse_sessions(DATA_START, END)
    with MarketData() as md:
        data = {s: load(md, s) for s in (PRIMARY, CROSS, REPORTED)}

    # Data checks from RULES.md.
    spy = data[PRIMARY]
    first_eval = spy[WARMUP].day
    assert first_eval == EVAL_START, first_eval
    assert date(2021, 12, 31) in calendar and all(s.day != date(2021, 12, 31) for s in spy)
    for sym, ss in data.items():
        assert len(ss) == 1254, (sym, len(ss))
        assert ss[WARMUP].day == EVAL_START, sym

    cal_eval = [d for d in calendar if d >= EVAL_START]
    base = COST_BPS[PRIMARY]
    prim = simulate(spy, calendar, cost_bps=base)
    bh = bh_returns(spy, calendar)
    out: dict = {"rules_sha256": rh, "git_head": head, "git_dirty": dirty,
                 "seeds": {"direction": SEED_DIR, "timing": SEED_TIME, "bootstrap": SEED_BOOT},
                 "calendar": {"first": str(cal_eval[0]), "last": str(cal_eval[-1]), "sessions": len(cal_eval),
                              "zero_days_no_bars": [str(d) for d in cal_eval if d not in {s.day for s in spy}]}}
    out["primary"] = three(prim)
    out["benchmark_bh"] = {"full": bh_metrics(bh, EVAL_START, END), "is": bh_metrics(bh, EVAL_START, IS_END),
                           "oos": bh_metrics(bh, OOS_START, END)}
    out["primary_gross"] = {"full_sharpe": sharpe(list(prim.gross.values())),
                            "avg_gross_trade_bp": float(np.mean([t.gross for t in prim.trades]) * 1e4)}

    # Costs.
    out["costs"] = {}
    for m in COST_SWEEP:
        r = simulate(spy, calendar, cost_bps=base * m)
        mm = three(r)
        out["costs"][str(m)] = {"cost_bps": base * m, "full_sharpe": mm["full"]["sharpe"],
                                "oos_sharpe": mm["oos"]["sharpe"], "full_return": mm["full"]["total_return"],
                                "oos_return": mm["oos"]["total_return"], "is_sharpe": mm["is"]["sharpe"]}
    # Fill delay and upper bound.
    out["delay_D1"] = three(simulate(spy, calendar, cost_bps=base, mode="D1"))
    out["upper_U"] = three(simulate(spy, calendar, cost_bps=base, mode="U"))

    # Placebos and bootstrap.
    dp = direction_placebo(prim, SEED_DIR)
    tp = timing_placebo(prim, bh, SEED_TIME)
    out["direction_placebo"] = {k: v for k, v in dp.items() if k != "draws"}
    out["timing_placebo"] = {k: v for k, v in tp.items() if k != "draws"}
    np.save(HERE / "placebo_direction.npy", np.array(dp["draws"]))
    np.save(HERE / "placebo_timing.npy", np.array(tp["draws"]))
    out["bootstrap"] = block_bootstrap([prim.net[d] for d in cal_eval], SEED_BOOT)

    # Grid.
    grid = []
    for er in GRID_RSI:
        for sm in GRID_SMA:
            r = simulate(spy, calendar, entry_rsi=er, exit_sma=sm, cost_bps=base)
            mi, mo = metrics(r, EVAL_START, IS_END), metrics(r, OOS_START, END)
            grid.append({"entry_rsi": er, "exit_sma": sm, "is_sharpe": mi["sharpe"], "oos_sharpe": mo["sharpe"],
                         "is_trades": mi["trades"], "oos_trades": mo["trades"],
                         "is_win_rate": mi["win_rate"], "is_trade_skew": mi["trade_skew"]})
    out["grid"] = grid
    pos_cells = sum(1 for g in grid if g["is_sharpe"] is not None and g["is_sharpe"] > 0)
    out["grid_positive_is"] = pos_cells

    # Cross-market.
    cross = {}
    for sym in (CROSS, REPORTED):
        r = simulate(data[sym], calendar, cost_bps=COST_BPS[sym])
        b = bh_returns(data[sym], calendar)
        cross[sym] = {**three(r), "bh": {"full": bh_metrics(b, EVAL_START, END), "oos": bh_metrics(b, OOS_START, END)},
                      "corr_with_spy": float(np.corrcoef([r.net[d] for d in cal_eval], [prim.net[d] for d in cal_eval])[0, 1])}
        cross[sym]["_trades"] = [trade_row(sym, t) for t in r.trades]
    out["cross"] = {k: {kk: vv for kk, vv in v.items() if kk != "_trades"} for k, v in cross.items()}

    # Secondary S1.
    s1 = simulate(spy, calendar, cost_bps=base, trend_sma=TREND_SMA_S1)
    s1_2x = simulate(spy, calendar, cost_bps=2 * base, trend_sma=TREND_SMA_S1)
    s1_q = simulate(data[CROSS], calendar, cost_bps=COST_BPS[CROSS], trend_sma=TREND_SMA_S1)
    out["s1"] = {**three(s1), "full_return_2x": metrics(s1_2x, EVAL_START, END)["total_return"],
                 "qqq_oos_sharpe": metrics(s1_q, OOS_START, END)["sharpe"],
                 "direction_placebo": {k: v for k, v in direction_placebo(s1, SEED_DIR).items() if k != "draws"},
                 "timing_placebo": {k: v for k, v in timing_placebo(s1, bh, SEED_TIME).items() if k != "draws"}}

    # Breakdowns and predictions.
    closes = [s.c for s in spy]
    rets = [None] + [closes[k] / closes[k - 1] - 1 for k in range(1, len(closes))]
    tr = prim.trades
    for t in tr:
        window = rets[t.entry_idx - 20:t.entry_idx]
        t.entry_vol = float(np.std(window, ddof=1))
    year = {}
    for d in cal_eval:
        year.setdefault(d.year, []).append(d)
    out["by_year"] = []
    for y, ds in sorted(year.items()):
        r = np.array([prim.net[d] for d in ds])
        b = np.array([bh[d] for d in ds])
        eq = np.cumprod(1 + r)
        pk = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
        ty = [t for t in tr if t.entry_day.year == y]
        out["by_year"].append({"year": y, "sessions": len(ds), "return": float(eq[-1] - 1), "sharpe": sharpe(r),
                               "max_dd": float((eq / pk - 1).min()), "bh_return": float(np.prod(1 + b) - 1),
                               "trades": len(ty), "win_rate": float(np.mean([t.net > 0 for t in ty])) if ty else None})
    out["by_exit_reason"] = {rsn: {"trades": len(g), "avg_net_bp": float(np.mean([t.net for t in g]) * 1e4)}
                             for rsn in ("sma", "end") if (g := [t for t in tr if t.reason == rsn])}
    lo_rsi = [t for t in tr if t.entry_rsi < 5]
    hi_rsi = [t for t in tr if 5 <= t.entry_rsi < 10]
    out["by_entry_rsi"] = {"lt5": {"trades": len(lo_rsi), "avg_gross_bp": float(np.mean([t.gross for t in lo_rsi]) * 1e4) if lo_rsi else None},
                           "5to10": {"trades": len(hi_rsi), "avg_gross_bp": float(np.mean([t.gross for t in hi_rsi]) * 1e4) if hi_rsi else None}}
    med_vol = float(np.median([t.entry_vol for t in tr]))
    hv = [t for t in tr if t.entry_vol > med_vol]
    lv = [t for t in tr if t.entry_vol <= med_vol]
    out["by_entry_vol"] = {"median_vol": med_vol,
                           "high": {"trades": len(hv), "avg_gross_bp": float(np.mean([t.gross for t in hv]) * 1e4)},
                           "low": {"trades": len(lv), "avg_gross_bp": float(np.mean([t.gross for t in lv]) * 1e4)}}
    hold_hist = {}
    for t in tr:
        h = t.exit_idx - t.entry_idx
        hold_hist.setdefault(h, []).append(t.net)
    out["by_hold"] = {str(h): {"trades": len(v), "avg_net_bp": float(np.mean(v) * 1e4), "win_rate": float(np.mean([x > 0 for x in v]))}
                      for h, v in sorted(hold_hist.items())}
    days_bars = [d for d in cal_eval if d in {s.day for s in spy}]
    mv = np.array([bh[d] for d in days_bars])
    qs = np.quantile(mv, [0.2, 0.4, 0.6, 0.8])
    qbins = np.digitize(mv, qs)
    out["by_move_quintile"] = [{"quintile": q + 1, "sessions": int((qbins == q).sum()),
                                "spy_avg_bp": float(mv[qbins == q].mean() * 1e4),
                                "strategy_avg_bp": float(np.mean([prim.net[d] for d, b in zip(days_bars, qbins) if b == q]) * 1e4)}
                               for q in range(5)]
    nets = np.array([t.net for t in tr])
    wins, losses = nets[nets > 0], nets[nets <= 0]
    worst_n = math.ceil(0.1 * len(nets))
    worst_share = float(np.sort(nets)[:worst_n].sum() / losses.sum()) if losses.sum() < 0 else None
    first_day = [t.pieces[1][1] for t in tr if len(t.pieces) > 1]
    later = [p[1] for t in tr for p in t.pieces[2:]]
    pred = {
        "p1_loss_to_win_ratio": float(abs(losses.mean()) / wins.mean()) if len(losses) and len(wins) else None,
        "p2_lt5_minus_5to10_bp": (out["by_entry_rsi"]["lt5"]["avg_gross_bp"] - out["by_entry_rsi"]["5to10"]["avg_gross_bp"])
        if lo_rsi and hi_rsi else None,
        "p3_high_minus_low_vol_bp": out["by_entry_vol"]["high"]["avg_gross_bp"] - out["by_entry_vol"]["low"]["avg_gross_bp"],
        "p4_worst10pct_share_of_losses": worst_share, "p4_worst_n": worst_n,
        "p5_first_day_avg_bp": float(np.mean(first_day) * 1e4), "p5_later_day_avg_bp": float(np.mean(later) * 1e4),
        "p5_first_days": len(first_day), "p5_later_days": len(later),
    }
    pred["scores"] = {
        "p1": "consistent" if pred["p1_loss_to_win_ratio"] is not None and pred["p1_loss_to_win_ratio"] >= 2 else "not consistent",
        "p2": "not testable" if pred["p2_lt5_minus_5to10_bp"] is None else ("consistent" if pred["p2_lt5_minus_5to10_bp"] > 0 else "not consistent"),
        "p3": "consistent" if pred["p3_high_minus_low_vol_bp"] > 0 else "not consistent",
        "p4": "not testable" if worst_share is None else ("consistent" if worst_share >= 0.5 else "not consistent"),
        "p5": "consistent" if pred["p5_first_day_avg_bp"] > 0 and pred["p5_first_day_avg_bp"] > pred["p5_later_day_avg_bp"] else "not consistent",
    }
    out["predictions"] = pred

    # Acceptance.
    P = out["primary"]
    oos_pf = P["oos"]["profit_factor"]
    acc = {
        "1_oos_sharpe": {"required": ">= 0.5", "actual": P["oos"]["sharpe"], "pass": (P["oos"]["sharpe"] or -9) >= 0.5},
        "1_oos_pf": {"required": ">= 1.10", "actual": oos_pf, "pass": oos_pf is not None and oos_pf >= 1.10},
        "2_direction_p": {"required": "<= 0.05", "actual": dp["p"], "pass": dp["p"] <= 0.05},
        "2_timing_p": {"required": "<= 0.05", "actual": tp["p"], "pass": tp["p"] <= 0.05},
        "3_is_sharpe": {"required": "> 0", "actual": P["is"]["sharpe"], "pass": (P["is"]["sharpe"] or -9) > 0},
        "3_grid": {"required": ">= 9 of 15", "actual": pos_cells, "pass": pos_cells >= 9},
        "4_full_return_2x": {"required": "> 0", "actual": out["costs"]["2.0"]["full_return"], "pass": out["costs"]["2.0"]["full_return"] > 0},
        "5_qqq_oos_sharpe": {"required": "> 0", "actual": out["cross"][CROSS]["oos"]["sharpe"], "pass": (out["cross"][CROSS]["oos"]["sharpe"] or -9) > 0},
        "6_oos_trades": {"required": f">= {MIN_OOS_TRADES}", "actual": P["oos"]["trades"], "pass": P["oos"]["trades"] >= MIN_OOS_TRADES},
        "7_win_rate": {"required": ">= 0.60", "actual": P["full"]["win_rate"], "pass": (P["full"]["win_rate"] or 0) >= MIN_WIN_RATE},
        "7_trade_skew": {"required": "< 0", "actual": P["full"]["trade_skew"], "pass": P["full"]["trade_skew"] is not None and P["full"]["trade_skew"] < 0},
    }
    if not acc["6_oos_trades"]["pass"]:
        status = "Inconclusive"
    elif all(v["pass"] for v in acc.values()):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    out["acceptance"] = acc
    out["status"] = status
    S = out["s1"]
    s1_acc = {
        "1_oos_sharpe": (S["oos"]["sharpe"] or -9) >= 0.5,
        "1_oos_pf": S["oos"]["profit_factor"] is not None and S["oos"]["profit_factor"] >= 1.10,
        "2_direction_p": S["direction_placebo"]["p"] <= 0.05,
        "2_timing_p": S["timing_placebo"]["p"] <= 0.05,
        "4_full_return_2x": S["full_return_2x"] > 0,
        "5_qqq_oos_sharpe": (S["qqq_oos_sharpe"] or -9) > 0,
        "7_win_rate": (S["full"]["win_rate"] or 0) >= MIN_WIN_RATE,
        "7_trade_skew": S["full"]["trade_skew"] is not None and S["full"]["trade_skew"] < 0,
        "6_oos_trades": S["oos"]["trades"] >= MIN_OOS_TRADES_S1,
    }
    out["s1_acceptance"] = s1_acc
    out["s1_status"] = ("Inconclusive" if not s1_acc["6_oos_trades"] else
                        "Passes its lines (not promotable if the primary fails)" if all(s1_acc.values()) else "Rejected")

    # Write outputs.
    (HERE / "results.json").write_text(json.dumps(out, indent=2, default=str))
    with open(HERE / "daily.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["date", "window", "strategy_net", "strategy_gross", "held_at_close", "spy_bh", "qqq_net", "s1_net"])
        qn = simulate(data[CROSS], calendar, cost_bps=COST_BPS[CROSS]).net
        for d in cal_eval:
            w.writerow([d, "IS" if d <= IS_END else "OOS", f"{prim.net[d]:.10f}", f"{prim.gross[d]:.10f}",
                        prim.held[d], f"{bh[d]:.10f}", f"{qn[d]:.10f}", f"{s1.net[d]:.10f}"])
    rows = [trade_row(PRIMARY, t) for t in tr] + cross[CROSS]["_trades"] + cross[REPORTED]["_trades"]
    with open(HERE / "trades.csv", "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    fmt = lambda v: "n/a" if v is None else f"{v:.3f}"
    entry = (f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n\n"
             f"- Reason: {args.reason}\n- Rules sha256: `{rh}`\n- Git HEAD: `{head}` (dirty: {dirty})\n"
             f"- SPY Sharpe full / IS / OOS: {fmt(P['full']['sharpe'])} / {fmt(P['is']['sharpe'])} / {fmt(P['oos']['sharpe'])}\n"
             f"- SPY total return full / IS / OOS: {fmt(P['full']['total_return'])} / {fmt(P['is']['total_return'])} / {fmt(P['oos']['total_return'])}\n"
             f"- Trades full / OOS: {P['full']['trades']} / {P['oos']['trades']}; win rate {fmt(P['full']['win_rate'])}; trade skew {fmt(P['full']['trade_skew'])}\n"
             f"- Status: {status}\n")
    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nOne entry per store run, appended by `backtest.py` (and `verify.py`). Never edited.\n")
    with open(log, "a") as fh:
        fh.write(entry)
    print(entry)


if __name__ == "__main__":
    main()
