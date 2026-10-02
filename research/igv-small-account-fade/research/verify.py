# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the IGV small-account fade.

Shares no signal or exit code with backtest.py. Compares every trade in
trades.csv on side, signal time, entry time and price, and exit time,
price, and reason.

    python research/igv-small-account-fade/research/verify.py
"""

from __future__ import annotations

import csv
import math
import statistics
import sys
from collections import deque
from datetime import date, datetime, timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData  # noqa: E402

HERE = Path(__file__).resolve().parent
D_MIN, D_MAX = 250_000.0, 1_000_000.0
RV_MIN, MAG_MULT, MAG_FLOOR = 2.0, 2.0, 0.001
LOOKBACK, HOLD = 20, 15
NOTIONAL, COST = 5_000.0, 2.0 / 10_000.0 * 5_000.0


def clock(day: date, minute: int) -> str:
    stamp = datetime(day.year, day.month, day.day, 9, 30) + timedelta(minutes=minute)
    return stamp.strftime("%Y-%m-%d %H:%M")


def load():
    with MarketData() as md:
        rows = md.bars("IGV", "1m")
    sessions: dict[date, dict[int, tuple]] = {}
    for bar in rows:
        minute = bar.time.hour * 60 + bar.time.minute - (9 * 60 + 30)
        sessions.setdefault(bar.session, {})[minute] = (bar.open, bar.close, bar.volume)
    return sessions


def replay(sessions: dict[date, dict[int, tuple]]):
    hist_abs = [deque(maxlen=LOOKBACK) for _ in range(390)]
    hist_dol = [deque(maxlen=LOOKBACK) for _ in range(390)]
    trades = []
    for day in sorted(sessions):
        bars = sessions[day]
        existing = sorted(bars)
        last = existing[-1]
        early = day in EARLY_CLOSES
        k_max = 150 if early else 360
        flatten_at = 205 if early else 385
        position = None
        working = None
        leaving = None
        blocked = False

        def later(m):
            return [k for k in existing if k > m]

        for m in existing:
            o, c, v = bars[m]
            if leaving is not None and m == leaving["minute"]:
                position["exit_m"] = m
                position["exit_px"] = leaving["price"]
                position["exit_on_close"] = False
                position["reason"] = leaving["reason"]
                trades.append(position)
                blocked = leaving["reason"] == "stop"
                position = None
                leaving = None
            if working is not None and position is None and m == working["minute"]:
                position = working["pos"]
                position["entry_m"] = m
                position["entry_px"] = o
                working = None

            decided_exit = False
            if position is not None and leaving is None and m >= position["first_check"]:
                side = position["side"]
                reason = None
                if (side == 1 and c <= position["stop"]) or (side == -1 and c >= position["stop"]):
                    reason = "stop"
                elif (side == 1 and c >= position["target"]) or (side == -1 and c <= position["target"]):
                    reason = "target"
                elif m >= flatten_at or m == last:
                    reason = "flatten"
                elif m >= position["signal_m"] + HOLD:
                    reason = "time"
                if reason is not None:
                    decided_exit = True
                    nxt = later(m)
                    if nxt:
                        leaving = {"minute": nxt[0], "price": bars[nxt[0]][0], "reason": reason}
                    else:
                        position["exit_m"] = m
                        position["exit_px"] = c
                        position["exit_on_close"] = True
                        position["reason"] = reason
                        trades.append(position)
                        blocked = reason == "stop"
                        position = None

            prev = bars.get(m - 1)
            if prev is None or prev[1] <= 0 or c <= 0:
                continue
            r = math.log(c / prev[1])
            dollar = c * v
            ready = len(hist_abs[m]) >= LOOKBACK
            med_a = statistics.median(hist_abs[m]) if ready else None
            med_d = statistics.median(hist_dol[m]) if ready else None
            hist_abs[m].append(abs(r))
            hist_dol[m].append(dollar)
            if decided_exit or position is not None or working is not None or blocked:
                continue
            if med_a is None or med_d is None or med_d <= 0 or r == 0:
                continue
            if not (15 <= m <= k_max) or not (D_MIN <= dollar < D_MAX):
                continue
            if dollar / med_d < RV_MIN or abs(r) < max(MAG_FLOOR, MAG_MULT * med_a):
                continue
            nxt = later(m)
            if not nxt:
                continue
            side = -1 if r > 0 else 1
            move = c - prev[1]
            working = {
                "minute": nxt[0],
                "pos": {
                    "day": day,
                    "side": side,
                    "signal_m": m,
                    "first_check": nxt[0],
                    "target": c - 0.5 * move,
                    "stop": c + move,
                    "entry_m": None,
                    "entry_px": None,
                },
            }
    return trades


def main() -> None:
    found = replay(load())
    with (HERE / "trades.csv").open(encoding="utf-8") as fh:
        saved = list(csv.DictReader(fh))
    if len(found) != len(saved):
        raise SystemExit(f"trade count {len(found)} != {len(saved)}")
    for i, (got, row) in enumerate(zip(found, saved)):
        side = "long" if got["side"] == 1 else "short"
        checks = {
            "side": (side, row["side"]),
            "signal": (clock(got["day"], got["signal_m"]), row["signal_time"]),
            "entry": (clock(got["day"], got["entry_m"]), row["entry_time"]),
            "exit": (clock(got["day"], got["exit_m"]), row["exit_time"]),
            "reason": (got["reason"], row["exit_reason"]),
            "on_close": (int(got["exit_on_close"]), int(row["exit_on_close"])),
        }
        for name, (a, b) in checks.items():
            if a != b:
                raise SystemExit(f"trade {i} {name}: {a} != {b}")
        if abs(got["entry_px"] - float(row["entry_price"])) > 1e-5:
            raise SystemExit(f"trade {i} entry price {got['entry_px']} != {row['entry_price']}")
        if abs(got["exit_px"] - float(row["exit_price"])) > 1e-5:
            raise SystemExit(f"trade {i} exit price {got['exit_px']} != {row['exit_price']}")
    gross = sum(
        got["side"] * (NOTIONAL / got["entry_px"]) * (got["exit_px"] - got["entry_px"])
        for got in found
    )
    print(f"verify matched {len(found)} trades; gross dollars {gross:.2f}")


if __name__ == "__main__":
    main()
