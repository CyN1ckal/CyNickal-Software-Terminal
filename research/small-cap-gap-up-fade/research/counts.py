# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Coverage and gap-up counts before any forward return is computed.

Prints which instruments exist, how many daily bars they have, and how often
the open is at least 5% above the previous session's close. It does not print
a close, an open-to-close return, a P&L, or any other outcome.

    python research/small-cap-gap-up-fade/research/counts.py
"""

from __future__ import annotations

import csv
import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
SCREEN = ROOT / "research" / "low-liq-high-vol-mean-reversion" / "screener.csv"
START = "2016-01-04"
END = "2026-09-25"
OOS = date(2024, 1, 2)
GAP = 0.05

# Even/odd split of the September 2026 screen, alphabetical, as locked by
# low-liq-high-vol-mean-reversion. Dropped names are listed so this script
# can confirm they are still absent. Not chosen from any return.
PRIMARY = (
    "ACCO", "AUDC", "BGS", "BOOM", "CHCT", "CLW", "CYH", "DSX", "ELME",
    "FNWD", "FSBW", "FXNC", "HDSN", "III", "IMMR", "JILL", "LMNR", "NAGE",
    "OSUR", "PTLO", "RM", "RWAY", "SMTI", "STRT", "VFF", "XPER", "ZUMZ",
)
CROSS = (
    "AIV", "AVNW", "BNED", "CCCC", "CION", "CZFS", "EGAN", "FNKO", "FRAF",
    "FSTR", "GCO", "HLLY", "HRZN", "IIIV", "INGN", "LE", "LOVE", "OPI",
    "OVBC", "PERI", "RAIL", "RMNI", "SAR", "SPOK", "TBCH", "UIS", "VNDA", "ZH",
)
DROPPED = ("HLS", "TCS", "CVO", "PARK")
CONTROLS = ("QQQ", "SPY", "IGV", "AAPL", "AMZN", "META", "GOOGL", "ADBE", "AAL")


def screen_caps() -> None:
    caps: list[float] = []
    notionals: list[float] = []
    with SCREEN.open(newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            caps.append(float(row["Market Cap"]))
            notionals.append(float(row["Volume Notional"]))
    caps.sort()
    notionals.sort()

    def pct(xs: list[float], p: float) -> float:
        k = (len(xs) - 1) * p
        lo = int(k)
        hi = min(lo + 1, len(xs) - 1)
        return xs[lo] * (1 - (k - lo)) + xs[hi] * (k - lo)

    print(
        f"screen_rows {len(caps)} market_cap_millions "
        f"min {caps[0]:.2f} p50 {pct(caps, 0.5):.2f} max {caps[-1]:.2f}"
    )
    print(
        f"screen_market_cap_bands "
        f"under_300m {sum(c < 300 for c in caps)} "
        f"300m_to_2b {sum(300 <= c < 2000 for c in caps)} "
        f"2b_and_up {sum(c >= 2000 for c in caps)}"
    )
    print(
        f"screen_volume_notional "
        f"min {notionals[0]:.0f} p50 {pct(notionals, 0.5):.0f} max {notionals[-1]:.0f}"
    )


def bucket(gap: float) -> str:
    if gap < 0.10:
        return "05_10"
    if gap < 0.20:
        return "10_20"
    if gap < 0.50:
        return "20_50"
    if gap < 1.00:
        return "50_100"
    return "ge_100"


def load(md: MarketData, sym: str, adjust: bool) -> dict[date, tuple[float, float, float, float]]:
    out: dict[date, tuple[float, float, float, float]] = {}
    for bar in md.bars(sym, "1d", start=START, end=END, adjust=adjust):
        out[bar.session] = (bar.open, bar.high, bar.low, bar.close)
    return out


def main() -> None:
    screen_caps()
    sessions = nyse_sessions(START, END)
    prev = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    print(f"nyse_sessions {len(sessions)} {sessions[0].isoformat()} {sessions[-1].isoformat()}")

    with MarketData() as md:
        print("instruments")
        known = {row["symbol"] for row in md.instruments()}
        for row in md.instruments():
            print(
                f"  {row['symbol']} class={row['asset_class']} "
                f"1d={row['1d_sessions']} {row['1d_first']} {row['1d_last']} "
                f"1m={row['1m_sessions']} {row['1m_first']} {row['1m_last']}"
            )
        screen = set(PRIMARY) | set(CROSS) | set(DROPPED)
        extra = sorted(known - screen - set(CONTROLS))
        print("symbols_outside_screen_and_controls " + " ".join(extra))

        for sym in DROPPED:
            try:
                md.resolve(sym)
                bars = md.bars(sym, "1d", start=START, end=END)
                print(f"dropped_but_present {sym} bars={len(bars)}")
            except LookupError:
                print(f"dropped_absent {sym}")

        books = {"primary": PRIMARY, "cross": CROSS, "control": CONTROLS}
        for book, names in books.items():
            totals = Counter()
            session_hits: set[date] = set()
            session_hits_capped: set[date] = set()
            nonpositive = 0
            missing_prev = 0
            bars_n = 0
            for sym in names:
                try:
                    md.resolve(sym)
                except LookupError:
                    print(f"missing_symbol {book} {sym}")
                    continue
                adj = load(md, sym, True)
                raw = load(md, sym, False)
                acts = md.corporate_actions(sym)
                splits = {a["ex_date"] for a in acts if a["type"] == "split"}
                kinds = Counter(a["type"] for a in acts)
                bars_n += len(adj)
                sym_ge5 = 0
                sym_bad_ohlc = 0
                for day, (op, hi, lo, cl) in adj.items():
                    if op <= 0 or hi <= 0 or lo <= 0 or cl <= 0:
                        nonpositive += 1
                        print(f"nonpositive {book} {sym} {day.isoformat()}")
                    elif not (lo <= op <= hi and lo <= cl <= hi):
                        sym_bad_ohlc += 1
                        print(f"ohlc_order {book} {sym} {day.isoformat()}")
                for day in sessions:
                    if day not in adj:
                        continue
                    pday = prev.get(day)
                    if pday is None or pday not in adj or adj[pday][3] <= 0 or adj[day][0] <= 0:
                        if pday is not None and day in adj and pday not in adj:
                            missing_prev += 1
                        continue
                    gap = adj[day][0] / adj[pday][3] - 1.0
                    if gap >= GAP:
                        sym_ge5 += 1
                print(
                    f"keep {book} {sym} bars={len(adj)} ge5={sym_ge5} bad_ohlc={sym_bad_ohlc} "
                    f"splits={kinds.get('split', 0)} dividends={kinds.get('dividend', 0)}"
                )
                for day in sessions:
                    if day not in adj:
                        continue
                    pday = prev.get(day)
                    if pday is None or pday not in adj or adj[pday][3] <= 0 or adj[day][0] <= 0:
                        continue
                    gap = adj[day][0] / adj[pday][3] - 1.0
                    if gap < GAP:
                        continue
                    sample = "OOS" if day >= OOS else "IS"
                    totals[f"ge5_{sample}"] += 1
                    totals["ge5"] += 1
                    totals[bucket(gap)] += 1
                    session_hits.add(day)
                    if gap < 1.0:
                        totals[f"cap100_{sample}"] += 1
                        totals["cap100"] += 1
                        session_hits_capped.add(day)
                    if gap >= 0.50:
                        raw_gap = None
                        if pday in raw and raw[pday][3] > 0 and day in raw and raw[day][0] > 0:
                            raw_gap = raw[day][0] / raw[pday][3] - 1.0
                        print(
                            f"large_gap {book} {sym} {day.isoformat()} "
                            f"adj_gap={gap:.4f} raw_gap={raw_gap} "
                            f"split_ex={day.isoformat() in splits} sample={sample}"
                        )
            oos_days = sum(1 for d in session_hits_capped if d >= OOS)
            is_days = sum(1 for d in session_hits_capped if d < OOS)
            print(
                f"BOOK {book} bars={bars_n} nonpositive={nonpositive} "
                f"missing_prev_session={missing_prev} "
                f"ge5={totals['ge5']} ge5_IS={totals['ge5_IS']} ge5_OOS={totals['ge5_OOS']} "
                f"buckets 05_10={totals['05_10']} 10_20={totals['10_20']} "
                f"20_50={totals['20_50']} 50_100={totals['50_100']} ge_100={totals['ge_100']} "
                f"cap100={totals['cap100']} cap100_IS={totals['cap100_IS']} "
                f"cap100_OOS={totals['cap100_OOS']} "
                f"cap100_sessions_IS={is_days} cap100_sessions_OOS={oos_days}"
            )


if __name__ == "__main__":
    main()
