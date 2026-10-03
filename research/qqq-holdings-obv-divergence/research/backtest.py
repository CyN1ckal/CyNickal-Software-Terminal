# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered OBV divergence book on the current QQQ holdings.

The self-test runs before the store is opened. A hash mismatch or a failed
self-test writes nothing. One store run appends one RUNLOG entry.
"""

from __future__ import annotations

import sys
from datetime import date, timedelta
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from research.kit import (  # noqa: E402
    append_runlog,
    assemble,
    assert_lock,
    block_bootstrap,
    by_year,
    direction_placebo,
    move_quintiles,
    performance,
    pvalue,
    write_daily,
    write_results,
    write_trades,
)
from research.kit.metrics import profit_factor, sharpe  # noqa: E402

NAMES = (
    "NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX "
    "PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP "
    "GILD ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS "
    "MAR DDOG CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR "
    "TER ORLY ABNB HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL "
    "XEL ADSK RKLB MCHP CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP "
    "AXON DXCM ALNY GEHC CPRT"
).split()

W = 5
MIN_SEP = 10
MAX_SEP = 60
HOLD = 20
COST = 0.0005
CROSS_COST = 0.0001
SEED = 20261003
EVAL_START = date(2022, 10, 3)
EVAL_END = date(2026, 10, 2)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
STORE_START = date(2021, 10, 4)
REASON = "initial pre-registered run"


class Bar:
    __slots__ = ("session", "open", "close", "volume")

    def __init__(self, session: date, open_: float, close: float, volume: float) -> None:
        self.session = session
        self.open = float(open_)
        self.close = float(close)
        self.volume = float(volume)


def obv_values(closes: list[float], volumes: list[float]) -> list[float]:
    """Granville OBV. The first bar has no prior close and stays at 0."""
    out: list[float] = []
    acc = 0.0
    prev = None
    for close, volume in zip(closes, volumes):
        if prev is not None:
            if close > prev:
                acc += volume
            elif close < prev:
                acc -= volume
        out.append(acc)
        prev = close
    return out


def choose_side(bull: bool, bear: bool, busy: bool) -> str | None:
    if busy or (bull and bear):
        return None
    if bull:
        return "long"
    if bear:
        return "short"
    return None


def divergences(bars: list[Bar], full_index: dict[date, int], w: int, min_sep: int, max_sep: int,
                eval_start: date, eval_end: date) -> list[dict]:
    """Confirmed swing divergences. A pivot at p is known only at p+w."""
    closes = [bar.close for bar in bars]
    obv = obv_values(closes, [bar.volume for bar in bars])
    last_low = None
    last_high = None
    events = []
    for t in range(len(bars)):
        pivot = t - w
        bull = False
        bear = False
        gap = None
        level = None
        if pivot >= w:
            left = closes[pivot - w:pivot]
            right = closes[pivot + 1:t + 1]
            price = closes[pivot]
            is_low = all(price < other for other in left) and all(price < other for other in right)
            is_high = all(price > other for other in left) and all(price > other for other in right)
            swing_day = bars[pivot].session
            if is_low and last_low is not None:
                dist = full_index[swing_day] - full_index[last_low["session"]]
                if min_sep <= dist <= max_sep and price < last_low["close"] and obv[pivot] > last_low["obv"]:
                    bull = True
                    gap = abs(price - last_low["close"])
                    level = price
            if is_high and last_high is not None:
                dist = full_index[swing_day] - full_index[last_high["session"]]
                if min_sep <= dist <= max_sep and price > last_high["close"] and obv[pivot] < last_high["obv"]:
                    bear = True
                    gap = abs(price - last_high["close"])
                    level = price
            if is_low:
                last_low = {"session": swing_day, "close": price, "obv": obv[pivot]}
            if is_high:
                last_high = {"session": swing_day, "close": price, "obv": obv[pivot]}
        side = choose_side(bull, bear, False)
        day = bars[t].session
        if side is not None and eval_start <= day <= eval_end:
            events.append({
                "confirm_pos": t,
                "confirm_session": day,
                "side": side,
                "level": float(level),
                "gap": float(gap),
            })
    return events


def _blank_trade(symbol: str, side: str, confirm: date, entry: date, entry_price: float,
                 exit_day: date, exit_price: float, reason: str, entry_index: int,
                 exclusive_end: int, level: float, gap: float) -> dict:
    return {
        "symbol": symbol,
        "side": side,
        "confirm_session": confirm,
        "session": entry,
        "entry_time": f"{entry.isoformat()}T09:30:00",
        "entry_price": float(entry_price),
        "exit_time": f"{exit_day.isoformat()}T09:30:00",
        "exit_price": float(exit_price),
        "exit_reason": reason,
        "entry_index": int(entry_index),
        "exclusive_end": int(exclusive_end),
        "failure_level": float(level),
        "swing_gap": float(gap),
        "gross": 0.0,
        "net": 0.0,
        "pieces": [],
    }


def build_open_trades(symbol: str, bars: list[Bar], events: list[dict], eval_index: dict[date, int],
                      hold: int, fill_lag: int, last_index: int) -> list[dict]:
    """Next-open fills. fill_lag 1 is the primary. fill_lag 2 skips one printed bar."""
    by_pos: dict[int, list[dict]] = {}
    for event in events:
        by_pos.setdefault(event["confirm_pos"], []).append(event)
    trades = []
    state = "flat"
    pending = None
    waited = 0
    entry_pos = None
    entry_index = None
    side = None
    level = None
    gap = None
    confirm = None
    scheduled = None
    for pos, bar in enumerate(bars):
        k = eval_index.get(bar.session)
        if state == "pending" and bar.session > pending["confirm_session"] and k is not None:
            waited += 1
            if waited >= fill_lag:
                state = "open"
                entry_pos = pos
                entry_index = k
                side = pending["side"]
                level = pending["level"]
                gap = pending["gap"]
                confirm = pending["confirm_session"]
                scheduled = None
                pending = None
        if state == "open" and pos > entry_pos and k is not None:
            if scheduled == "failure":
                trades.append(_blank_trade(
                    symbol, side, confirm, bars[entry_pos].session, bars[entry_pos].open,
                    bar.session, bar.open, "failure", entry_index, k, level, gap,
                ))
                state = "flat"
                scheduled = None
            elif k >= entry_index + hold:
                trades.append(_blank_trade(
                    symbol, side, confirm, bars[entry_pos].session, bars[entry_pos].open,
                    bar.session, bar.open, "time", entry_index, k, level, gap,
                ))
                state = "flat"
        if state == "open" and k is not None:
            if side == "long" and bar.close < level:
                scheduled = "failure"
            elif side == "short" and bar.close > level:
                scheduled = "failure"
        if state == "flat":
            found = by_pos.get(pos, [])
            sides = {event["side"] for event in found}
            if found and not ("long" in sides and "short" in sides) and len(found) == 1:
                state = "pending"
                pending = found[0]
                waited = 0
    if state == "open":
        last_bar = bars[entry_pos]
        for bar in bars:
            k = eval_index.get(bar.session)
            if k is not None and entry_index <= k <= last_index:
                last_bar = bar
        trades.append(_blank_trade(
            symbol, side, confirm, bars[entry_pos].session, bars[entry_pos].open,
            last_bar.session, last_bar.open, "sample_end", entry_index, last_index + 1, level, gap,
        ))
    return trades


def build_close_trades(symbol: str, bars: list[Bar], events: list[dict], eval_index: dict[date, int],
                       hold: int, last_index: int) -> list[dict]:
    """Same-bar close upper bound. Not used for the verdict."""
    by_pos: dict[int, list[dict]] = {}
    for event in events:
        by_pos.setdefault(event["confirm_pos"], []).append(event)
    trades = []
    state = "flat"
    entry_pos = None
    entry_index = None
    side = None
    level = None
    gap = None
    confirm = None
    for pos, bar in enumerate(bars):
        k = eval_index.get(bar.session)
        if state == "open" and k is not None and pos > entry_pos:
            failed = (side == "long" and bar.close < level) or (side == "short" and bar.close > level)
            timed = k >= entry_index + hold
            if failed or timed:
                reason = "failure" if failed else "time"
                trades.append(_blank_trade(
                    symbol, side, confirm, bars[entry_pos].session, bars[entry_pos].close,
                    bar.session, bar.close, reason, entry_index, k, level, gap,
                ))
                state = "flat"
        if state == "flat" and k is not None:
            found = by_pos.get(pos, [])
            sides = {event["side"] for event in found}
            if found and not ("long" in sides and "short" in sides) and len(found) == 1:
                event = found[0]
                state = "open"
                entry_pos = pos
                entry_index = k
                side = event["side"]
                level = event["level"]
                gap = event["gap"]
                confirm = event["confirm_session"]
    if state == "open":
        last_bar = bars[entry_pos]
        for bar in bars:
            k = eval_index.get(bar.session)
            if k is not None and entry_index <= k <= last_index:
                last_bar = bar
        trades.append(_blank_trade(
            symbol, side, confirm, bars[entry_pos].session, bars[entry_pos].close,
            last_bar.session, last_bar.close, "sample_end", entry_index, last_index + 1, level, gap,
        ))
    return trades


def collect_trades(bars_by: dict[str, list[Bar]], full_index: dict[date, int], eval_index: dict[date, int],
                   eval_start: date, eval_end: date, *, w: int, hold: int, fill_lag: int,
                   mode: str, min_sep: int = MIN_SEP, max_sep: int = MAX_SEP) -> list[dict]:
    last_index = max(eval_index.values())
    trades = []
    for symbol in sorted(bars_by):
        bars = bars_by[symbol]
        events = divergences(bars, full_index, w, min_sep, max_sep, eval_start, eval_end)
        if mode == "close":
            trades.extend(build_close_trades(symbol, bars, events, eval_index, hold, last_index))
        else:
            trades.extend(build_open_trades(symbol, bars, events, eval_index, hold, fill_lag, last_index))
    trades.sort(key=lambda trade: (trade["session"], trade["symbol"], trade["side"]))
    return trades


def _weights(trades: list[dict], n_sessions: int, close_book: bool) -> list[dict[str, float]]:
    holders: list[list[dict]] = [[] for _ in range(n_sessions)]
    for trade in trades:
        if close_book:
            # Earn session k when the entry close is already behind and the exit close is not.
            for k in range(trade["entry_index"] + 1, min(n_sessions, trade["exclusive_end"] + 1)):
                holders[k].append(trade)
        else:
            end = min(n_sessions, trade["exclusive_end"])
            for k in range(trade["entry_index"], end):
                holders[k].append(trade)
    weights = []
    for group in holders:
        slot: dict[str, float] = {}
        if group:
            mag = 1.0 / len(group)
            for trade in group:
                sign = 1.0 if trade["side"] == "long" else -1.0
                slot[trade["symbol"]] = slot.get(trade["symbol"], 0.0) + sign * mag
        weights.append(slot)
    return weights


def apply_account(trades: list[dict], bars_by: dict[str, list[Bar]], calendar: list[date],
                  cost_rate: float, *, close_book: bool) -> tuple[list[float], list[float], list[float]]:
    """Return gross path, net path, and gross exposure. Mutates trade gross, net, and pieces."""
    index = {day: i for i, day in enumerate(calendar)}
    n = len(calendar)
    weights = _weights(trades, n, close_book)
    gross = [0.0] * n
    priced = {
        symbol: {index[bar.session]: bar for bar in bars if bar.session in index}
        for symbol, bars in bars_by.items()
    }
    for trade in trades:
        trade["pieces"] = []
        trade["gross"] = 0.0
        trade["net"] = 0.0
        symbol = trade["symbol"]
        have = priced[symbol]
        if close_book:
            prev_px = trade["entry_price"]
            start = trade["entry_index"] + 1
            stop = trade["exclusive_end"]
        else:
            if trade["entry_index"] not in have or have[trade["entry_index"]].open == 0.0:
                raise RuntimeError(f"{symbol} entry open is missing")
            prev_k = trade["entry_index"]
            prev_px = have[prev_k].open
            start = trade["entry_index"] + 1
            stop = trade["exclusive_end"]
        for k in range(start, min(n, stop + 1)):
            if k not in have:
                continue
            price = have[k].close if close_book else have[k].open
            if price == 0.0 or prev_px == 0.0:
                raise RuntimeError(f"{symbol} price is zero")
            base = weights[k] if close_book else weights[prev_k]
            contrib = base.get(symbol, 0.0) * (price / prev_px - 1.0)
            gross[k] += contrib
            trade["pieces"].append((calendar[k], contrib))
            prev_px = price
            if not close_book:
                prev_k = k
        trade["gross"] = float(sum(piece[1] for piece in trade["pieces"]))
    cost = [0.0] * n
    prev: dict[str, float] = {}
    by_symbol: dict[str, list[dict]] = {}
    for trade in trades:
        by_symbol.setdefault(trade["symbol"], []).append(trade)
    # Position after the decision, used only to assign costs. Open book: the target
    # weight vector. Close book: in after the entry close, out at the exit close.
    position = []
    for k in range(n):
        if close_book:
            slot: dict[str, float] = {}
            group = [trade for trade in trades if trade["entry_index"] <= k < trade["exclusive_end"]]
            if group:
                mag = 1.0 / len(group)
                for trade in group:
                    sign = 1.0 if trade["side"] == "long" else -1.0
                    slot[trade["symbol"]] = slot.get(trade["symbol"], 0.0) + sign * mag
            position.append(slot)
        else:
            position.append(weights[k])
    for k in range(n):
        current = position[k]
        names = set(prev) | set(current)
        for symbol in names:
            delta = abs(current.get(symbol, 0.0) - prev.get(symbol, 0.0))
            if delta == 0.0:
                continue
            piece = cost_rate * delta
            cost[k] += piece
            for trade in by_symbol.get(symbol, ()):
                entered = trade["entry_index"] == k
                exited = trade["exclusive_end"] == k
                resized = trade["entry_index"] < k < trade["exclusive_end"]
                if entered or exited or resized:
                    trade["net"] -= piece
                    break
        prev = current
    for trade in trades:
        trade["net"] = float(trade["gross"] + trade["net"])
    net = [gross[k] - cost[k] for k in range(n)]
    held = [1.0 if weights[k] else 0.0 for k in range(n)]
    return gross, net, held


def _dates(count: int) -> list[date]:
    return [date(2024, 1, 1) + timedelta(days=i) for i in range(count)]


def _bars(rows: list[tuple[float, float, float]], days: list[date]) -> list[Bar]:
    return [Bar(day, row[0], row[1], row[2]) for day, row in zip(days, rows)]


def _near(actual: float, expected: float, label: str) -> None:
    if abs(actual - expected) > 1e-9:
        raise AssertionError(f"{label}: {actual} != {expected}")


def self_test() -> None:
    if not (W == 5 and MIN_SEP == 10 and MAX_SEP == 60 and HOLD == 20 and COST == 0.0005):
        raise AssertionError("primary constants drifted")
    if choose_side(True, True, False) is not None:
        raise AssertionError("both sides on one session must be flat")
    if choose_side(True, False, True) is not None:
        raise AssertionError("a busy name must ignore a signal")
    if choose_side(False, True, False) != "short" or choose_side(True, False, False) != "long":
        raise AssertionError("single-side signal")

    # W=5 pivot is not known one bar early, and it dies if the last required close undercuts it.
    wave = [5, 4, 3, 2, 1, 0, 1, 2, 3, 4]
    early = _bars([(c, c, 1) for c in wave], _dates(len(wave)))
    full_early = {bar.session: i for i, bar in enumerate(early)}
    if divergences(early, full_early, 5, 10, 60, early[0].session, early[-1].session):
        raise AssertionError("an unfinished swing was confirmed")
    finished = wave + [5]
    done = _bars([(c, c, 1) for c in finished], _dates(len(finished)))
    full_done = {bar.session: i for i, bar in enumerate(done)}
    if divergences(done, full_done, 5, 10, 60, done[0].session, done[-1].session):
        raise AssertionError("one swing is not a divergence")
    undercut = wave + [ -1]
    bad = _bars([(c, c, 1) for c in undercut], _dates(len(undercut)))
    full_bad = {bar.session: i for i, bar in enumerate(bad)}
    if divergences(bad, full_bad, 5, 10, 60, bad[0].session, bad[-1].session):
        raise AssertionError("a future undercut still confirmed")

    days = _dates(15)
    bull = [
        (20, 20, 5), (18, 18, 5), (16, 16, 5), (17, 17, 5), (19, 19, 5),
        (18, 18, 5), (14, 14, 1), (15, 15, 5), (16, 16, 5), (15.0, 15.5, 5),
        (15.5, 16.0, 5), (16.0, 16.0, 5), (16.0, 15.0, 5), (15.2, 15.2, 5),
        (15.0, 15.0, 5),
    ]
    bars = _bars(bull, days)
    index = {day: i for i, day in enumerate(days)}
    book = {"A": bars}
    trades = collect_trades(book, index, index, days[0], days[-1], w=2, hold=4, fill_lag=1, mode="open",
                         min_sep=4, max_sep=30)
    if len(trades) != 1 or trades[0]["side"] != "long" or trades[0]["exit_reason"] != "time":
        raise AssertionError(f"bullish time exit: {[(t['side'], t['exit_reason']) for t in trades]}")
    if trades[0]["session"] != days[9] or trades[0]["exit_time"] != f"{days[13].isoformat()}T09:30:00":
        raise AssertionError("bullish fill sessions")
    _near(trades[0]["entry_price"], 15.0, "bull entry")
    _near(trades[0]["exit_price"], 15.2, "bull exit")
    _, net, _ = apply_account(trades, book, days, 0.0005, close_book=False)
    _near(net[9], -0.0005, "entry cost")
    _near(net[10], 15.5 / 15.0 - 1.0, "first step")
    _near(net[11], 16.0 / 15.5 - 1.0, "second step")
    _near(net[12], 0.0, "flat step")
    _near(net[13], 15.2 / 16.0 - 1.0 - 0.0005, "exit step")
    _near(trades[0]["gross"], (15.5 / 15.0 - 1.0) + (16.0 / 15.5 - 1.0) + (15.2 / 16.0 - 1.0), "trade gross")

    bear_rows = [
        (10, 10, 5), (12, 12, 5), (14, 14, 5), (13, 13, 5), (11, 11, 5),
        (12, 12, 5), (16, 16, 1), (15, 15, 5), (13, 13, 5), (15, 15, 5),
        (14, 14, 5), (14, 14, 5), (13, 13, 5), (13.5, 13, 5), (13, 13, 5),
    ]
    bear_bars = _bars(bear_rows, days)
    bear_trades = collect_trades({"B": bear_bars}, index, index, days[0], days[-1],
                                 w=2, hold=4, fill_lag=1, mode="open", min_sep=4, max_sep=30)
    if len(bear_trades) != 1 or bear_trades[0]["side"] != "short" or bear_trades[0]["exit_reason"] != "time":
        raise AssertionError(f"bearish time exit: {[(t['side'], t['exit_reason']) for t in bear_trades]}")
    _near(bear_trades[0]["entry_price"], 15.0, "bear entry")
    _near(bear_trades[0]["exit_price"], 13.5, "bear exit")

    fail_rows = [row for row in bull]
    fail_rows[10] = (15.5, 13.0, 5)
    fail_rows[11] = (14.0, 14.0, 5)
    fail_bars = _bars(fail_rows, days)
    fail = collect_trades({"A": fail_bars}, index, index, days[0], days[-1], w=2, hold=4, fill_lag=1,
                         mode="open", min_sep=4, max_sep=30)
    if len(fail) != 1 or fail[0]["exit_reason"] != "failure" or fail[0]["session"] != days[9]:
        raise AssertionError(f"failure exit: {[(t['exit_reason'], t['exit_time']) for t in fail]}")
    _near(fail[0]["exit_price"], 14.0, "failure price")

    # Failure on the close before the time open keeps the failure reason.
    tie_rows = [row for row in bull]
    tie_rows[12] = (16.0, 13.0, 5)
    tie = collect_trades({"A": _bars(tie_rows, days)}, index, index, days[0], days[-1],
                         w=2, hold=4, fill_lag=1, mode="open", min_sep=4, max_sep=30)
    if len(tie) != 1 or tie[0]["exit_reason"] != "failure" or tie[0]["exit_time"] != f"{days[13].isoformat()}T09:30:00":
        raise AssertionError("failure must beat the time exit on the same open")

    # Missing bar between the signal and the next calendar day delays the fill.
    kept = [bar for bar in bars if bar.session != days[9]]
    delayed = collect_trades({"A": kept}, index, index, days[0], days[-1], w=2, hold=4, fill_lag=1,
                            mode="open", min_sep=4, max_sep=30)
    if len(delayed) != 1 or delayed[0]["session"] != days[10] or delayed[0]["exit_reason"] != "time":
        raise AssertionError(f"missing bar: {[(t['session'], t['exit_reason']) for t in delayed]}")
    _near(delayed[0]["entry_price"], 15.5, "delayed entry")
    hole_gross, hole_net, _ = apply_account(delayed, {"A": kept}, days, 0.0, close_book=False)
    _near(hole_net[9], 0.0, "missing session contributes 0")
    _near(hole_gross[11], 16.0 / 15.5 - 1.0, "next real open")

    # A second signal while the first trade is open does not add or flip.
    busy = collect_trades(book, index, index, days[0], days[-1], w=2, hold=4, fill_lag=1, mode="open",
                         min_sep=4, max_sep=30)
    extra = {
        "confirm_pos": 11,
        "confirm_session": days[11],
        "side": "short",
        "level": 99.0,
        "gap": 1.0,
    }
    mixed = build_open_trades("A", bars, divergences(bars, index, 2, 4, 30, days[0], days[-1]) + [extra],
                              index, 4, 1, len(days) - 1)
    if len(mixed) != 1 or mixed[0]["side"] != "long":
        raise AssertionError("in-position signal was taken")
    if len(busy) != 1:
        raise AssertionError("busy control")

    both = [
        {"confirm_pos": 8, "confirm_session": days[8], "side": "long", "level": 14.0, "gap": 2.0},
        {"confirm_pos": 8, "confirm_session": days[8], "side": "short", "level": 19.0, "gap": 3.0},
    ]
    none = build_open_trades("A", bars, both, index, 4, 1, len(days) - 1)
    if none:
        raise AssertionError("both sides on one day opened a trade")

    # Two names, equal absolute weight, one shared holding window.
    left = _bars([(10, 10, 1), (10, 11, 1), (12, 12, 1), (12, 12, 1)], days[:4])
    right = _bars([(20, 20, 1), (20, 20, 1), (20, 22, 1), (22, 22, 1)], days[:4])
    made = [
        _blank_trade("L", "long", days[0], days[1], 10, days[3], 12, "time", 1, 3, 9, 1),
        _blank_trade("R", "short", days[0], days[1], 20, days[3], 22, "time", 1, 3, 21, 1),
    ]
    gross, _, held = apply_account(made, {"L": left, "R": right}, days[:4], 0.0005, close_book=False)
    _near(gross[2], 0.5 * (12 / 10 - 1) + 0.5 * (20 / 20 - 1), "two-name step")
    _near(gross[3], 0.5 * (12 / 12 - 1) + (-0.5) * (22 / 20 - 1), "short step")
    _near(held[1], 1.0, "exposure")
    _near(held[3], 0.0, "flat after exit")


def _check_store(eval_calendar: list[date], bars_by: dict[str, list[Bar]]) -> None:
    if len(NAMES) != 101 or len(set(NAMES)) != 101:
        raise RuntimeError("universe is not the 101 names")
    if len(eval_calendar) != 1004 or eval_calendar[0] != EVAL_START or eval_calendar[-1] != EVAL_END:
        raise RuntimeError("evaluation calendar is not 2022-10-03 through 2026-10-02")
    for symbol in ("QQQ", "SPY"):
        have = {bar.session for bar in bars_by[symbol]}
        missing = [day for day in eval_calendar if day not in have]
        if missing != [date(2026, 10, 2)]:
            raise RuntimeError(f"{symbol} coverage is not the expected one-day hole")
    for symbol in NAMES:
        if not bars_by[symbol]:
            raise RuntimeError(f"{symbol} has no bars")
        for bar in bars_by[symbol]:
            if bar.open <= 0.0 or bar.close <= 0.0:
                raise RuntimeError(f"{symbol} has a non-positive price")


def _bench(qqq: list[Bar], full_calendar: list[date], eval_calendar: list[date]) -> list[float]:
    closes = {bar.session: bar.close for bar in qqq}
    out = []
    prev = None
    wanted = set(eval_calendar)
    for day in full_calendar:
        if day not in wanted:
            if day in closes:
                prev = closes[day]
            continue
        if day in closes and prev is not None and prev > 0.0:
            out.append(closes[day] / prev - 1.0)
            prev = closes[day]
        else:
            out.append(0.0)
    if len(out) != len(eval_calendar):
        raise RuntimeError("benchmark length")
    return out


def _window_path(calendar: list[date], net: list[float]) -> dict:
    return performance(calendar, net, [], is_end=IS_END, oos_start=OOS_START, model="compound")


def _score_predictions(trades: list[dict]) -> dict:
    def side_pf(side: str):
        return profit_factor([trade["gross"] for trade in trades if trade["side"] == side])

    long_pf = side_pf("long")
    short_pf = side_pf("short")
    if long_pf is None or short_pf is None:
        side_score = "not testable"
    elif long_pf > 1.0 and short_pf > 1.0:
        side_score = "consistent"
    else:
        side_score = "not consistent"
    failure = sum(1 for trade in trades if trade["exit_reason"] == "failure")
    timed = sum(1 for trade in trades if trade["exit_reason"] == "time")
    ended = sum(1 for trade in trades if trade["exit_reason"] == "sample_end")
    if failure + timed == 0:
        fail_share = None
        fail_score = "not testable"
    else:
        fail_share = failure / (failure + timed)
        fail_score = "consistent" if fail_share < 0.5 else "not consistent"
    gaps = sorted(trade["swing_gap"] for trade in trades)
    if not gaps:
        return {
            "long_gross_pf": long_pf,
            "short_gross_pf": short_pf,
            "side_score": side_score,
            "failure_exits": failure,
            "time_exits": timed,
            "sample_end_exits": ended,
            "failure_share": fail_share,
            "failure_score": fail_score,
            "gap_median": None,
            "gap_above_mean_gross": None,
            "gap_below_mean_gross": None,
            "gap_above_n": 0,
            "gap_below_n": 0,
            "gap_score": "not testable",
        }
    mid = len(gaps) // 2
    median = gaps[mid] if len(gaps) % 2 else 0.5 * (gaps[mid - 1] + gaps[mid])
    above = [trade["gross"] for trade in trades if trade["swing_gap"] > median]
    below = [trade["gross"] for trade in trades if trade["swing_gap"] < median]
    above_mean = float(sum(above) / len(above)) if above else None
    below_mean = float(sum(below) / len(below)) if below else None
    if above_mean is None or below_mean is None:
        gap_score = "not testable"
    elif above_mean > below_mean:
        gap_score = "consistent"
    else:
        gap_score = "not consistent"
    return {
        "long_gross_pf": long_pf,
        "short_gross_pf": short_pf,
        "side_score": side_score,
        "failure_exits": failure,
        "time_exits": timed,
        "sample_end_exits": ended,
        "failure_share": fail_share,
        "failure_score": fail_score,
        "gap_median": float(median),
        "gap_above_mean_gross": above_mean,
        "gap_below_mean_gross": below_mean,
        "gap_above_n": len(above),
        "gap_below_n": len(below),
        "gap_score": gap_score,
    }


def _mean(values: list[float]) -> float | None:
    if not values:
        return None
    return float(sum(values) / len(values))


def _timing(bars_by, full_index, eval_calendar, eval_index, target: int, actual: float) -> dict:
    last = len(eval_calendar) - 1
    candidates = []
    names = {symbol: bars for symbol, bars in bars_by.items() if symbol not in ("QQQ", "SPY")}
    for symbol, bars in names.items():
        prev_close = None
        for pos, bar in enumerate(bars):
            k = eval_index.get(bar.session)
            if k is None:
                prev_close = bar.close
                continue
            if prev_close is not None and k < last:
                candidates.append((symbol, pos, k, prev_close))
            prev_close = bar.close
    templates = {}
    for symbol, pos, k, level in candidates:
        for side in ("long", "short"):
            templates[(symbol, pos, side)] = _forced_entry(
                symbol, names[symbol], pos, side, level, k, eval_index, HOLD, last)
    generator = np.random.default_rng(SEED)
    draws = np.empty(500, dtype=float)
    for draw in range(500):
        placed = []
        for _attempt in range(30):
            order = generator.permutation(len(candidates))
            sides = generator.integers(0, 2, size=len(candidates))
            placed = []
            occupied = {symbol: [] for symbol in names}
            for slot, pick in enumerate(order):
                symbol, pos, k, _level = candidates[int(pick)]
                side = "long" if int(sides[slot]) == 1 else "short"
                template = templates[(symbol, pos, side)]
                if template is None:
                    continue
                end = template["exclusive_end"]
                if any(k < stop and start < end for start, stop in occupied[symbol]):
                    continue
                trade = dict(template)
                trade["pieces"] = []
                placed.append(trade)
                occupied[symbol].append((k, end))
                if len(placed) == target:
                    break
            if len(placed) == target:
                break
        if (draw + 1) % 50 == 0:
            print(f"timing {draw + 1}", flush=True)
        if len(placed) != target:
            raise RuntimeError(f"timing draw {draw} placed {len(placed)} of {target}")
        gross, _, _ = apply_account(placed, names, eval_calendar, 0.0, close_book=False)
        value = sharpe(gross)
        draws[draw] = np.nan if value is None else value
    finite = draws[np.isfinite(draws)]
    if len(finite) == 0:
        raise RuntimeError("timing placebo produced no finite Sharpe")
    return {
        "actual_gross_sharpe": actual,
        "null_mean": float(finite.mean()),
        "null_p95": float(np.percentile(finite, 95)),
        "p": pvalue(actual, draws),
        "draws": 500,
        "seed": SEED,
        "samples": draws,
    }


def _forced_entry(symbol, bars, entry_pos, side, level, entry_index, eval_index, hold, last_index):
    """Random entry at a known open, then the locked failure and time exits."""
    if entry_pos >= len(bars):
        return None
    scheduled = None
    entry_bar = bars[entry_pos]
    for pos in range(entry_pos, len(bars)):
        bar = bars[pos]
        k = eval_index.get(bar.session)
        if k is None or pos == entry_pos:
            if pos == entry_pos and k is not None:
                if side == "long" and bar.close < level:
                    scheduled = "failure"
                elif side == "short" and bar.close > level:
                    scheduled = "failure"
            continue
        if scheduled == "failure":
            return _blank_trade(symbol, side, entry_bar.session, entry_bar.session, entry_bar.open,
                                bar.session, bar.open, "failure", entry_index, k, level, 0.0)
        if k >= entry_index + hold:
            return _blank_trade(symbol, side, entry_bar.session, entry_bar.session, entry_bar.open,
                                bar.session, bar.open, "time", entry_index, k, level, 0.0)
        if side == "long" and bar.close < level:
            scheduled = "failure"
        elif side == "short" and bar.close > level:
            scheduled = "failure"
    last_bar = entry_bar
    for bar in bars[entry_pos:]:
        k = eval_index.get(bar.session)
        if k is not None and entry_index <= k <= last_index:
            last_bar = bar
    return _blank_trade(symbol, side, entry_bar.session, entry_bar.session, entry_bar.open,
                        last_bar.session, last_bar.open, "sample_end", entry_index, last_index + 1, level, 0.0)


def _public_trade(trade: dict) -> dict:
    return {
        "session": trade["session"],
        "side": trade["side"],
        "entry_time": trade["entry_time"],
        "entry_price": trade["entry_price"],
        "exit_time": trade["exit_time"],
        "exit_price": trade["exit_price"],
        "gross": trade["gross"],
        "net": trade["net"],
        "exit_reason": trade["exit_reason"],
        "symbol": trade["symbol"],
        "swing_gap": trade["swing_gap"],
        "failure_level": trade["failure_level"],
        "confirm_session": trade["confirm_session"].isoformat(),
    }


def store_run(digest: str) -> None:
    from mdq import MarketData, nyse_sessions

    full_calendar = nyse_sessions(STORE_START, EVAL_END)
    eval_calendar = [day for day in full_calendar if EVAL_START <= day <= EVAL_END]
    bars_by: dict[str, list[Bar]] = {}
    with MarketData() as md:
        for symbol in NAMES + ["QQQ", "SPY"]:
            rows = md.bars(symbol, "1d", start=STORE_START, end=EVAL_END)
            bars_by[symbol] = [Bar(bar.session, bar.open, bar.close, bar.volume) for bar in rows]
    _check_store(eval_calendar, bars_by)
    full_index = {day: i for i, day in enumerate(full_calendar)}
    eval_index = {day: i for i, day in enumerate(eval_calendar)}
    names = {symbol: bars_by[symbol] for symbol in NAMES}
    benchmark = _bench(bars_by["QQQ"], full_calendar, eval_calendar)

    print("primary", flush=True)
    trades = collect_trades(names, full_index, eval_index, EVAL_START, EVAL_END,
                            w=W, hold=HOLD, fill_lag=1, mode="open")
    gross, net, held = apply_account(trades, names, eval_calendar, COST, close_book=False)

    print("costs", flush=True)
    costs = []
    for multiple in (0.0, 0.5, 1.0, 2.0, 3.0):
        _, priced, _ = apply_account(trades, names, eval_calendar, COST * multiple, close_book=False)
        stats = _window_path(eval_calendar, priced)
        costs.append({
            "multiple": multiple,
            "full_sharpe": stats["full"]["sharpe"],
            "oos_sharpe": stats["oos"]["sharpe"],
            "full_return": stats["full"]["total_return"],
        })
    # apply_account at 1x above replaced trade nets. Restore the base-cost nets.
    gross, net, held = apply_account(trades, names, eval_calendar, COST, close_book=False)

    print("delay", flush=True)
    delayed = collect_trades(names, full_index, eval_index, EVAL_START, EVAL_END,
                             w=W, hold=HOLD, fill_lag=2, mode="open")
    _, delay_net, _ = apply_account(delayed, names, eval_calendar, COST, close_book=False)
    delay_stats = _window_path(eval_calendar, delay_net)
    delay = {
        "full_sharpe": delay_stats["full"]["sharpe"],
        "oos_sharpe": delay_stats["oos"]["sharpe"],
        "full_return": delay_stats["full"]["total_return"],
        "oos_return": delay_stats["oos"]["total_return"],
    }

    print("upper bound", flush=True)
    bound_trades = collect_trades(names, full_index, eval_index, EVAL_START, EVAL_END,
                                  w=W, hold=HOLD, fill_lag=1, mode="close")
    _, bound_net, _ = apply_account(bound_trades, names, eval_calendar, COST, close_book=True)
    bound_stats = _window_path(eval_calendar, bound_net)

    print("grid", flush=True)
    grid = []
    for width in (3, 5, 8):
        for hold in (10, 20, 40):
            cell = collect_trades(names, full_index, eval_index, EVAL_START, EVAL_END,
                                  w=width, hold=hold, fill_lag=1, mode="open")
            _, cell_net, _ = apply_account(cell, names, eval_calendar, COST, close_book=False)
            stats = _window_path(eval_calendar, cell_net)
            grid.append({
                "params": {"W": width, "hold": hold},
                "is_sharpe": stats["is"]["sharpe"],
                "oos_sharpe": stats["oos"]["sharpe"],
                "full_return": stats["full"]["total_return"],
                "primary": width == W and hold == HOLD,
            })
    # The primary cell repriced `trades` via apply_account's mutation of `cell`.
    # Rebuild the primary account so the saved trades are the base book.
    gross, net, held = apply_account(trades, names, eval_calendar, COST, close_book=False)

    print("cross market", flush=True)
    cross = []
    for symbol in ("QQQ", "SPY"):
        cross_trades = collect_trades({symbol: bars_by[symbol]}, full_index, eval_index, EVAL_START, EVAL_END,
                                      w=W, hold=HOLD, fill_lag=1, mode="open")
        _, cross_net, _ = apply_account(cross_trades, {symbol: bars_by[symbol]}, eval_calendar, CROSS_COST,
                                       close_book=False)
        stats = performance(eval_calendar, cross_net, [_public_trade(trade) for trade in cross_trades],
                            is_end=IS_END, oos_start=OOS_START, model="compound")
        cross.append({
            "symbol": symbol,
            "is_sharpe": stats["is"]["sharpe"],
            "oos_sharpe": stats["oos"]["sharpe"],
            "full_return": stats["full"]["total_return"],
            "full_profit_factor": stats["full"]["profit_factor"],
        })

    print("placebo", flush=True)
    pieces = [list(trade["pieces"]) for trade in trades]
    direction = direction_placebo(eval_calendar, pieces, seed=SEED, draws=2000)
    timing = _timing(bars_by, full_index, eval_calendar, eval_index, len(trades), direction["actual_gross_sharpe"])
    bootstrap = block_bootstrap(net, seed=SEED, block=20, draws=2000)
    years = by_year(eval_calendar, net, benchmark, model="compound")
    quintiles = move_quintiles(eval_calendar, net, benchmark)
    predictions = _score_predictions(trades)
    lengths = [trade["exclusive_end"] - trade["entry_index"] for trade in trades]
    winners = [trade["net"] for trade in trades if trade["net"] > 0.0]
    losers = [trade["net"] for trade in trades if trade["net"] < 0.0]
    study = {
        "predictions": predictions,
        "by_side": [
            {
                "side": side,
                "trades": sum(1 for trade in trades if trade["side"] == side),
                "gross_profit_factor": predictions["long_gross_pf"] if side == "long" else predictions["short_gross_pf"],
                "net_profit_factor": profit_factor([trade["net"] for trade in trades if trade["side"] == side]),
            }
            for side in ("long", "short")
        ],
        "by_exit": [
            {"reason": reason, "trades": sum(1 for trade in trades if trade["exit_reason"] == reason)}
            for reason in ("failure", "time", "sample_end")
        ],
        "holding_sessions_mean": _mean([float(length) for length in lengths]),
        "holding_sessions_median": float(sorted(lengths)[len(lengths) // 2]) if lengths else None,
        "entry_sessions": len({trade["session"] for trade in trades}),
        "symbols_traded": len({trade["symbol"] for trade in trades}),
        "avg_winner": _mean(winners),
        "avg_loser": _mean(losers),
        "upper_bound": {
            "full_sharpe": bound_stats["full"]["sharpe"],
            "oos_sharpe": bound_stats["oos"]["sharpe"],
            "full_return": bound_stats["full"]["total_return"],
            "oos_return": bound_stats["oos"]["total_return"],
        },
        "qqq_missing_session": "2026-10-02",
    }
    if lengths and len(lengths) % 2 == 0:
        ordered = sorted(lengths)
        mid = len(ordered) // 2
        study["holding_sessions_median"] = float(0.5 * (ordered[mid - 1] + ordered[mid]))
    public = [_public_trade(trade) for trade in trades]
    doc = assemble(
        label="OBV divergence",
        benchmark_name="QQQ",
        model="compound",
        sessions=eval_calendar,
        strategy_net=net,
        benchmark=benchmark,
        trades=public,
        is_end=IS_END,
        oos_start=OOS_START,
        seeds={"direction": SEED, "timing": SEED, "bootstrap": SEED},
        costs=costs,
        delay=delay,
        grid=grid,
        placebo_direction=direction,
        placebo_timing=timing,
        bootstrap=bootstrap,
        by_year=years,
        by_move_quintile=quintiles,
        cross_market=cross,
        study=study,
        held=held,
        study_dir=HERE,
        rules_sha256=digest,
    )
    write_results(HERE / "results.json", doc)
    write_daily(HERE / "daily.csv", eval_calendar, net, benchmark, held)
    write_trades(HERE / "trades.csv", public)
    append_runlog(HERE, doc, reason=REASON)
    print(doc["status"], doc["primary"]["oos"]["sharpe"], doc["primary"]["oos"]["total_return"], flush=True)


def main() -> None:
    digest = assert_lock(HERE)
    self_test()
    store_run(digest)


if __name__ == "__main__":
    main()
