# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered GLD/SLV ratio backtest.

The self-test runs before the store is opened. A hash mismatch or a failed
self-test writes nothing. A store run appends one RUNLOG entry.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import subprocess
import sys
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))

WINDOW = 60
ENTRY = 2.0
COST_BPS = 1.0
OOS_START = date(2024, 7, 1)
IS_END = date(2024, 6, 28)
SAMPLE_END = date(2026, 10, 1)
SKIP = {date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)}
SEED_DIRECTION = 20261081
SEED_BOOTSTRAP = 20261082
SEED_TIMING = 20261083
ANNUAL = 252
WEIGHTS = {
    "flat": (0.0, 0.0),
    "short_ratio": (-0.5, 0.5),
    "long_ratio": (0.5, -0.5),
}


def rules_sha256() -> str:
    payload = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(payload).hexdigest()


def lock_hash() -> str:
    path = HERE / "RULES.lock"
    if not path.is_file():
        print("RULES.lock is missing. Refusing to run.", file=sys.stderr)
        sys.exit(1)
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("sha256 "):
            return line.split(" ", 1)[1].strip()
    print("RULES.lock has no sha256 line. Refusing to run.", file=sys.stderr)
    sys.exit(1)


def assert_lock() -> str:
    have = rules_sha256()
    want = lock_hash()
    if have != want:
        print(f"RULES.md hash {have} does not match RULES.lock {want}. Refusing to run.", file=sys.stderr)
        sys.exit(1)
    return have


def git_state() -> tuple[str, bool]:
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT)
    dirty = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT)
    if head.returncode != 0:
        raise RuntimeError(head.stderr.strip() or "git rev-parse failed")
    return head.stdout.strip(), bool(dirty.stdout.strip())


def z_value(window: list[float]) -> float:
    """Population z of the last point. Divide by N. Exact zero sigma maps to 0."""
    n = len(window)
    mu = 0.0
    for x in window:
        mu += x
    mu /= n
    acc = 0.0
    for x in window:
        diff = x - mu
        acc += diff * diff
    sigma = math.sqrt(acc / n)
    if sigma == 0.0:
        return 0.0
    return (window[-1] - mu) / sigma


def desired_side(side: str, z: float, entry: float) -> str:
    if side == "flat":
        if z > entry:
            return "short_ratio"
        if z < -entry:
            return "long_ratio"
        return "flat"
    if side == "short_ratio":
        if z < -entry:
            return "long_ratio"
        if z <= 0.0:
            return "flat"
        return "short_ratio"
    if z > entry:
        return "short_ratio"
    if z >= 0.0:
        return "flat"
    return "long_ratio"


def run_book(
    sessions: list[date],
    bars_a: dict[date, tuple[float, float]],
    bars_b: dict[date, tuple[float, float]],
    *,
    entry: float = ENTRY,
    window: int = WINDOW,
    cost_mult: float = 1.0,
    delay: int = 1,
    mode: str = "next_open",
    schedule: dict[date, str] | None = None,
    close_flatten: set[date] | None = None,
) -> dict:
    """Dollar-neutral pair. Leg A is the numerator of the ratio.

    mode 'next_open' is the primary. mode 'close' is the labelled upper bound.
    A schedule, when given, replaces the signal. It is the timing placebo.
    """
    rate = (COST_BPS / 10000.0) * cost_mult
    last_session = sessions[-1] if sessions else None
    close_flatten = close_flatten or set()
    return _run_loop(
        sessions, bars_a, bars_b, entry, window, rate, delay, mode, schedule, close_flatten, last_session
    )


def _run_loop(sessions, bars_a, bars_b, entry, window, rate, delay, mode, schedule, close_flatten, last_session):
    st = {
        "cash": 1.0,
        "shares": [0.0, 0.0],
        "side": "flat",
        "trade": None,
        "last": [None, None],
        "next_id": 1,
        "bad_equity": 0,
    }
    pending = None
    ratios: list[float] = []
    pair_i = -1
    trades: list[dict] = []
    daily: list[dict] = []

    def equity_at(prices):
        return st["cash"] + st["shares"][0] * prices[0] + st["shares"][1] * prices[1]

    def fill_to(target, prices, equity, close_reason, when):
        if target != "flat" and not equity > 0.0:
            target = "flat"
            close_reason = "flat"
            st["bad_equity"] += 1
        new = [0.0, 0.0]
        if target != "flat":
            w = WEIGHTS[target]
            new = [w[0] * equity / prices[0], w[1] * equity / prices[1]]
        closed_cost = 0.0
        opened_cost = 0.0
        for i in range(2):
            delta = new[i] - st["shares"][i]
            px = prices[i]
            cost = abs(delta) * px * rate
            closed_n = abs(st["shares"][i]) * px
            opened_n = abs(new[i]) * px
            den = closed_n + opened_n
            if den > 0.0:
                closed_cost += cost * (closed_n / den)
                opened_cost += cost * (opened_n / den)
            st["cash"] -= delta * px + cost
        old = st["side"]
        if st["trade"] is not None and target != old:
            tr = st["trade"]
            tr["cost"] += closed_cost
            tr["exit_date"] = when
            tr["exit_prices"] = (prices[0], prices[1])
            tr["exit_reason"] = close_reason
            tr["net"] = tr["gross"] - tr["cost"]
            trades.append(tr)
            st["trade"] = None
        st["shares"] = new
        st["side"] = target
        if target != "flat":
            st["trade"] = {
                "id": st["next_id"],
                "side": target,
                "entry_date": when,
                "entry_prices": (prices[0], prices[1]),
                "shares": (new[0], new[1]),
                "gross": 0.0,
                "a_gross": 0.0,
                "b_gross": 0.0,
                "cost": opened_cost,
                "exit_date": None,
                "exit_prices": None,
                "exit_reason": None,
                "net": None,
            }
            st["next_id"] += 1

    def add_price(tr, a_pnl, b_pnl):
        if tr is None:
            return
        tr["gross"] += a_pnl + b_pnl
        tr["a_gross"] += a_pnl
        tr["b_gross"] += b_pnl

    running = 1.0
    for d in sessions:
        ba = bars_a.get(d)
        bb = bars_b.get(d)
        if ba is None or bb is None:
            daily.append(
                {
                    "date": d,
                    "strategy_net": 0.0,
                    "strategy_gross": 0.0,
                    "equity": running,
                    "prev_equity": running,
                    "side": st["side"],
                    "parts": [],
                    "pair": False,
                }
            )
            continue
        oa, ca = ba
        ob, cb = bb
        prev_equity = running
        parts: list[tuple[int, float]] = []
        gap_a = st["shares"][0] * (oa - st["last"][0]) if st["last"][0] is not None else 0.0
        gap_b = st["shares"][1] * (ob - st["last"][1]) if st["last"][1] is not None else 0.0
        held = st["trade"]
        add_price(held, gap_a, gap_b)
        if held is not None:
            parts.append((held["id"], gap_a + gap_b))
        prices_open = [oa, ob]
        equity_open = equity_at(prices_open)
        is_last = d == last_session
        if schedule is not None:
            target = schedule.get(d, "flat")
            if target != st["side"]:
                if st["side"] == "flat" or target == "flat":
                    reason = "flat"
                else:
                    reason = "flip"
                fill_to(target, prices_open, equity_open, reason, d)
        elif mode == "next_open" and pending is not None and pair_i + 1 - pending["signal_i"] == delay:
            # pair_i is the previous pair session. This session will be pair_i + 1.
            target = pending["side"]
            if st["side"] == "flat" or target == "flat":
                reason = "flat"
            else:
                reason = "flip"
            fill_to(target, prices_open, equity_open, reason, d)
            pending = None
        oc_a = st["shares"][0] * (ca - oa)
        oc_b = st["shares"][1] * (cb - ob)
        held = st["trade"]
        add_price(held, oc_a, oc_b)
        if held is not None:
            parts.append((held["id"], oc_a + oc_b))
        side_after_open = st["side"]
        prices_close = [ca, cb]
        equity_close = equity_at(prices_close)
        pair_i += 1
        ratios.append(ca / cb)
        z = z_value(ratios[-window:]) if len(ratios) >= window else None
        if schedule is not None:
            if d in close_flatten and st["side"] != "flat":
                fill_to("flat", prices_close, equity_close, "end", d)
        elif mode == "close":
            if z is not None:
                want = desired_side(st["side"], z, entry)
                if want != st["side"]:
                    if st["side"] == "flat" or want == "flat":
                        reason = "flat"
                    else:
                        reason = "flip"
                    fill_to(want, prices_close, equity_close, reason, d)
                    equity_close = equity_at(prices_close)
            if is_last and st["side"] != "flat":
                fill_to("flat", prices_close, equity_close, "end", d)
        else:
            if is_last and st["side"] != "flat":
                fill_to("flat", prices_close, equity_close, "end", d)
            if is_last or z is None:
                pending = None
            else:
                want = desired_side(st["side"], z, entry)
                if want == st["side"]:
                    pending = None
                elif pending is None or pending["side"] != want:
                    pending = {"side": want, "signal_i": pair_i}
        st["last"] = [ca, cb]
        if st["side"] == "flat":
            equity_end = st["cash"]
        else:
            equity_end = equity_at(prices_close)
        gross_pnl = gap_a + gap_b + oc_a + oc_b
        net_ret = (equity_end - prev_equity) / prev_equity
        gross_ret = gross_pnl / prev_equity
        running = equity_end
        daily.append(
            {
                "date": d,
                "strategy_net": net_ret,
                "strategy_gross": gross_ret,
                "equity": equity_end,
                "prev_equity": prev_equity,
                "side": side_after_open,
                "parts": parts,
                "pair": True,
            }
        )

    if st["trade"] is not None:
        tr = st["trade"]
        tr["net"] = tr["gross"] - tr["cost"]
    nets = [t["net"] for t in trades]
    if st["trade"] is not None:
        nets.append(st["trade"]["net"])
    final = daily[-1]["equity"] if daily else 1.0
    if abs(sum(nets) - (final - 1.0)) > 1e-8:
        raise RuntimeError(f"trade net {sum(nets)} != equity change {final - 1.0}")
    for t in trades:
        if abs((t["a_gross"] + t["b_gross"]) - t["gross"]) > 1e-8:
            raise RuntimeError("leg gross does not sum to trade gross")
    eq = 1.0
    for row in daily:
        eq *= 1.0 + row["strategy_net"]
    if abs(eq - final) > 1e-6:
        raise RuntimeError(f"compounded equity {eq} != mark {final}")
    return {
        "daily": daily,
        "trades": trades,
        "final_equity": final,
        "bad_equity": st["bad_equity"],
        "n_ratio": len(ratios),
        "open_trade": st["trade"],
    }


def _synth(ratios, missing_b=None):
    start = date(2020, 1, 2)
    sessions = [start + timedelta(days=i) for i in range(len(ratios))]
    bars_a = {}
    bars_b = {}
    missing_b = missing_b or set()
    for i, d in enumerate(sessions):
        bars_a[d] = (ratios[i] * 10.0, ratios[i] * 10.0)
        if i not in missing_b:
            bars_b[d] = (10.0, 10.0)
    return sessions, bars_a, bars_b


def _require(cond, msg):
    if not cond:
        raise AssertionError(msg)


def self_test() -> None:
    spike = [1.0] * 59 + [2.0]
    got = z_value(spike)
    _require(abs(got - math.sqrt(59.0)) < 1e-12, f"spike z {got}")
    follow = z_value(([1.0] * 59 + [2.0, 1.0])[-60:])
    _require(abs(follow - (-1.0 / math.sqrt(59.0))) < 1e-12, f"follow z {follow}")
    _require(z_value([5.0] * 60) == 0.0, "zero sigma")

    sessions, a, b = _synth([1.0] * 59 + [2.0, 1.0, 1.0])
    out = run_book(sessions, a, b)
    _require(len(out["trades"]) == 1, f"entry/exit trades {out['trades']}")
    t = out["trades"][0]
    _require(t["side"] == "short_ratio", t["side"])
    _require(t["entry_date"] == sessions[60], f"entry {t['entry_date']}")
    _require(t["exit_date"] == sessions[61] and t["exit_reason"] == "flat", t["exit_reason"])
    _require(t["shares"][0] < 0.0 and t["shares"][1] > 0.0, t["shares"])
    _require(abs(t["gross"]) < 1e-12, t["gross"])
    _require(abs(out["final_equity"] - 0.9998) < 1e-12, out["final_equity"])

    sessions, a, b = _synth([1.0] * 59 + [0.5, 0.5, 0.5, 0.5])
    out = run_book(sessions, a, b)
    _require(len(out["trades"]) == 1, f"long trades {len(out['trades'])}")
    t = out["trades"][0]
    _require(t["side"] == "long_ratio" and t["entry_date"] == sessions[60], (t["side"], t["entry_date"]))
    _require(t["shares"][0] > 0.0 and t["shares"][1] < 0.0, t["shares"])
    _require(t["exit_reason"] == "end" and t["exit_date"] == sessions[-1], t["exit_reason"])

    path = [1.0] * 59 + [2.0] * 30 + [0.2, 0.2, 0.2]
    sessions, a, b = _synth(path)
    out = run_book(sessions, a, b)
    _require(len(out["trades"]) == 2, f"flip count {len(out['trades'])}")
    a_tr, b_tr = out["trades"]
    _require(a_tr["side"] == "short_ratio" and a_tr["exit_reason"] == "flip", a_tr["exit_reason"])
    _require(a_tr["entry_date"] == sessions[60] and a_tr["exit_date"] == sessions[90], "flip dates")
    _require(b_tr["side"] == "long_ratio" and b_tr["entry_date"] == sessions[90], b_tr["side"])
    _require(b_tr["exit_reason"] == "end" and b_tr["exit_date"] == sessions[91], b_tr["exit_reason"])

    out = run_book(*_synth([5.0] * 80)[:3])
    _require(out["trades"] == [], "zero-sigma window traded")

    ratios = [1.0] * 59 + [2.0, 9.0, 2.0, 9.0, 2.0]
    sessions, a, b = _synth(ratios, missing_b={60, 62})
    out = run_book(sessions, a, b)
    _require(out["n_ratio"] == len(ratios) - 2, out["n_ratio"])
    _require(out["trades"], "missing-bar book did not enter")
    _require(out["trades"][0]["entry_date"] == sessions[61], out["trades"][0]["entry_date"])
    _require(all(t["entry_date"] != sessions[60] for t in out["trades"]), "filled on a missing bar")
    by_date = {row["date"]: row for row in out["daily"]}
    _require(by_date[sessions[60]]["strategy_net"] == 0.0, "missing signal day was not 0")
    _require(by_date[sessions[62]]["strategy_net"] == 0.0, "missing hold day was not 0")
    _require(by_date[sessions[62]]["equity"] == by_date[sessions[61]]["equity"], "marked a missing day")

    sessions, a, b = _synth([1.0] * 59 + [2.0] * 4)
    out = run_book(sessions, a, b)
    _require(len(out["trades"]) == 1, f"last-session trades {len(out['trades'])}")
    t = out["trades"][0]
    _require(t["side"] == "short_ratio" and t["exit_reason"] == "end", t["exit_reason"])
    _require(t["entry_date"] == sessions[60] and t["exit_date"] == sessions[-1], "last hold")
    _require(t["exit_prices"][0] == 20.0 and t["exit_prices"][1] == 10.0, t["exit_prices"])
    _require(abs(out["final_equity"] - 0.9998) < 1e-12, out["final_equity"])

    sessions, a, b = _synth([1.0] * 59 + [2.0])
    out = run_book(sessions, a, b)
    _require(out["trades"] == [], "last-day signal was filled")


def sharpe(values) -> float | None:
    arr = np.asarray(values, dtype=float)
    if arr.size < 2:
        return None
    sd = float(arr.std(ddof=1))
    if sd == 0.0 or not math.isfinite(sd):
        return None
    return float(arr.mean() / sd * math.sqrt(ANNUAL))


def ann_vol(values) -> float | None:
    arr = np.asarray(values, dtype=float)
    if arr.size < 2:
        return None
    sd = float(arr.std(ddof=1))
    if sd == 0.0 or not math.isfinite(sd):
        return None
    return float(sd * math.sqrt(ANNUAL))


def total_return(values) -> float:
    arr = np.asarray(values, dtype=float)
    if arr.size == 0:
        return 0.0
    return float(np.prod(1.0 + arr) - 1.0)


def cagr(values) -> float | None:
    arr = np.asarray(values, dtype=float)
    if arr.size == 0:
        return None
    end = float(np.prod(1.0 + arr))
    if end <= 0.0:
        return None
    return float(end ** (ANNUAL / arr.size) - 1.0)


def max_drawdown(values) -> float:
    arr = np.asarray(values, dtype=float)
    if arr.size == 0:
        return 0.0
    equity = np.cumprod(1.0 + arr)
    peak = np.maximum.accumulate(np.concatenate([np.array([1.0]), equity]))[1:]
    return float((equity / peak - 1.0).min())


def t_stat(values) -> float | None:
    arr = np.asarray(values, dtype=float)
    if arr.size < 2:
        return None
    sd = float(arr.std(ddof=1))
    if sd == 0.0 or not math.isfinite(sd):
        return None
    return float(arr.mean() / (sd / math.sqrt(arr.size)))


def profit_factor(nets) -> float | None:
    if not nets:
        return None
    wins = sum(x for x in nets if x > 0.0)
    losses = -sum(x for x in nets if x < 0.0)
    if losses == 0.0:
        return None
    return wins / losses


def trade_bp(trade) -> float:
    notional = abs(trade["shares"][0]) * trade["entry_prices"][0] + abs(trade["shares"][1]) * trade["entry_prices"][1]
    if notional == 0.0:
        return 0.0
    return trade["net"] / notional * 10000.0


def window_metrics(rows, trades) -> dict:
    rets = [row["strategy_net"] for row in rows]
    gross = [row["strategy_gross"] for row in rows]
    nets = [t["net"] for t in trades]
    exposed = sum(1 for row in rows if row["side"] != "flat")
    return {
        "sessions": len(rows),
        "total_return": total_return(rets),
        "cagr": cagr(rets),
        "ann_vol": ann_vol(rets),
        "sharpe": sharpe(rets),
        "max_dd": max_drawdown(rets),
        "t_stat": t_stat(rets),
        "trades": len(trades),
        "win_rate": (sum(1 for x in nets if x > 0.0) / len(nets)) if nets else None,
        "profit_factor": profit_factor(nets),
        "avg_net_bp": (sum(trade_bp(t) for t in trades) / len(trades)) if trades else None,
        "exposure": (exposed / len(rows)) if rows else None,
        "gross_sharpe": sharpe(gross),
    }


def bench_metrics(values) -> dict:
    return {
        "sessions": len(values),
        "total_return": total_return(values),
        "cagr": cagr(values),
        "ann_vol": ann_vol(values),
        "sharpe": sharpe(values),
        "max_dd": max_drawdown(values),
        "t_stat": t_stat(values),
    }


def slice_rows(rows, start, end):
    return [row for row in rows if start <= row["date"] <= end]


def slice_trades(trades, start, end):
    return [t for t in trades if start <= t["entry_date"] <= end]


def buy_hold(bars, eval_dates):
    out = []
    started = False
    prev_close = None
    for d in eval_dates:
        bar = bars.get(d)
        if bar is None:
            out.append(0.0)
            continue
        o, c = bar
        if not started:
            out.append((c - o) / o)
            started = True
        else:
            out.append(c / prev_close - 1.0)
        prev_close = c
    return out


def gld_close_to_close(bars, eval_dates, prev_close):
    out = []
    prev = prev_close
    for d in eval_dates:
        c = bars[d][1]
        out.append(c / prev - 1.0)
        prev = c
    return out


def pack_window(rows, trades):
    m = window_metrics(rows, trades)
    return m


def summarize_group(trades, keyfn):
    groups: dict = {}
    for t in trades:
        groups.setdefault(keyfn(t), []).append(t)
    out = []
    for key in groups:
        rows = groups[key]
        nets = [t["net"] for t in rows]
        out.append(
            {
                "key": key,
                "trades": len(rows),
                "gross_pnl": sum(t["gross"] for t in rows),
                "net_pnl": sum(nets),
                "profit_factor": profit_factor(nets),
                "win_rate": sum(1 for x in nets if x > 0.0) / len(nets),
                "avg_net_bp": sum(trade_bp(t) for t in rows) / len(rows),
            }
        )
    return out


def by_year(rows, gld, slv):
    buckets: dict[int, list] = {}
    for i, row in enumerate(rows):
        buckets.setdefault(row["date"].year, []).append(i)
    out = []
    for year in sorted(buckets):
        idx = buckets[year]
        r = [rows[i]["strategy_net"] for i in idx]
        out.append(
            {
                "year": year,
                "sessions": len(idx),
                "total_return": total_return(r),
                "sharpe": sharpe(r),
                "max_dd": max_drawdown(r),
                "gld_return": total_return([gld[i] for i in idx]),
                "slv_return": total_return([slv[i] for i in idx]),
            }
        )
    return out


def quintiles(strategy, market, dates):
    order = sorted(range(len(market)), key=lambda i: (market[i], dates[i].isoformat()))
    n = len(order)
    buckets = {k: [] for k in range(1, 6)}
    for rank, i in enumerate(order):
        q = (rank * 5) // n + 1
        buckets[q].append(i)
    out = []
    for q in range(1, 6):
        idx = buckets[q]
        out.append(
            {
                "quintile": q,
                "sessions": len(idx),
                "mean_strategy": float(np.mean([strategy[i] for i in idx])) if idx else None,
                "mean_gld": float(np.mean([market[i] for i in idx])) if idx else None,
            }
        )
    return out


def direction_placebo(rows, trades):
    if not rows:
        return {"p": None, "actual": None, "null_mean": None, "null_std": None, "null_p95": None, "draws": []}
    n = len(rows)
    ids = [t["id"] for t in trades]
    contrib = {tid: np.zeros(n) for tid in ids}
    id_set = set(ids)
    for i, row in enumerate(rows):
        for tid, amt in row["parts"]:
            if tid in id_set:
                contrib[tid][i] += amt
    prev = np.array([row["prev_equity"] for row in rows], dtype=float)
    actual_pnl = np.zeros(n)
    for tid in ids:
        actual_pnl += contrib[tid]
    actual = sharpe(actual_pnl / prev)
    rng = np.random.default_rng(SEED_DIRECTION)
    draws = np.empty(2000)
    n_ge = 0
    for i in range(2000):
        pnl = np.zeros(n)
        signs = rng.integers(0, 2, size=len(ids)) * 2 - 1
        for sign, tid in zip(signs, ids):
            pnl += sign * contrib[tid]
        value = sharpe(pnl / prev)
        draws[i] = np.nan if value is None else value
        if value is not None and actual is not None and value >= actual:
            n_ge += 1
    finite = draws[np.isfinite(draws)]
    return {
        "p": (1 + n_ge) / 2001,
        "actual": actual,
        "null_mean": float(finite.mean()) if finite.size else None,
        "null_std": float(finite.std(ddof=1)) if finite.size > 1 else None,
        "null_p95": float(np.percentile(finite, 95)) if finite.size else None,
        "draws": draws,
        "n_ge": n_ge,
    }


def _trade_interval(trade, index):
    i = index[trade["entry_date"]]
    if trade["exit_reason"] == "end":
        k = index[trade["exit_date"]]
        return i, k + 1, True, k
    j = index[trade["exit_date"]]
    return i, j, False, j


def timing_placebo(sessions, bars_a, bars_b, rows, trades, actual_gross):
    pair_dates = [d for d in sessions if d in bars_a and d in bars_b]
    index = {d: i for i, d in enumerate(pair_dates)}
    n = len(pair_dates)
    eval_dates = [row["date"] for row in rows]
    if not trades or not rows:
        return {"testable": False, "p": None, "attempts": 0, "accepted": 0, "reason": "no trades or no evaluation sessions"}

    def accept(shifts):
        intervals = []
        shifted_entry = []
        for trade, shift in zip(trades, shifts):
            i0, j0, at_close, exit_i0 = _trade_interval(trade, index)
            i = i0 + int(shift)
            if at_close:
                exit_i = exit_i0 + int(shift)
                j = exit_i + 1
                if i < 0 or exit_i > n - 1 or exit_i < i:
                    return None
            else:
                j = j0 + int(shift)
                exit_i = j
                if i < 0 or j > n - 1 or j <= i:
                    return None
            intervals.append((i, j, trade["side"], at_close, exit_i if at_close else None))
            shifted_entry.append(i)
        for a, b in zip(shifted_entry, shifted_entry[1:]):
            if a >= b:
                return None
        ordered = sorted(intervals, key=lambda item: item[0])
        for left, right in zip(ordered, ordered[1:]):
            if right[0] < left[1]:
                return None
        schedule = {d: "flat" for d in pair_dates}
        flatten = set()
        for i, j, side, at_close, exit_i in intervals:
            if at_close:
                for k in range(i, exit_i + 1):
                    schedule[pair_dates[k]] = side
                flatten.add(pair_dates[exit_i])
            else:
                for k in range(i, j):
                    schedule[pair_dates[k]] = side
        return schedule, flatten

    rng = np.random.default_rng(SEED_TIMING)
    accepted = []
    attempts = 0
    while len(accepted) < 500 and attempts < 200000:
        attempts += 1
        shifts = rng.integers(1, 61, size=len(trades))
        built = accept(shifts)
        if built is None:
            continue
        schedule, flatten = built
        replay = run_book(sessions, bars_a, bars_b, cost_mult=0.0, schedule=schedule, close_flatten=flatten)
        by_date = {row["date"]: row["strategy_net"] for row in replay["daily"]}
        series = [by_date[d] for d in eval_dates]
        accepted.append(sharpe(series))
    if len(accepted) < 500:
        return {
            "testable": False,
            "p": None,
            "attempts": attempts,
            "accepted": len(accepted),
            "actual": actual_gross,
            "reason": "fewer than 500 draws stayed inside the sample without overlapping",
        }
    n_ge = sum(1 for value in accepted if value is not None and actual_gross is not None and value >= actual_gross)
    finite = np.array([np.nan if value is None else value for value in accepted], dtype=float)
    good = finite[np.isfinite(finite)]
    return {
        "testable": True,
        "p": (1 + n_ge) / 501,
        "attempts": attempts,
        "accepted": 500,
        "actual": actual_gross,
        "null_mean": float(good.mean()) if good.size else None,
        "null_std": float(good.std(ddof=1)) if good.size > 1 else None,
        "null_p95": float(np.percentile(good, 95)) if good.size else None,
        "draws": finite,
        "n_ge": n_ge,
    }


def block_bootstrap(values):
    arr = np.asarray(values, dtype=float)
    n = arr.size
    if n < 2:
        return {"p2_5": None, "p50": None, "p97_5": None}
    rng = np.random.default_rng(SEED_BOOTSTRAP)
    n_blocks = int(math.ceil(n / 20))
    draws = np.empty(2000)
    for i in range(2000):
        starts = rng.integers(0, n, size=n_blocks)
        chunks = []
        for s in starts:
            if s + 20 <= n:
                chunks.append(arr[s : s + 20])
            else:
                take = 20 - (n - s)
                chunks.append(np.concatenate([arr[s:], arr[:take]]))
        sample = np.concatenate(chunks)[:n]
        value = sharpe(sample)
        draws[i] = np.nan if value is None else value
    finite = draws[np.isfinite(draws)]
    return {
        "p2_5": float(np.percentile(finite, 2.5)),
        "p50": float(np.percentile(finite, 50)),
        "p97_5": float(np.percentile(finite, 97.5)),
        "draws": int(finite.size),
    }


def bars_map(rows):
    return {b.session: (b.open, b.close) for b in rows}


def load_and_check(md):
    from mdq import nyse_sessions

    gld = md.bars("GLD", "1d", start="2011-01-04", end="2026-10-01")
    slv = md.bars("SLV", "1d", start="2011-01-04", end="2026-10-01")
    pplt = md.bars("PPLT", "1d", start="2011-01-04", end="2026-10-01")
    pplt_all = md.bars("PPLT", "1d")
    gld_raw_n = len(md.bars("GLD", "1d", start="2011-01-04", end="2026-10-01", adjust=False))
    problems = []
    if len(gld) != 3959 or gld[0].session.isoformat() != "2011-01-04" or gld[-1].session.isoformat() != "2026-10-01":
        problems.append(f"GLD bars {len(gld)} {gld[0].session if gld else None} {gld[-1].session if gld else None}")
    if len(slv) != 3959 or slv[0].session != gld[0].session or slv[-1].session != gld[-1].session:
        problems.append(f"SLV bars {len(slv)}")
    if len(pplt_all) != 3960 or pplt_all[-1].session.isoformat() != "2026-10-02":
        problems.append(f"PPLT all {len(pplt_all)} last {pplt_all[-1].session if pplt_all else None}")
    if [b.session for b in gld] != [b.session for b in slv] or [b.session for b in gld] != [b.session for b in pplt]:
        problems.append("session sets differ through 2026-10-01")
    if gld_raw_n != 3959:
        problems.append("raw count")
    for name, rows in (("GLD", gld), ("SLV", slv), ("PPLT", pplt)):
        for b in rows:
            if b.open <= 0 or b.high <= 0 or b.low <= 0 or b.close <= 0:
                problems.append(f"nonpositive {name} {b.session}")
                break
        if len({b.session for b in rows}) != len(rows):
            problems.append(f"duplicate {name}")
    if md.corporate_actions("GLD") or md.corporate_actions("SLV"):
        problems.append("unexpected GLD or SLV corporate action")
    acts = md.corporate_actions("PPLT")
    if len(acts) != 1 or acts[0]["type"] != "split" or acts[0]["split_ratio"] != 10.0 or acts[0]["ex_date"] != "2026-05-18":
        problems.append(f"PPLT actions {acts}")
    raw = md.bars("PPLT", "1d", adjust=False)
    adj = pplt_all
    raw_ex = [b for b in raw if b.session.isoformat() <= "2026-05-18"]
    adj_ex = [b for b in adj if b.session.isoformat() <= "2026-05-18"]
    raw_ratio = raw_ex[-1].close / raw_ex[-2].close
    adj_ratio = adj_ex[-1].close / adj_ex[-2].close
    if raw_ex[-2].session.isoformat() != "2026-05-15" or not raw_ratio < 0.2 or not 0.5 <= adj_ratio <= 1.5:
        problems.append(f"PPLT jump raw {raw_ratio} adj {adj_ratio}")
    book = [d for d in nyse_sessions("2011-01-04", "2026-10-01") if d not in SKIP]
    if len(book) != 3959 or any(d in book for d in SKIP) or book != [b.session for b in gld]:
        problems.append(f"book {len(book)}")
    if problems:
        for item in problems:
            print(item, file=sys.stderr)
        raise RuntimeError("data checks failed")
    return book, bars_map(gld), bars_map(slv), bars_map(pplt)


def py(value):
    if value is None:
        return None
    if isinstance(value, (np.floating, np.integer)):
        value = value.item()
    if isinstance(value, float) and not math.isfinite(value):
        return None
    return value


def clean(obj):
    if isinstance(obj, dict):
        return {k: clean(v) for k, v in obj.items() if k != "draws"}
    if isinstance(obj, list):
        return [clean(v) for v in obj]
    if isinstance(obj, (date, datetime)):
        return obj.isoformat()
    if isinstance(obj, tuple):
        return [clean(v) for v in obj]
    return py(obj) if not isinstance(obj, (str, int, bool)) else obj


def fmt(value) -> str:
    return format(value, ".17g")


def write_daily(path, rows):
    fields = ["date", "strategy_net", "strategy_gross", "gld_bh", "slv_bh"]
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    "date": row["date"].isoformat(),
                    "strategy_net": fmt(row["strategy_net"]),
                    "strategy_gross": fmt(row["strategy_gross"]),
                    "gld_bh": fmt(row["gld_bh"]),
                    "slv_bh": fmt(row["slv_bh"]),
                }
            )


def write_trades(path, trades):
    fields = [
        "side",
        "entry_date",
        "exit_date",
        "a_entry",
        "b_entry",
        "a_exit",
        "b_exit",
        "a_shares",
        "b_shares",
        "gross_pnl",
        "net_pnl",
        "a_gross",
        "b_gross",
        "cost",
        "exit_reason",
    ]
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for t in trades:
            writer.writerow(
                {
                    "side": t["side"],
                    "entry_date": t["entry_date"].isoformat(),
                    "exit_date": t["exit_date"].isoformat(),
                    "a_entry": fmt(t["entry_prices"][0]),
                    "b_entry": fmt(t["entry_prices"][1]),
                    "a_exit": fmt(t["exit_prices"][0]),
                    "b_exit": fmt(t["exit_prices"][1]),
                    "a_shares": fmt(t["shares"][0]),
                    "b_shares": fmt(t["shares"][1]),
                    "gross_pnl": fmt(t["gross"]),
                    "net_pnl": fmt(t["net"]),
                    "a_gross": fmt(t["a_gross"]),
                    "b_gross": fmt(t["b_gross"]),
                    "cost": fmt(t["cost"]),
                    "exit_reason": t["exit_reason"],
                }
            )


def append_runlog(doc, reason):
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nOne entry per store run, appended by the scripts. Never edited.\n", encoding="utf-8", newline="\n")
    primary = doc["primary"]
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")

    def f3(value):
        return "null" if value is None else f"{value:.3f}"

    text = (
        f"\n## {stamp}\n\n"
        f"- Reason: {reason}\n"
        f"- Rules sha256: `{doc['rules_sha256']}`\n"
        f"- Git HEAD: `{doc['git_head']}` (dirty: {doc['git_dirty']})\n"
        f"- Sharpe full / IS / OOS: {f3(primary['full']['sharpe'])} / {f3(primary['is']['sharpe'])} / {f3(primary['oos']['sharpe'])}\n"
        f"- Total return full / IS / OOS: {f3(primary['full']['total_return'])} / {f3(primary['is']['total_return'])} / {f3(primary['oos']['total_return'])}\n"
        f"- Trades full / OOS: {primary['full']['trades']} / {primary['oos']['trades']}\n"
        f"- Line 6 OOS flat-or-flip: {doc['line6_oos_round_trips']}\n"
        f"- Status: {doc['status']}\n"
    )
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def three_windows(rows, trades, first, last):
    full_rows = slice_rows(rows, first, last)
    is_rows = slice_rows(rows, first, IS_END)
    oos_rows = slice_rows(rows, max(first, OOS_START), last)
    return {
        "full": pack_window(full_rows, slice_trades(trades, first, last)),
        "is": pack_window(is_rows, slice_trades(trades, first, IS_END)) if is_rows else pack_window([], []),
        "oos": pack_window(oos_rows, slice_trades(trades, OOS_START, last)) if oos_rows else pack_window([], []),
    }


def headline_from_run(run, first_override=None):
    trades = run["trades"]
    if not trades:
        return {"full": pack_window([], []), "is": pack_window([], []), "oos": pack_window([], [])}
    first = first_override or trades[0]["entry_date"]
    last = SAMPLE_END
    rows = [row for row in run["daily"] if first <= row["date"] <= last]
    return three_windows(rows, trades, first, last)


def main():
    digest = assert_lock()
    self_test()
    reason = sys.argv[1] if len(sys.argv) > 1 else "initial pre-registered run"
    from mdq import MarketData

    head, dirty = git_state()
    with MarketData() as md:
        sessions, gld, slv, pplt = load_and_check(md)
    primary = run_book(sessions, gld, slv)
    if primary["open_trade"] is not None:
        raise RuntimeError("primary left a trade open")
    trades = primary["trades"]
    if not trades:
        first = None
        eval_rows = []
    else:
        first = trades[0]["entry_date"]
        eval_rows = [row for row in primary["daily"] if first <= row["date"] <= SAMPLE_END]
    windows = three_windows(eval_rows, trades, first, SAMPLE_END) if first else {
        "full": pack_window([], []),
        "is": pack_window([], []),
        "oos": pack_window([], []),
    }
    gld_bh = buy_hold(gld, [row["date"] for row in eval_rows]) if eval_rows else []
    slv_bh = buy_hold(slv, [row["date"] for row in eval_rows]) if eval_rows else []
    for row, g, s in zip(eval_rows, gld_bh, slv_bh):
        row["gld_bh"] = g
        row["slv_bh"] = s
    bench = {}
    if eval_rows:
        is_idx = [i for i, row in enumerate(eval_rows) if row["date"] <= IS_END]
        oos_idx = [i for i, row in enumerate(eval_rows) if row["date"] >= OOS_START]
        for name, series in (("gld", gld_bh), ("slv", slv_bh)):
            bench[name] = {
                "full": bench_metrics(series),
                "is": bench_metrics([series[i] for i in is_idx]),
                "oos": bench_metrics([series[i] for i in oos_idx]),
            }
    cost_rows = []
    for mult in (0.0, 0.5, 1.0, 2.0, 3.0):
        run = primary if mult == 1.0 else run_book(sessions, gld, slv, cost_mult=mult)
        packed = headline_from_run(run)
        cost_rows.append(
            {
                "multiple": mult,
                "full_sharpe": packed["full"]["sharpe"],
                "oos_sharpe": packed["oos"]["sharpe"],
                "full_return": packed["full"]["total_return"],
            }
        )
    delay_run = headline_from_run(run_book(sessions, gld, slv, delay=2))
    close_run = headline_from_run(run_book(sessions, gld, slv, mode="close"))
    grid = []
    for level in (1.0, 1.5, 2.0, 2.5, 3.0):
        run = primary if level == 2.0 else run_book(sessions, gld, slv, entry=level)
        packed = headline_from_run(run)
        grid.append(
            {
                "entry": level,
                "first_fill": run["trades"][0]["entry_date"].isoformat() if run["trades"] else None,
                "is_sharpe": packed["is"]["sharpe"],
                "is_return": packed["is"]["total_return"],
                "oos_sharpe": packed["oos"]["sharpe"],
                "oos_return": packed["oos"]["total_return"],
                "full_sharpe": packed["full"]["sharpe"],
            }
        )
    if grid[2]["is_sharpe"] != windows["is"]["sharpe"]:
        raise RuntimeError("grid E=2 IS Sharpe does not match the primary")
    cross = run_book(sessions, gld, pplt)
    cross_packed = headline_from_run(cross)
    if eval_rows:
        prev_date = sessions[sessions.index(first) - 1]
        market = gld_close_to_close(gld, [row["date"] for row in eval_rows], gld[prev_date][1])
        q = quintiles([row["strategy_net"] for row in eval_rows], market, [row["date"] for row in eval_rows])
        years = by_year(eval_rows, gld_bh, slv_bh)
    else:
        q = []
        years = []
    completed = [t for t in trades if t["entry_date"] >= OOS_START and t["exit_reason"] in ("flat", "flip")]
    dir_res = direction_placebo(eval_rows, trades)
    timing = timing_placebo(sessions, gld, slv, eval_rows, trades, windows["full"]["gross_sharpe"])
    boot = block_bootstrap([row["strategy_net"] for row in eval_rows])
    short_gross = sum(t["gross"] for t in trades if t["side"] == "short_ratio")
    long_gross = sum(t["gross"] for t in trades if t["side"] == "long_ratio")
    gold_gross = sum(t["a_gross"] for t in trades)
    silver_gross = sum(t["b_gross"] for t in trades)
    predictions = {
        "short_ratio_gross": short_gross,
        "long_ratio_gross": long_gross,
        "prediction_1": short_gross > 0.0 and long_gross > 0.0,
        "gold_gross": gold_gross,
        "silver_gross": silver_gross,
        "prediction_2": gold_gross > 0.0 and silver_gross > 0.0,
    }
    positive_cells = sum(1 for cell in grid if cell["is_sharpe"] is not None and cell["is_sharpe"] > 0.0)
    oos = windows["oos"]
    is_m = windows["is"]
    cost2 = next(row for row in cost_rows if row["multiple"] == 2.0)
    line1 = oos["sharpe"] is not None and oos["sharpe"] >= 0.5 and oos["profit_factor"] is not None and oos["profit_factor"] >= 1.10
    line2 = dir_res["p"] is not None and dir_res["p"] <= 0.05
    line3 = is_m["sharpe"] is not None and is_m["sharpe"] > 0.0 and positive_cells >= 3
    line4 = cost2["full_return"] is not None and cost2["full_return"] > 0.0
    line5 = cross_packed["oos"]["sharpe"] is not None and cross_packed["oos"]["sharpe"] > 0.0
    line6 = len(completed) >= 15
    if not line6:
        status = "Inconclusive"
    elif not (line1 and line2 and line3 and line4 and line5):
        status = "Rejected"
    else:
        status = "Paper-trading candidate"
    lines = [
        {"line": 1, "pass": line1, "actual": {"oos_sharpe": oos["sharpe"], "oos_profit_factor": oos["profit_factor"]}},
        {"line": 2, "pass": line2, "actual": {"p": dir_res["p"]}},
        {"line": 3, "pass": line3, "actual": {"is_sharpe": is_m["sharpe"], "positive_cells": positive_cells}},
        {"line": 4, "pass": line4, "actual": {"full_return_2x": cost2["full_return"]}},
        {"line": 5, "pass": line5, "actual": {"cross_oos_sharpe": cross_packed["oos"]["sharpe"]}},
        {"line": 6, "pass": line6, "actual": {"oos_flat_or_flip": len(completed)}},
    ]
    holds = [t for t in trades if t["exit_reason"] in ("flat", "flip")]
    hold_sessions = []
    # Holding time needs the pair index. Primary pair sessions are the book.
    pair_index = {d: i for i, d in enumerate(sessions)}
    for t in trades:
        i = pair_index[t["entry_date"]]
        j = pair_index[t["exit_date"]]
        hold_sessions.append((j - i + 1) if t["exit_reason"] == "end" else (j - i))
    doc = {
        "rules_sha256": digest,
        "git_head": head,
        "git_dirty": dirty,
        "reason": reason,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOTSTRAP,
            "timing": SEED_TIMING,
            "verify": 20261084,
        },
        "sample": {
            "first_fill": None if first is None else first.isoformat(),
            "is_end": IS_END.isoformat(),
            "oos_start": OOS_START.isoformat(),
            "last": SAMPLE_END.isoformat(),
        },
        "bad_equity_flattens": primary["bad_equity"],
        "primary": windows,
        "benchmarks": bench,
        "predictions": predictions,
        "costs": cost_rows,
        "delay": {
            "full_sharpe": delay_run["full"]["sharpe"],
            "oos_sharpe": delay_run["oos"]["sharpe"],
            "full_return": delay_run["full"]["total_return"],
        },
        "close_fill": {
            "full_sharpe": close_run["full"]["sharpe"],
            "oos_sharpe": close_run["oos"]["sharpe"],
            "full_return": close_run["full"]["total_return"],
        },
        "grid": grid,
        "grid_positive_is_cells": positive_cells,
        "cross_market": cross_packed,
        "cross_trades": len(cross["trades"]),
        "direction_placebo": {k: v for k, v in dir_res.items() if k != "draws"},
        "timing_placebo": {k: v for k, v in timing.items() if k != "draws"},
        "bootstrap": boot,
        "by_year": years,
        "by_side": summarize_group(trades, lambda t: t["side"]),
        "by_exit": summarize_group(trades, lambda t: t["exit_reason"]),
        "quintiles": q,
        "line6_oos_round_trips": len(completed),
        "oos_trades_including_end": windows["oos"]["trades"],
        "holding_sessions_median": float(np.median(hold_sessions)) if hold_sessions else None,
        "holding_sessions_mean": float(np.mean(hold_sessions)) if hold_sessions else None,
        "acceptance": lines,
        "status": status,
        "final_equity": primary["final_equity"],
    }
    # holding median uses all trades. `holds` kept the definition visible for line 6's subset; the
    # reported holding time is every closed trade, as locked.
    del holds
    np.save(HERE / "placebo_direction.npy", dir_res["draws"])
    if "draws" in timing:
        np.save(HERE / "placebo_timing.npy", timing["draws"])
    (HERE / "results.json").write_text(json.dumps(clean(doc), indent=2) + "\n", encoding="utf-8", newline="\n")
    write_daily(HERE / "daily.csv", eval_rows)
    write_trades(HERE / "trades.csv", trades)
    write_trades(HERE / "cross_trades.csv", cross["trades"])
    append_runlog(clean(doc), reason)
    print(
        f"status {status} OOS sharpe {oos['sharpe']} OOS return {oos['total_return']} "
        f"line6 {len(completed)} trades {len(trades)}"
    )


if __name__ == "__main__":
    main()
