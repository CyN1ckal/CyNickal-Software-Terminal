# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock alignment check. Prints session counts and blank/non-finite counts only.

No return, P&L, volatility, or any other statistic of an outcome column is computed.
"""
import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
R = ROOT / "research"

# (label, file, date column, used columns)
FILES = [
    ("Q/T", "qqq-15m-turtle-overnight/research/daily.csv", "session", ["bench_qqq_ret", "qqq_ret", "qqq_2bp_ret"]),
    ("P1", "qqq-intraday-trend/research/daily.csv", "session", ["p1", "hold"]),
    ("C", "intraday-channel-trend/research/daily.csv", "session", ["r_book"]),
    ("F", "igv-small-account-fade/research/daily.csv", "session", ["strategy", "equity"]),
    ("M", "micro-futures-trend/research/daily.csv", "date", ["ret", "gross_ret", "in_window"]),
    ("POP", "index-opening-pop-fade/research/daily.csv", "session", ["primary_net", "primary_gross"]),
    ("SCALE", "qqq-atr-scale-in/research/daily.csv", "session", ["qqq_ret", "equity"]),
    ("MART", "qqq-atr-martingale/research/daily.csv", "session", ["pnl", "equity", "gross_notional", "side"]),
    ("BOLL", "qqq-bollinger-adding/research/daily.csv", "session", ["qqq_net", "qqq_gross"]),
    ("GAP", "small-cap-gap-up-fade/research/daily.csv", "session", ["ret", "equity"]),
    ("REV", "low-liq-high-vol-mean-reversion/research/daily.csv", "session", ["ret", "equity"]),
    ("RSI2", "spy-rsi2-dip-buy/research/daily.csv", "date", ["strategy_net", "strategy_gross", "held_at_close"]),
    ("FINVIZ", "finviz-gap-up-fade/research/daily.csv", "session", ["ret", "equity", "exposure"]),
]

master = None
for label, rel, dcol, cols in FILES:
    rows = list(csv.DictReader(open(R / rel, newline="")))
    dates = [r[dcol] for r in rows]
    if label == "Q/T":
        master = dates
    ms = set(master)
    inm = [r for r in rows if r[dcol] in ms]
    bad = {c: sum(1 for r in inm if r.get(c, "") == "" or not math.isfinite(float(r[c]))) for c in cols}
    missing = sorted(ms - set(dates))
    print(f"{label:6} rows={len(rows):5} first={dates[0]} last={dates[-1]} on_master={len(inm):5} "
          f"missing_master={len(missing)} {missing[:6]} extra_off_master={len(set(dates) - ms) if label not in ('GAP','REV','FINVIZ') else 'n/a'} blanks={bad}")
print("master", len(master), master[0], master[-1],
      "IS", sum(1 for d in master if d <= "2024-06-28"), "OOS", sum(1 for d in master if d >= "2024-07-01"))

# FINVIZ: exposure on master sessions (a count of sessions with a position, not an outcome)
rows = list(csv.DictReader(open(R / "finviz-gap-up-fade/research/daily.csv", newline="")))
print("FINVIZ sessions with exposure on master:", sum(1 for r in rows if r["session"] in set(master) and float(r["exposure"]) != 0))
# M: in_window flag on master
rows = list(csv.DictReader(open(R / "micro-futures-trend/research/daily.csv", newline="")))
mm = [r for r in rows if r["date"] in set(master)]
print("M in_window on master:", sum(1 for r in mm if r["in_window"] == "1"), "first", next(r["date"] for r in mm if r["in_window"] == "1"))
print("M in_window dates off master:", [r["date"] for r in rows if r["in_window"] == "1" and r["date"] not in set(master)])
