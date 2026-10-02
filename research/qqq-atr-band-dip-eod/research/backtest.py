# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""QQQ daily-ATR-band dip buy, flat at the close. Pre-registered runs.

Refuses to execute against the store unless RULES.md still hashes to
RULES.lock. Runs the synthetic self-test before opening the store.
Appends to RUNLOG.md on every store run."""

import bisect
import dataclasses
import datetime as dt
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, ny_datetime, nyse_sessions, session_date_of, EARLY_CLOSES  # noqa: E402

# ---- constants mirroring RULES.md -------------------------------------------
FIRST = dt.date(2021, 10, 7)
LAST = dt.date(2026, 9, 25)
IS_END = dt.date(2024, 6, 28)
K_PRIMARY = 1.0
ATR_N = 14
COST_BPS = 1.0
KS_GRID = [0.50, 0.75, 1.00, 1.25, 1.50]
COST_SWEEP = [0.0, 0.5, 1.0, 2.0, 3.0]
SEED_DIRECTION = 20260930
SEED_TIMING = 20260931
SEED_BOOTSTRAP = 20260932
SYMBOLS = ("QQQ", "SPY", "IGV")
ANNUAL = 252


@dataclasses.dataclass
class Min:
    ts: int
    o: float
    h: float
    l: float
    c: float


def rules_hash_check():
    want = (HERE / "RULES.lock").read_text().splitlines()[0].split()[1]
    have = hashlib.sha256(
        (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    if want != have:
        sys.exit(f"RULES.md hash {have[:12]} != RULES.lock {want[:12]}; refusing to run")
    return have


def wilder_atr(daily):
    """{session_date: ATR fixed at that close}. Wilder smoothing, seed = mean of first 14 TRs."""
    out, trs, atr, prev_c = {}, [], None, None
    for b in daily:
        d = session_date_of(b.ts)
        tr = (b.high - b.low) if prev_c is None else max(
            b.high - b.low, abs(b.high - prev_c), abs(b.low - prev_c))
        prev_c = b.close
        if not out:
            trs.append(tr)
            if len(trs) == ATR_N:
                atr = sum(trs) / ATR_N
                out[d] = atr
        else:
            atr = (ATR_N - 1) / ATR_N * atr + tr / ATR_N
            out[d] = atr
    return out


def load_symbol(md, sym):
    daily = md.bars(sym, "1d")
    atr = wilder_atr(daily)
    days = sorted(atr)
    mins = md.bars(sym, "1m", start=FIRST, end=LAST)
    by_sess = {}
    for b in mins:
        by_sess.setdefault(session_date_of(b.ts), []).append(Min(b.ts, b.open, b.high, b.low, b.close))
    for v in by_sess.values():
        v.sort(key=lambda m: m.ts)
    dailyc = {session_date_of(b.ts): b.close for b in daily}
    return dict(atr=atr, atr_days=days, by_sess=by_sess, daily_close=dailyc)


def atr_for(data, day):
    """ATR fixed on a strictly earlier session, or None."""
    i = bisect.bisect_left(data["atr_days"], day)
    return None if i == 0 else data["atr"][data["atr_days"][i - 1]]


# ---- the rule ---------------------------------------------------------------
def run_session(bars, A, K, fill_mode):
    if not bars or A is None or A <= 0:
        return None
    B = bars[0].o - K * A
    for j, m in enumerate(bars):
        if m.l <= B:
            if fill_mode == "limit":
                e_ts, e_px = m.ts, min(B, m.o)
            elif j + 1 < len(bars):
                e_ts, e_px = bars[j + 1].ts, bars[j + 1].o
            else:
                e_ts, e_px = m.ts, m.c
            last = bars[-1]
            if e_ts > last.ts:
                return None
            return dict(entry_ts=e_ts, entry_px=e_px, exit_ts=last.ts,
                        exit_px=last.c, j=j, n_bars=len(bars))
    return None


def run_symbol(data, sessions, K, cost_bp, fill_mode="limit"):
    """One pass over the calendar. Equity compounds at start-of-session mark.
    Returns trades, net/gross daily series, exposure, hold stats, minute marks."""
    equity, trades, r_net, r_gross = 1.0, [], [], []
    held_bars = tot_bars = 0
    holds = []
    for d in sessions:
        bars = data["by_sess"].get(d, [])
        A = atr_for(data, d)
        rn = rg = 0.0
        if bars and A is not None and A > 0:
            tot_bars += len(bars)
            t = run_session(bars, A, K, fill_mode)
            if t is not None:
                rg = t["exit_px"] / t["entry_px"] - 1.0
                rn = rg - 2.0 * cost_bp / 1e4
                n = equity * rn
                trades.append(dict(session=str(d), side="long",
                                   entry_time=ny_datetime(t["entry_ts"]).isoformat(sep=" ", timespec="minutes"),
                                   entry_px=t["entry_px"],
                                   exit_time=ny_datetime(t["exit_ts"]).isoformat(sep=" ", timespec="minutes"),
                                   exit_px=t["exit_px"], gross=rg, net=rn,
                                   dollars_net=n, hold_min=len(bars) - 1 - t["j"] + 1,
                                   reason="session"))
                held_bars += trades[-1]["hold_min"]
                holds.append(trades[-1]["hold_min"])
        else:
            tot_bars += len(bars)
        equity *= 1.0 + rn
        r_net.append(rn)
        r_gross.append(rg)
    return dict(trades=trades, r_net=r_net, r_gross=r_gross,
                exposure=held_bars / tot_bars if tot_bars else 0.0,
                hold_med=int(np.median(holds)) if holds else None,
                hold_mean=float(np.mean(holds)) if holds else None)


# ---- metrics ----------------------------------------------------------------
def sharpe(rs):
    r = np.asarray(rs, float)
    sd = r.std(ddof=1) if len(r) > 1 else 0.0
    return float(r.mean() / sd * math.sqrt(ANNUAL)) if sd > 0 else 0.0


def metrics(r_net, trades):
    r = np.asarray(r_net, float)
    eq = np.cumprod(1.0 + r)
    dd = float((eq / np.maximum.accumulate(eq) - 1.0).min()) if len(eq) else 0.0
    dn = np.asarray([t["dollars_net"] for t in trades], float)
    w, l = dn[dn > 0], dn[dn <= 0]
    pf = float(w.sum() / abs(l.sum())) if l.sum() != 0 else (float("inf") if len(w) else 0.0)
    n = len(r)
    return dict(sessions=n,
                total_return=float(eq[-1] - 1.0) if n else 0.0,
                cagr=float(eq[-1] ** (ANNUAL / n) - 1.0) if n and eq[-1] > 0 else 0.0,
                vol=float(r.std(ddof=1) * math.sqrt(ANNUAL)) if n > 1 else 0.0,
                sharpe=sharpe(r),
                max_dd=dd,
                tstat=float(r.mean() / r.std(ddof=1) * math.sqrt(n)) if n > 1 and r.std(ddof=1) > 0 else 0.0,
                trades=len(trades),
                profit_factor=pf,
                win_rate=float((dn > 0).mean()) if len(dn) else 0.0,
                avg_net_trade_bp=float(np.mean([t["net"] for t in trades]) * 1e4) if trades else 0.0)


# ---- self test ---------------------------------------------------------------
def mk(times_start, pattern):
    t0 = 1719840600
    return [Min(t0 + i * 60, o, max(o, c, lo) + 0.05, lo, c) for i, (o, lo, c) in enumerate(pattern)]


def self_test():
    cases = []

    def ck(name, got, want):
        cases.append((name, got, want))

    A = 1.0
    p = [(101, 100.5, 100.8)] * 2 + [(100.8, 99.9, 100.2)] + [(100.2, 100.6, 100.9)] * 6 + [(100.9, 100.9, 101.0)]
    t = run_session(mk(0, p), A, 1.0, "limit")
    ck("mid touch fills at band", (round(t["entry_px"], 9), t["j"], t["exit_px"]), (100.0, 2, 101.0))
    p2 = [(101, 100.5, 100.8)] * 4 + [(99.0, 98.5, 99.5)] + [(99.5, 100.0, 100.1)] * 4 + [(100.1, 100.1, 100.5)]
    t = run_session(mk(0, p2), A, 1.0, "limit")
    ck("gap-through bar fills at its open", (t["entry_px"], t["j"]), (99.0, 4))
    ck("no touch -> no trade", run_session(mk(0, [(101, 100.2, 100.5)] * 10), A, 1.0, "limit"), None)
    p3 = [(101, 99.0, 100.0)] + [(100.0, 100.4, 100.2)] * 8 + [(100.2, 100.2, 100.6)]
    t = run_session(mk(0, p3), A, 1.0, "limit")
    ck("bar-1 touch at band", (t["entry_px"], t["j"]), (100.0, 0))
    p4 = [(101, 100.4, 100.6)] * 9 + [(100.6, 99.8, 99.9)]
    t = run_session(mk(0, p4), A, 1.0, "limit")
    ck("last-bar touch", (t["entry_px"], t["exit_px"]), (100.0, 99.9))
    t = run_session(mk(0, p4), A, 1.0, "next_open")
    ck("next_open on last bar", (t["entry_px"], t["exit_px"]), (99.9, 99.9))
    t = run_session(mk(0, [(101, 100.5, 100.8)] * 2 + [(100.8, 99.9, 100.2)] + [(100.3, 100.6, 100.9)] * 6 + [(100.9, 100.9, 101.2)]),
                    A, 1.0, "next_open")
    ck("next_open mid-session", (t["entry_px"], t["j"]), (100.3, 2))
    ck("session end is last bar", run_session(mk(0, p), A, 1.0, "limit")["exit_ts"], mk(0, p)[-1].ts)
    ck("ATR None -> no trade", run_session(mk(0, p), None, 1.0, "limit"), None)
    ck("ATR zero -> no trade", run_session(mk(0, p), 0.0, 1.0, "limit"), None)
    ck("no tape -> no trade", run_session([], A, 1.0, "limit"), None)
    p5 = [(101, 100.5, 100.8)] + [(99.0, 98.0, 98.5)] * 5 + [(98.5, 99.0, 99.4)] * 3 + [(99.4, 99.4, 99.6)]
    ck("only first touch fills", run_session(mk(0, p5), A, 1.0, "limit")["j"], 1)
    d0 = dt.date(2024, 7, 1)
    one = dict(atr={dt.date(2024, 6, 28): 1.0}, atr_days=[dt.date(2024, 6, 28)],
               by_sess={d0: mk(0, p)}, daily_close={})
    res = run_symbol(one, [d0], 1.0, 1.0)
    ck("net = gross - 2bp", round(res["trades"][0]["net"] - ((101.0 / 100.0 - 1.0) - 2e-4), 12), 0.0)
    ck("hold minutes = bars from fill to last, inclusive", res["trades"][0]["hold_min"], 8)
    flat = dict(atr={dt.date(2024, 6, 28): 1.0}, atr_days=[dt.date(2024, 6, 28)],
                by_sess={d0: mk(0, [(101, 100.5, 100.8)] * 10)}, daily_close={})
    ck("flat session return 0", run_symbol(flat, [d0], 1.0, 1.0)["r_net"], [0.0])
    ck("no tape -> return 0", run_symbol(dict(atr={}, atr_days=[], by_sess={}, daily_close={}), [d0], 1.0, 1.0)["r_net"], [0.0])
    ck("carry-forward ATR across 2-day gap", atr_for(dict(atr={dt.date(2024, 6, 28): 2.0}, atr_days=[dt.date(2024, 6, 28)]), d0), 2.0)

    for name, got, want in cases:
        if got != want:
            sys.exit(f"SELF-TEST FAILED: {name}: got {got!r} want {want!r}")
    print(f"self-test: {len(cases)} cases passed")


# ---- main store run -----------------------------------------------------------
def main():
    self_test()
    sha = rules_hash_check()

    with MarketData() as md:
        data = {s: load_symbol(md, s) for s in SYMBOLS}
    sessions = nyse_sessions(FIRST, LAST)
    assert sessions[0] == dt.date(2021, 10, 7) and sessions[-1] == dt.date(2026, 9, 25)
    assert dt.date(2021, 12, 31) in sessions
    assert dt.date(2021, 12, 31) not in data["QQQ"]["by_sess"]
    odd_bars = {}
    for s in SYMBOLS:
        odd = []
        for d, bars in data[s]["by_sess"].items():
            want = 211 if d in EARLY_CLOSES else 390
            if len(bars) != want:
                odd.append([str(d), len(bars)])
        if s == "QQQ":
            assert not odd, odd        # RULES.md hard check on the primary symbol
        odd_bars[s] = sorted(odd)      # recorded, not asserted (deviation in RUNLOG)
        assert (sessions[-1] - max(data[s]["atr_days"])).days <= 4
    S = len(sessions)
    ix_is = [i for i, d in enumerate(sessions) if d <= IS_END]
    ix_oos = [i for i, d in enumerate(sessions) if d > IS_END]

    out = {}
    for s in SYMBOLS:
        res = run_symbol(data[s], sessions, K_PRIMARY, COST_BPS)
        d1 = run_symbol(data[s], sessions, K_PRIMARY, COST_BPS, "next_open")
        costs = {}
        for c in COST_SWEEP:
            rc = res if c == COST_BPS else run_symbol(data[s], sessions, K_PRIMARY, c)
            costs[f"{c:g}bp"] = metrics(rc["r_net"], rc["trades"])
        grid = {}
        for k in KS_GRID:
            rg = res if k == K_PRIMARY else run_symbol(data[s], sessions, k, COST_BPS)
            grid[f"{k:g}"] = dict(
                IS_sharpe=sharpe([rg["r_net"][i] for i in ix_is]),
                OOS_sharpe=sharpe([rg["r_net"][i] for i in ix_oos]),
                IS_trades=sum(1 for t in rg["trades"] if t["session"] <= str(IS_END)),
                OOS_trades=sum(1 for t in rg["trades"] if t["session"] > str(IS_END)))
        out[s] = dict(
            res=res, d1=d1,
            full=metrics(res["r_net"], res["trades"]),
            IS=metrics([res["r_net"][i] for i in ix_is],
                       [t for t in res["trades"] if t["session"] <= str(IS_END)]),
            OOS=metrics([res["r_net"][i] for i in ix_oos],
                        [t for t in res["trades"] if t["session"] > str(IS_END)]),
            delay1=dict(full=metrics(d1["r_net"], d1["trades"]),
                        OOS_sharpe=sharpe([d1["r_net"][i] for i in ix_oos])),
            costs=costs, grid=grid,
            exposure=res["exposure"],
            hold=dict(median=res["hold_med"], mean=res["hold_mean"]))

    # benchmarks, QQQ
    bh, o2c, marks = [], [], 0
    prev = None
    for d in sessions:
        c = data["QQQ"]["daily_close"].get(d)
        bars = data["QQQ"]["by_sess"].get(d, [])
        if c is None and bars:
            c = bars[-1].c
        bh.append(0.0 if prev is None or c is None else c / prev - 1.0)
        if c is not None:
            prev = c
        o2c.append(bars[-1].c / bars[0].o - 1.0 if bars else 0.0)
    marks = sum(1 for d in sessions
                if d not in data["QQQ"]["daily_close"] and data["QQQ"]["by_sess"].get(d))

    # placebos and bootstrap on QQQ
    q = out["QQQ"]["res"]
    t_idx, t_gross = [], []
    for t in q["trades"]:
        i = sessions.index(dt.date.fromisoformat(t["session"]))
        t_idx.append(i)
        t_gross.append(t["gross"])
    contrib = np.asarray(t_gross)
    cols = np.asarray(t_idx)
    rng = np.random.default_rng(SEED_DIRECTION)
    N = 2000
    signs = rng.choice([-1.0, 1.0], size=(N, len(cols)))
    daily = np.zeros((N, S))
    daily[np.repeat(np.arange(N), len(cols)), np.tile(cols, N)] = (signs * contrib[None, :]).reshape(-1)
    mu, sd = daily.mean(axis=1), daily.std(axis=1, ddof=1)
    null_sh = np.where(sd > 0, mu / sd * math.sqrt(ANNUAL), 0.0)
    gd = np.zeros(S)
    gd[cols] = contrib
    actual_gross = sharpe(gd)
    p_dir = float((1 + int((null_sh >= actual_gross).sum())) / (N + 1))

    rng = np.random.default_rng(SEED_TIMING)
    N2 = 500
    opens = {}
    for i in t_idx:
        bars = data["QQQ"]["by_sess"][sessions[i]]
        opens[i] = (np.asarray([m.o for m in bars[1:]]), bars[-1].c)
    drawn = np.zeros((N2, S))
    for n in range(N2):
        for i in t_idx:
            o, lc = opens[i]
            drawn[n, i] = lc / o[int(rng.integers(0, len(o)))] - 1.0
    mu2, sd2 = drawn.mean(axis=1), drawn.std(axis=1, ddof=1)
    null2 = np.where(sd2 > 0, mu2 / sd2 * math.sqrt(ANNUAL), 0.0)
    p_time = float((1 + int((null2 >= actual_gross).sum())) / (N2 + 1))

    rng = np.random.default_rng(SEED_BOOTSTRAP)
    r_net = np.asarray(q["r_net"])
    L, nb = 20, 2000
    bs = np.empty(nb)
    for n in range(nb):
        starts = rng.integers(0, S, size=math.ceil(S / L))
        idx = np.concatenate([(np.arange(st, st + L) % S) for st in starts])[:S]
        x = r_net[idx]
        bs[n] = sharpe(x)

    # calendar-year breakdown, QQQ
    by_year = {}
    for i, d in enumerate(sessions):
        by_year.setdefault(d.year, []).append(i)
    year_tbl = {}
    for y, ix in sorted(by_year.items()):
        m = metrics([q["r_net"][i] for i in ix],
                    [t for t in q["trades"] if dt.date.fromisoformat(t["session"]).year == y])
        year_tbl[y] = dict(return_=m["total_return"], sharpe=m["sharpe"], max_dd=m["max_dd"],
                           bh=metrics([bh[i] for i in ix], [])["total_return"])

    # entry-time and vol quintile breakdowns (trades, net dollars)
    half = {}
    for t in q["trades"]:
        hm = t["entry_time"][11:16]
        b = min(int((int(hm[:2]) * 60 + int(hm[3:]) - 570) // 30), 13)
        half.setdefault(b, []).append(t)
    entry_time_tbl = {f"{(570 + 30 * k) // 60:02d}:{(570 + 30 * k) % 60:02d}": dict(
                      trades=len(v),
                     mean_net_bp=float(np.mean([t["net"] for t in v]) * 1e4),
                     mean_gross_bp=float(np.mean([t["gross"] for t in v]) * 1e4))
                     for k, v in sorted(half.items())}
    pred1 = None
    h1 = [t for t in q["trades"] if t["entry_time"][11:16] <= "11:30"]
    h2 = [t for t in q["trades"] if t["entry_time"][11:16] > "14:00"]
    if h1 and h2:
        pred1 = dict(mean_net_bp_early=float(np.mean([t["net"] for t in h1]) * 1e4),
                     mean_net_bp_late=float(np.mean([t["net"] for t in h2]) * 1e4))
    tr_atr = []
    for t in q["trades"]:
        d = dt.date.fromisoformat(t["session"])
        A = atr_for(data["QQQ"], d)
        bars = data["QQQ"]["by_sess"][d]
        tr_atr.append((A / bars[0].o, t))
    Nq = len(tr_atr)
    tr_atr_sorted = sorted(tr_atr, key=lambda x: x[0])
    qn = {}
    for i, (_, t) in enumerate(tr_atr_sorted):
        qn.setdefault(min(i * 5 // Nq, 4), []).append(t)
    quintile_vol = {k: dict(trades=len(v), mean_net_bp=float(np.mean([t["net"] for t in v]) * 1e4),
                            win_rate=float(np.mean([t["dollars_net"] > 0 for t in v])))
                    for k, v in sorted(qn.items())}
    pred2 = dict(top=quintile_vol.get(4, {}).get("mean_net_bp"),
                 bottom=quintile_vol.get(0, {}).get("mean_net_bp"))
    mvt = []
    for t in q["trades"]:
        bars = data["QQQ"]["by_sess"][dt.date.fromisoformat(t["session"])]
        mvt.append((abs(bars[-1].c / bars[0].o - 1.0), t))
    mvt = sorted(mvt, key=lambda x: x[0])
    qm = {}
    for i, (_, t) in enumerate(mvt):
        qm.setdefault(min(i * 5 // len(mvt), 4), []).append(t)
    quintile_move = {k: dict(trades=len(v), mean_net_bp=float(np.mean([t["net"] for t in v]) * 1e4),
                              win_rate=float(np.mean([t["dollars_net"] > 0 for t in v])))
                     for k, v in sorted(qm.items())}

    # pre-registered descriptive: day outcome (o2c) on touch vs non-touch sessions
    tset = {t["session"] for t in q["trades"]}
    o2c_touch = [o2c[i] for i, d in enumerate(sessions) if str(d) in tset]
    o2c_no = [o2c[i] for i, d in enumerate(sessions) if str(d) not in tset]
    touch_vs_no = dict(touch_mean_o2c=float(np.mean(o2c_touch)), n_touch=len(o2c_touch),
                       notouch_mean_o2c=float(np.mean(o2c_no)), n_notouch=len(o2c_no))

    acc = dict(
        line1_oos_sharpe=out["QQQ"]["OOS"]["sharpe"], line1_oos_pf=out["QQQ"]["OOS"]["profit_factor"],
        line1=bool(out["QQQ"]["OOS"]["sharpe"] >= 0.5 and out["QQQ"]["OOS"]["profit_factor"] >= 1.10),
        line2_p=p_dir, line2=bool(p_dir <= 0.05),
        line3_is_sharpe=out["QQQ"]["IS"]["sharpe"],
        line3_grid_pos=sum(1 for g in out["QQQ"]["grid"].values() if g["IS_sharpe"] > 0),
        line3=bool(out["QQQ"]["IS"]["sharpe"] > 0 and sum(1 for g in out["QQQ"]["grid"].values() if g["IS_sharpe"] > 0) >= 3),
        line4_return_2bp=out["QQQ"]["costs"]["2bp"]["total_return"],
        line4=bool(out["QQQ"]["costs"]["2bp"]["total_return"] > 0),
        line5_spy_oos_sharpe=out["SPY"]["OOS"]["sharpe"], line5=bool(out["SPY"]["OOS"]["sharpe"] > 0),
        line6_oos_trades=out["QQQ"]["OOS"]["trades"], line6=bool(out["QQQ"]["OOS"]["trades"] >= 40))
    status = ("Inconclusive" if not acc["line6"] else
              "Paper-trading candidate" if all(acc[k] for k in ("line1", "line2", "line3", "line4", "line5")) else
              "Rejected")

    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = "yes" if subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True).stdout.strip() else "no"
    run_utc = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")

    results = dict(
        rules_sha256=sha, run_utc=run_utc, git=dict(head=head, dirty=dirty),
        seeds=dict(direction=SEED_DIRECTION, timing=SEED_TIMING, bootstrap=SEED_BOOTSTRAP),
        params=dict(K=K_PRIMARY, ATR_N=ATR_N, cost_bp=COST_BPS, symbols=list(SYMBOLS),
                    first=str(FIRST), last=str(LAST), is_end=str(IS_END)),
        sessions=S, status=status, acceptance=acc,
        odd_bar_sessions=odd_bars,
        symbols={s: {k: out[s][k] for k in ("full", "IS", "OOS", "delay1", "costs", "grid",
                                            "exposure", "hold")} for s in SYMBOLS},
        benchmark=dict(QQQ_bh=dict(full=metrics(bh, []), IS=metrics([bh[i] for i in ix_is], []),
                                   OOS=metrics([bh[i] for i in ix_oos], []), minute_marks=marks),
                       QQQ_open_to_close_full_sharpe=sharpe(o2c)),
        placebo=dict(direction_p=p_dir, direction_actual_gross_sharpe=actual_gross,
                     direction_null_mean=float(null_sh.mean()), direction_null_p95=float(np.percentile(null_sh, 95)),
                     timing_p=p_time, timing_null_mean=float(null2.mean())),
        bootstrap=dict(sharpe_p2_5=float(np.percentile(bs, 2.5)), sharpe_p97_5=float(np.percentile(bs, 97.5)),
                       share_le_0=float((bs <= 0).mean())),
        breakdown=dict(by_year=year_tbl, entry_time=entry_time_tbl,
                       touch_vs_no_touch=touch_vs_no,
                       pred1_early_late=pred1, pred2_vol_quintiles=pred2,
                       quintile_vol=quintile_vol, quintile_move=quintile_move),
        corr={f"{a}-{b}": float(np.corrcoef([out[a]["res"]["r_net"][i] for i in range(S)],
                                            [out[b]["res"]["r_net"][i] for i in range(S)])[0, 1])
              for a, b in (("QQQ", "SPY"), ("QQQ", "IGV"), ("SPY", "IGV"))},
    )
    (HERE / "results.json").write_text(json.dumps(results, indent=1, default=str))
    with open(HERE / "daily.csv", "w") as f:
        f.write("session,strategy_net,strategy_gross,buy_hold,open_to_close,has_trade\n")
        tg = dict(zip([t["session"] for t in q["trades"]], t_gross))
        for i, d in enumerate(sessions):
            f.write(f"{d},{q['r_net'][i]!r},{tg.get(str(d), 0.0)!r},{bh[i]!r},{o2c[i]!r},{int(str(d) in tg)}\n")
    with open(HERE / "trades.csv", "w") as f:
        f.write("session,side,entry_time,entry_px,exit_time,exit_px,gross,net,dollars_net,hold_min,reason\n")
        for t in q["trades"]:
            f.write(f"{t['session']},{t['side']},{t['entry_time']},{t['entry_px']!r},{t['exit_time']},"
                    f"{t['exit_px']!r},{t['gross']!r},{t['net']!r},{t['dollars_net']!r},{t['hold_min']},{t['reason']}\n")

    with open(HERE / "RUNLOG.md", "a") as f:
        if not (HERE / "RUNLOG.md").exists():
            f.write("# Run log\n\nAppend-only. One entry per store run.\n")
        qq = out["QQQ"]
        f.write(f"\n## {run_utc}\n- rules_sha256 {sha}\n- git_head {head} dirty={dirty}\n"
                f"- reason: initial pre-registered run\n"
                f"- QQQ full Sharpe {qq['full']['sharpe']:.3f} return {qq['full']['total_return']:+.4f} | "
                f"IS Sharpe {qq['IS']['sharpe']:.3f} | OOS Sharpe {qq['OOS']['sharpe']:.3f} "
                f"return {qq['OOS']['total_return']:+.4f} | trades full/IS/OOS "
                f"{qq['full']['trades']}/{qq['IS']['trades']}/{qq['OOS']['trades']} | status {status}\n")
    print(json.dumps(dict(status=status, acceptance=acc,
                          full=out["QQQ"]["full"], IS=out["QQQ"]["IS"], OOS=out["QQQ"]["OOS"]), indent=1))


if __name__ == "__main__":
    main()
