# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the primary book. Shares no signal code with backtest.py.

    python research/low-liq-high-vol-mean-reversion/research/verify.py

Every primary trade must match on side, entry session, entry price, exit session,
and exit price. A mismatch is a bug.
"""

from __future__ import annotations

import csv
import math
import sys
from collections import defaultdict
from datetime import date
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent

PRIMARY = (
    "ACCO", "AUDC", "BGS", "BOOM", "CHCT", "CLW", "CYH", "DSX", "ELME", "FNWD",
    "FSBW", "FXNC", "HDSN", "III", "IMMR", "JILL", "LMNR", "NAGE", "OSUR", "PTLO",
    "RM", "RWAY", "SMTI", "STRT", "VFF", "XPER", "ZUMZ",
)
START, END = date(2016, 1, 4), date(2026, 9, 25)
LOOKBACK, MIN_OBS, MIN_RETURNS = 63, 50, 40
VOL_LO, VOL_HI = 0.20, 0.60
DOLLAR_LO, DOLLAR_HI = 1_000_000.0, 10_000_000.0
FORMATION, K, Q_MIN = 1, 5, 2


def load():
    with MarketData() as md:
        raw = {sym: {b.session: b for b in md.bars(sym, "1d", start="2016-01-04", end="2026-09-25")}
               for sym in PRIMARY}
    present_dates = [d for d in nyse_sessions(START, END) if any(d in raw[s] for s in PRIMARY)]
    sessions = [d for d in nyse_sessions(min(present_dates), max(present_dates))]
    n, m = len(PRIMARY), len(sessions)
    o = np.zeros((n, m))
    c = np.zeros((n, m))
    v = np.zeros((n, m))
    present = np.zeros((n, m), bool)
    for i, sym in enumerate(PRIMARY):
        for j, day in enumerate(sessions):
            bar = raw[sym].get(day)
            if bar is None or bar.open <= 0 or bar.close <= 0:
                continue
            o[i, j], c[i, j], v[i, j], present[i, j] = bar.open, bar.close, bar.volume, True
    weeks = []
    for day in sessions:
        key = day.isocalendar()[:2]
        if not weeks or weeks[-1][0].isocalendar()[:2] != key:
            weeks.append([day])
        else:
            weeks[-1].append(day)
    index = {day: j for j, day in enumerate(sessions)}
    return sessions, index, weeks, o, c, v, present


def eligible(c, v, present, weeks, index, signal_week: int) -> list[tuple[str, float]]:
    sig = index[weeks[signal_week][-1]]
    base = index[weeks[signal_week - FORMATION][-1]]
    lo = sig - LOOKBACK + 1
    rows = []
    if lo < 0:
        return rows
    for i, sym in enumerate(PRIMARY):
        window = present[i, lo:sig + 1]
        if int(window.sum()) < MIN_OBS:
            continue
        rets = []
        for j in range(max(lo, 1), sig + 1):
            if present[i, j] and present[i, j - 1] and c[i, j - 1] > 0 and c[i, j] > 0:
                rets.append(c[i, j] / c[i, j - 1] - 1.0)
        if len(rets) < MIN_RETURNS:
            continue
        vol = float(np.std(np.array(rets), ddof=1) * math.sqrt(252))
        if not VOL_LO <= vol <= VOL_HI:
            continue
        dollars = (c[i, lo:sig + 1] * v[i, lo:sig + 1])[window]
        med = float(np.median(dollars))
        if not DOLLAR_LO <= med <= DOLLAR_HI:
            continue
        if not present[i, base] or not present[i, sig] or c[i, base] <= 0 or c[i, sig] <= 0:
            continue
        rows.append((sym, float(c[i, sig] / c[i, base] - 1.0)))
    return rows


def choose(rows: list[tuple[str, float]]) -> dict[str, float]:
    q = len(rows) // K
    if q < Q_MIN:
        return {}
    ordered = sorted(rows, key=lambda row: (row[1], row[0]))
    weights = {sym: 0.5 / q for sym, _ in ordered[:q]}
    for sym, _ in ordered[-q:]:
        weights[sym] = -0.5 / q
    return weights


def replay() -> list[dict]:
    sessions, index, weeks, o, c, v, present = load()
    sym_i = {sym: i for i, sym in enumerate(PRIMARY)}
    entries = {}
    exits = set()
    for s in range(FORMATION, len(weeks) - 2):
        entries[weeks[s + 1][0]] = s
        exits.add(weeks[s + 2][0])
    shares = {sym: 0.0 for sym in PRIMARY}
    desired = {sym: 0.0 for sym in PRIMARY}
    lots = {}
    trades = []
    equity = 1.0
    last = {}
    eval_start = min(entries) if entries else sessions[-1]

    def close(sym, px, day, reason):
        lot = lots.pop(sym)
        shares[sym] = 0.0
        trades.append({"symbol": sym, "side": "long" if lot["side"] > 0 else "short",
                       "entry_session": lot["entry"], "entry_price": lot["px"],
                       "exit_session": day.isoformat(), "exit_price": px, "reason": reason})

    def trade(sym, new, px, day, reason, info):
        nonlocal equity
        old = shares[sym]
        if abs(new - old) < 1e-12:
            return
        old_sign = 0 if abs(old) < 1e-12 else (1 if old > 0 else -1)
        new_sign = 0 if abs(new) < 1e-12 else (1 if new > 0 else -1)
        if old_sign and new_sign and old_sign != new_sign:
            close(sym, px, day, "flip")
            lots[sym] = {"side": new_sign, "entry": day.isoformat(), "px": px}
            shares[sym] = new
            return
        if old_sign == 0:
            lots[sym] = {"side": new_sign, "entry": day.isoformat(), "px": px, "info": info}
            shares[sym] = new
            return
        if new_sign == 0:
            close(sym, px, day, reason)
            return
        shares[sym] = new

    for j, day in enumerate(sessions):
        for i, sym in enumerate(PRIMARY):
            if present[i, j] and c[i, j] > 0 and day < eval_start:
                last[sym] = c[i, j]
        if day < eval_start:
            continue
        signal = entries.get(day)
        exiting = day in exits
        if signal is not None or exiting:
            if signal is None or equity <= 0:
                weights = {}
            else:
                raw = choose(eligible(c, v, present, weeks, index, signal))
                tradable = {}
                for sym, weight in raw.items():
                    i = sym_i[sym]
                    if present[i, j] and o[i, j] > 0:
                        tradable[sym] = weight
                longs = [s for s, w in tradable.items() if w > 0]
                shorts = [s for s, w in tradable.items() if w < 0]
                weights = {}
                if longs and shorts:
                    for sym in longs:
                        weights[sym] = 0.5 / len(longs)
                    for sym in shorts:
                        weights[sym] = -0.5 / len(shorts)
            base = equity
            desired = {sym: 0.0 for sym in PRIMARY}
            for sym, weight in weights.items():
                desired[sym] = weight * base / o[sym_i[sym], j]
            for sym in PRIMARY:
                i = sym_i[sym]
                if not present[i, j] or o[i, j] <= 0:
                    continue
                info = {"weight": weights.get(sym, 0.0)}
                trade(sym, desired[sym], float(o[i, j]), day, "schedule", info)
                last[sym] = float(o[i, j])
            # Equity is not needed for prices. Keep a dummy so a later insolvent
            # path is unreachable: this replay does not charge costs. Prices only.
            equity = 1.0
        else:
            for sym in PRIMARY:
                if abs(shares[sym] - desired[sym]) < 1e-12:
                    continue
                i = sym_i[sym]
                if not present[i, j] or o[i, j] <= 0:
                    continue
                trade(sym, desired[sym], float(o[i, j]), day, "schedule", {})
                last[sym] = float(o[i, j])
        if day == sessions[-1]:
            for sym in list(shares):
                if abs(shares[sym]) < 1e-12:
                    continue
                i = sym_i[sym]
                px = float(c[i, j]) if present[i, j] and c[i, j] > 0 else last.get(sym, 0.0)
                if px > 0:
                    trade(sym, 0.0, px, day, "end_of_sample", {})
    return trades


def main() -> None:
    got = replay()
    path = HERE / "trades.csv"
    if not path.exists():
        raise SystemExit("trades.csv is missing. Run backtest.py first.")
    stored = list(csv.DictReader(path.open(encoding="utf-8")))
    def key(row):
        return (row["symbol"], row["side"], row["entry_session"], round(float(row["entry_price"]), 6),
                row["exit_session"], round(float(row["exit_price"]), 6))
    got_keys = [key({"symbol": t["symbol"], "side": t["side"], "entry_session": t["entry_session"],
                     "entry_price": t["entry_price"], "exit_session": t["exit_session"],
                     "exit_price": t["exit_price"]}) for t in got]
    stored_keys = [key(row) for row in stored]
    if got_keys == stored_keys:
        print(f"verify matched {len(stored_keys)} primary trades")
        return
    print(f"MISMATCH replay {len(got_keys)} stored {len(stored_keys)}")
    gs, ss = set(got_keys), set(stored_keys)
    only_replay = sorted(gs - ss)[:8]
    only_stored = sorted(ss - gs)[:8]
    for row in only_replay:
        print("replay only", row)
    for row in only_stored:
        print("stored only", row)
    raise SystemExit(1)


if __name__ == "__main__":
    main()
