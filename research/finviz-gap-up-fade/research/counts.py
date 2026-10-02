# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Bar counts and opening-gap counts. No forward return is computed.

    python research/finviz-gap-up-fade/research/counts.py
"""

from __future__ import annotations

import json
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
START, END = "2016-01-04", "2026-09-25"
OOS = date(2024, 1, 2)
MIN_BARS = 252


def main() -> None:
    universe = json.loads((HERE / "universe.json").read_text(encoding="utf-8"))
    sessions = nyse_sessions(START, END)
    prev = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    kept = {}
    counts = {}
    with MarketData() as md:
        for book in ("primary", "cross"):
            kept[book] = []
            totals = Counter()
            for sym in universe[book]:
                try:
                    md.resolve(sym)
                except LookupError:
                    print(f"drop {book} {sym} not_in_store")
                    continue
                bars = md.bars(sym, "1d", start=START, end=END)
                if len(bars) < MIN_BARS:
                    first = bars[0].session.isoformat() if bars else "-"
                    last = bars[-1].session.isoformat() if bars else "-"
                    print(f"drop {book} {sym} bars={len(bars)} {first} {last}")
                    continue
                bad = 0
                by_day = {}
                for bar in bars:
                    if bar.open <= 0 or bar.high <= 0 or bar.low <= 0 or bar.close <= 0:
                        bad += 1
                    elif not (bar.low <= bar.open <= bar.high and bar.low <= bar.close <= bar.high):
                        bad += 1
                        print(f"ohlc {book} {sym} {bar.session.isoformat()}")
                    by_day[bar.session] = bar.open, bar.close
                if bad:
                    print(f"drop {book} {sym} bad_bars={bad}")
                    continue
                n_cap = 0
                for day, (op, _cl) in by_day.items():
                    pday = prev.get(day)
                    if pday is None or pday not in by_day or by_day[pday][1] <= 0 or op <= 0:
                        continue
                    gap = op / by_day[pday][1] - 1.0
                    if gap < 0.05:
                        continue
                    totals["ge5"] += 1
                    totals["OOS" if day >= OOS else "IS"] += 1
                    if gap < 1.0:
                        n_cap += 1
                        totals["cap"] += 1
                        totals["cap_OOS" if day >= OOS else "cap_IS"] += 1
                    else:
                        print(f"exclude_double {book} {sym} {day.isoformat()} gap={gap:.3f}")
                kept[book].append(sym)
                print(f"keep {book} {sym} bars={len(bars)} cap100={n_cap}")
            counts[book] = {
                "cap": totals["cap"],
                "cap_IS": totals["cap_IS"],
                "cap_OOS": totals["cap_OOS"],
                "ge5": totals["ge5"],
            }
            print(f"BOOK {book} names={len(kept[book])} {dict(counts[book])}")
    universe["primary_kept"] = kept["primary"]
    universe["cross_kept"] = kept["cross"]
    universe["prelock_signals"] = {
        "primary": counts["primary"]["cap"],
        "cross": counts["cross"]["cap"],
    }
    universe["prelock_detail"] = counts
    (HERE / "universe.json").write_text(json.dumps(universe, indent=2) + "\n", encoding="utf-8")
    print("WROTE kept lists and prelock signal counts. No forward return was computed.")


if __name__ == "__main__":
    main()
