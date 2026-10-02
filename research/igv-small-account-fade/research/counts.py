# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock counts for the IGV small-account fade.

Dollar-volume percentiles and how often the primary signal fires.
No forward return, P&L, or hit rate is computed.

Run from the repo root:
    python research/igv-small-account-fade/research/counts.py
"""

from __future__ import annotations

import math
import statistics
import sys
from collections import defaultdict, deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, resample  # noqa: E402

D_MIN = 250_000.0
D_MAX = 1_000_000.0
RV_MIN = 2.0
MAG_MULT = 2.0
MAG_FLOOR = 0.0010
LOOKBACK = 20
K_MIN = 15
K_MAX = 360
IS_END = "2024-06-28"
OOS_START = "2024-07-01"
SYMBOLS = ("IGV", "QQQ", "SPY")


def percentile(xs: list[float], p: float) -> float:
    ordered = sorted(xs)
    k = (len(ordered) - 1) * p / 100.0
    lo = int(k)
    hi = min(lo + 1, len(ordered) - 1)
    weight = k - lo
    return ordered[lo] * (1.0 - weight) + ordered[hi] * weight


def volume_census(bars) -> None:
    dollars = [b.close * b.volume for b in bars if b.close > 0]
    closes = [b.close for b in bars if b.close > 0]
    print(f"  1m bars={len(dollars)}")
    for p in (1, 5, 10, 25, 50, 75, 90, 99):
        print(f"    p{p} dollar volume {percentile(dollars, p):,.0f}")
    n = len(dollars)
    for level in (250_000, 1_000_000):
        share = sum(value < level for value in dollars) / n
        print(f"    share under ${level:,.0f} = {share:.4%}")
    print(
        f"  close p5={percentile(closes, 5):.2f} "
        f"p50={percentile(closes, 50):.2f} p95={percentile(closes, 95):.2f}"
    )
    five = [b.close * b.volume for b in resample(bars, 300) if b.close > 0]
    print(
        f"  5m bars={len(five)} p5={percentile(five, 5):,.0f} "
        f"p50={percentile(five, 50):,.0f} p95={percentile(five, 95):,.0f}"
    )


def count_signals(bars) -> None:
    by_session: dict = defaultdict(list)
    for bar in bars:
        by_session[bar.session].append(bar)
    sessions = sorted(by_session)
    hist_abs: dict[int, deque] = defaultdict(lambda: deque(maxlen=LOOKBACK))
    hist_dol: dict[int, deque] = defaultdict(lambda: deque(maxlen=LOOKBACK))
    raw = {"all": 0, "is": 0, "oos": 0}
    spaced = {"all": 0, "is": 0, "oos": 0}
    eligible = 0
    evaluated = 0
    for session in sessions:
        present = {}
        for bar in by_session[session]:
            minute = bar.time.hour * 60 + bar.time.minute - (9 * 60 + 30)
            if 0 <= minute <= 389 and bar.close > 0:
                present[minute] = bar
        fired: list[int] = []
        for minute, bar in present.items():
            prev = present.get(minute - 1)
            if prev is None or prev.close <= 0:
                continue
            log_abs = abs(math.log(bar.close / prev.close))
            dollar = bar.close * bar.volume
            evaluated += 1
            if K_MIN <= minute <= K_MAX and D_MIN <= dollar < D_MAX:
                eligible += 1
                abs_hist = hist_abs[minute]
                dol_hist = hist_dol[minute]
                if len(abs_hist) >= LOOKBACK and len(dol_hist) >= LOOKBACK:
                    med_abs = statistics.median(abs_hist)
                    med_dol = statistics.median(dol_hist)
                    magnitude = max(MAG_FLOOR, MAG_MULT * med_abs)
                    relative = dollar / med_dol if med_dol > 0 else float("inf")
                    if log_abs >= magnitude and relative >= RV_MIN:
                        raw["all"] += 1
                        key = session.isoformat()
                        if key <= IS_END:
                            raw["is"] += 1
                        elif key >= OOS_START:
                            raw["oos"] += 1
                        fired.append(minute)
            hist_abs[minute].append(log_abs)
            hist_dol[minute].append(dollar)
        last = -10_000
        bucket = (
            "is" if session.isoformat() <= IS_END
            else "oos" if session.isoformat() >= OOS_START
            else "warmup"
        )
        for minute in fired:
            if minute - last >= 15:
                spaced["all"] += 1
                if bucket in spaced:
                    spaced[bucket] += 1
                last = minute
    print(f"  consecutive minutes={evaluated} capacity-window minutes={eligible}")
    print(f"  raw signals all={raw['all']} is={raw['is']} oos={raw['oos']}")
    print(
        f"  15-min-spaced signals all={spaced['all']} "
        f"is={spaced['is']} oos={spaced['oos']}"
    )


def main() -> None:
    with MarketData() as md:
        for symbol in SYMBOLS:
            print(f"\n## {symbol}")
            bars = md.bars(symbol, "1m")
            volume_census(bars)
            count_signals(bars)


if __name__ == "__main__":
    main()
