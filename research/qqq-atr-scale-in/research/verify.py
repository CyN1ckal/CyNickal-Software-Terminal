# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the QQQ ATR scale-in.

Shares no signal function with backtest.py. It rebuilds every QQQ campaign
from the store and matches trades.csv on side, unit count, leg prices, entry
time, average entry, exit time, exit price, and exit reason.

    python research/qqq-atr-scale-in/research/verify.py
"""
from __future__ import annotations

import csv
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    MarketData,
    ny_datetime,
    nyse_sessions,
    resample,
)

HERE = Path(__file__).resolve().parent
BAR = 300
ATR_N = 14
SPACING = 0.5
TARGET = 0.25
MAX_UNITS = 3
LAST = date(2026, 9, 25)


def minute(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def atr_known_before(daily) -> list[tuple[date, float]]:
    """Wilder ATR(14) at each daily close, from the rules, written out again."""
    ranges = []
    days = []
    for i in range(1, len(daily)):
        pc = daily[i - 1].close
        hi, lo = daily[i].high, daily[i].low
        ranges.append(max(hi - lo, abs(hi - pc), abs(lo - pc)))
        days.append(daily[i].session)
    if len(ranges) < ATR_N:
        return []
    value = sum(ranges[:ATR_N]) / ATR_N
    known = [(days[ATR_N - 1], value)]
    for j in range(ATR_N, len(ranges)):
        value = ((ATR_N - 1) * value + ranges[j]) / ATR_N
        if value > 0:
            known.append((days[j], value))
    return known


def atr_on(known: list[tuple[date, float]], day: date) -> float | None:
    use = None
    for d, value in known:
        if d < day:
            use = value
        else:
            break
    return use


def replay_symbol() -> list[dict]:
    with MarketData() as md:
        daily = md.bars("QQQ", "1d")
        bars = resample(md.bars("QQQ", "1m"), BAR)
    known = atr_known_before(daily)
    by_day: dict[date, list] = {}
    for b in bars:
        by_day.setdefault(b.session, []).append(b)
    first = next(d for d in sorted(by_day) if atr_on(known, d) is not None)
    out = []
    for day in nyse_sessions(first, LAST):
        atr = atr_on(known, day)
        session_bars = by_day.get(day, [])
        if atr is None or atr <= 0 or not session_bars:
            continue
        if minute(session_bars[0].ts) != 9 * 60 + 30:
            continue
        end_m = 13 * 60 if day in EARLY_CLOSES else 16 * 60
        last_m = minute(session_bars[-1].ts)
        need = 12 * 60 + 55 if day in EARLY_CLOSES else 15 * 60 + 55
        if last_m < need:
            continue
        out.extend(one_session(day, session_bars, atr, end_m))
    return out


def one_session(day: date, bars: list, atr: float, end_m: int) -> list[dict]:
    by_ts = {b.ts: b for b in bars}
    index = {b.ts: i for i, b in enumerate(bars)}
    S = bars[0].open
    side = 0
    legs: list[tuple[int, float]] = []
    pending = None
    done = []

    def finish(price: float, ts: int, reason: str, bar_i: int) -> None:
        nonlocal side, legs
        entry_ts = legs[0][0]
        entry_i = index[entry_ts] if entry_ts in index else index[entry_ts - BAR]
        prices = [px for _, px in legs]
        done.append({
            "session": day.isoformat(),
            "side": side,
            "n_units": len(legs),
            "entry_ts": entry_ts,
            "exit_ts": ts,
            "entry_price": sum(prices) / len(prices),
            "exit_price": price,
            "exit_reason": reason,
            "leg_prices": prices,
            "bars_held": bar_i - entry_i + 1,
        })
        side = 0
        legs = []

    for i, b in enumerate(bars):
        last = i == len(bars) - 1
        if pending is not None and pending[0] == b.ts:
            kind, fill_side = pending[1], pending[2]
            if kind == "enter":
                side = fill_side
                legs = [(b.ts, b.open)]
            elif kind == "add":
                legs.append((b.ts, b.open))
            else:
                finish(b.open, b.ts, "target", i)
            pending = None
        if last:
            if side != 0:
                finish(b.close, b.ts + BAR, "session", i)
            break
        if pending is not None:
            continue
        order = judge(b.close, side, legs, S, atr)
        if order is None:
            continue
        fill_ts = b.ts + BAR
        if fill_ts not in by_ts:
            continue
        if minute(fill_ts) >= end_m - 30:
            continue
        pending = (fill_ts, order[0], order[1])
    return done


def judge(close: float, side: int, legs: list[tuple[int, float]], S: float, atr: float):
    step = SPACING * atr
    if side == 0:
        if close <= S - step:
            return ("enter", 1)
        if close >= S + step:
            return ("enter", -1)
        return None
    prices = [px for _, px in legs]
    avg = sum(prices) / len(prices)
    n = len(prices)
    if side > 0:
        if close >= avg + TARGET * atr:
            return ("exit", 0)
        if n < MAX_UNITS and close <= S - (n + 1) * step and close < min(prices):
            return ("add", 0)
        return None
    if close <= avg - TARGET * atr:
        return ("exit", 0)
    if n < MAX_UNITS and close >= S + (n + 1) * step and close > max(prices):
        return ("add", 0)
    return None


def load_csv() -> list[dict]:
    rows = []
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if row["symbol"] != "QQQ":
                continue
            rows.append({
                "session": row["session"],
                "side": int(row["side"]),
                "n_units": int(row["n_units"]),
                "entry_ts": int(row["entry_ts"]),
                "exit_ts": int(row["exit_ts"]),
                "entry_price": float(row["entry_price"]),
                "exit_price": float(row["exit_price"]),
                "exit_reason": row["exit_reason"],
                "leg_prices": [float(x) for x in row["leg_prices"].split(";") if x],
                "bars_held": int(row["bars_held"]),
            })
    return rows


def same(a: dict, b: dict) -> bool:
    if (a["session"], a["side"], a["n_units"], a["entry_ts"], a["exit_ts"], a["exit_reason"], a["bars_held"]) != (
        b["session"], b["side"], b["n_units"], b["entry_ts"], b["exit_ts"], b["exit_reason"], b["bars_held"]
    ):
        return False
    if abs(a["entry_price"] - b["entry_price"]) > 1e-6 or abs(a["exit_price"] - b["exit_price"]) > 1e-6:
        return False
    if len(a["leg_prices"]) != len(b["leg_prices"]):
        return False
    return all(abs(x - y) <= 1e-6 for x, y in zip(a["leg_prices"], b["leg_prices"]))


def append_log(text: str) -> None:
    path = HERE / "RUNLOG.md"
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main() -> None:
    mine = replay_symbol()
    theirs = load_csv()
    if len(mine) != len(theirs):
        sys.exit(f"campaign count {len(mine)} != trades.csv {len(theirs)}")
    for i, (a, b) in enumerate(zip(mine, theirs)):
        if not same(a, b):
            sys.exit(f"mismatch at {i}: verify {a} csv {b}")
    # Headlines stay the ones backtest.py already wrote. This entry only records the match.
    import json
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    head = git = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True).stdout.strip())
    full, ins, oos = results["primary"]["full"], results["primary"]["is"], results["primary"]["oos"]
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    append_log(
        f"## {stamp}\n"
        f"- reason: verify.py replayed QQQ independently and matched all {len(mine)} campaigns "
        f"on side, unit count, leg prices, entry time and price, exit time and price, and exit reason. "
        f"No strategy code change. Headlines unchanged from the initial run.\n"
        f"- rules_sha256: {results['rules_sha256']}\n"
        f"- git_head: {head}\n"
        f"- git_dirty: {str(dirty).lower()}\n"
        f"- full: sharpe {full['sharpe']:.4f}, return {full['total_return']:.4%}\n"
        f"- IS: sharpe {ins['sharpe']:.4f}, return {ins['total_return']:.4%}\n"
        f"- OOS: sharpe {oos['sharpe']:.4f}, return {oos['total_return']:.4%}\n\n"
    )
    print(f"matched {len(mine)} QQQ campaigns")


if __name__ == "__main__":
    main()
