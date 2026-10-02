# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of spdr-sector-momentum. Shares no signal code with backtest.py.

    python research/spdr-sector-momentum/research/verify.py
"""

from __future__ import annotations

import csv
import sys
from datetime import date, timedelta
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData, nyse_sessions  # noqa: E402

UNIVERSE = ["XLK", "XLF", "XLE", "XLV", "XLI", "XLY", "XLP", "XLU", "XLB", "XLRE", "XLC"]
SKIP = 21
FAR = 252 + SKIP
SAMPLE_END = date(2026, 10, 1)
SEED = 20261014


def completed_month_ends(spy_dates: list[date]) -> list[date]:
    have = set(spy_dates)
    first, last = spy_dates[0], spy_dates[-1]
    year, month = first.year, first.month
    found = []
    while (year, month) <= (last.year, last.month):
        nxt = date(year + 1, 1, 1) if month == 12 else date(year, month + 1, 1)
        days = nyse_sessions(date(year, month, 1), nxt - timedelta(days=1))
        if days and days[-1] <= last:
            chosen = None
            for day in reversed(days):
                if day in have:
                    chosen = day
                    break
            if chosen is not None:
                found.append(chosen)
        year, month = (year + 1, 1) if month == 12 else (year, month + 1)
    return found


def formation_return(own_closes: list[float], own_dates: list[date], signal: date) -> float | None:
    """12-1 formation on this name's own closes. None if the signal date has no bar or the far lag is short."""
    if not own_dates or own_dates[-1] < signal:
        return None
    # Linear scan. The signal bar has to be this name's bar, not a filled calendar day.
    at = None
    for i, day in enumerate(own_dates):
        if day == signal:
            at = i
            break
        if day > signal:
            break
    if at is None or at < FAR:
        return None
    base = own_closes[at - FAR]
    if base <= 0.0:
        return None
    return own_closes[at - SKIP] / base - 1.0


def main() -> None:
    saved = list(csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")))
    with MarketData() as md:
        spy = md.bars("SPY", "1d")
        sessions = [b.session for b in spy]
        opens: dict[str, dict[date, float]] = {}
        closes: dict[str, dict[date, float]] = {}
        own_dates: dict[str, list[date]] = {}
        own_closes: dict[str, list[float]] = {}
        for sym in UNIVERSE:
            bars = md.bars(sym, "1d")
            opens[sym] = {b.session: b.open for b in bars}
            closes[sym] = {b.session: b.close for b in bars}
            own_dates[sym] = [b.session for b in bars]
            own_closes[sym] = [b.close for b in bars]

    signals = completed_month_ends(sessions)
    loc = {day: i for i, day in enumerate(sessions)}
    # Latest target sign, and whether that target has been applied.
    target = {sym: 0 for sym in UNIVERSE}
    applied = {sym: 0 for sym in UNIVERSE}
    need = {sym: False for sym in UNIVERSE}
    fill_i = None
    open_side = {sym: None for sym in UNIVERSE}  # dict of the live trip, or None
    built = []

    def close_live(sym: str, day: date, price: float, reason: str) -> None:
        live = open_side[sym]
        if live is None:
            return
        live["exit_date"] = day.isoformat()
        live["exit_price"] = price
        live["exit_reason"] = reason
        built.append(live)
        open_side[sym] = None

    def open_live(sym: str, day: date, price: float, side: str, form: float | None) -> None:
        open_side[sym] = {
            "symbol": sym,
            "side": side,
            "entry_date": day.isoformat(),
            "entry_price": price,
            "formation": form,
        }

    for i, day in enumerate(sessions):
        if fill_i is not None and i >= fill_i:
            for sym in UNIVERSE:
                if not need[sym] or day not in opens[sym]:
                    continue
                price = opens[sym][day]
                new = target[sym]
                old = applied[sym]
                if new != old:
                    if old != 0 and new == 0:
                        close_live(sym, day, price, "flat")
                    elif old != 0 and new == -old:
                        close_live(sym, day, price, "flip")
                        open_live(sym, day, price, "long" if new > 0 else "short", None)
                    elif old == 0 and new != 0:
                        open_live(sym, day, price, "long" if new > 0 else "short", None)
                    elif old == new:
                        pass
                    else:
                        raise SystemExit(f"unexpected sign change {sym} {old} -> {new}")
                applied[sym] = new
                need[sym] = False
        if day in set(signals):
            score = {}
            for sym in UNIVERSE:
                value = formation_return(own_closes[sym], own_dates[sym], day)
                if value is not None:
                    score[sym] = value
            ranked = sorted(score, key=lambda sym: (-score[sym], sym))
            fresh = {sym: 0 for sym in UNIVERSE}
            if len(ranked) >= 6:
                for sym in ranked[:3]:
                    fresh[sym] = 1
                for sym in ranked[-3:]:
                    fresh[sym] = -1
            # A signal on the last session has no next open. Do not change the book.
            if i + 1 < len(sessions):
                target = fresh
                fill_i = i + 1
                for sym in UNIVERSE:
                    need[sym] = applied[sym] != target[sym]

    last = sessions[-1]
    for sym in UNIVERSE:
        if open_side[sym] is not None:
            if last in closes[sym]:
                px = closes[sym][last]
            else:
                px = own_closes[sym][-1]
            close_live(sym, last, px, "sample_end")

    def key(row: dict) -> tuple:
        return (row["symbol"], row["side"], row["entry_date"], row["exit_date"], row["exit_reason"])

    saved_keys = [key(row) for row in saved]
    built_keys = [key(row) for row in built]
    saved_keys_sorted = sorted(saved_keys)
    built_keys_sorted = sorted(built_keys)
    if saved_keys_sorted != built_keys_sorted:
        only_saved = [k for k in saved_keys_sorted if k not in set(built_keys_sorted)]
        only_built = [k for k in built_keys_sorted if k not in set(saved_keys_sorted)]
        print(f"trade identity mismatch saved={len(saved)} built={len(built)}")
        print("only in trades.csv", only_saved[:8])
        print("only in replay", only_built[:8])
        raise SystemExit(1)

    saved_by = {key(row): row for row in saved}
    built_by = {key(row): row for row in built}
    mismatches = []
    for k, row in saved_by.items():
        other = built_by[k]
        for field in ("entry_price", "exit_price"):
            if abs(float(row[field]) - float(other[field])) > 1e-8:
                mismatches.append((k, field, row[field], other[field]))
    if mismatches:
        print("price mismatches", mismatches[:8])
        raise SystemExit(1)

    # Seeded draw of 40 signal dates. Every trade that enters on the following session must already have matched.
    rng = np.random.default_rng(SEED)
    pool = np.array([i for i, day in enumerate(signals) if loc[day] + 1 < len(sessions)])
    draw = rng.choice(pool, size=40, replace=False)
    drawn_fills = {sessions[loc[signals[int(i)]] + 1].isoformat() for i in draw}
    touched = [row for row in saved if row["entry_date"] in drawn_fills]
    print(
        f"all {len(saved)} trades match on side, entry date, entry price, exit date, exit price, and exit reason"
    )
    print(f"seed {SEED} drew 40 rebalance signals; {len(touched)} trades enter on those fill dates, all matched")


if __name__ == "__main__":
    main()
