# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock data-quality checks on the raw futures files. Counts only: no prices,
returns, or outcome statistics are printed or written.

Checks per symbol: bar count, first/last date, duplicate or weekend dates, missing
or non-positive OHLC, zero volume, dates missing versus the union calendar, and
volume-based roll detection (a session whose volume is > ROLL_RATIO x the previous
session's), tallied per year and per calendar month, to compare with each
contract's expiry cycle.

    python research/micro-futures-trend/research/dq.py
"""
import json
from collections import Counter
from datetime import date
from pathlib import Path

RAW = Path(__file__).resolve().parent / "data" / "raw"
ROLL_RATIO = 3.0


def load(path: Path) -> list[dict]:
    body = json.loads(path.read_text())["body"]
    return [body[k] for k in sorted(body, key=int)]


def main() -> None:
    series = {p.stem.replace("_", "="): load(p) for p in sorted(RAW.glob("*.json"))}
    series = {s: rows for s, rows in series.items() if len(rows) > 1}
    union = sorted({r["date"] for rows in series.values() for r in rows})
    print(f"union calendar: {len(union)} dates {union[0]} .. {union[-1]}\n")
    for sym, rows in series.items():
        dates = [r["date"] for r in rows]
        dup = len(dates) - len(set(dates))
        weekend = sum(date.fromisoformat(d).weekday() >= 5 for d in dates)
        bad = sum(any(r.get(f) is None or r.get(f) <= 0 for f in ("open", "high", "low", "close")) for r in rows)
        zero_vol = sum(not r.get("volume") for r in rows)
        missing = len(set(union) - set(dates))
        rolls = [rows[i]["date"] for i in range(1, len(rows))
                 if rows[i - 1].get("volume") and rows[i].get("volume")
                 and rows[i]["volume"] > ROLL_RATIO * rows[i - 1]["volume"]]
        by_year = Counter(d[:4] for d in rolls)
        by_month = Counter(int(d[5:7]) for d in rolls)
        print(f"{sym:6} n={len(rows)} {dates[0]}..{dates[-1]} dup={dup} weekend={weekend} "
              f"badOHLC={bad} zeroVol={zero_vol} missingVsUnion={missing}")
        print(f"       rolls>{ROLL_RATIO}x: {len(rolls)}  by year {dict(sorted(by_year.items()))}")
        print(f"       by month {dict(sorted(by_month.items()))}")


if __name__ == "__main__":
    main()
