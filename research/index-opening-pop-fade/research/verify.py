# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent re-implementation of the opening-pop fade, checked trade for trade.

Shares no signal code with backtest.py. It works on UTC timestamps (10:00 ET =
the session's rth_window start + 1,800 s), NumPy arrays, and a vectorized
trailing RMS, then compares every trade with trades.csv for the primary and S1
books on SPY and QQQ (every session is replayed).

    python research/index-opening-pop-fade/research/verify.py
"""
from __future__ import annotations

import csv
import sys
from datetime import date
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, rth_window  # noqa: E402

START, END = date(2021, 9, 27), date(2026, 9, 25)
LOOK, THRESH, COST = 60, 1.0, 1e-4


def replay(sym: str, gap_inclusive: bool) -> list[tuple]:
    with MarketData() as md:
        bars = md.bars(sym, "1m", START, END)
    ts = np.array([b.ts for b in bars], dtype=np.int64)
    op = np.array([b.open for b in bars])
    cl = np.array([b.close for b in bars])
    sess = np.array([b.session.toordinal() for b in bars])
    days = np.unique(sess)
    rows = []   # (day, open_ok, P, entry_ts, entry_px, exit_ts, exit_px)
    for dn in days:
        idx = np.nonzero(sess == dn)[0]
        d = date.fromordinal(int(dn))
        t0, _ = rth_window(d)
        ten = t0 + 1800
        before = idx[ts[idx] < ten]
        after = idx[ts[idx] >= ten]
        rows.append((d, ts[idx[0]] == t0, op[idx[0]], cl[before[-1]] if len(before) else np.nan,
                     ts[after[0]] if len(after) else None, op[after[0]] if len(after) else np.nan,
                     ts[idx[-1]], cl[idx[-1]]))
    pops = []
    for k, r in enumerate(rows):
        d, ok, o, p = r[0], r[1], r[2], r[3]
        if gap_inclusive:
            v = p / rows[k - 1][7] - 1 if k > 0 and not np.isnan(p) else np.nan
        else:
            v = p / o - 1 if ok and not np.isnan(p) else np.nan
        pops.append(v)
    pops = np.array(pops)
    valid = np.nonzero(~np.isnan(pops))[0]
    sq = pops[valid] ** 2
    csum = np.concatenate([[0.0], np.cumsum(sq)])
    trades = []
    for j in range(LOOK, len(valid)):
        k = valid[j]
        rms = np.sqrt((csum[j] - csum[j - LOOK]) / LOOK)
        if pops[k] >= THRESH * rms and rows[k][4] is not None:
            d, e_ts, e_px, x_ts, x_px = rows[k][0], rows[k][4], rows[k][5], rows[k][6], rows[k][7]
            t0, _ = rth_window(d)
            e_min = 570 + (e_ts - t0) // 60
            x_min = 570 + (x_ts - t0) // 60
            trades.append((str(d), f"{e_min // 60:02d}:{e_min % 60:02d}", round(float(e_px), 6),
                           f"{x_min // 60:02d}:{x_min % 60:02d}", round(float(x_px), 6),
                           float(1 - x_px / e_px - COST * (1 + x_px / e_px))))
    return trades


def main() -> None:
    want: dict[tuple, list] = {}
    with (HERE / "trades.csv").open() as f:
        for r in csv.DictReader(f):
            want.setdefault((r["book"], r["symbol"]), []).append(r)
    ok = True
    for book, gap in (("primary", False), ("S1", True)):
        for sym in ("SPY", "QQQ"):
            got = replay(sym, gap)
            ref = want[(book, sym)]
            bad = 0
            if len(got) != len(ref):
                print(f"{book} {sym}: {len(got)} trades vs {len(ref)} in trades.csv")
                bad += 1
            for g, r in zip(got, ref):
                same = (g[0] == r["session"] and r["side"] == "short" and g[1] == r["entry_time"]
                        and abs(g[2] - float(r["entry_px"])) < 1e-6 and g[3] == r["exit_time"]
                        and abs(g[4] - float(r["exit_px"])) < 1e-6 and abs(g[5] - float(r["net"])) < 1e-7)
                if not same:
                    bad += 1
                    if bad <= 5:
                        print("MISMATCH", book, sym, g, {k: r[k] for k in ("session", "entry_time", "entry_px",
                                                                           "exit_time", "exit_px", "net")})
            print(f"{book} {sym}: {len(got)} trades replayed, {bad} mismatches")
            ok &= bad == 0
    if not ok:
        sys.exit("verify: MISMATCH")
    print("verify: every trade matches")


if __name__ == "__main__":
    main()
