# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered runs for research/micro-futures-trend (see RULES.md).

Refuses to run unless RULES.md matches RULES.lock. Runs the synthetic self-test
first and aborts on any failure. Writes results.json, daily.csv, trades.csv, and
appends one entry to RUNLOG.md.

    python research/micro-futures-trend/research/backtest.py --reason "first run"
"""
import argparse
import csv
import hashlib
import json
import math
import subprocess
from dataclasses import dataclass, field
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
RAW = HERE / "data" / "raw"

# ---- constants mirroring RULES.md -------------------------------------------------
LOOKBACKS = (21, 63, 252)
EWMA_COM = 60
EWMA_SEED = 60
SIGMA_P = 0.20
IDM = 2.0
E0 = 100_000.0
MIN_BARS = 253
IS_END = "2024-09-30"
OOS_START = "2024-10-01"
SEED = 20260926
N_PLACEBO = 2000
N_BOOT = 2000
BOOT_BLOCK = 20
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
GRID_SCALES = (0.5, 0.75, 1.0)
GRID_REBAL = ("weekly", "monthly")
ROLL_EXTRA_RATIO = 3.0
CLASS_WEIGHT = {"equity": 1 / 12, "rates": 1 / 12, "commodities": 1 / 9}
QUARTERLY = {3, 6, 9, 12}


@dataclass(frozen=True)
class Spec:
    cls: str
    micro: str
    mult: float | None      # $ per price point (equity, commodities)
    notional: float | None  # fixed notional-equivalent (rates)
    cost: float             # $ per contract per side
    roll_months: frozenset  # expected roll months for the close-to-close roll detector


SPECS = {
    "ES=F": Spec("equity", "MES", 5.0, None, 2.25, frozenset(QUARTERLY)),
    "NQ=F": Spec("equity", "MNQ", 2.0, None, 1.50, frozenset(QUARTERLY)),
    "RTY=F": Spec("equity", "M2K", 5.0, None, 1.50, frozenset(QUARTERLY)),
    "YM=F": Spec("equity", "MYM", 0.5, None, 1.50, frozenset(QUARTERLY)),
    "ZT=F": Spec("rates", "2YY", None, 100_000 / 1.9, 2.00, frozenset(QUARTERLY)),
    "ZF=F": Spec("rates", "5YY", None, 100_000 / 4.2, 2.00, frozenset(QUARTERLY)),
    "ZN=F": Spec("rates", "10Y", None, 100_000 / 6.3, 2.00, frozenset(QUARTERLY)),
    "ZB=F": Spec("rates", "30Y", None, 100_000 / 11.5, 2.00, frozenset(QUARTERLY)),
    "GC=F": Spec("commodities", "MGC", 10.0, None, 2.00, frozenset({1, 3, 5, 7, 11})),
    "HG=F": Spec("commodities", "MHG", 2500.0, None, 2.25, frozenset({2, 4, 6, 8, 11})),
    "CL=F": Spec("commodities", "MCL", 100.0, None, 2.00, frozenset(range(1, 13))),
}
MARKETS = list(SPECS)


@dataclass(frozen=True)
class Bar:
    date: str
    open: float
    close: float
    volume: float


@dataclass
class Params:
    lookbacks: tuple = LOOKBACKS
    ewma_com: float = EWMA_COM
    ewma_seed: int = EWMA_SEED
    sigma_p: float = SIGMA_P
    idm: float = IDM
    e0: float = E0
    min_bars: int = MIN_BARS
    rebalance: str = "monthly"
    integer: bool = True
    cost_mult: float = 1.0
    fill_delay: int = 1
    basis: str = "oc"               # "oc" open-to-close, "cc" close-to-close with roll days
    long_only: bool = False         # B2: s == +1
    weights: dict = field(default_factory=dict)
    roll_cost_dates: dict | None = None   # market -> set of raw dates; None = RULES calendar
    first_rebalance: str | None = None    # force the first rebalance date (grid cells)


# ---- data ----------------------------------------------------------------------------

def valid(r: dict) -> bool:
    return all((r.get(f) or 0) > 0 for f in ("open", "high", "low", "close", "volume"))


def load_market(sym: str) -> tuple[list[Bar], int]:
    body = json.loads((RAW / (sym.replace("=", "_") + ".json")).read_text())["body"]
    rows = [body[k] for k in sorted(body, key=int)]
    dates = [r["date"] for r in rows]
    if len(rows) != 1260 or len(set(dates)) != len(dates):
        raise SystemExit(f"data check failed: {sym} has {len(rows)} bars, {len(set(dates))} distinct dates")
    bars = [Bar(r["date"], float(r["open"]), float(r["close"]), float(r["volume"])) for r in rows if valid(r)]
    return bars, len(rows)


# ---- calendar helpers ---------------------------------------------------------------

def third_friday(y: int, m: int) -> str:
    d = date(y, m, 1)
    first_fri = d + timedelta(days=(4 - d.weekday()) % 7)
    return (first_fri + timedelta(days=14)).isoformat()


def next_session(sessions: list[str], d: str) -> str | None:
    for s in sessions:
        if s >= d:
            return s
    return None


def last_sessions_by(sessions: list[str], key) -> set[str]:
    """Sessions followed by a later session with a different key (so the data's final,
    possibly partial, period never counts)."""
    out = set()
    for a, b in zip(sessions, sessions[1:]):
        if key(a) != key(b):
            out.add(a)
    return out


def month_key(d: str) -> str:
    return d[:7]


def week_key(d: str) -> tuple:
    return date.fromisoformat(d).isocalendar()[:2]


def roll_cost_calendar(sessions: list[str], markets: list[str]) -> dict[str, set[str]]:
    """RULES.md roll-cost dates, moved to book sessions."""
    months = sorted({s[:7] for s in sessions})
    # last book session of each complete month (the data's final, partial month has none)
    month_last = {s[:7]: s for s in last_sessions_by(sessions, month_key)}
    out = {}
    for sym in markets:
        cls = SPECS[sym].cls
        dates = set()
        for ym in months:
            y, m = int(ym[:4]), int(ym[5:])
            raw = None
            if cls == "equity" and m in QUARTERLY:
                raw = third_friday(y, m)
            elif sym == "CL=F":
                raw = f"{ym}-20"
            elif sym == "GC=F" and m in {1, 3, 5, 7, 11}:
                raw = "LAST"
            elif sym == "HG=F" and m in {2, 4, 6, 8, 11}:
                raw = "LAST"
            elif cls == "rates":
                raw = "LAST"
            if raw is None:
                continue
            if raw == "LAST":
                s = month_last.get(ym)
            else:
                s = next_session(sessions, raw)
            if s is not None:
                dates.add(s)
        out[sym] = dates
    return out


def detected_roll_days(bars: list[Bar], roll_months: frozenset) -> set[str]:
    """rollcheck.py rule: max volume ratio in each expected roll month, plus ratio >= 3."""
    ratio = {bars[i].date: bars[i].volume / bars[i - 1].volume for i in range(1, len(bars))}
    best: dict[str, tuple[float, str]] = {}
    for d, x in ratio.items():
        if int(d[5:7]) in roll_months and (d[:7] not in best or x > best[d[:7]][0]):
            best[d[:7]] = (x, d)
    days = {d for _, d in best.values()}
    days |= {d for d, x in ratio.items() if x >= ROLL_EXTRA_RATIO}
    return days


# ---- engine --------------------------------------------------------------------------

def round_half_away(x: float) -> int:
    return int(math.copysign(math.floor(abs(x) + 0.5), x))


def sgn(x: float) -> int:
    return int(x > 0) - int(x < 0)


def market_series(bars: list[Bar], basis: str, roll_days: set[str]):
    """Return per-bar r, index I, EWMA variance (per bar; nan before the seed)."""
    n = len(bars)
    r = np.empty(n)
    for j, b in enumerate(bars):
        if basis == "oc" or j == 0 or b.date in roll_days:
            r[j] = b.close / b.open - 1
        else:
            r[j] = b.close / bars[j - 1].close - 1
    idx = np.cumprod(1 + r)
    return r, idx


def ewma_var(r: np.ndarray, com: float, seed: int) -> np.ndarray:
    lam = com / (com + 1)
    v = np.full(len(r), np.nan)
    if len(r) >= seed:
        v[seed - 1] = np.mean(r[:seed] ** 2)
        for t in range(seed, len(r)):
            v[t] = lam * v[t - 1] + (1 - lam) * r[t] ** 2
    return v


@dataclass
class Trade:
    market: str
    side: int
    entry_date: str
    entry_price: float
    entry_contracts: float
    entry_value: float
    exit_date: str = ""
    exit_price: float = 0.0
    exit_reason: str = ""
    gross: float = 0.0
    costs: float = 0.0
    max_contracts: float = 0.0
    daily: dict = field(default_factory=dict)   # session index -> gross P&L

    @property
    def net(self) -> float:
        return self.gross - self.costs


def run(data: dict[str, list[Bar]], specs: dict[str, Spec], p: Params) -> dict:
    markets = list(data)
    sessions = sorted({b.date for bars in data.values() for b in bars})
    s_index = {d: t for t, d in enumerate(sessions)}
    weights = p.weights or {m: CLASS_WEIGHT[specs[m].cls] for m in markets}
    roll_cost = p.roll_cost_dates if p.roll_cost_dates is not None else roll_cost_calendar(sessions, markets)

    series = {}
    for m in markets:
        rd = detected_roll_days(data[m], specs[m].roll_months) if p.basis == "cc" else set()
        r, idx = market_series(data[m], p.basis, rd)
        series[m] = dict(r=r, idx=idx, var=ewma_var(r, p.ewma_com, p.ewma_seed), roll=rd,
                         at={b.date: j for j, b in enumerate(data[m])})

    key = month_key if p.rebalance == "monthly" else week_key
    schedule = last_sessions_by(sessions, key)
    month_ends = sorted(last_sessions_by(sessions, month_key))
    if p.first_rebalance is None:
        first = None
        for d in month_ends:
            if all(sum(1 for b in data[m] if b.date <= d) >= p.min_bars for m in markets):
                first = d
                break
        if first is None:
            raise SystemExit("no rebalance date with enough warm-up")
    else:
        first = p.first_rebalance
    rebal = sorted(d for d in schedule if d > first) + [first]
    rebal = set(rebal)

    nS, nM = len(sessions), len(markets)
    gross = np.zeros((nM, nS))
    cost = np.zeros((nM, nS))
    held = np.zeros((nM, nS))
    equity = np.zeros(nS)
    pos = {m: 0.0 for m in markets}
    last_j = {m: -1 for m in markets}
    pending: dict[str, tuple[int, float]] = {}
    open_tr: dict[str, Trade] = {}
    trades: list[Trade] = []
    rebal_log = []
    E = p.e0

    def value_at(m: str, j: int, price: float) -> float:
        sp = specs[m]
        return sp.notional if sp.notional is not None else sp.mult * price

    def leg(m: str, n: float, a: float, b: float) -> float:
        sp = specs[m]
        if n == 0:
            return 0.0
        return n * sp.notional * (b / a - 1) if sp.notional is not None else n * sp.mult * (b - a)

    for t, d in enumerate(sessions):
        day = 0.0
        for i, m in enumerate(markets):
            j = series[m]["at"].get(d)
            c_side = specs[m].cost * p.cost_mult
            if j is not None:
                bar = data[m][j]
                n_old = pos[m]
                n_new = n_old
                fill = pending.get(m)
                if fill is not None and fill[0] == j:
                    n_new = fill[1]
                    del pending[m]
                    dn = abs(n_new - n_old)
                    fc = dn * c_side
                    cost[i, t] += fc
                    if n_old != 0 and (n_new == 0 or sgn(n_new) != sgn(n_old)):
                        tr = open_tr.pop(m)
                        share = abs(n_old) / dn if dn else 1.0
                        tr.costs += fc * share
                        fc_rest = fc * (1 - share)
                        tr.exit_date, tr.exit_price = d, bar.open
                        tr.exit_reason = "flat" if n_new == 0 else "flip"
                        tr._closing_leg = True
                        trades.append(tr)
                        closed = tr
                    else:
                        closed = None
                        fc_rest = fc
                    if n_new != 0 and (n_old == 0 or sgn(n_new) != sgn(n_old)):
                        tr = Trade(m, sgn(n_new), d, bar.open, abs(n_new), value_at(m, j, bar.open))
                        tr.costs += fc_rest
                        open_tr[m] = tr
                    elif n_new != 0:
                        open_tr[m].costs += fc_rest
                else:
                    closed = None
                # P&L legs
                gap = 0.0
                if p.basis == "cc" and j > 0 and bar.date not in series[m]["roll"]:
                    gap = leg(m, n_old, data[m][j - 1].close, bar.open)
                body = leg(m, n_new, bar.open, bar.close)
                gross[i, t] += gap + body
                if gap:
                    tgt = closed if closed is not None else open_tr.get(m)
                    tgt.gross += gap
                    tgt.daily[t] = tgt.daily.get(t, 0.0) + gap
                if body:
                    tr = open_tr[m]
                    tr.gross += body
                    tr.daily[t] = tr.daily.get(t, 0.0) + body
                pos[m] = n_new
                last_j[m] = j
                if m in open_tr:
                    open_tr[m].max_contracts = max(open_tr[m].max_contracts, abs(n_new))
            held[i, t] = pos[m]
            if d in roll_cost[m] and pos[m] != 0:
                rc = 2 * abs(pos[m]) * c_side
                cost[i, t] += rc
                open_tr[m].costs += rc
            day += gross[i, t] - cost[i, t]
        E += day
        equity[t] = E

        if d in rebal:
            for m in markets:
                j = last_j[m]
                if j < 0 or j + 1 < p.min_bars and p.first_rebalance is None:
                    continue
                s = series[m]
                if p.long_only:
                    sig = 1.0
                else:
                    sig = sum(sgn(s["idx"][j] / s["idx"][j - k] - 1) for k in p.lookbacks) / len(p.lookbacks)
                sigma = math.sqrt(252 * s["var"][j])
                V = value_at(m, j, data[m][j].close)
                x = sig * E * p.sigma_p * weights[m] * p.idm / (V * sigma)
                n = float(round_half_away(x)) if p.integer else x
                rebal_log.append(dict(date=d, market=m, signal=sig, sigma=sigma, value=V, x=x, n=n))
                fj = j + p.fill_delay
                if fj < len(data[m]):
                    if n != pos[m] or m in pending:
                        pending[m] = (fj, n)
                else:
                    pending.pop(m, None)

    for m, tr in open_tr.items():
        last = data[m][last_j[m]]
        tr.exit_date, tr.exit_price, tr.exit_reason = last.date, last.close, "end"
        trades.append(tr)
    trades.sort(key=lambda tr: (tr.entry_date, tr.market))
    start = s_index[first] + 1
    return dict(sessions=sessions, equity=equity, gross=gross, cost=cost, held=held,
                trades=trades, markets=markets, start=start, first=first, rebal=rebal_log,
                e0=p.e0, roll_days={m: sorted(series[m]["roll"]) for m in markets})


# ---- statistics --------------------------------------------------------------------

def daily_returns(res: dict) -> np.ndarray:
    eq = res["equity"]
    prev = np.concatenate([[res["e0"]], eq[:-1]])
    return (eq - prev) / prev


def prev_equity(res: dict) -> np.ndarray:
    eq = res["equity"]
    return np.concatenate([[res["e0"]], eq[:-1]])


def sharpe(x: np.ndarray) -> float:
    if len(x) < 2 or np.std(x, ddof=1) == 0:
        return float("nan")
    return float(np.mean(x) / np.std(x, ddof=1) * math.sqrt(252))


def max_dd(x: np.ndarray) -> float:
    curve = np.cumprod(1 + x)
    peak = np.maximum.accumulate(np.concatenate([[1.0], curve]))[1:]
    return float(np.min(curve / peak - 1)) if len(x) else float("nan")


def window_mask(res: dict, which: str) -> np.ndarray:
    ses = np.array(res["sessions"])
    m = np.arange(len(ses)) >= res["start"]
    if which == "is":
        m &= ses <= IS_END
    elif which == "oos":
        m &= ses >= OOS_START
    return m


def trade_window(tr: Trade, which: str) -> bool:
    if which == "is":
        return tr.entry_date <= IS_END
    if which == "oos":
        return tr.entry_date >= OOS_START
    return True


def metrics(res: dict, rets: np.ndarray, which: str, trades: list | None = None) -> dict:
    m = window_mask(res, which)
    x = rets[m]
    ses = np.array(res["sessions"])[m]
    n = len(x)
    tr_ = x.sum()
    total = float(np.prod(1 + x) - 1)
    out = dict(window=f"{ses[0]} -> {ses[-1]}", sessions=n, total_return=total,
               cagr=float((1 + total) ** (252 / n) - 1), ann_vol=float(np.std(x, ddof=1) * math.sqrt(252)),
               sharpe=sharpe(x), max_dd=max_dd(x),
               t_stat=float(np.mean(x) / (np.std(x, ddof=1) / math.sqrt(n))))
    if trades is not None:
        tw = [t for t in trades if trade_window(t, which)]
        wins = [t.net for t in tw if t.net > 0]
        losses = [t.net for t in tw if t.net <= 0]
        held = res["held"][:, m]
        out.update(trades=len(tw), win_rate=len(wins) / len(tw) if tw else float("nan"),
                   profit_factor=(sum(wins) / abs(sum(losses))) if losses and sum(losses) != 0 else float("inf"),
                   avg_net_trade_bp=float(np.mean([t.net / (t.entry_contracts * t.entry_value) * 1e4 for t in tw]))
                   if tw else float("nan"),
                   avg_win=float(np.mean(wins)) if wins else float("nan"),
                   avg_loss=float(np.mean(losses)) if losses else float("nan"),
                   exposure=float(np.mean(np.any(held != 0, axis=0))))
    return out


def all_metrics(res, rets, trades=None):
    return {w: metrics(res, rets, w, trades) for w in ("full", "is", "oos")}


def placebo(res: dict, rng: np.random.Generator) -> dict:
    m = window_mask(res, "full")
    idx = np.where(m)[0]
    trades = res["trades"]
    G = np.zeros((len(trades), len(idx)))
    pos_of = {t: k for k, t in enumerate(idx)}
    for a, tr in enumerate(trades):
        for t, v in tr.daily.items():
            if t in pos_of:
                G[a, pos_of[t]] += v
    denom = prev_equity(res)[idx]
    actual = sharpe(G.sum(0) / denom)
    signs = rng.choice([-1.0, 1.0], size=(N_PLACEBO, len(trades)))
    draws = (signs @ G) / denom
    null = np.array([sharpe(r) for r in draws])
    p = (1 + int(np.sum(null >= actual))) / (N_PLACEBO + 1)
    return dict(actual_gross_sharpe=actual, null_mean=float(np.mean(null)), null_sd=float(np.std(null)),
                null_p95=float(np.percentile(null, 95)), p=p, draws=N_PLACEBO, null=null.tolist())


def bootstrap(x: np.ndarray, rng: np.random.Generator) -> dict:
    n = len(x)
    nb = math.ceil(n / BOOT_BLOCK)
    vals = []
    for _ in range(N_BOOT):
        starts = rng.integers(0, n, nb)
        ix = (starts[:, None] + np.arange(BOOT_BLOCK)[None, :]).ravel()[:n] % n
        vals.append(sharpe(x[ix]))
    return dict(lo=float(np.percentile(vals, 2.5)), hi=float(np.percentile(vals, 97.5)),
                median=float(np.median(vals)), draws=N_BOOT, block=BOOT_BLOCK)


def monthly(res: dict, rets: np.ndarray) -> dict[str, float]:
    m = window_mask(res, "full")
    out: dict[str, float] = {}
    for d, x in zip(np.array(res["sessions"])[m], rets[m]):
        out[d[:7]] = (1 + out.get(d[:7], 0.0)) * (1 + x) - 1
    return out


# ---- self-test -----------------------------------------------------------------------

def self_test() -> None:
    fails = []

    def check(name, got, want, tol=1e-9):
        ok = (abs(got - want) <= tol) if isinstance(want, float) else got == want
        if not ok:
            fails.append(f"{name}: got {got!r}, want {want!r}")

    # units
    for x, want in ((2.5, 3), (-2.5, -3), (0.49, 0), (-0.5, -1), (1.49, 1), (0.0, 0)):
        check(f"round_half_away({x})", round_half_away(x), want)
    v = ewma_var(np.array([0.1, 0.3, 0.2]), com=1.0, seed=2)   # lambda = 0.5
    check("ewma seed", float(v[1]), 0.05)
    check("ewma step", float(v[2]), 0.045)
    check("third friday 2025-06", third_friday(2025, 6), "2025-06-20")
    check("third friday 2024-03", third_friday(2024, 3), "2024-03-15")
    ses = ["2024-01-30", "2024-01-31", "2024-02-01", "2024-02-29", "2024-03-01"]
    check("month ends", sorted(last_sessions_by(ses, month_key)), ["2024-01-31", "2024-02-29"])
    check("signal thirds", sum(sgn(v) for v in (0.02, -0.01, 0.03)) / 3, 1 / 3)

    sigma = 0.01 * math.sqrt(252)   # |r| = 1% every bar -> constant EWMA sigma
    base = dict(lookbacks=(1,), ewma_com=1.0, ewma_seed=1, sigma_p=sigma, idm=1.0, e0=1000.0, min_bars=2)

    # scenario X: sizing, fill at next open, invalid bar, roll cost moved to next session, flip, end mark
    X = [Bar("2024-01-30", 100, 101, 1), Bar("2024-01-31", 100, 101, 1), Bar("2024-02-01", 100, 99, 1),
         Bar("2024-02-29", 100, 99, 1), Bar("2024-03-01", 100, 99, 1), Bar("2024-03-04", 100, 101, 1)]
    specX = {"X": Spec("equity", "X", 1.0, None, 0.5, frozenset())}
    sessX = [b.date for b in X]
    rc = {"X": {next_session(sessX, "2024-02-02")}}   # 2024-02-02 has no valid bar -> 2024-02-29
    res = run({"X": X}, specX, Params(**base, weights={"X": 1.0}, roll_cost_dates=rc))
    tr = res["trades"]
    check("X trades", len(tr), 2)
    if len(tr) == 2:
        a, b = tr
        check("T1 side", a.side, 1); check("T1 entry", (a.entry_date, a.entry_price), ("2024-02-01", 100.0))
        check("T1 exit", (a.exit_date, a.exit_price, a.exit_reason), ("2024-03-01", 100.0, "flip"))
        check("T1 gross", a.gross, -20.0); check("T1 costs", a.costs, 20.0)
        check("T2 side", b.side, -1); check("T2 entry", (b.entry_date, b.entry_price), ("2024-03-01", 100.0))
        check("T2 exit", (b.exit_date, b.exit_price, b.exit_reason), ("2024-03-04", 101.0, "end"))
        check("T2 gross", b.gross, 0.0); check("T2 costs", b.costs, 5.0)
    check("X equity", [float(e) for e in res["equity"]], [1000.0, 1000.0, 985.0, 965.0, 965.0, 955.0])
    check("X held", [float(h) for h in res["held"][0]], [0.0, 0.0, 10.0, 10.0, -10.0, -10.0])

    # scenario Y: rates notional, fill skips an invalid bar; the invalid bar is not a session
    Yraw = [("2024-01-30", 100, 99, 1), ("2024-01-31", 100, 99, 1), ("2024-02-01", 100, 99, 0),
            ("2024-02-02", 100, 101, 1)]
    Y = [Bar(*r) for r in Yraw if r[3] > 0]
    specY = {"Y": Spec("rates", "Y", None, 1000.0, 1.0, frozenset())}
    res = run({"Y": Y}, specY, Params(**base, weights={"Y": 1.0}, roll_cost_dates={"Y": set()}))
    check("Y sessions", res["sessions"], ["2024-01-30", "2024-01-31", "2024-02-02"])
    check("Y equity", [round(float(e), 9) for e in res["equity"]], [1000.0, 1000.0, 989.0])
    t = res["trades"]
    check("Y trade", [(x.side, x.entry_date, x.entry_price, x.exit_date, x.exit_price, x.exit_reason,
                       round(x.gross, 9), x.costs) for x in t],
          [(-1, "2024-02-02", 100.0, "2024-02-02", 101.0, "end", -10.0, 1.0)])
    # fill delay 2: no second valid bar after the decision -> no change
    res = run({"Y": Y}, specY, Params(**base, weights={"Y": 1.0}, roll_cost_dates={"Y": set()}, fill_delay=2))
    check("Y delay2 equity", [float(e) for e in res["equity"]], [1000.0, 1000.0, 1000.0])
    check("Y delay2 trades", len(res["trades"]), 0)

    # fractional: no rounding (X at 2024-01-31: 1000/101 contracts)
    res = run({"X": X}, specX, Params(**base, weights={"X": 1.0}, roll_cost_dates={"X": set()}, integer=False))
    check("X fractional n", float(res["held"][0][2]), 1000 / 101)

    if fails:
        raise SystemExit("SELF-TEST FAILED:\n  " + "\n  ".join(fails))
    print("self-test: all cases passed")


# ---- main ----------------------------------------------------------------------------

def rules_hash() -> str:
    b = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    h = hashlib.sha256(b).hexdigest()
    lock = (HERE / "RULES.lock").read_text().split()
    if lock[1] != h:
        raise SystemExit(f"RULES.md hash {h} does not match RULES.lock {lock[1]}; refusing to run")
    return h


def git_state() -> tuple[str, bool]:
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True,
                                cwd=ROOT).stdout.strip())
    return head, dirty


def trade_row(tr: Trade) -> dict:
    return dict(market=tr.market, micro=SPECS[tr.market].micro, cls=SPECS[tr.market].cls,
                side="long" if tr.side > 0 else "short", entry_date=tr.entry_date,
                entry_price=tr.entry_price, entry_contracts=tr.entry_contracts,
                max_contracts=tr.max_contracts, exit_date=tr.exit_date, exit_price=tr.exit_price,
                exit_reason=tr.exit_reason, gross=round(tr.gross, 6), costs=round(tr.costs, 6),
                net=round(tr.net, 6),
                net_bp=round(tr.net / (tr.entry_contracts * tr.entry_value) * 1e4, 4))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reason", required=True)
    args = ap.parse_args()
    h = rules_hash()
    self_test()

    data, raw_counts = {}, {}
    for m in MARKETS:
        data[m], raw_counts[m] = load_market(m)
    rng = np.random.default_rng(SEED)

    P = Params()
    prim = run(data, SPECS, P)
    for m in MARKETS:
        if sum(1 for b in data[m] if b.date <= prim["first"]) < MIN_BARS:
            raise SystemExit(f"data check failed: {m} lacks warm-up")
    r_prim = daily_returns(prim)
    first = prim["first"]

    s1 = run(data, SPECS, Params(integer=False))
    r_s1 = daily_returns(s1)
    b2 = run(data, SPECS, Params(integer=False, long_only=True, cost_mult=0.0))
    r_b2 = daily_returns(b2)
    es = {b.date: b.close / b.open - 1 for b in data["ES=F"]}
    r_b1 = np.array([es.get(d, 0.0) for d in prim["sessions"]])
    gross_prim = prim["gross"].sum(0) / prev_equity(prim)

    res = dict(rules_sha256=h, seed=SEED, generated_utc=datetime.now(timezone.utc).isoformat(timespec="seconds"),
               first_rebalance=first, first_pnl_session=prim["sessions"][prim["start"]],
               last_session=prim["sessions"][-1], raw_bars=raw_counts,
               valid_bars={m: len(data[m]) for m in MARKETS})
    res["primary"] = all_metrics(prim, r_prim, prim["trades"])
    res["primary_gross"] = all_metrics(prim, gross_prim)
    res["s1"] = all_metrics(s1, r_s1, s1["trades"])
    res["b1"] = all_metrics(prim, r_b1)
    res["b2"] = all_metrics(b2, r_b2)

    # placebo and bootstrap
    res["placebo_primary"] = placebo(prim, rng)
    res["placebo_s1"] = placebo(s1, rng)
    res["bootstrap_primary"] = bootstrap(r_prim[window_mask(prim, "full")], rng)

    # grid (IS plateau, OOS for selection bias only)
    grid = []
    for sc in GRID_SCALES:
        for rb in GRID_REBAL:
            lb = tuple(int(round(k * sc)) for k in LOOKBACKS)
            g = run(data, SPECS, Params(lookbacks=lb, rebalance=rb, first_rebalance=first))
            rg = daily_returns(g)
            grid.append(dict(scale=sc, rebalance=rb, lookbacks=lb, primary=(sc == 1.0 and rb == "monthly"),
                             is_sharpe=metrics(g, rg, "is")["sharpe"], oos_sharpe=metrics(g, rg, "oos")["sharpe"],
                             full_sharpe=metrics(g, rg, "full")["sharpe"]))
    res["grid"] = grid

    # costs
    costs = []
    for cm in COST_MULTS:
        g = prim if cm == 1.0 else run(data, SPECS, Params(cost_mult=cm))
        rg = daily_returns(g)
        costs.append(dict(mult=cm, full_sharpe=metrics(g, rg, "full")["sharpe"],
                          oos_sharpe=metrics(g, rg, "oos")["sharpe"], is_sharpe=metrics(g, rg, "is")["sharpe"],
                          full_return=metrics(g, rg, "full")["total_return"]))
    res["costs"] = costs
    tc = sum(t.costs for t in prim["trades"])
    res["total_costs_usd"] = tc
    res["total_gross_usd"] = float(prim["gross"][:, prim["start"]:].sum())

    delay = run(data, SPECS, Params(fill_delay=2))
    res["delay"] = all_metrics(delay, daily_returns(delay), delay["trades"])
    cc = run(data, SPECS, Params(basis="cc"))
    res["cc_basis"] = all_metrics(cc, daily_returns(cc), cc["trades"])
    res["cc_roll_days"] = {m: len(v) for m, v in cc["roll_days"].items()}

    # sleeves and markets
    pe = prev_equity(prim)
    sleeves = {}
    for cls in ("equity", "rates", "commodities"):
        rows = [i for i, m in enumerate(prim["markets"]) if SPECS[m].cls == cls]
        x = (prim["gross"][rows].sum(0) - prim["cost"][rows].sum(0)) / pe
        sleeves[cls] = {w: dict(sharpe=sharpe(x[window_mask(prim, w)]),
                                pnl_usd=float((prim["gross"][rows] - prim["cost"][rows])[:, window_mask(prim, w)].sum()))
                        for w in ("full", "is", "oos")}
    res["sleeves"] = sleeves
    per_market = {}
    s1_gross_cls = {"equity": 0.0, "rates": 0.0, "commodities": 0.0}
    for i, m in enumerate(prim["markets"]):
        mk = window_mask(prim, "full")
        x = (prim["gross"][i] - prim["cost"][i]) / pe
        tw = [t for t in prim["trades"] if t.market == m]
        per_market[m] = dict(micro=SPECS[m].micro, cls=SPECS[m].cls,
                             gross_usd=float(prim["gross"][i, mk].sum()), cost_usd=float(prim["cost"][i, mk].sum()),
                             net_usd=float((prim["gross"][i] - prim["cost"][i])[mk].sum()),
                             sharpe_full=sharpe(x[mk]), sharpe_oos=sharpe(x[window_mask(prim, "oos")]),
                             trades=len(tw), share_sessions_held=float(np.mean(prim["held"][i, mk] != 0)),
                             s1_gross_usd=float(s1["gross"][i, window_mask(s1, "full")].sum()))
        s1_gross_cls[SPECS[m].cls] += per_market[m]["s1_gross_usd"]
    res["markets"] = per_market
    res["s1_gross_by_class"] = s1_gross_cls

    # granularity
    ms = window_mask(prim, "full")
    gran = dict(corr_primary_s1=float(np.corrcoef(r_prim[ms], r_s1[ms])[0, 1]))
    s1_reb = {(x["date"], x["market"]): x for x in s1["rebal"]}
    per = {}
    for m in MARKETS:
        pr = [x for x in prim["rebal"] if x["market"] == m]
        zero = sum(1 for x in pr if x["n"] == 0 and s1_reb[(x["date"], m)]["n"] != 0)
        ratios = [abs(x["n"]) / abs(s1_reb[(x["date"], m)]["n"]) for x in pr if s1_reb[(x["date"], m)]["n"] != 0]
        per[m] = dict(rebalances=len(pr), share_rounded_to_zero=zero / len(pr),
                      mean_abs_ratio=float(np.mean(ratios)),
                      mean_abs_s1_contracts=float(np.mean([abs(s1_reb[(x["date"], m)]["n"]) for x in pr])))
    gran["markets"] = per
    res["granularity"] = gran

    # breakdowns
    years = {}
    for y in sorted({d[:4] for d in np.array(prim["sessions"])[ms]}):
        mk = ms & np.array([d[:4] == y for d in prim["sessions"]])
        years[y] = dict(primary=float(np.prod(1 + r_prim[mk]) - 1), sharpe=sharpe(r_prim[mk]),
                        max_dd=max_dd(r_prim[mk]), s1=float(np.prod(1 + r_s1[mk]) - 1),
                        b1=float(np.prod(1 + r_b1[mk]) - 1), b2=float(np.prod(1 + r_b2[mk]) - 1))
    res["by_year"] = years
    side = {}
    for sd, lab in ((1, "long"), (-1, "short")):
        tw = [t for t in prim["trades"] if t.side == sd]
        w = [t.net for t in tw if t.net > 0]
        l_ = [t.net for t in tw if t.net <= 0]
        side[lab] = dict(trades=len(tw), net_usd=float(sum(t.net for t in tw)),
                         profit_factor=sum(w) / abs(sum(l_)) if l_ and sum(l_) else float("inf"))
    res["by_side"] = side
    ex = {}
    for reason in ("flip", "flat", "end"):
        tw = [t for t in prim["trades"] if t.exit_reason == reason]
        ex[reason] = dict(trades=len(tw), net_usd=float(sum(t.net for t in tw)))
    res["by_exit"] = ex

    mp, mb = monthly(prim, r_prim), monthly(prim, r_b1)
    months = sorted(mp)
    order = np.argsort([mb[k] for k in months], kind="stable")
    quint = []
    for q, grp in enumerate(np.array_split(order, 5)):
        ks = [months[g] for g in grp]
        quint.append(dict(quintile=q + 1, months=len(ks), b1_mean=float(np.mean([mb[k] for k in ks])),
                          primary_mean=float(np.mean([mp[k] for k in ks]))))
    res["move_quintiles"] = quint
    res["monthly"] = dict(primary=mp, b1=mb)

    # predictions
    full_tr = prim["trades"]
    wins = [t.net for t in full_tr if t.net > 0]
    losses = [t.net for t in full_tr if t.net <= 0]
    payoff = float(np.mean(wins) / abs(np.mean(losses))) if wins and losses else float("nan")
    tails = [quint[0]["primary_mean"], quint[4]["primary_mean"]]
    q_tail = float(np.mean([mp[months[g]] for g in list(np.array_split(order, 5)[0]) + list(np.array_split(order, 5)[4])]))
    q_mid = float(np.mean([mp[months[g]] for q in (1, 2, 3) for g in np.array_split(order, 5)[q]]))
    res["predictions"] = dict(
        payoff_ratio=payoff, p1_consistent=payoff > 1,
        tail_mean=q_tail, mid_mean=q_mid, tails_by_quintile=tails, p2_consistent=q_tail > q_mid,
        s1_gross_by_class=s1_gross_cls, p3_consistent=sum(v > 0 for v in s1_gross_cls.values()) >= 2)

    # acceptance
    P_ = res["primary"]
    grid_pos = sum(1 for g in grid if g["is_sharpe"] > 0)
    two_x = next(c for c in costs if c["mult"] == 2.0)
    sleeves_pos = sum(1 for v in sleeves.values() if v["oos"]["sharpe"] > 0)
    acc = [
        dict(line="OOS Sharpe >= 0.5", required=0.5, actual=P_["oos"]["sharpe"], passed=P_["oos"]["sharpe"] >= 0.5),
        dict(line="OOS profit factor >= 1.10", required=1.10, actual=P_["oos"]["profit_factor"],
             passed=P_["oos"]["profit_factor"] >= 1.10),
        dict(line="Direction placebo p <= 0.05", required=0.05, actual=res["placebo_primary"]["p"],
             passed=res["placebo_primary"]["p"] <= 0.05),
        dict(line="IS Sharpe > 0", required=0.0, actual=P_["is"]["sharpe"], passed=P_["is"]["sharpe"] > 0),
        dict(line="IS grid cells with Sharpe > 0 >= 4 of 6", required=4, actual=grid_pos, passed=grid_pos >= 4),
        dict(line="Full-sample return > 0 at 2x cost", required=0.0, actual=two_x["full_return"],
             passed=two_x["full_return"] > 0),
        dict(line="OOS Sharpe > 0 in >= 2 of 3 sleeves", required=2, actual=sleeves_pos, passed=sleeves_pos >= 2),
    ]
    enough = P_["oos"]["trades"] >= 40
    acc.append(dict(line="OOS trades >= 40 (minimum sample)", required=40, actual=P_["oos"]["trades"], passed=enough))
    res["acceptance"] = acc
    res["status"] = ("Inconclusive" if not enough else
                     "Paper-trading candidate" if all(a["passed"] for a in acc) else "Rejected")
    s1a = [res["s1"]["oos"]["sharpe"] >= 0.5, res["placebo_s1"]["p"] <= 0.05, res["s1"]["is"]["sharpe"] > 0]
    res["s1_acceptance"] = dict(oos_sharpe=res["s1"]["oos"]["sharpe"], placebo_p=res["placebo_s1"]["p"],
                                is_sharpe=res["s1"]["is"]["sharpe"], passed=all(s1a))

    # outputs
    out_res = dict(res)
    out_res["placebo_primary"] = {k: v for k, v in res["placebo_primary"].items() if k != "null"}
    out_res["placebo_s1"] = {k: v for k, v in res["placebo_s1"].items() if k != "null"}
    (HERE / "results.json").write_text(json.dumps(out_res, indent=1, default=float))
    np.save(HERE / "placebo_null.npy", np.array(res["placebo_primary"]["null"]))
    with (HERE / "daily.csv").open("w", newline="") as f:
        w = csv.writer(f)
        rows_cls = {c: [i for i, m in enumerate(prim["markets"]) if SPECS[m].cls == c]
                    for c in ("equity", "rates", "commodities")}
        w.writerow(["date", "in_window", "equity", "ret", "gross_ret", "s1_ret", "b1_ret", "b2_ret",
                    "equity_sleeve_ret", "rates_sleeve_ret", "commodities_sleeve_ret"])
        for t, d in enumerate(prim["sessions"]):
            sl = [(prim["gross"][rows_cls[c], t].sum() - prim["cost"][rows_cls[c], t].sum()) / pe[t]
                  for c in rows_cls]
            w.writerow([d, int(t >= prim["start"]), round(float(prim["equity"][t]), 6), r_prim[t], gross_prim[t],
                        r_s1[t], r_b1[t], r_b2[t], *sl])
    with (HERE / "trades.csv").open("w", newline="") as f:
        rows = [trade_row(t) for t in prim["trades"]]
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)

    head, dirty = git_state()
    runlog = HERE / "RUNLOG.md"
    if not runlog.exists():
        runlog.write_text("# Run log\n\nOne entry per run that computes returns. Append-only.\n")
    with runlog.open("a") as f:
        f.write(f"\n## {res['generated_utc']}\n\n- Reason: {args.reason}\n- Rules sha256: `{h}`\n"
                f"- Git HEAD: `{head}` (dirty: {dirty})\n"
                f"- Primary Sharpe full / IS / OOS: {P_['full']['sharpe']:.3f} / {P_['is']['sharpe']:.3f} / "
                f"{P_['oos']['sharpe']:.3f}\n"
                f"- Primary total return full / IS / OOS: {P_['full']['total_return']:.4f} / "
                f"{P_['is']['total_return']:.4f} / {P_['oos']['total_return']:.4f}\n"
                f"- Status from acceptance table: {res['status']}\n")
    print(json.dumps({k: res[k] for k in ("status", "first_rebalance", "first_pnl_session")}, indent=1))
    for a in acc:
        print(f"  {'PASS' if a['passed'] else 'FAIL'}  {a['line']}: {a['actual']}")


if __name__ == "__main__":
    main()
