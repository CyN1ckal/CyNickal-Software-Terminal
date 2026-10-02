# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock count check of the roll-day rule in RULES.md. Uses volume and dates only:
no prices, returns, or outcome statistics.

A bar is valid when open/high/low/close are all > 0 and volume > 0. On valid bars,
ratio(t) = volume(t) / volume(previous valid bar). A roll day is
  (a) the valid bar with the largest ratio in each expected roll month, and
  (b) any valid bar with ratio >= 3.0.
Prints per symbol: rolls per year from (a), extra days from (b), the smallest (a)
ratio, and the dates where fewer than all markets have a valid bar.

    python research/micro-futures-trend/research/rollcheck.py
"""
import json
from collections import Counter, defaultdict
from pathlib import Path

RAW = Path(__file__).resolve().parent / "data" / "raw"
QUARTERLY = {3, 6, 9, 12}
ROLL_MONTHS = {
    "ES=F": QUARTERLY, "NQ=F": QUARTERLY, "RTY=F": QUARTERLY, "YM=F": QUARTERLY,
    "ZT=F": QUARTERLY, "ZF=F": QUARTERLY, "ZN=F": QUARTERLY, "ZB=F": QUARTERLY,
    "GC=F": {1, 3, 5, 7, 11}, "HG=F": {2, 4, 6, 8, 11}, "CL=F": set(range(1, 13)),
}
EXTRA_RATIO = 3.0


def valid(r: dict) -> bool:
    return all((r.get(f) or 0) > 0 for f in ("open", "high", "low", "close", "volume"))


def main() -> None:
    have = defaultdict(int)
    for sym, months in ROLL_MONTHS.items():
        body = json.loads((RAW / (sym.replace("=", "_") + ".json")).read_text())["body"]
        rows = [body[k] for k in sorted(body, key=int)]
        for r in rows:
            have[r["date"]] += valid(r)
        rows = [r for r in rows if valid(r)]
        ratio = {rows[i]["date"]: rows[i]["volume"] / rows[i - 1]["volume"] for i in range(1, len(rows))}
        best: dict[str, tuple[float, str]] = {}
        for d, x in ratio.items():
            m = d[:7]
            if int(d[5:7]) in months and (m not in best or x > best[m][0]):
                best[m] = (x, d)
        # a month is only judged when the whole month is inside the data
        best = {m: v for m, v in best.items() if "2021-10" <= m <= "2026-08"}
        main_days = {d for _, d in best.values()}
        extra = sorted(d for d, x in ratio.items() if x >= EXTRA_RATIO and d not in main_days)
        per_year = Counter(d[:4] for d in main_days)
        weakest = sorted(best.values())[:3]
        print(f"{sym:6} (a) {len(main_days):3d} {dict(sorted(per_year.items()))}  "
              f"weakest (a) ratios {[f'{x:.1f}@{d}' for x, d in weakest]}  (b) extra {len(extra)}")
    partial = sorted(d for d, n in have.items() if 0 < n < len(ROLL_MONTHS))
    none = sorted(d for d, n in have.items() if n == 0)
    print(f"\ndates with no valid bar in any market: {none}")
    print(f"dates with valid bars in some markets only: {len(partial)} {partial[:20]}")


if __name__ == "__main__":
    main()
