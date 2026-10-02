# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the primary opening-gap shorts.

Shares no signal or sizing code with backtest.py. Every primary trade must
match on symbol, side, entry session and price, and exit session and price.

    python research/small-cap-gap-up-fade/research/verify.py
"""

from __future__ import annotations

import csv
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent

PRIMARY = (
    "ACCO", "AUDC", "BGS", "BOOM", "CHCT", "CLW", "CYH", "DSX", "ELME",
    "FNWD", "FSBW", "FXNC", "HDSN", "III", "IMMR", "JILL", "LMNR", "NAGE",
    "OSUR", "PTLO", "RM", "RWAY", "SMTI", "STRT", "VFF", "XPER", "ZUMZ",
)
START, EVAL, END = date(2016, 1, 4), date(2016, 1, 5), date(2026, 9, 25)
GAP_MIN, GAP_MAX = 0.05, 1.0
RATE = 20 / 10000.0


def usable(o: float, h: float, l: float, c: float) -> bool:
    return o > 0 and h > 0 and l > 0 and c > 0 and l <= o <= h and l <= c <= h


def main() -> None:
    sessions = nyse_sessions(START, END)
    prev = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    eval_sessions = [d for d in sessions if d >= EVAL]
    bars: dict[str, dict[date, tuple]] = {}
    with MarketData() as md:
        for sym in PRIMARY:
            bars[sym] = {}
            for bar in md.bars(sym, "1d", start="2016-01-04", end="2026-09-25"):
                bars[sym][bar.session] = (bar.open, bar.high, bar.low, bar.close)

    equity = 1.0
    ruined = False
    replay = []
    daily = []
    for day in eval_sessions:
        if ruined or equity <= 0:
            ruined = True
            daily.append(0.0)
            continue
        p = prev[day]
        rows = []
        for sym in PRIMARY:
            today = bars[sym].get(day)
            yday = bars[sym].get(p)
            if today is None or yday is None or yday[3] <= 0 or not usable(*today):
                continue
            ratio = today[0] / yday[3]
            if ratio >= 1.0 + GAP_MIN and ratio < 1.0 + GAP_MAX:
                rows.append((sym, today[0], today[3]))
        if not rows:
            daily.append(0.0)
            continue
        n = len(rows)
        pnl = 0.0
        staged = []
        for sym, open_, close in rows:
            notional = equity / n
            shares = notional / open_
            gross = shares * (open_ - close)
            cost = RATE * notional + RATE * shares * close
            net = gross - cost
            pnl += net
            staged.append((sym, open_, close, notional))
        if equity + pnl <= 0:
            daily.append(-1.0)
            equity = 0.0
            ruined = True
        else:
            daily.append(pnl / equity)
            equity += pnl
        exit_time = "13:00" if day in EARLY_CLOSES else "16:00"
        for sym, open_, close, _notional in staged:
            replay.append({
                "symbol": sym, "side": "short", "entry_session": day.isoformat(),
                "entry_price": open_, "exit_session": day.isoformat(),
                "exit_price": close, "exit_time": exit_time,
            })

    with (HERE / "trades.csv").open(newline="", encoding="utf-8") as fh:
        logged = list(csv.DictReader(fh))
    with (HERE / "daily.csv").open(newline="", encoding="utf-8") as fh:
        logged_daily = list(csv.DictReader(fh))

    if len(replay) != len(logged):
        sys.exit(f"trade count {len(replay)} != trades.csv {len(logged)}")
    if len(daily) != len(logged_daily):
        sys.exit(f"session count {len(daily)} != daily.csv {len(logged_daily)}")
    bad = 0
    for i, (got, row) in enumerate(zip(replay, logged)):
        for key in ("symbol", "side", "entry_session", "exit_session", "exit_time"):
            if got[key] != row[key]:
                bad += 1
                print(f"mismatch {i} {key}: {got[key]} != {row[key]}")
                break
        else:
            if abs(got["entry_price"] - float(row["entry_price"])) > 1e-8:
                bad += 1
                print(f"mismatch {i} entry_price {got['entry_price']} != {row['entry_price']}")
            elif abs(got["exit_price"] - float(row["exit_price"])) > 1e-8:
                bad += 1
                print(f"mismatch {i} exit_price {got['exit_price']} != {row['exit_price']}")
        if bad >= 8:
            break
    for i, (ret, row) in enumerate(zip(daily, logged_daily)):
        if abs(ret - float(row["ret"])) > 1e-8:
            bad += 1
            print(f"daily mismatch {row['session']}: {ret} != {row['ret']}")
            if bad >= 12:
                break
    if bad:
        sys.exit(f"verify failed with {bad} mismatches")
    print(f"verify matched {len(replay)} trades and {len(daily)} sessions")


if __name__ == "__main__":
    main()
