# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered FX ETF time-series momentum.

Refuses to run unless RULES.md hashes to RULES.lock (CRLF normalized to LF).
The synthetic self-test runs before the store is opened. A store run appends
to RUNLOG.md and writes results.json, daily.csv, and trades.csv.
"""

import csv
import hashlib
import math
import subprocess
import sys
from datetime import date, timedelta
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    MarketData,
    nyse_sessions,
    session_date_of,
)

# Constants mirror RULES.md. Do not retune them from results.
NAMES = ("FXE", "FXB", "FXA", "FXC", "FXF", "FXY")
DENOM = 6
LOOKBACK = 63
GRID = (21, 42, 63, 84, 126)
COST_BPS = 5.0
COST_RATE = 0.0005
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
CLOSED = frozenset({date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)})
DATA_START = date(2011, 1, 4)
DATA_END = date(2026, 10, 1)
OOS_START = date(2024, 7, 1)
N_DIR = 2000
N_TIME = 500
N_BOOT = 2000
BLOCK = 20
SEED_DIR = 20261031
SEED_BOOT = 20261032
SEED_TIME = 20261033
SEED_VERIFY = 20261034
ANNUAL = 252


def rules_hash():
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    have = hashlib.sha256(raw).hexdigest()
    lock = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()
    want = lock[0].split()[1]
    if have != want:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    locked = lock[1].split(" ", 1)[1] if len(lock) > 1 else ""
    return have, locked


def sign_ret(r):
    if r > 0.0:
        return 1
    if r < 0.0:
        return -1
    return 0


def sgn_shares(x):
    if x > 0.0:
        return 1
    if x < 0.0:
        return -1
    return 0


def sharpe(r):
    r = np.asarray(r, float)
    if len(r) < 2:
        return None
    sd = r.std(ddof=1)
    if sd == 0.0 or not np.isfinite(sd):
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def sharpe_rows(a):
    """Sharpe of each row. Undefined rows are 0, matching the placebo rule."""
    mu = a.mean(axis=1)
    sd = a.std(axis=1, ddof=1)
    out = np.zeros(len(a))
    ok = sd > 0.0
    out[ok] = mu[ok] / sd[ok] * math.sqrt(ANNUAL)
    return out


def compound_stats(r):
    r = np.asarray(r, float)
    n = len(r)
    if n == 0:
        return {"sessions": 0, "total_return": None, "cagr": None, "ann_vol": None,
                "sharpe": None, "max_dd": None, "t_stat": None, "equity_end": None}
    eq = np.cumprod(1.0 + r)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    sd = float(r.std(ddof=1)) if n > 1 else 0.0
    end = float(eq[-1])
    return {
        "sessions": n,
        "total_return": float(end - 1.0),
        "cagr": float(end ** (ANNUAL / n) - 1.0) if end > 0.0 else None,
        "ann_vol": float(sd * math.sqrt(ANNUAL)) if n > 1 else None,
        "sharpe": sharpe(r),
        "max_dd": float((eq / peak - 1.0).min()),
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if sd > 0.0 else None,
        "equity_end": end,
    }


def profit_factor(dollars):
    dollars = np.asarray(dollars, float)
    if len(dollars) == 0:
        return None
    wins = float(dollars[dollars > 0.0].sum())
    losses = float(dollars[dollars < 0.0].sum())
    if losses < 0.0:
        return wins / abs(losses)
    if wins > 0.0:
        return "inf"
    return None


def pf_ok(pf, floor):
    if pf == "inf":
        return True
    if pf is None:
        return False
    return pf >= floor


def month_signal_dates(book_days):
    """Last NYSE session of each month, if that session is in the book.

    The month's last session comes from nyse_sessions minus CLOSED, not from
    the last stored bar. A trailing partial month is not a signal.
    """
    book = set(book_days)
    if not book_days:
        return []
    y, m = book_days[0].year, book_days[0].month
    end_y, end_m = book_days[-1].year, book_days[-1].month
    out = []
    while (y, m) <= (end_y, end_m):
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        month_last = nxt - timedelta(days=1)
        sessions = [d for d in nyse_sessions(date(y, m, 1), month_last) if d not in CLOSED]
        if sessions and sessions[-1] in book:
            out.append(sessions[-1])
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return out


def simulate(names, calendar, bars, signal_dates, lookback, cost_rate, mode,
             fill_delay=0, fill_at_close=False, signal_override=None):
    """Book the locked accounting. `bars[name][date] = (open, close)`."""
    cal_index = {d: i for i, d in enumerate(calendar)}
    series = {}
    own_index = {}
    for n in names:
        seq = []
        own_index[n] = {}
        for d in calendar:
            bar = bars[n].get(d)
            if bar is not None:
                own_index[n][d] = len(seq)
                seq.append(bar[1])
        series[n] = seq

    def signal_sign(name, d):
        if mode == "longonly":
            return 1
        if signal_override is not None:
            return int(signal_override[name][d])
        i = own_index[name].get(d)
        if i is None or i < lookback:
            return 0
        r = series[name][i] / series[name][i - lookback] - 1.0
        return sign_ret(r)

    eligible = [d for d in signal_dates if cal_index[d] >= lookback]
    signals = {}
    fill_weights = {}
    for d in eligible:
        signs = {n: signal_sign(n, d) for n in names}
        signals[d] = signs
        weights = {n: signs[n] / DENOM for n in names}
        if fill_at_close:
            fill_weights[d] = weights
        else:
            fi = cal_index[d] + 1 + fill_delay
            if fi < len(calendar):
                fill_weights[calendar[fi]] = weights

    equity = 1.0
    shares = {n: 0.0 for n in names}
    last_close = {n: None for n in names}
    open_tr = {n: None for n in names}
    closed = []
    daily = []

    def new_trade(name, side, d, px, sh, gross, cost, piece):
        return {
            "name": name, "side": side, "entry_date": d, "entry_price": px,
            "entry_shares": sh, "gross": gross, "cost": cost,
            "pieces": [(d, piece)], "exit_date": None, "exit_price": None, "reason": None,
        }

    for d in calendar:
        weights = fill_weights.get(d)
        is_fill = weights is not None
        equity_prev = equity
        bar_of = {n: bars[n].get(d) for n in names}

        if not fill_at_close:
            gap_n = {}
            gap = 0.0
            for n in names:
                bar = bar_of[n]
                g = 0.0
                if bar is not None and shares[n] != 0.0 and last_close[n] is not None:
                    g = shares[n] * (bar[0] - last_close[n])
                gap_n[n] = g
                gap += g
            equity_open = equity_prev + gap
            new_shares = dict(shares)
            cost = 0.0
            if is_fill:
                for n in names:
                    bar = bar_of[n]
                    if bar is None:
                        continue
                    tgt = weights[n] * equity_open / bar[0]
                    cost += cost_rate * abs(tgt - shares[n]) * bar[0]
                    new_shares[n] = tgt
            oc_n = {}
            oc = 0.0
            for n in names:
                bar = bar_of[n]
                v = 0.0
                if bar is not None:
                    v = new_shares[n] * (bar[1] - bar[0])
                    last_close[n] = bar[1]
                oc_n[n] = v
                oc += v
            for n in names:
                bar = bar_of[n]
                if bar is None:
                    continue
                old, new = shares[n], new_shares[n]
                os_, ns_ = sgn_shares(old), sgn_shares(new)
                if os_ != 0 and open_tr[n] is None:
                    raise AssertionError(f"shares without a trade on {d} {n}")
                if open_tr[n] is not None:
                    open_tr[n]["gross"] += gap_n[n]
                    open_tr[n]["pieces"].append((d, gap_n[n]))
                px = bar[0]
                if os_ != 0 and ns_ != os_:
                    open_tr[n]["cost"] += cost_rate * abs(old) * px
                    open_tr[n]["exit_date"] = d
                    open_tr[n]["exit_price"] = px
                    open_tr[n]["reason"] = "flip" if ns_ != 0 else "flat"
                    closed.append(open_tr[n])
                    open_tr[n] = None
                if ns_ != 0 and ns_ != os_:
                    side = "long" if ns_ > 0 else "short"
                    open_cost = cost_rate * abs(new) * px
                    open_tr[n] = new_trade(n, side, d, px, new, oc_n[n], open_cost, oc_n[n])
                elif ns_ != 0 and ns_ == os_:
                    if is_fill:
                        open_tr[n]["cost"] += cost_rate * abs(new - old) * px
                    open_tr[n]["gross"] += oc_n[n]
                    open_tr[n]["pieces"].append((d, oc_n[n]))
            equity = equity_open - cost + oc
            price = gap + oc
        else:
            old_pnl = {}
            price_old = 0.0
            for n in names:
                bar = bar_of[n]
                v = 0.0
                if bar is not None and shares[n] != 0.0 and last_close[n] is not None:
                    v = shares[n] * (bar[1] - last_close[n])
                old_pnl[n] = v
                price_old += v
            equity_at_close = equity_prev + price_old
            new_shares = dict(shares)
            cost = 0.0
            if is_fill:
                for n in names:
                    bar = bar_of[n]
                    if bar is None:
                        continue
                    tgt = weights[n] * equity_at_close / bar[1]
                    cost += cost_rate * abs(tgt - shares[n]) * bar[1]
                    new_shares[n] = tgt
            for n in names:
                bar = bar_of[n]
                if bar is None:
                    continue
                old, new = shares[n], new_shares[n]
                os_, ns_ = sgn_shares(old), sgn_shares(new)
                if os_ != 0 and open_tr[n] is None:
                    raise AssertionError(f"shares without a trade on {d} {n}")
                if open_tr[n] is not None:
                    open_tr[n]["gross"] += old_pnl[n]
                    open_tr[n]["pieces"].append((d, old_pnl[n]))
                px = bar[1]
                if os_ != 0 and ns_ != os_:
                    open_tr[n]["cost"] += cost_rate * abs(old) * px
                    open_tr[n]["exit_date"] = d
                    open_tr[n]["exit_price"] = px
                    open_tr[n]["reason"] = "flip" if ns_ != 0 else "flat"
                    closed.append(open_tr[n])
                    open_tr[n] = None
                if ns_ != 0 and ns_ != os_:
                    side = "long" if ns_ > 0 else "short"
                    open_cost = cost_rate * abs(new) * px
                    open_tr[n] = new_trade(n, side, d, px, new, 0.0, open_cost, 0.0)
                elif ns_ != 0 and ns_ == os_ and is_fill:
                    open_tr[n]["cost"] += cost_rate * abs(new - old) * px
                if bar is not None:
                    last_close[n] = bar[1]
            equity = equity_at_close - cost
            price = price_old
            oc_n = {n: 0.0 for n in names}
            gap_n = old_pnl

        shares = new_shares
        if equity_prev != 0.0:
            net_r = equity / equity_prev - 1.0
            gross_r = price / equity_prev
        else:
            net_r = 0.0
            gross_r = 0.0
        gexp = nexp = 0.0
        if equity != 0.0:
            for n in names:
                bar = bar_of[n]
                if bar is None:
                    continue
                gexp += abs(shares[n]) * bar[1]
                nexp += shares[n] * bar[1]
            gexp /= equity
            nexp /= equity
        by_name = {n: (gap_n[n] + oc_n[n]) for n in names}
        daily.append({
            "date": d, "net": net_r, "gross": gross_r, "equity": equity,
            "equity_prev": equity_prev, "cost": cost, "net_dollar": price - cost,
            "gross_dollar": price, "by_name": by_name, "shares": dict(shares),
            "in_market": any(s != 0.0 for s in shares.values()),
            "gross_exposure": gexp, "net_exposure": nexp,
        })

    if calendar:
        last_d = calendar[-1]
        for n in names:
            tr = open_tr[n]
            if tr is None:
                continue
            tr["exit_date"] = last_d
            tr["exit_price"] = last_close[n]
            tr["reason"] = "end"
            closed.append(tr)
            open_tr[n] = None

    for t in closed:
        t["net_dollar"] = t["gross"] - t["cost"]
        t["entry_notional"] = abs(t["entry_shares"]) * t["entry_price"]
        t["gross_return"] = t["gross"] / t["entry_notional"]
        t["net_return"] = t["net_dollar"] / t["entry_notional"]
        t["hold"] = cal_index[t["exit_date"]] - cal_index[t["entry_date"]]
        piece_sum = sum(p for _, p in t["pieces"])
        if abs(piece_sum - t["gross"]) > 1e-8:
            raise AssertionError(f"piece sum {piece_sum} != gross {t['gross']} for {t['name']}")

    net_sum = sum(t["net_dollar"] for t in closed)
    if abs((equity - 1.0) - net_sum) > 1e-8:
        raise AssertionError(f"trade net {net_sum} != equity change {equity - 1.0}")
    dollar_sum = sum(rec["net_dollar"] for rec in daily)
    if abs(dollar_sum - (equity - 1.0)) > 1e-6:
        raise AssertionError(f"daily dollars {dollar_sum} != equity change {equity - 1.0}")
    return {"daily": daily, "trades": closed, "signals": signals, "equity": equity}


def _bars_from(spec, names):
    """spec: list of (date, {name: (open, close) or None}). Missing names have no bar."""
    calendar = [d for d, _ in spec]
    bars = {n: {} for n in names}
    for d, row in spec:
        for n, v in row.items():
            if v is not None:
                bars[n][d] = v
    return calendar, bars


def _flat(names, px=100.0):
    return {n: (px, px) for n in names}


def selftest():
    fails = []

    def check(name, cond):
        if not cond:
            fails.append(name)

    check("constants", LOOKBACK == 63 and DENOM == 6 and COST_RATE == 0.0005 and len(NAMES) == 6)

    names = ("E", "B", "A", "C", "F", "Y")
    d0, d1, d2, d3 = date(2020, 1, 2), date(2020, 1, 3), date(2020, 1, 6), date(2020, 1, 7)
    d4, d5 = date(2020, 1, 8), date(2020, 1, 9)

    # Positive signal, sample-end mark, no exit cost, size before cost, flat names stay 1/6.
    row2 = _flat(names)
    row2["E"] = (100.0, 110.0)
    row3 = _flat(names)
    row3["E"] = (120.0, 132.0)
    cal, bars = _bars_from([(d0, _flat(names)), (d1, _flat(names)), (d2, row2), (d3, row3)], names)
    r = simulate(names, cal, bars, [d2], 2, 0.01, "tsmom")
    e = r["daily"][-1]
    sh = e["shares"]["E"]
    check("positive equity", abs(e["equity"] - 1.015) < 1e-12)
    check("positive shares", abs(sh - (1.0 / 6.0) / 120.0) < 1e-12)
    check("not rescaled to 1", abs(abs(sh) * 120.0 - 1.0 / 6.0) < 1e-12)
    check("others flat", all(e["shares"][n] == 0.0 for n in names if n != "E"))
    check("warm-up flat", all(rec["net"] == 0.0 for rec in r["daily"][:3]))
    check("one long end", len(r["trades"]) == 1 and r["trades"][0]["side"] == "long"
          and r["trades"][0]["reason"] == "end"
          and r["trades"][0]["entry_price"] == 120.0 and r["trades"][0]["exit_price"] == 132.0)
    tr = r["trades"][0]
    check("no exit cost", abs(tr["cost"] - 0.01 / 6.0) < 1e-12)
    check("gross 1/60", abs(tr["gross"] - 1.0 / 60.0) < 1e-12)
    check("net 0.015", abs(tr["net_dollar"] - 0.015) < 1e-12)

    # Negative signal.
    row2 = _flat(names)
    row2["E"] = (100.0, 90.0)
    row3 = _flat(names)
    row3["E"] = (80.0, 76.0)
    cal, bars = _bars_from([(d0, _flat(names)), (d1, _flat(names)), (d2, row2), (d3, row3)], names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom")
    check("negative equity", abs(r["equity"] - (121.0 / 120.0)) < 1e-12)
    check("short end", len(r["trades"]) == 1 and r["trades"][0]["side"] == "short"
          and r["trades"][0]["entry_price"] == 80.0 and r["trades"][0]["exit_price"] == 76.0
          and r["trades"][0]["reason"] == "end")
    check("short gross", abs(r["trades"][0]["gross"] - 1.0 / 120.0) < 1e-12)

    # Exact zero, with a different intermediate close.
    z0, z1, z2, z3 = _flat(names), _flat(names), _flat(names), _flat(names)
    z1["E"] = (100.0, 50.0)
    z2["E"] = (50.0, 100.0)
    z3["E"] = (100.0, 200.0)
    cal, bars = _bars_from([(d0, z0), (d1, z1), (d2, z2), (d3, z3)], names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom")
    check("exact zero", r["signals"][d2]["E"] == 0 and r["equity"] == 1.0 and r["trades"] == [])

    # Two positive names stay at 1/6 each, not 1/2.
    p2 = _flat(names)
    p2["E"] = (100.0, 110.0)
    p2["B"] = (100.0, 130.0)
    cal, bars = _bars_from([(d0, _flat(names)), (d1, _flat(names)), (d2, p2), (d3, _flat(names))], names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom")
    last = r["daily"][-1]
    check("two weights", abs(abs(last["shares"]["E"]) * 100.0 - 1.0 / 6.0) < 1e-12
          and abs(abs(last["shares"]["B"]) * 100.0 - 1.0 / 6.0) < 1e-12)
    check("gross notional 1/3", abs(
        (abs(last["shares"]["E"]) + abs(last["shares"]["B"])) * 100.0 - 1.0 / 3.0) < 1e-12)

    # Missing signal bar is not forward-filled: the next target is flat.
    # d3 earns the open-to-close of the new long; d4 earns 0; d5 flattens at an unchanged open.
    m = [_flat(names) for _ in range(6)]
    m[2]["E"] = (100.0, 110.0)
    m[3]["E"] = (110.0, 130.0)
    m[4]["E"] = None
    m[5]["E"] = (130.0, 143.0)
    cal, bars = _bars_from(list(zip((d0, d1, d2, d3, d4, d5), m)), names)
    r = simulate(names, cal, bars, [d2, d4], 2, 0.0, "tsmom")
    by = {rec["date"]: rec for rec in r["daily"]}
    check("missing signal is zero", r["signals"][d4]["E"] == 0)
    check("d3 earns oc", abs(by[d3]["net"] - (1.0 / 33.0)) < 1e-12)
    check("missing day earns 0", by[d4]["net"] == 0.0 and by[d4]["shares"]["E"] != 0.0)
    check("not forward-filled", abs(by[d5]["net"]) < 1e-12 and by[d5]["shares"]["E"] == 0.0)
    check("equity 34/33", abs(r["equity"] - (34.0 / 33.0)) < 1e-12)
    check("flatten reason", any(t["reason"] == "flat" and t["name"] == "E" for t in r["trades"]))

    # Gap after a missing non-signal day is recognized from the last close, on the next print.
    g = [_flat(names) for _ in range(6)]
    g[2]["E"] = (100.0, 110.0)
    g[3]["E"] = (100.0, 100.0)
    g[4]["E"] = None
    g[5]["E"] = (80.0, 80.0)
    cal, bars = _bars_from(list(zip((d0, d1, d2, d3, d4, d5), g)), names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom")
    by = {rec["date"]: rec for rec in r["daily"]}
    check("gap day 0 then recognized", by[d4]["net"] == 0.0 and abs(by[d5]["net"] - (-1.0 / 30.0)) < 1e-12)
    check("gap equity", abs(r["equity"] - (29.0 / 30.0)) < 1e-12)

    # Own-bar lookback skips a hole. Calendar offset of 2 would see a zero return.
    o = [_flat(names) for _ in range(5)]
    o[0]["E"] = (80.0, 80.0)
    o[1]["E"] = (100.0, 100.0)
    o[2]["E"] = None
    o[3]["E"] = (100.0, 100.0)
    o[4]["E"] = (100.0, 100.0)
    ds = (d0, d1, d2, d3, d4)
    cal, bars = _bars_from(list(zip(ds, o)), names)
    r = simulate(names, cal, bars, [d3], 2, 0.0, "tsmom")
    check("own-bar lookback", r["signals"][d3]["E"] == 1)
    check("own-bar shares", abs(abs(r["daily"][-1]["shares"]["E"]) * 100.0 - 1.0 / 6.0) < 1e-12)

    # Warm-up: index < lookback is not a signal, even if the close is up.
    w2 = _flat(names)
    w2["E"] = (100.0, 200.0)
    cal, bars = _bars_from([(d0, _flat(names)), (d1, w2), (d2, _flat(names, 200.0))], names)
    r = simulate(names, cal, bars, [d1], 2, 0.0, "tsmom")
    check("warmup not eligible", r["signals"] == {} and r["trades"] == [] and r["equity"] == 1.0)

    # Same-sign resize is one trip. Old shares earn the gap into the open used for sizing.
    s = [_flat(names) for _ in range(6)]
    s[2]["E"] = (100.0, 110.0)
    s[3]["E"] = (100.0, 100.0)
    s[4]["E"] = (100.0, 120.0)
    s[5]["E"] = (200.0, 200.0)
    cal, bars = _bars_from(list(zip((d0, d1, d2, d3, d4, d5), s)), names)
    r = simulate(names, cal, bars, [d2, d4], 2, 0.0, "tsmom")
    check("one trip", len(r["trades"]) == 1 and r["trades"][0]["reason"] == "end")
    check("resize shares", abs(r["daily"][-1]["shares"]["E"] - (7.0 / 7200.0)) < 1e-12)
    check("resize equity", abs(r["equity"] - (7.0 / 6.0)) < 1e-12)
    # Cost is charged after sizing. Pre-cost shares differ from post-cost shares.
    r = simulate(names, cal, bars, [d2, d4], 2, 0.01, "tsmom")
    # Replay the locked steps with the engine's own first-fill shares off equity 1,
    # then require the second fill to use equity_open before cost.
    by = {rec["date"]: rec for rec in r["daily"]}
    # Independent arithmetic for the costed resize.
    from fractions import Fraction
    c = Fraction(1, 100)
    sh1 = Fraction(1, 6) / 100
    cost1 = c * Fraction(1, 6)
    eq1 = 1 - cost1  # d3 open=close=100, gap 0
    # d4 open 100 close 120, shares unchanged
    eq4 = eq1 + sh1 * (120 - 100)
    gap5 = sh1 * (200 - 120)
    eq_open = eq4 + gap5
    sh2 = Fraction(1, 6) * eq_open / 200
    cost2 = c * abs(sh2 - sh1) * 200
    eq5 = eq_open - cost2
    check("size before cost", abs(by[d5]["shares"]["E"] - float(sh2)) < 1e-9)
    post = Fraction(1, 6) * (eq_open - cost2) / 200
    check("not sized after cost", abs(float(sh2) - float(post)) > 1e-8)
    check("costed resize equity", abs(r["equity"] - float(eq5)) < 1e-9)

    # Flip: two trips, gap on the old trip, open-to-close on the new, cost 0.
    f = [_flat(names) for _ in range(6)]
    f[2]["E"] = (100.0, 110.0)
    f[3]["E"] = (110.0, 110.0)
    f[4]["E"] = (110.0, 90.0)
    f[5]["E"] = (80.0, 88.0)
    cal, bars = _bars_from(list(zip((d0, d1, d2, d3, d4, d5), f)), names)
    r = simulate(names, cal, bars, [d2, d4], 2, 0.0, "tsmom")
    check("flip count", len(r["trades"]) == 2)
    a, b = r["trades"]
    check("flip sides", a["side"] == "long" and a["reason"] == "flip" and a["exit_price"] == 80.0
          and b["side"] == "short" and b["reason"] == "end" and b["entry_price"] == 80.0
          and b["exit_price"] == 88.0)
    check("flip gross old", abs(a["gross"] - (-1.0 / 22.0)) < 1e-12)
    check("flip gross new", abs(b["gross"] - (-7.0 / 440.0)) < 1e-12)
    check("flip equity", abs(r["equity"] - (413.0 / 440.0)) < 1e-12)

    # Delay fills two sessions after the signal, and not at all if that session is absent.
    row3 = _flat(names)
    row3["E"] = (110.0, 150.0)
    row4 = _flat(names)
    row4["E"] = (150.0, 150.0)
    cal, bars = _bars_from(
        [(d0, _flat(names)), (d1, _flat(names)), (d2, row2_pos(names)), (d3, row3), (d4, row4)],
        names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom", fill_delay=1)
    by = {rec["date"]: rec for rec in r["daily"]}
    check("delay holds off", by[d3]["shares"]["E"] == 0.0 and by[d3]["equity"] == 1.0)
    check("delay fills later", abs(abs(by[d4]["shares"]["E"]) * 150.0 - 1.0 / 6.0) < 1e-12)
    cal, bars = _bars_from([(d0, _flat(names)), (d1, _flat(names)), (d2, row2_pos(names))], names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom", fill_delay=1)
    check("delay past end", r["trades"] == [] and r["equity"] == 1.0)

    # Close fill earns nothing on the signal day and earns the next gap. Base fill does not.
    c0 = _flat(names)
    c1 = _flat(names)
    c2 = _flat(names)
    c2["E"] = (100.0, 110.0)
    c3 = _flat(names)
    c3["E"] = (132.0, 132.0)
    cal, bars = _bars_from([(d0, c0), (d1, c1), (d2, c2), (d3, c3)], names)
    rc = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom", fill_at_close=True)
    rb = simulate(names, cal, bars, [d2], 2, 0.0, "tsmom")
    byc = {rec["date"]: rec for rec in rc["daily"]}
    byb = {rec["date"]: rec for rec in rb["daily"]}
    check("close fill signal day flat", abs(byc[d2]["net"]) < 1e-12)
    check("close fill earns gap", abs(byc[d3]["net"] - (1.0 / 30.0)) < 1e-12)
    check("base misses that gap", abs(byb[d3]["net"]) < 1e-12)
    check("close entry at close", rc["trades"][0]["entry_price"] == 110.0 and rc["trades"][0]["reason"] == "end")

    # Benchmark: +1/6 each, and a missing fill bar is not rescaled onto the others.
    b = [_flat(names) for _ in range(5)]
    b[3] = {n: (100.0, 110.0) for n in names}
    b[3]["Y"] = None
    b[4] = {n: (110.0, 110.0) for n in names}
    b[4]["Y"] = (100.0, 200.0)
    cal, bars = _bars_from(list(zip((d0, d1, d2, d3, d4), b)), names)
    r = simulate(names, cal, bars, [d2], 2, 0.0, "longonly")
    by = {rec["date"]: rec for rec in r["daily"]}
    check("benchmark five", abs(by[d3]["equity"] - (13.0 / 12.0)) < 1e-12)
    check("benchmark not rescaled", all(
        abs(abs(by[d3]["shares"][n]) * 100.0 - 1.0 / 6.0) < 1e-12 for n in names if n != "Y"))
    check("missing stays flat", by[d3]["shares"]["Y"] == 0.0 and by[d4]["shares"]["Y"] == 0.0)
    check("benchmark no extra pnl", abs(by[d4]["net"]) < 1e-12)

    # Calendar month-ends, including the three closures and the truncated last month.
    oct2012 = [d for d in nyse_sessions(date(2012, 10, 1), date(2012, 10, 31)) if d not in CLOSED]
    check("sandy skipped", date(2012, 10, 29) not in oct2012 and date(2012, 10, 30) not in oct2012)
    check("oct 2012 end", month_signal_dates(oct2012) == [date(2012, 10, 31)])
    dec2018 = [d for d in nyse_sessions(date(2018, 12, 1), date(2018, 12, 31)) if d not in CLOSED]
    check("bush skipped", date(2018, 12, 5) not in dec2018)
    check("dec 2018 end", month_signal_dates(dec2018) == [date(2018, 12, 31)])
    tail = [d for d in nyse_sessions(date(2026, 9, 1), date(2026, 10, 1)) if d not in CLOSED]
    sig = month_signal_dates(tail)
    check("sep 2026 end", date(2026, 9, 30) in sig)
    check("oct 1 2026 not a signal", date(2026, 10, 1) not in sig)

    if fails:
        sys.exit("self-test failed: " + ", ".join(fails))


def row2_pos(names):
    row = _flat(names)
    row["E"] = (100.0, 110.0)
    return row


def slice_days(res, dates):
    want = set(dates)
    return [rec for rec in res["daily"] if rec["date"] in want]


def trades_in(trades, lo, hi, lo_incl=True):
    out = []
    for t in trades:
        d = t["entry_date"]
        if lo_incl and lo <= d <= hi:
            out.append(t)
        elif not lo_incl and lo < d <= hi:
            out.append(t)
    return out


def trade_block(trades):
    if not trades:
        return {"trades": 0, "win_rate": None, "profit_factor": None, "avg_net_bp": None,
                "avg_win_bp": None, "avg_loss_bp": None, "net_dollar": 0.0, "gross_dollar": 0.0,
                "hold_median": None, "hold_mean": None}
    nets = np.array([t["net_dollar"] for t in trades])
    rets = np.array([t["net_return"] for t in trades])
    wins = rets[nets > 0.0]
    losses = rets[nets < 0.0]
    holds = np.array([t["hold"] for t in trades], float)
    return {
        "trades": len(trades),
        "win_rate": float((nets > 0.0).mean()),
        "profit_factor": profit_factor(nets),
        "avg_net_bp": float(rets.mean() * 1e4),
        "avg_win_bp": float(wins.mean() * 1e4) if len(wins) else None,
        "avg_loss_bp": float(losses.mean() * 1e4) if len(losses) else None,
        "net_dollar": float(nets.sum()),
        "gross_dollar": float(sum(t["gross"] for t in trades)),
        "hold_median": float(np.median(holds)),
        "hold_mean": float(holds.mean()),
    }


def metrics(rows, trades):
    stats = compound_stats([rec["net"] for rec in rows])
    gross = compound_stats([rec["gross"] for rec in rows])
    tb = trade_block(trades)
    n = len(rows)
    out = dict(stats)
    out.update(tb)
    out["sharpe_gross"] = gross["sharpe"]
    out["gross_dollars"] = float(sum(rec["gross_dollar"] for rec in rows)) if rows else 0.0
    out["total_cost"] = float(sum(rec["cost"] for rec in rows)) if rows else 0.0
    out["time_in_market"] = float(np.mean([rec["in_market"] for rec in rows])) if n else None
    out["exposure_gross"] = float(np.mean([rec["gross_exposure"] for rec in rows])) if n else None
    out["exposure_net"] = float(np.mean([rec["net_exposure"] for rec in rows])) if n else None
    out["trades_per_year"] = float(tb["trades"] / (n / ANNUAL)) if n else None
    if rows:
        out["window"] = [str(rows[0]["date"]), str(rows[-1]["date"])]
    else:
        out["window"] = None
    return out


def window_metrics(res, dates, lo, hi):
    rows = [rec for rec in slice_days(res, dates) if lo <= rec["date"] <= hi]
    trips = [t for t in res["trades"] if lo <= t["entry_date"] <= hi]
    return metrics(rows, trips)


def spearman(x, y):
    def rank(a):
        a = np.asarray(a, float)
        order = np.argsort(a, kind="mergesort")
        r = np.empty(len(a), float)
        r[order] = np.arange(len(a), dtype=float)
        vals = a[order]
        i = 0
        while i < len(a):
            j = i
            while j + 1 < len(a) and vals[j + 1] == vals[i]:
                j += 1
            if j > i:
                r[order[i:j + 1]] = r[order[i:j + 1]].mean()
            i = j + 1
        return r
    rx, ry = rank(x), rank(y)
    if rx.std(ddof=1) == 0.0 or ry.std(ddof=1) == 0.0:
        return None
    return float(np.corrcoef(rx, ry)[0, 1])


def direction_placebo(rows, trades):
    days = [rec["date"] for rec in rows]
    pos = {d: i for i, d in enumerate(days)}
    n_t = len(trades)
    contrib = np.zeros((len(days), n_t))
    for j, t in enumerate(trades):
        for d, pnl in t["pieces"]:
            i = pos.get(d)
            if i is not None:
                contrib[i, j] += pnl
    ep = np.array([rec["equity_prev"] for rec in rows])
    if np.any(ep == 0.0):
        ep = np.where(ep == 0.0, np.nan, ep)
    base = contrib.sum(axis=1) / ep
    actual = sharpe(base)
    rng = np.random.default_rng(SEED_DIR)
    signs = rng.choice(np.array([-1.0, 1.0]), size=(N_DIR, n_t))
    draws = sharpe_rows((signs @ contrib.T) / ep)
    p = float((1 + np.sum(draws >= actual)) / (N_DIR + 1)) if actual is not None else None
    return {"actual_gross_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)), "p": p, "draws": draws.tolist()}


def timing_placebo(names, calendar, bars, signal_dates, base_signals, window):
    dates = sorted(base_signals)
    rng = np.random.default_rng(SEED_TIME)
    actual_res = simulate(names, calendar, bars, signal_dates, LOOKBACK, 0.0, "tsmom")
    actual = sharpe([rec["net"] for rec in slice_days(actual_res, window)])
    draws = []
    for _ in range(N_TIME):
        override = {}
        for n in names:
            signs = np.array([base_signals[d][n] for d in dates], dtype=int)
            shuffled = rng.permutation(signs)
            override[n] = {d: int(s) for d, s in zip(dates, shuffled)}
        res = simulate(names, calendar, bars, signal_dates, LOOKBACK, 0.0, "tsmom",
                       signal_override=override)
        draws.append(sharpe([rec["net"] for rec in slice_days(res, window)]) or 0.0)
    draws = np.array(draws, float)
    p = float((1 + np.sum(draws >= actual)) / (N_TIME + 1)) if actual is not None else None
    return {"actual_zero_cost_sharpe": actual, "null_mean": float(draws.mean()),
            "null_p95": float(np.percentile(draws, 95)), "p": p}


def block_bootstrap(r):
    r = np.asarray(r, float)
    n = len(r)
    rng = np.random.default_rng(SEED_BOOT)
    nb = math.ceil(n / BLOCK)
    starts = rng.integers(0, n, size=(N_BOOT, nb))
    idx = (starts[..., None] + np.arange(BLOCK)) % n
    idx = idx.reshape(N_BOOT, -1)[:, :n]
    sh = sharpe_rows(r[idx])
    return {"sharpe_p2_5": float(np.percentile(sh, 2.5)),
            "sharpe_p97_5": float(np.percentile(sh, 97.5)),
            "share_le_0": float(np.mean(sh <= 0.0))}


def quintiles(score, strategy):
    score = np.asarray(score, float)
    strategy = np.asarray(strategy, float)
    order = np.argsort(score, kind="mergesort")
    rows = []
    for i, b in enumerate(np.array_split(order, 5), start=1):
        rows.append({
            "quintile": i,
            "n": int(len(b)),
            "mean_strategy": float(strategy[b].mean()) if len(b) else None,
            "mean_score": float(score[b].mean()) if len(b) else None,
        })
    return rows


def load_store():
    with MarketData() as md:
        spy = md.bars("SPY", "1d", start=DATA_START, end=DATA_END)
        spy_days = [session_date_of(b.ts) for b in spy]
        spy_close = {session_date_of(b.ts): b.close for b in spy}
        series = {}
        problems = []
        for sym in NAMES:
            bars = md.bars(sym, "1d", start=DATA_START, end=DATA_END)
            raw = md.bars(sym, "1d", start=DATA_START, end=DATA_END, adjust=False)
            days = [session_date_of(b.ts) for b in bars]
            if len(bars) != 3959 or days[0] != DATA_START or days[-1] != DATA_END:
                problems.append(f"{sym} count/range {len(bars)} {days[0] if days else None} {days[-1] if days else None}")
            if days != spy_days:
                problems.append(f"{sym} dates != SPY")
            if any(not (b.open > 0 and b.high > 0 and b.low > 0 and b.close > 0 and b.high >= b.low
                        and b.high >= max(b.open, b.close) and b.low <= min(b.open, b.close)) for b in bars):
                problems.append(f"{sym} ohlc")
            if len(raw) != len(bars) or any(a.open != r.open or a.close != r.close for a, r in zip(bars, raw)):
                problems.append(f"{sym} adjust differs or length")
            if md.corporate_actions(sym):
                problems.append(f"{sym} has corporate actions")
            series[sym] = {session_date_of(b.ts): (b.open, b.close) for b in bars}
        if any(d in spy_days for d in CLOSED):
            problems.append("closure date has a SPY bar")
        expected = [d for d in nyse_sessions(spy_days[0], spy_days[-1]) if d not in CLOSED]
        if spy_days != expected:
            problems.append(f"book != nyse_sessions minus CLOSED ({len(spy_days)} vs {len(expected)})")
        if problems:
            sys.exit("data check failed: " + "; ".join(problems))
        return spy_days, series, spy_close


def run_reason():
    if "--reason" in sys.argv:
        return sys.argv[sys.argv.index("--reason") + 1]
    if (HERE / "results.json").exists():
        sys.exit("results.json exists; pass --reason to rerun")
    return "initial pre-registered run"


def jsonable(x):
    if isinstance(x, dict):
        return {str(k): jsonable(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [jsonable(v) for v in x]
    if isinstance(x, float):
        if math.isnan(x) or math.isinf(x):
            return None
        return x
    if isinstance(x, (np.floating,)):
        v = float(x)
        return None if math.isnan(v) or math.isinf(v) else v
    if isinstance(x, (np.integer,)):
        return int(x)
    if isinstance(x, date):
        return x.isoformat()
    return x


def main():
    sha, locked = rules_hash()
    selftest()
    reason = run_reason()
    book, bars, spy_close = load_store()
    book_index = {d: i for i, d in enumerate(book)}
    signals_cal = month_signal_dates(book)
    primary = simulate(NAMES, book, bars, signals_cal, LOOKBACK, COST_RATE, "tsmom")
    bench = simulate(NAMES, book, bars, signals_cal, LOOKBACK, 0.0, "longonly")
    eligible = sorted(primary["signals"])
    if not eligible:
        sys.exit("no eligible signal")
    first_fill = book[book_index[eligible[0]] + 1]
    window = [d for d in book if d >= first_fill]
    is_dates = [d for d in window if d < OOS_START]
    oos_dates = [d for d in window if d >= OOS_START]
    is_end = is_dates[-1]
    full_rows = slice_days(primary, window)
    bench_rows = slice_days(bench, window)
    by_date_b = {rec["date"]: rec for rec in bench_rows}

    def pack(lo, hi):
        rows = [rec for rec in full_rows if lo <= rec["date"] <= hi]
        trips = [t for t in primary["trades"] if lo <= t["entry_date"] <= hi]
        m = metrics(rows, trips)
        b_rows = [by_date_b[rec["date"]] for rec in rows]
        b = compound_stats([rec["net"] for rec in b_rows])
        m["benchmark_total_return"] = b["total_return"]
        m["benchmark_sharpe"] = b["sharpe"]
        m["benchmark_max_dd"] = b["max_dd"]
        m["benchmark_ann_vol"] = b["ann_vol"]
        m["benchmark_cagr"] = b["cagr"]
        return m

    full_m = pack(window[0], window[-1])
    is_m = pack(is_dates[0], is_dates[-1])
    oos_m = pack(oos_dates[0], oos_dates[-1])

    costs = {}
    for mult in COST_MULTS:
        bp = COST_BPS * mult
        res = primary if mult == 1.0 else simulate(
            NAMES, book, bars, signals_cal, LOOKBACK, COST_RATE * mult, "tsmom")
        rows = slice_days(res, window)
        fm = compound_stats([rec["net"] for rec in rows])
        om = compound_stats([rec["net"] for rec in rows if rec["date"] >= OOS_START])
        costs[f"{bp:g}"] = {"bp": bp, "mult": mult, "full_sharpe": fm["sharpe"],
                            "oos_sharpe": om["sharpe"], "full_return": fm["total_return"],
                            "oos_return": om["total_return"]}

    def variant(delay=0, at_close=False):
        res = simulate(NAMES, book, bars, signals_cal, LOOKBACK, COST_RATE, "tsmom",
                       fill_delay=delay, fill_at_close=at_close)
        rows = slice_days(res, window)
        pre_cost = sum(rec["cost"] for rec in res["daily"] if rec["date"] < first_fill)
        fm = compound_stats([rec["net"] for rec in rows])
        om = compound_stats([rec["net"] for rec in rows if rec["date"] >= OOS_START])
        return {"full_sharpe": fm["sharpe"], "oos_sharpe": om["sharpe"],
                "full_return": fm["total_return"], "oos_return": om["total_return"],
                "cost_before_window": pre_cost}

    delay1 = variant(delay=1)
    close_fill = variant(at_close=True)

    grid = {}
    for L in GRID:
        res = primary if L == LOOKBACK else simulate(
            NAMES, book, bars, signals_cal, L, COST_RATE, "tsmom")
        rows = slice_days(res, window)
        grid[str(L)] = {
            "lookback": L,
            "IS_sharpe": sharpe([rec["net"] for rec in rows if rec["date"] < OOS_START]),
            "OOS_sharpe": sharpe([rec["net"] for rec in rows if rec["date"] >= OOS_START]),
            "IS_return": compound_stats([rec["net"] for rec in rows if rec["date"] < OOS_START])["total_return"],
            "OOS_return": compound_stats([rec["net"] for rec in rows if rec["date"] >= OOS_START])["total_return"],
        }
    is_s = [grid[str(L)]["IS_sharpe"] for L in GRID]
    oos_s = [grid[str(L)]["OOS_sharpe"] for L in GRID]
    rank_corr = None if any(v is None for v in is_s + oos_s) else spearman(is_s, oos_s)
    n_grid_pos = sum(1 for v in is_s if v is not None and v > 0.0)

    by_symbol = {}
    for n in NAMES:
        gross = sum(rec["by_name"][n] for rec in full_rows)
        trips = [t for t in primary["trades"] if t["name"] == n]
        tb = trade_block(trips)
        by_symbol[n] = {"gross_dollar": gross, "net_dollar": tb["net_dollar"], "trades": tb["trades"],
                        "profit_factor": tb["profit_factor"], "avg_net_bp": tb["avg_net_bp"],
                        "win_rate": tb["win_rate"]}
    pos_names = [n for n in NAMES if by_symbol[n]["gross_dollar"] > 0.0]
    pred1 = len(pos_names) >= 4
    others = [grid[str(L)]["IS_sharpe"] for L in (21, 42, 63, 84)]
    s126 = grid["126"]["IS_sharpe"]
    if s126 is None or any(v is None for v in others):
        pred2 = None
    else:
        pred2 = max(others) > s126

    dir_p = direction_placebo(full_rows, primary["trades"])
    tim_p = timing_placebo(NAMES, book, bars, signals_cal, primary["signals"], window)
    boot = block_bootstrap([rec["net"] for rec in full_rows])

    # Breakdowns
    years = {}
    for y in sorted({d.year for d in window}):
        rows = [rec for rec in full_rows if rec["date"].year == y]
        b_rows = [by_date_b[rec["date"]] for rec in rows]
        st = compound_stats([rec["net"] for rec in rows])
        bt = compound_stats([rec["net"] for rec in b_rows])
        trips = [t for t in primary["trades"] if t["entry_date"].year == y]
        years[str(y)] = {"return": st["total_return"], "sharpe": st["sharpe"], "max_dd": st["max_dd"],
                         "benchmark_return": bt["total_return"], "sessions": st["sessions"],
                         "trades": len(trips)}
    by_side = {}
    for side in ("long", "short"):
        trips = [t for t in primary["trades"] if t["side"] == side]
        by_side[side] = trade_block(trips)
    by_reason = {}
    for reason in ("flat", "flip", "end"):
        trips = [t for t in primary["trades"] if t["reason"] == reason]
        by_reason[reason] = trade_block(trips)

    b_score = np.array([by_date_b[rec["date"]]["net"] for rec in full_rows])
    s_score = np.array([rec["net"] for rec in full_rows])
    spy_r = []
    for d in window:
        prev = book[book_index[d] - 1]
        spy_r.append(spy_close[d] / spy_close[prev] - 1.0)
    q_bench = quintiles(b_score, s_score)
    q_spy = quintiles(spy_r, s_score)

    sig_counts = {}
    for n in NAMES:
        vals = [primary["signals"][d][n] for d in eligible]
        sig_counts[n] = {"pos": sum(v == 1 for v in vals), "neg": sum(v == -1 for v in vals),
                         "zero": sum(v == 0 for v in vals)}

    oos_trips = [t for t in primary["trades"] if t["entry_date"] >= OOS_START]
    line1 = (oos_m["sharpe"] is not None and oos_m["sharpe"] >= 0.5
             and pf_ok(oos_m["profit_factor"], 1.10))
    line2 = dir_p["p"] is not None and dir_p["p"] <= 0.05
    line3 = is_m["sharpe"] is not None and is_m["sharpe"] > 0.0 and n_grid_pos >= 3
    line4 = costs["10"]["full_return"] is not None and costs["10"]["full_return"] > 0.0
    line6 = len(oos_trips) >= 24
    if not line6:
        status = "Inconclusive"
    elif line1 and line2 and line3 and line4:
        status = "Paper-trading candidate"
    else:
        status = "Rejected"

    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    por = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True).stdout
    dirty = "yes" if por.strip() else "no"
    import datetime as dt
    run_utc = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")

    results = {
        "rules_sha256": sha,
        "locked_utc": locked,
        "run_utc": run_utc,
        "git": {"head": head, "dirty": dirty},
        "reason": reason,
        "seeds": {"direction": SEED_DIR, "bootstrap": SEED_BOOT, "timing": SEED_TIME, "verify": SEED_VERIFY},
        "params": {"lookback": LOOKBACK, "denom": DENOM, "cost_bp": COST_BPS, "names": list(NAMES),
                   "oos_start": str(OOS_START)},
        "status": status,
        "acceptance": {
            "line1": line1, "line1_oos_sharpe": oos_m["sharpe"], "line1_oos_pf": oos_m["profit_factor"],
            "line2": line2, "line2_p": dir_p["p"],
            "line3": line3, "line3_is_sharpe": is_m["sharpe"], "line3_grid_positive": n_grid_pos,
            "line4": line4, "line4_return_10bp": costs["10"]["full_return"],
            "line5_applicable": False,
            "line6": line6, "line6_oos_trips": len(oos_trips),
        },
        "window": {"first_signal": str(eligible[0]), "first_fill": str(first_fill),
                   "last_signal": str(eligible[-1]), "is_end": str(is_end),
                   "oos_start": str(oos_dates[0]), "oos_end": str(oos_dates[-1]),
                   "n_month_ends_in_book": len(signals_cal), "n_eligible": len(eligible),
                   "n_fills": len(eligible)},
        "full": full_m, "IS": is_m, "OOS": oos_m,
        "costs": costs, "delay1": delay1, "close_fill": close_fill,
        "grid": grid, "grid_is_oos_spearman": rank_corr, "grid_is_positive": n_grid_pos,
        "placebo": {"direction_p": dir_p["p"], "direction_actual_gross_sharpe": dir_p["actual_gross_sharpe"],
                    "direction_null_mean": dir_p["null_mean"], "direction_null_p95": dir_p["null_p95"],
                    "direction_draws": dir_p["draws"],
                    "timing_p": tim_p["p"], "timing_actual_zero_cost_sharpe": tim_p["actual_zero_cost_sharpe"],
                    "timing_null_mean": tim_p["null_mean"], "timing_null_p95": tim_p["null_p95"]},
        "bootstrap": boot,
        "by_symbol": by_symbol,
        "signal_counts": sig_counts,
        "predictions": {"p1_positive_names": pos_names, "p1_n": len(pos_names), "p1_consistent": pred1,
                        "p2_consistent": pred2, "p2_is_sharpe_126": s126,
                        "p2_max_other_is_sharpe": None if any(v is None for v in others) else max(others)},
        "breakdown": {"by_year": years, "by_side": by_side, "by_reason": by_reason,
                      "quintile_benchmark": q_bench, "quintile_spy": q_spy},
    }
    import json
    (HERE / "results.json").write_text(json.dumps(jsonable(results), indent=1), encoding="utf-8", newline="\n")
    with open(HERE / "daily.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["date", "strategy_net", "strategy_gross", "benchmark", "equity",
                    "benchmark_equity", "cost", "gross_exposure", "net_exposure"])
        for rec in full_rows:
            b = by_date_b[rec["date"]]
            w.writerow([rec["date"].isoformat(), repr(rec["net"]), repr(rec["gross"]), repr(b["net"]),
                        repr(rec["equity"]), repr(b["equity"]), repr(rec["cost"]),
                        repr(rec["gross_exposure"]), repr(rec["net_exposure"])])
    trips = sorted(primary["trades"], key=lambda t: (t["entry_date"], t["name"], t["exit_date"], t["reason"]))
    with open(HERE / "trades.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["symbol", "side", "entry_date", "entry_price", "exit_date", "exit_price",
                    "entry_shares", "gross_dollar", "net_dollar", "gross_return", "net_return",
                    "cost_dollar", "exit_reason", "hold_sessions"])
        for t in trips:
            w.writerow([t["name"], t["side"], t["entry_date"].isoformat(), repr(t["entry_price"]),
                        t["exit_date"].isoformat(), repr(t["exit_price"]), repr(t["entry_shares"]),
                        repr(t["gross"]), repr(t["net_dollar"]), repr(t["gross_return"]),
                        repr(t["net_return"]), repr(t["cost"]), t["reason"], t["hold"]])

    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nAppend-only. One entry per execution that computes returns from the store.\n",
                       encoding="utf-8", newline="\n")
    with open(log, "a", encoding="utf-8", newline="\n") as f:
        f.write(
            f"\n## {run_utc}\n"
            f"- rules_sha256 {sha}\n"
            f"- git_head {head} dirty={dirty}\n"
            f"- reason: {reason}\n"
            f"- full Sharpe {full_m['sharpe']} return {full_m['total_return']} | "
            f"IS Sharpe {is_m['sharpe']} return {is_m['total_return']} | "
            f"OOS Sharpe {oos_m['sharpe']} return {oos_m['total_return']} | "
            f"OOS PF {oos_m['profit_factor']} | OOS trips {len(oos_trips)} | status {status}\n"
        )
    print(json.dumps(jsonable({
        "status": status, "acceptance": results["acceptance"],
        "full_sharpe": full_m["sharpe"], "full_return": full_m["total_return"],
        "is_sharpe": is_m["sharpe"], "oos_sharpe": oos_m["sharpe"], "oos_return": oos_m["total_return"],
        "oos_pf": oos_m["profit_factor"], "oos_trips": len(oos_trips),
        "pred1": pred1, "pred2": pred2, "first_fill": str(first_fill),
    }), indent=1))


if __name__ == "__main__":
    main()
