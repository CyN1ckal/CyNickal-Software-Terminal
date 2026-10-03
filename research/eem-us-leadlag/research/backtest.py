# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered EEM lead-lag backtest. Refuses to run if RULES.md does not match RULES.lock."""

import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData  # noqa: E402

HERE = Path(__file__).resolve().parent
LAST = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
COST_BPS = 1.0
DEADZONES = (0.0, 0.0025, 0.005, 0.01, 0.015)
SEED_DIRECTION = 20261101
SEED_BOOT = 20261102
SEED_TIMING = 20261103
N_DIRECTION = 2000
N_BOOT = 2000
N_TIMING = 500
BLOCK = 20

DAILY_FIELDS = [
    "date", "strategy_net", "strategy_gross", "eem_otc", "spy_c2c",
    "weight", "signal", "spy_prior_ret", "sample", "equity", "efa_net", "efa_gross",
]
TRADE_FIELDS = [
    "side", "entry_time", "entry_px", "exit_time", "exit_px",
    "gross", "net", "exit_reason", "spy_prior_ret",
]


class DataError(RuntimeError):
    pass


def digest_rules() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(raw).hexdigest()


def lock_fields() -> dict:
    out = {}
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        key, _, value = line.partition(" ")
        out[key] = value.strip()
    return out


def require_lock() -> dict:
    lock = lock_fields()
    got = digest_rules()
    if lock.get("sha256") != got:
        raise SystemExit(f"RULES.md hash {got} does not match RULES.lock {lock.get('sha256')}")
    return lock


def git_state() -> tuple[str, str]:
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    return head, "yes" if dirty else "no"


def fnum(x) -> str:
    return format(float(x), ".17g")


def sharpe(r: np.ndarray):
    r = np.asarray(r, dtype=float)
    if len(r) < 2:
        return None
    sd = float(np.std(r, ddof=1))
    if sd == 0.0 or not np.isfinite(sd):
        return None
    return float(np.mean(r) / sd * math.sqrt(252))


def sharpe_rows(samples: np.ndarray) -> np.ndarray:
    mu = samples.mean(axis=1)
    sd = samples.std(axis=1, ddof=1)
    out = np.full(len(samples), np.nan)
    ok = sd > 0
    out[ok] = mu[ok] / sd[ok] * math.sqrt(252)
    return out


def equity_end(r: np.ndarray) -> float:
    eq = 1.0
    for x in np.asarray(r, dtype=float):
        eq *= 1.0 + float(x)
    return eq


def total_return(r: np.ndarray):
    return float(equity_end(r) - 1.0)


def max_dd(r: np.ndarray):
    eq = 1.0
    peak = 1.0
    worst = 0.0
    for x in np.asarray(r, dtype=float):
        eq *= 1.0 + float(x)
        peak = max(peak, eq)
        if peak != 0.0:
            worst = min(worst, eq / peak - 1.0)
    return float(worst)


def cagr(r: np.ndarray):
    n = len(r)
    end = equity_end(r)
    if n == 0 or end <= 0.0:
        return None
    return float(end ** (252.0 / n) - 1.0)


def vol_ann(r: np.ndarray):
    r = np.asarray(r, dtype=float)
    if len(r) < 2:
        return None
    return float(np.std(r, ddof=1) * math.sqrt(252))


def tstat(r: np.ndarray):
    r = np.asarray(r, dtype=float)
    n = len(r)
    if n < 2:
        return None
    sd = float(np.std(r, ddof=1))
    if sd == 0.0:
        return None
    return float(np.mean(r) / (sd / math.sqrt(n)))


def profit_factor(nets: np.ndarray):
    nets = np.asarray(nets, dtype=float)
    losses = float(nets[nets < 0].sum()) if np.any(nets < 0) else 0.0
    if losses == 0.0:
        return None
    wins = float(nets[nets > 0].sum()) if np.any(nets > 0) else 0.0
    return wins / abs(losses)


def hold_hours(d: date) -> float:
    return 3.5 if d in EARLY_CLOSES else 6.5


def exit_stamp(d: date) -> str:
    return "13:00" if d in EARLY_CLOSES else "16:00"


def build_book(spy_close, opn, cls, has, deadzone, cost_side):
    """Arrays include the two warm-up bars at the front. Returns evaluation-session arrays."""
    n = len(spy_close)
    idx = np.arange(2, n)
    prior = spy_close[idx - 1] / spy_close[idx - 2] - 1.0
    signal = np.where(prior > 0.0, 1.0, np.where(prior < 0.0, -1.0, 0.0))
    take = (np.abs(prior) >= deadzone) & (signal != 0.0) & has[idx]
    weight = np.where(take, signal, 0.0)
    otc = np.zeros(len(idx))
    got = has[idx]
    otc[got] = cls[idx][got] / opn[idx][got] - 1.0
    gross = weight * otc
    net = gross - np.where(weight != 0.0, 2.0 * cost_side, 0.0)
    spy_c2c = spy_close[idx] / spy_close[idx - 1] - 1.0
    return {
        "prior": prior,
        "signal": signal,
        "weight": weight,
        "otc": otc,
        "gross": gross,
        "net": net,
        "spy_c2c": spy_c2c,
        "opn": opn[idx],
        "cls": cls[idx],
        "has": got,
    }


def delay_book(signal, has, otc, cost_side):
    weight = np.zeros(len(signal))
    if len(signal) > 1:
        weight[1:] = np.where(has[1:] & (signal[:-1] != 0.0), signal[:-1], 0.0)
    gross = weight * otc
    net = gross - np.where(weight != 0.0, 2.0 * cost_side, 0.0)
    return weight, gross, net


def trades_from(dates, book):
    rows = []
    for i, d in enumerate(dates):
        if book["weight"][i] == 0.0:
            continue
        rows.append({
            "date": d,
            "side": "long" if book["weight"][i] > 0 else "short",
            "entry_time": f"{d.isoformat()} 09:30",
            "entry_px": round(float(book["opn"][i]), 6),
            "exit_time": f"{d.isoformat()} {exit_stamp(d)}",
            "exit_px": round(float(book["cls"][i]), 6),
            "gross": float(book["gross"][i]),
            "net": float(book["net"][i]),
            "exit_reason": "session_close",
            "spy_prior_ret": float(book["prior"][i]),
        })
    return rows


def self_test() -> None:
    if date(2024, 7, 3) not in EARLY_CLOSES:
        raise AssertionError("2024-07-03 missing from EARLY_CLOSES")
    dates_all = [
        date(2011, 1, 4), date(2011, 1, 5), date(2011, 1, 6), date(2011, 1, 7),
        date(2011, 1, 10), date(2011, 1, 11), date(2024, 7, 3),
    ]
    spy = np.array([100.0, 110.0, 90.0, 90.0, 80.0, 88.0, 88.0])
    opn = np.array([0.0, 0.0, 50.0, 40.0, 44.0, 0.0, 70.0])
    cls = np.array([0.0, 0.0, 55.0, 44.0, 30.0, 0.0, 77.0])
    has = np.array([False, False, True, True, True, False, True])
    book = build_book(spy, opn, cls, has, 0.0, 0.0001)
    eval_dates = dates_all[2:]
    trades = trades_from(eval_dates, book)
    assert [t["date"] for t in trades] == [dates_all[2], dates_all[3], dates_all[6]]
    assert [t["side"] for t in trades] == ["long", "short", "long"]
    assert abs(trades[0]["gross"] - 0.10) < 1e-12
    assert abs(trades[0]["net"] - (0.10 - 0.0002)) < 1e-12
    assert trades[0]["entry_px"] == 50.0 and trades[0]["exit_px"] == 55.0
    assert trades[0]["exit_time"].endswith("16:00")
    assert book["spy_c2c"][0] < 0 and book["weight"][0] == 1.0
    assert abs(trades[1]["gross"] + 0.10) < 1e-12
    assert abs(trades[1]["net"] + 0.1002) < 1e-12
    assert book["prior"][2] == 0.0 and book["weight"][2] == 0.0 and book["net"][2] == 0.0
    assert book["signal"][3] == -1.0 and book["has"][3] == False and book["weight"][3] == 0.0
    assert trades[2]["exit_time"] == "2024-07-03 13:00"
    assert abs(trades[2]["gross"] - 0.10) < 1e-12
    assert date(2024, 7, 5) not in eval_dates
    wide = build_book(spy, opn, cls, has, 0.15, 0.0001)
    wide_trades = trades_from(eval_dates, wide)
    assert len(wide_trades) == 1 and wide_trades[0]["side"] == "short"
    d_w, _, _ = delay_book(book["signal"], book["has"], book["otc"], 0.0001)
    assert d_w[0] == 0.0 and d_w[1] == 1.0
    # The missing-bar session's signal still trades on the next session, which has a bar.
    assert book["signal"][3] == -1.0 and d_w[4] == -1.0


def align(spy_dates, bars):
    by = {b.session: b for b in bars}
    opn = np.zeros(len(spy_dates))
    cls = np.zeros(len(spy_dates))
    has = np.zeros(len(spy_dates), dtype=bool)
    for i, d in enumerate(spy_dates):
        b = by.get(d)
        if b is None:
            continue
        has[i] = True
        opn[i] = b.open
        cls[i] = b.close
    return opn, cls, has


def check_prices(bars, name) -> None:
    for b in bars:
        if b.open <= 0 or b.high <= 0 or b.low <= 0 or b.close <= 0:
            raise DataError(f"{name} nonpositive price on {b.session}")
        if b.high + 1e-6 < max(b.open, b.close) or b.low - 1e-6 > min(b.open, b.close):
            raise DataError(f"{name} OHLC inconsistency on {b.session}")


def load():
    absent = {date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5), date(2025, 1, 9)}
    with MarketData() as md:
        eem = md.bars("EEM", "1d", start="2011-01-04", end="2026-10-02")
        efa = md.bars("EFA", "1d", start="2011-01-04", end="2026-10-02")
        spy = md.bars("SPY", "1d", start="2011-01-04", end="2026-10-01")
        for sym in ("EEM", "EFA", "SPY"):
            if md.corporate_actions(sym):
                raise DataError(f"{sym} has a corporate action")
    if len(eem) != 3960 or eem[0].session != date(2011, 1, 4) or eem[-1].session != date(2026, 10, 2):
        raise DataError("EEM coverage does not match the locked counts")
    if len(efa) != 3960 or efa[0].session != date(2011, 1, 4) or efa[-1].session != date(2026, 10, 2):
        raise DataError("EFA coverage does not match the locked counts")
    if len(spy) != 3959 or spy[0].session != date(2011, 1, 4) or spy[-1].session != date(2026, 10, 1):
        raise DataError("SPY coverage does not match the locked counts")
    for name, bars in (("EEM", eem), ("EFA", efa), ("SPY", spy)):
        sessions = [b.session for b in bars]
        if len(sessions) != len(set(sessions)):
            raise DataError(f"{name} has a duplicate session")
        check_prices(bars, name)
        if any(d in sessions for d in absent):
            raise DataError(f"{name} contains a locked-absent session")
    spy_dates = [b.session for b in spy]
    eem_dates = [b.session for b in eem if b.session <= LAST]
    efa_dates = [b.session for b in efa if b.session <= LAST]
    if eem_dates != spy_dates or efa_dates != spy_dates:
        raise DataError("EEM, EFA, and SPY dates through 2026-10-01 differ")
    for d in (date(2021, 12, 31), date(2024, 6, 28), date(2024, 7, 1)):
        if d not in spy_dates:
            raise DataError(f"{d} missing")
    if len(spy_dates) - 2 != 3957:
        raise DataError("evaluation length is not 3957")
    spy_close = np.array([b.close for b in spy], dtype=float)
    eem_opn, eem_cls, eem_has = align(spy_dates, [b for b in eem if b.session <= LAST])
    efa_opn, efa_cls, efa_has = align(spy_dates, [b for b in efa if b.session <= LAST])
    if not eem_has.all() or not efa_has.all():
        raise DataError("a SPY session has no EEM or EFA bar")
    return spy_dates, spy_close, eem_opn, eem_cls, eem_has, efa_opn, efa_cls, efa_has


def mask_for(dates, which):
    if which == "full":
        return np.ones(len(dates), dtype=bool)
    if which == "is":
        return np.array([d < OOS_START for d in dates])
    if which == "oos":
        return np.array([d >= OOS_START for d in dates])
    raise ValueError(which)


def trade_metrics(dates, net, book_trades, which):
    m = mask_for(dates, which)
    r = np.asarray(net, dtype=float)[m]
    chosen = [d for d, keep in zip(dates, m) if keep]
    trades = [t for t in book_trades if (t["date"] < OOS_START if which == "is" else t["date"] >= OOS_START if which == "oos" else True)]
    nets = np.array([t["net"] for t in trades], dtype=float)
    grosses = np.array([t["gross"] for t in trades], dtype=float)
    sides = [t["side"] for t in trades]
    long_g = grosses[np.array([s == "long" for s in sides])] if trades else np.array([])
    short_g = grosses[np.array([s == "short" for s in sides])] if trades else np.array([])
    long_n = nets[np.array([s == "long" for s in sides])] if trades else np.array([])
    short_n = nets[np.array([s == "short" for s in sides])] if trades else np.array([])
    winners = nets[nets > 0] if len(nets) else np.array([])
    losers = nets[nets < 0] if len(nets) else np.array([])
    exposed = int(np.sum(np.asarray([t["date"] in set(chosen) for t in book_trades]))) if False else None
    # Exposure is nonzero weights, not the trade list of another book.
    return {
        "start": chosen[0].isoformat() if chosen else None,
        "end": chosen[-1].isoformat() if chosen else None,
        "sessions": int(m.sum()),
        "trades": int(len(trades)),
        "total_return": total_return(r),
        "terminal_equity": equity_end(r),
        "cagr": cagr(r),
        "vol": vol_ann(r),
        "sharpe": sharpe(r),
        "max_dd": max_dd(r),
        "tstat": tstat(r),
        "profit_factor": profit_factor(nets) if len(nets) else None,
        "win_rate": float(np.sum(nets > 0) / len(nets)) if len(nets) else None,
        "avg_net": float(np.mean(nets)) if len(nets) else None,
        "avg_net_bp": float(np.mean(nets) * 10000) if len(nets) else None,
        "avg_winner": float(np.mean(winners)) if len(winners) else None,
        "avg_winner_bp": float(np.mean(winners) * 10000) if len(winners) else None,
        "avg_loser": float(np.mean(losers)) if len(losers) else None,
        "avg_loser_bp": float(np.mean(losers) * 10000) if len(losers) else None,
        "long_n": int(len(long_n)),
        "short_n": int(len(short_n)),
        "long_pf": profit_factor(long_n) if len(long_n) else None,
        "short_pf": profit_factor(short_n) if len(short_n) else None,
        "long_gross_sum": float(long_g.sum()) if len(long_g) else 0.0,
        "short_gross_sum": float(short_g.sum()) if len(short_g) else 0.0,
        "long_net_sum": float(long_n.sum()) if len(long_n) else 0.0,
        "short_net_sum": float(short_n.sum()) if len(short_n) else 0.0,
        "win_n": int(np.sum(nets > 0)) if len(nets) else 0,
        "loss_n": int(np.sum(nets < 0)) if len(nets) else 0,
        "_exposed_placeholder": exposed,
    }


def bench_metrics(dates, series, which):
    m = mask_for(dates, which)
    r = np.asarray(series, dtype=float)[m]
    chosen = [d for d, keep in zip(dates, m) if keep]
    return {
        "start": chosen[0].isoformat(),
        "end": chosen[-1].isoformat(),
        "sessions": int(len(r)),
        "total_return": total_return(r),
        "terminal_equity": equity_end(r),
        "cagr": cagr(r),
        "vol": vol_ann(r),
        "sharpe": sharpe(r),
        "max_dd": max_dd(r),
        "tstat": tstat(r),
    }


def exposure(dates, weight, which) -> float:
    m = mask_for(dates, which)
    w = np.asarray(weight)[m]
    return float(np.mean(w != 0.0))


def attach_exposure(block, dates, weight):
    for which in ("full", "is", "oos"):
        block[which]["exposure"] = exposure(dates, weight, which)
        block[which].pop("_exposed_placeholder", None)


def quintiles(trades):
    prior = np.array([t["spy_prior_ret"] for t in trades], dtype=float)
    gross = np.array([t["gross"] for t in trades], dtype=float)
    net = np.array([t["net"] for t in trades], dtype=float)
    order = np.argsort(prior, kind="mergesort")
    n = len(order)
    q = np.empty(n, dtype=int)
    for i, j in enumerate(order):
        q[j] = min(5, 1 + (i * 5) // n)
    rows = []
    for k in range(1, 6):
        m = q == k
        rows.append({
            "quintile": k,
            "n": int(m.sum()),
            "mean_spy_prior": float(np.mean(prior[m])),
            "mean_gross": float(np.mean(gross[m])),
            "mean_net": float(np.mean(net[m])),
            "sum_net": float(np.sum(net[m])),
        })
    return rows


def predictions(trades):
    long_g = [t["gross"] for t in trades if t["side"] == "long"]
    short_g = [t["gross"] for t in trades if t["side"] == "short"]
    long_sum = float(np.sum(long_g)) if long_g else 0.0
    short_sum = float(np.sum(short_g)) if short_g else 0.0
    if long_g and short_g and long_sum > 0.0 and short_sum > 0.0:
        p1 = "consistent"
    else:
        p1 = "not consistent"
    abs_r = np.array([abs(t["spy_prior_ret"]) for t in trades], dtype=float)
    nets = np.array([t["net"] for t in trades], dtype=float)
    med = float(np.median(abs_r))
    top = abs_r >= med
    bot = abs_r < med
    if not np.any(bot):
        p2 = "not testable"
        top_mean = float(np.mean(nets[top])) if np.any(top) else None
        bot_mean = None
    else:
        top_mean = float(np.mean(nets[top]))
        bot_mean = float(np.mean(nets[bot]))
        p2 = "consistent" if top_mean > bot_mean else "not consistent"
    return {
        "long_n": len(long_g),
        "short_n": len(short_g),
        "long_gross_sum": long_sum,
        "short_gross_sum": short_sum,
        "prediction_1": p1,
        "median_abs_spy": med,
        "top_n": int(np.sum(top)),
        "bottom_n": int(np.sum(bot)),
        "top_mean_net": top_mean,
        "bottom_mean_net": bot_mean,
        "prediction_2": p2,
    }


def by_year(dates, net, eem_otc, spy_c2c, trades):
    rows = []
    years = sorted({d.year for d in dates})
    for year in years:
        m = np.array([d.year == year for d in dates])
        r = np.asarray(net)[m]
        chosen = [d for d in dates if d.year == year]
        yt = [t for t in trades if t["date"].year == year]
        rows.append({
            "year": year,
            "start": chosen[0].isoformat(),
            "end": chosen[-1].isoformat(),
            "sessions": int(m.sum()),
            "trades": len(yt),
            "total_return": total_return(r),
            "sharpe": sharpe(r),
            "max_dd": max_dd(r),
            "eem_otc_return": total_return(np.asarray(eem_otc)[m]),
            "spy_c2c_return": total_return(np.asarray(spy_c2c)[m]),
            "straddles_split": year == 2024,
        })
    return rows


def by_side(trades):
    out = {}
    for side in ("long", "short"):
        sub = [t for t in trades if t["side"] == side]
        nets = np.array([t["net"] for t in sub], dtype=float)
        gross = np.array([t["gross"] for t in sub], dtype=float)
        out[side] = {
            "n": len(sub),
            "gross_sum": float(gross.sum()) if len(sub) else 0.0,
            "net_sum": float(nets.sum()) if len(sub) else 0.0,
            "mean_net": float(np.mean(nets)) if len(sub) else None,
            "mean_net_bp": float(np.mean(nets) * 10000) if len(sub) else None,
            "win_rate": float(np.mean(nets > 0)) if len(sub) else None,
            "profit_factor": profit_factor(nets) if len(sub) else None,
        }
    return out


def direction_placebo(gross, weight):
    trade_ix = np.flatnonzero(weight != 0.0)
    g = np.asarray(gross, dtype=float)[trade_ix]
    n = len(gross)
    rng = np.random.default_rng(SEED_DIRECTION)
    flips = rng.choice(np.array([-1.0, 1.0]), size=(N_DIRECTION, len(g)))
    sums = flips @ g
    sumsq = float(np.dot(g, g))
    mean = sums / n
    var = (sumsq - n * mean * mean) / (n - 1)
    sd = np.sqrt(np.maximum(var, 0.0))
    draws = np.full(N_DIRECTION, np.nan)
    ok = sd > 0
    draws[ok] = mean[ok] / sd[ok] * math.sqrt(252)
    # One naive draw must match the closed form before the p-value is used.
    naive = np.zeros(n)
    naive[trade_ix] = flips[0] * g
    if abs(sharpe(naive) - draws[0]) > 1e-9:
        raise RuntimeError("direction placebo closed form does not match the daily series")
    actual = sharpe(np.asarray(gross, dtype=float))
    n_ge = int(np.sum(draws >= actual))
    return {
        "n": N_DIRECTION,
        "seed": SEED_DIRECTION,
        "actual_gross_sharpe": actual,
        "null_mean": float(np.nanmean(draws)),
        "null_p95": float(np.nanquantile(draws, 0.95)),
        "n_ge": n_ge,
        "p": (1 + n_ge) / (N_DIRECTION + 1),
        "draws": draws,
    }


def timing_placebo(signal, has, otc):
    rng = np.random.default_rng(SEED_TIMING)
    draws = np.empty(N_TIMING)
    base_counts = (int(np.sum(signal > 0)), int(np.sum(signal < 0)), int(np.sum(signal == 0)))
    for i in range(N_TIMING):
        p = rng.permutation(signal)
        if (int(np.sum(p > 0)), int(np.sum(p < 0)), int(np.sum(p == 0))) != base_counts:
            raise RuntimeError("timing permutation changed the signal counts")
        w = np.where(has & (p != 0.0), p, 0.0)
        draws[i] = sharpe(w * otc)
    actual = sharpe(np.where(has & (signal != 0.0), signal, 0.0) * otc)
    n_ge = int(np.sum(draws >= actual))
    return {
        "n": N_TIMING,
        "seed": SEED_TIMING,
        "actual_gross_sharpe": actual,
        "null_mean": float(np.mean(draws)),
        "null_p95": float(np.quantile(draws, 0.95)),
        "n_ge": n_ge,
        "p": (1 + n_ge) / (N_TIMING + 1),
        "positive": base_counts[0],
        "negative": base_counts[1],
        "zero": base_counts[2],
        "draws": draws,
    }


def block_bootstrap(net):
    r = np.asarray(net, dtype=float)
    n = len(r)
    n_blocks = math.ceil(n / BLOCK)
    rng = np.random.default_rng(SEED_BOOT)
    starts = rng.integers(0, n, size=(N_BOOT, n_blocks))
    offs = np.arange(BLOCK)
    idx = (starts[:, :, None] + offs[None, None, :]) % n
    idx = idx.reshape(N_BOOT, n_blocks * BLOCK)[:, :n]
    draws = sharpe_rows(r[idx])
    return {
        "n": N_BOOT,
        "seed": SEED_BOOT,
        "block": BLOCK,
        "p025": float(np.quantile(draws, 0.025)),
        "p975": float(np.quantile(draws, 0.975)),
        "median": float(np.quantile(draws, 0.5)),
        "fraction_le_0": float(np.mean(draws <= 0.0)),
    }


def cost_row(gross, weight, dates, bps):
    net = np.asarray(gross, dtype=float) - np.where(weight != 0.0, 2.0 * bps / 10000.0, 0.0)
    full = mask_for(dates, "full")
    oos = mask_for(dates, "oos")
    return {
        "bps_side": bps,
        "full_sharpe": sharpe(net[full]),
        "full_return": total_return(net[full]),
        "oos_sharpe": sharpe(net[oos]),
        "oos_return": total_return(net[oos]),
    }


def grid_row(spy_close, opn, cls, has, dates, deadzone):
    book = build_book(spy_close, opn, cls, has, deadzone, COST_BPS / 10000.0)
    is_m = mask_for(dates, "is")
    oos_m = mask_for(dates, "oos")
    return {
        "deadzone": deadzone,
        "is_sharpe": sharpe(book["net"][is_m]),
        "oos_sharpe": sharpe(book["net"][oos_m]),
        "is_return": total_return(book["net"][is_m]),
        "oos_return": total_return(book["net"][oos_m]),
        "is_trades": int(np.sum(book["weight"][is_m] != 0.0)),
        "oos_trades": int(np.sum(book["weight"][oos_m] != 0.0)),
    }


def clean(obj):
    if isinstance(obj, dict):
        return {k: clean(v) for k, v in obj.items() if not str(k).startswith("_")}
    if isinstance(obj, (list, tuple)):
        return [clean(v) for v in obj]
    if isinstance(obj, (np.floating, float)):
        x = float(obj)
        return x if np.isfinite(x) else None
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.bool_,)):
        return bool(obj)
    return obj


def append_log(text: str) -> None:
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\n", encoding="utf-8", newline="\n")
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)
        if not text.endswith("\n"):
            f.write("\n")


def log_abort(lock, reason: str) -> None:
    head, dirty = git_state()
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    append_log(
        f"## {now}\n"
        f"- reason: {reason}\n"
        f"- rules_sha256: {lock['sha256']}\n"
        f"- git_head: {head}\n"
        f"- dirty: {dirty}\n"
        "- headlines: none (aborted before results)\n"
    )


def main() -> None:
    lock = require_lock()
    self_test()
    try:
        spy_dates, spy_close, eem_opn, eem_cls, eem_has, efa_opn, efa_cls, efa_has = load()
    except DataError as exc:
        log_abort(lock, f"data check failed: {exc}")
        raise SystemExit(str(exc)) from exc

    dates = spy_dates[2:]
    book = build_book(spy_close, eem_opn, eem_cls, eem_has, 0.0, COST_BPS / 10000.0)
    efa = build_book(spy_close, efa_opn, efa_cls, efa_has, 0.0, COST_BPS / 10000.0)
    if not np.array_equal(book["signal"], efa["signal"]):
        raise RuntimeError("EFA signal differs from the EEM signal")
    trades = trades_from(dates, book)
    primary = {}
    gross_block = {}
    for which in ("full", "is", "oos"):
        primary[which] = trade_metrics(dates, book["net"], trades, which)
        gmask = mask_for(dates, which)
        gross_block[which] = {
            "sharpe": sharpe(book["gross"][gmask]),
            "total_return": total_return(book["gross"][gmask]),
        }
    attach_exposure(primary, dates, book["weight"])
    efa_trades = trades_from(dates, efa)
    efa_metrics = {which: trade_metrics(dates, efa["net"], efa_trades, which) for which in ("full", "is", "oos")}
    attach_exposure(efa_metrics, dates, efa["weight"])
    benches = {
        "eem_otc": {which: bench_metrics(dates, book["otc"], which) for which in ("full", "is", "oos")},
        "spy_c2c": {which: bench_metrics(dates, book["spy_c2c"], which) for which in ("full", "is", "oos")},
    }
    costs = {str(bps): cost_row(book["gross"], book["weight"], dates, bps) for bps in (0, 0.5, 1, 2, 3)}
    if abs(costs["1"]["full_sharpe"] - primary["full"]["sharpe"]) > 1e-12:
        raise RuntimeError("1 bp cost row does not match the primary")
    d_w, d_g, d_n = delay_book(book["signal"], book["has"], book["otc"], COST_BPS / 10000.0)
    delay = {
        "full_sharpe": sharpe(d_n),
        "full_return": total_return(d_n),
        "oos_sharpe": sharpe(d_n[mask_for(dates, "oos")]),
        "oos_return": total_return(d_n[mask_for(dates, "oos")]),
        "full_trades": int(np.sum(d_w != 0.0)),
        "oos_trades": int(np.sum(d_w[mask_for(dates, "oos")] != 0.0)),
    }
    grid = [grid_row(spy_close, eem_opn, eem_cls, eem_has, dates, z) for z in DEADZONES]
    if abs(grid[0]["is_sharpe"] - primary["is"]["sharpe"]) > 1e-12:
        raise RuntimeError("deadzone 0 is not the primary IS Sharpe")
    grid_pos = sum(1 for cell in grid if cell["is_sharpe"] is not None and cell["is_sharpe"] > 0)
    direction = direction_placebo(book["gross"], book["weight"])
    if abs(direction["actual_gross_sharpe"] - gross_block["full"]["sharpe"]) > 1e-12:
        raise RuntimeError("placebo actual Sharpe does not match gross Sharpe")
    timing = timing_placebo(book["signal"], book["has"], book["otc"])
    if abs(timing["actual_gross_sharpe"] - gross_block["full"]["sharpe"]) > 1e-12:
        raise RuntimeError("timing actual Sharpe does not match gross Sharpe")
    boot = block_bootstrap(book["net"])
    pred = predictions(trades)
    hours = np.array([hold_hours(t["date"]) for t in trades], dtype=float)
    line1 = (
        primary["oos"]["sharpe"] is not None
        and primary["oos"]["sharpe"] >= 0.5
        and primary["oos"]["profit_factor"] is not None
        and primary["oos"]["profit_factor"] >= 1.10
    )
    line2 = direction["p"] <= 0.05
    line3 = (
        primary["is"]["sharpe"] is not None
        and primary["is"]["sharpe"] > 0
        and grid_pos >= 3
    )
    line4 = costs["2"]["full_return"] is not None and costs["2"]["full_return"] > 0
    line5 = efa_metrics["oos"]["sharpe"] is not None and efa_metrics["oos"]["sharpe"] > 0
    line6 = primary["oos"]["trades"] >= 100
    flags = [line1, line2, line3, line4, line5, line6]
    if not line6:
        status = "Inconclusive"
    elif all(flags):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    acceptance = [
        {
            "line": 1,
            "name": "OOS Sharpe and OOS profit factor",
            "required": "Sharpe >= 0.5 and profit factor >= 1.10",
            "actual_sharpe": primary["oos"]["sharpe"],
            "actual_profit_factor": primary["oos"]["profit_factor"],
            "pass": line1,
        },
        {
            "line": 2,
            "name": "Direction placebo p, full-sample gross Sharpe",
            "required": "p <= 0.05",
            "actual": direction["p"],
            "pass": line2,
        },
        {
            "line": 3,
            "name": "IS Sharpe and IS grid share",
            "required": "IS Sharpe > 0 and at least 3 of 5 grid cells > 0",
            "actual_sharpe": primary["is"]["sharpe"],
            "actual_grid_positive": grid_pos,
            "actual_grid_cells": 5,
            "pass": line3,
        },
        {
            "line": 4,
            "name": "Full-sample total return at 2 bp per side",
            "required": "> 0",
            "actual": costs["2"]["full_return"],
            "pass": line4,
        },
        {
            "line": 5,
            "name": "EFA OOS Sharpe",
            "required": "> 0",
            "actual": efa_metrics["oos"]["sharpe"],
            "pass": line5,
        },
        {
            "line": 6,
            "name": "OOS trades",
            "required": ">= 100",
            "actual": primary["oos"]["trades"],
            "pass": line6,
        },
    ]
    head, dirty = git_state()
    results = {
        "rules_sha256": lock["sha256"],
        "locked_utc": lock.get("locked_utc"),
        "git_head": head,
        "git_dirty": dirty,
        "numpy": np.__version__,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOT,
            "timing": SEED_TIMING,
            "verify": 20261104,
        },
        "cost_bps_side": COST_BPS,
        "deadzone": 0.0,
        "primary_symbol": "EEM",
        "signal_symbol": "SPY",
        "cross_symbol": "EFA",
        "primary": primary,
        "gross": gross_block,
        "benchmarks": benches,
        "efa": efa_metrics,
        "costs": costs,
        "delay": delay,
        "grid": grid,
        "grid_is_positive": grid_pos,
        "grid_is_cells": 5,
        "direction_placebo": {k: v for k, v in direction.items() if k != "draws"},
        "timing_placebo": {k: v for k, v in timing.items() if k != "draws"},
        "bootstrap": boot,
        "predictions": pred,
        "by_year": by_year(dates, book["net"], book["otc"], book["spy_c2c"], trades),
        "by_side": by_side(trades),
        "by_exit": {"session_close": len(trades)},
        "quintiles": quintiles(trades),
        "how": {
            "sessions": len(dates),
            "sessions_with_trade": int(np.sum(book["weight"] != 0.0)),
            "flat_sessions": int(np.sum(book["weight"] == 0.0)),
            "trades_per_year": float(len(trades) * 252 / len(dates)),
            "exposure": primary["full"]["exposure"],
            "overnight_exposure": 0.0,
            "hold_hours_median": float(np.median(hours)),
            "hold_hours_mean": float(np.mean(hours)),
        },
        "acceptance": acceptance,
        "n_lines": 6,
        "n_failed": int(sum(not row["pass"] for row in acceptance)),
        "status": status,
        "coverage": {
            "eem_bars_through_2026_10_02": 3960,
            "efa_bars_through_2026_10_02": 3960,
            "spy_bars": 3959,
            "eval_sessions": len(dates),
            "eval_start": dates[0].isoformat(),
            "eval_end": dates[-1].isoformat(),
            "excluded_eem_efa_session": "2026-10-02",
        },
    }
    eq = 1.0
    daily_rows = []
    for i, d in enumerate(dates):
        eq *= 1.0 + float(book["net"][i])
        daily_rows.append({
            "date": d.isoformat(),
            "strategy_net": fnum(book["net"][i]),
            "strategy_gross": fnum(book["gross"][i]),
            "eem_otc": fnum(book["otc"][i]),
            "spy_c2c": fnum(book["spy_c2c"][i]),
            "weight": str(int(book["weight"][i])),
            "signal": str(int(book["signal"][i])),
            "spy_prior_ret": fnum(book["prior"][i]),
            "sample": "is" if d < OOS_START else "oos",
            "equity": fnum(eq),
            "efa_net": fnum(efa["net"][i]),
            "efa_gross": fnum(efa["gross"][i]),
        })
    if abs(eq - primary["full"]["terminal_equity"]) > 1e-9:
        raise RuntimeError("equity column does not match compounded return")
    with (HERE / "daily.csv").open("w", encoding="utf-8", newline="\n") as f:
        writer = csv.DictWriter(f, fieldnames=DAILY_FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(daily_rows)
    with (HERE / "trades.csv").open("w", encoding="utf-8", newline="\n") as f:
        writer = csv.DictWriter(f, fieldnames=TRADE_FIELDS, lineterminator="\n")
        writer.writeheader()
        for t in trades:
            writer.writerow({k: (fnum(t[k]) if k in ("gross", "net", "spy_prior_ret") else t[k]) for k in TRADE_FIELDS})
    np.save(HERE / "placebo_direction.npy", direction["draws"])
    np.save(HERE / "placebo_timing.npy", timing["draws"])
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8", newline="\n")
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    append_log(
        f"## {now}\n"
        f"- reason: initial run\n"
        f"- rules_sha256: {lock['sha256']}\n"
        f"- git_head: {head}\n"
        f"- dirty: {dirty}\n"
        f"- full_sharpe: {primary['full']['sharpe']}\n"
        f"- full_return: {primary['full']['total_return']}\n"
        f"- is_sharpe: {primary['is']['sharpe']}\n"
        f"- is_return: {primary['is']['total_return']}\n"
        f"- oos_sharpe: {primary['oos']['sharpe']}\n"
        f"- oos_return: {primary['oos']['total_return']}\n"
        f"- status: {status}\n"
    )
    print(json.dumps({
        "status": status,
        "n_failed": results["n_failed"],
        "oos_sharpe": primary["oos"]["sharpe"],
        "oos_return": primary["oos"]["total_return"],
        "oos_pf": primary["oos"]["profit_factor"],
        "oos_trades": primary["oos"]["trades"],
        "is_sharpe": primary["is"]["sharpe"],
        "full_sharpe": primary["full"]["sharpe"],
        "full_return": primary["full"]["total_return"],
        "p_dir": direction["p"],
        "p_tim": timing["p"],
        "grid_pos": grid_pos,
        "ret_2x": costs["2"]["full_return"],
        "efa_oos_sharpe": efa_metrics["oos"]["sharpe"],
        "pred": [pred["prediction_1"], pred["prediction_2"]],
        "boot": [boot["p025"], boot["p975"]],
    }, indent=2))


if __name__ == "__main__":
    main()
