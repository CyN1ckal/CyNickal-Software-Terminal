# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Locked 6-1 relative-strength book of the QQQ equity holdings.

The self-test runs before the store is opened. A store run appends RUNLOG.md.
"""

from __future__ import annotations

import math
import sys
from datetime import date, timedelta
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
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
from research.kit.metrics import profit_factor, sharpe, total_return  # noqa: E402

HERE = Path(__file__).resolve().parent
QQQ = "QQQ"
TICKERS = (
    "NVDA", "AAPL", "MSFT", "MU", "AMD", "AMZN", "META", "GOOGL", "TSLA", "SPCX",
    "GOOG", "INTC", "AVGO", "WMT", "CSCO", "LRCX", "PLTR", "AMAT", "COST", "PANW",
    "NFLX", "CRWD", "KLAC", "TXN", "SNDK", "MRVL", "LIN", "AMGN", "ADI", "QCOM",
    "STX", "SHOP", "GILD", "ASML", "TMUS", "PEP", "WDC", "ISRG", "ARM", "FTNT",
    "VRTX", "BKNG", "SBUX", "ADP", "LITE", "CDNS", "ADBE", "SNPS", "MAR", "DDOG",
    "CEG", "CSX", "MELI", "MNST", "APP", "WBD", "DASH", "CTAS", "INTU", "CMCSA",
    "MDLZ", "REGN", "ROST", "MPWR", "TER", "ORLY", "ABNB", "HON", "AEP", "NXPI",
    "ALAB", "MSTR", "FAST", "NBIS", "PCAR", "BKR", "FANG", "PDD", "HONA", "PYPL",
    "XEL", "ADSK", "RKLB", "MCHP", "CCEP", "EXC", "KDP", "CRWV", "IDXX", "FER",
    "TTWO", "ODFL", "TRI", "WDAY", "PAYX", "ROP", "AXON", "DXCM", "ALNY", "GEHC",
    "CPRT",
)
HALF1 = (
    "AAPL", "ABNB", "ADBE", "ADI", "ADP", "ADSK", "AEP", "ALAB", "ALNY", "AMAT",
    "AMD", "AMGN", "AMZN", "APP", "ARM", "ASML", "AVGO", "AXON", "BKNG", "BKR",
    "CCEP", "CDNS", "CEG", "CMCSA", "COST", "CPRT", "CRWD", "CRWV", "CSCO", "CSX",
    "CTAS", "DASH", "DDOG", "DXCM", "EXC", "FANG", "FAST", "FER", "FTNT", "GEHC",
    "GILD", "GOOG", "GOOGL", "HON", "HONA", "IDXX", "INTC", "INTU", "ISRG", "KDP",
    "KLAC",
)
HALF2 = (
    "LIN", "LITE", "LRCX", "MAR", "MCHP", "MDLZ", "MELI", "META", "MNST", "MPWR",
    "MRVL", "MSFT", "MSTR", "MU", "NBIS", "NFLX", "NVDA", "NXPI", "ODFL", "ORLY",
    "PANW", "PAYX", "PCAR", "PDD", "PEP", "PLTR", "PYPL", "QCOM", "REGN", "RKLB",
    "ROP", "ROST", "SBUX", "SHOP", "SNDK", "SNPS", "SPCX", "STX", "TER", "TMUS",
    "TRI", "TSLA", "TTWO", "TXN", "VRTX", "WBD", "WDAY", "WDC", "WMT", "XEL",
)
WARMUP_START = date(2021, 10, 4)
EVAL_START = date(2022, 10, 3)
EVAL_END = date(2026, 10, 2)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
LOOKBACK = 126
SKIP = 21
STEP = 21
MIN_NAMES = 5
COST = 0.0005
SEED = 20261003
DIRECTION_DRAWS = 2000
TIMING_DRAWS = 500
BOOTSTRAP_DRAWS = 2000
BOOTSTRAP_BLOCK = 20
LOOKBACKS = (63, 126, 252)
SKIPS = (0, 21)
COST_MULTIPLES = (0.0, 0.5, 1.0, 2.0, 3.0)
REASON = (
    "Rerun after the first execution computed the book and then crashed with "
    "NameError on write_results before results.json, daily.csv, trades.csv, or "
    "RUNLOG.md were written. The missing import does not change the book. "
    "No headline numbers were saved from the crash."
)


def _positive(value) -> bool:
    return isinstance(value, (int, float)) and math.isfinite(value) and value > 0.0


def formation_indexes(t: int, lookback: int, skip: int) -> tuple[int, int]:
    """Start and end indexes of close[t-skip] / close[t-skip-lookback] - 1."""
    end = t - skip
    start = end - lookback
    return start, end


def eligible_ranked(closes, symbols, qqq: str, t: int, lookback: int, skip: int) -> list[tuple[str, float]]:
    start, end = formation_indexes(t, lookback, skip)
    if start < 0 or end < 0:
        return []
    q_start = closes[qqq][start]
    q_end = closes[qqq][end]
    if not (_positive(q_start) and _positive(q_end)):
        return []
    qqq_return = q_end / q_start - 1.0
    rows = []
    for symbol in symbols:
        c_start = closes[symbol][start]
        c_end = closes[symbol][end]
        c_signal = closes[symbol][t]
        if not (_positive(c_start) and _positive(c_end) and _positive(c_signal)):
            continue
        excess = (c_end / c_start - 1.0) - qqq_return
        rows.append((symbol, excess))
    rows.sort(key=lambda row: (-row[1], row[0]))
    return rows


def target_weights(ranked: list[tuple[str, float]], min_names: int = MIN_NAMES) -> dict[str, float]:
    count = len(ranked)
    if count < min_names:
        return {}
    k = max(1, count // 5)
    weight = 1.0 / k
    return {symbol: weight for symbol, _excess in ranked[:k]}


def signal_indexes(calendar, closes, symbols, qqq, lookback, skip, step, min_names, eval_start, eval_end):
    evaluated = [i for i, day in enumerate(calendar) if eval_start <= day <= eval_end]
    first = None
    for pos, index in enumerate(evaluated):
        if index - skip - lookback < 0:
            continue
        if len(eligible_ranked(closes, symbols, qqq, index, lookback, skip)) >= min_names:
            first = pos
            break
    if first is None:
        return []
    return evaluated[first::step]


def _blank_panels(symbols, n, calendar):
    opens = {symbol: [100.0] * n for symbol in symbols}
    closes = {symbol: [100.0] * n for symbol in symbols}
    times = {
        symbol: [f"{calendar[i].isoformat()}T09:30:00" for i in range(n)]
        for symbol in symbols
    }
    return opens, closes, times


def simulate(
    calendar,
    price,
    times,
    closes,
    symbols,
    qqq,
    *,
    lookback,
    skip,
    step,
    min_names,
    cost_rate,
    fill_lag,
    eval_start,
    eval_end,
    targets_by_fill=None,
):
    """Open-to-open (or close-to-close) book. `price` is the fill series."""
    count = len(calendar)
    evaluated = [i for i, day in enumerate(calendar) if eval_start <= day <= eval_end]
    out_pos = {index: pos for pos, index in enumerate(evaluated)}
    fill_targets: dict[int, dict[str, float]] = {}
    signal_dates = []
    if targets_by_fill is None:
        signals = signal_indexes(
            calendar, closes, symbols, qqq, lookback, skip, step, min_names, eval_start, eval_end,
        )
        for signal in signals:
            signal_dates.append(calendar[signal])
            fill = signal + fill_lag
            if fill_lag == 0:
                ok = fill in out_pos
            else:
                ok = signal < fill < count and fill in out_pos
            if not ok:
                continue
            ranked = eligible_ranked(closes, symbols, qqq, signal, lookback, skip)
            fill_targets[fill] = target_weights(ranked, min_names)
    else:
        fill_targets = {index: dict(weights) for index, weights in targets_by_fill.items()}

    lots: dict[str, dict] = {}
    live: dict[str, float] = {}
    gross = [0.0] * len(evaluated)
    costs = [0.0] * len(evaluated)
    held = [0.0] * len(evaluated)
    trades: list[dict] = []

    def book_incoming(index: int) -> None:
        pos = out_pos[index]
        if pos == 0:
            gross[pos] = 0.0
            held[pos] = 0.0
            return
        day_gross = 0.0
        for symbol, lot in lots.items():
            prev = price[symbol][index - 1]
            cur = price[symbol][index]
            if not (_positive(prev) and _positive(cur)):
                continue
            piece = lot["weight"] * (cur / prev - 1.0)
            lot["gross"] += piece
            lot["pieces"].append((calendar[index], piece))
            day_gross += piece
        gross[pos] = day_gross
        held[pos] = sum(lot["weight"] for lot in lots.values())

    def close_lot(symbol: str, index: int, exit_cost: float, reason: str) -> None:
        lot = lots.pop(symbol)
        trades.append({
            "session": calendar[lot["entry_i"]],
            "side": "long",
            "entry_time": times[symbol][lot["entry_i"]],
            "entry_price": lot["entry_price"],
            "exit_time": times[symbol][index],
            "exit_price": price[symbol][index],
            "gross": lot["gross"],
            "net": lot["gross"] - lot["entry_cost"] - exit_cost,
            "exit_reason": reason,
            "symbol": symbol,
            "weight": lot["weight"],
            "entry_cost": lot["entry_cost"],
            "exit_cost": exit_cost,
            "holding_sessions": index - lot["entry_i"],
            "_pieces": lot["pieces"],
        })

    def open_lot(symbol: str, index: int, weight: float, entry_cost: float) -> None:
        lots[symbol] = {
            "weight": weight,
            "entry_i": index,
            "entry_price": price[symbol][index],
            "entry_cost": entry_cost,
            "gross": 0.0,
            "pieces": [],
        }

    def execute(index: int, intended: dict[str, float], roll: bool) -> float:
        """Apply a scheduled target. Failed buys are dropped. Failed sells stay pending."""
        nonlocal live
        names = sorted(set(lots) | set(intended))
        paid = 0.0
        can_trade = {symbol: _positive(price[symbol][index]) for symbol in names}
        for symbol in names:
            old = lots[symbol]["weight"] if symbol in lots else 0.0
            want = intended.get(symbol, 0.0)
            if not can_trade[symbol]:
                continue
            changed = abs(want - old) > 1e-15
            if symbol in lots and (changed or (roll and old > 0.0)):
                exit_cost = cost_rate * max(old - want, 0.0)
                paid += exit_cost
                close_lot(symbol, index, exit_cost, "rebalance")
                if want > 1e-15:
                    entry_cost = cost_rate * max(want - old, 0.0)
                    paid += entry_cost
                    open_lot(symbol, index, want, entry_cost)
            elif symbol not in lots and want > 1e-15:
                entry_cost = cost_rate * want
                paid += entry_cost
                open_lot(symbol, index, want, entry_cost)
        live_out: dict[str, float] = {}
        for symbol, lot in lots.items():
            want = intended.get(symbol, 0.0)
            if not can_trade.get(symbol, False) and want < lot["weight"] - 1e-15:
                if want > 1e-15:
                    live_out[symbol] = want
            else:
                live_out[symbol] = lot["weight"]
        live = live_out
        return paid

    def execute_pending(index: int) -> float:
        paid = 0.0
        for symbol in sorted(list(lots)):
            want = live.get(symbol, 0.0)
            old = lots[symbol]["weight"]
            if old <= want + 1e-15:
                continue
            if not _positive(price[symbol][index]):
                continue
            exit_cost = cost_rate * (old - want)
            paid += exit_cost
            close_lot(symbol, index, exit_cost, "rebalance")
            if want > 1e-15:
                open_lot(symbol, index, want, 0.0)
                live[symbol] = want
            else:
                live.pop(symbol, None)
        return paid

    for index in evaluated:
        book_incoming(index)
        if index in fill_targets:
            paid = execute(index, fill_targets[index], True)
        else:
            paid = execute_pending(index)
        costs[out_pos[index]] += paid

    last = evaluated[-1] if evaluated else None
    if last is not None:
        for symbol in sorted(list(lots)):
            exit_i = last
            if not _positive(price[symbol][exit_i]):
                found = None
                for prior in range(len(evaluated) - 2, -1, -1):
                    candidate = evaluated[prior]
                    if _positive(price[symbol][candidate]):
                        found = candidate
                        break
                exit_i = lots[symbol]["entry_i"] if found is None else found
            close_lot(symbol, exit_i, 0.0, "sample_end")

    trades.sort(key=lambda row: (row["session"], row["symbol"]))
    net = [value - cost for value, cost in zip(gross, costs)]
    return {
        "dates": [calendar[index] for index in evaluated],
        "gross": gross,
        "cost": costs,
        "net": net,
        "held": held,
        "trades": trades,
        "signal_dates": signal_dates,
        "fill_dates": [calendar[index] for index in sorted(fill_targets)],
    }


def _fail(message: str) -> None:
    raise AssertionError(message)


def self_test() -> None:
    if len(TICKERS) != 101 or len(set(TICKERS)) != 101:
        _fail("universe is not 101 unique tickers")
    if tuple(sorted(TICKERS)[:51]) != HALF1 or tuple(sorted(TICKERS)[51:]) != HALF2:
        _fail("alphabetical halves do not match the locked lists")
    if formation_indexes(180, 126, 21) != (33, 159):
        _fail("primary formation indexes are not t-147 and t-21")
    skipped = set(range(180 - 20, 181))
    if set(formation_indexes(180, 126, 21)) & skipped:
        _fail("skip window includes one of the last 21 closes")
    series = [float(i + 1) for i in range(200)]
    start, end = formation_indexes(180, 126, 21)
    base = series[end] / series[start] - 1.0
    for index in range(180 - 20, 181):
        series[index] = 99999.0
    if series[end] / series[start] - 1.0 != base:
        _fail("scrambling the last 21 closes changed the formation")

    ranked = [("BBB", 0.2), ("AAA", 0.2), ("CCC", 0.1)]
    ranked.sort(key=lambda row: (-row[1], row[0]))
    if [symbol for symbol, _excess in ranked] != ["AAA", "BBB", "CCC"]:
        _fail("tie break did not give the higher rank to the earlier symbol")
    if target_weights([("A", 1.0)] * 4) != {}:
        _fail("N<5 was not flat")
    held5 = target_weights([(chr(65 + i), float(10 - i)) for i in range(5)])
    if list(held5) != ["A"] or held5["A"] != 1.0:
        _fail("N=5 did not hold one name at weight 1")
    held10 = target_weights([(f"{i:02d}", float(20 - i)) for i in range(10)])
    if len(held10) != 2 or any(abs(weight - 0.5) > 1e-15 for weight in held10.values()):
        _fail("N=10 did not hold two names at weight 1/2")
    if len(target_weights([(str(i), float(i)) for i in range(25)])) != 5:
        _fail("N=25 did not hold five names")

    symbols = ["QQQ", "FFF", "EEE", "DDD", "CCC", "BBB", "AAA"]
    names = ["AAA", "BBB", "CCC", "DDD", "EEE", "FFF"]
    calendar = [date(2020, 1, 6) + timedelta(days=i) for i in range(8)]
    opens, closes, times = _blank_panels(symbols, 8, calendar)
    closes["QQQ"] = [100.0] * 8
    for symbol, end_px in (("AAA", 130.0), ("BBB", 130.0), ("CCC", 110.0), ("DDD", 105.0), ("EEE", 90.0)):
        closes[symbol][0] = 100.0
        closes[symbol][2] = end_px
    closes["FFF"][0] = math.nan
    closes["FFF"][2] = 1000.0
    closes["BBB"][5] = 150.0
    opens["AAA"][4] = 200.0
    opens["AAA"][5] = 210.0
    opens["AAA"][6] = 210.0
    opens["AAA"][7] = 231.0
    opens["BBB"][7] = 80.0
    ranked_t = eligible_ranked(closes, names, "QQQ", 3, 2, 1)
    if [symbol for symbol, _excess in ranked_t] != ["AAA", "BBB", "CCC", "DDD", "EEE"]:
        _fail(f"ranking or missing endpoint failed: {ranked_t}")
    if any(symbol == "FFF" for symbol, _excess in ranked_t):
        _fail("missing endpoint stayed eligible")
    result = simulate(
        calendar, opens, times, closes, names, "QQQ",
        lookback=2, skip=1, step=3, min_names=5, cost_rate=0.0005,
        fill_lag=1, eval_start=calendar[0], eval_end=calendar[-1],
    )
    if [row["symbol"] for row in result["trades"]] != ["AAA", "BBB"]:
        _fail(f"unexpected trades {result['trades']}")
    aaa, bbb = result["trades"]
    if abs(aaa["gross"] - 0.15) > 1e-12 or abs(aaa["net"] - 0.149) > 1e-12:
        _fail(f"AAA gross/net {aaa['gross']} {aaa['net']}")
    if aaa["entry_price"] != 200.0 or aaa["exit_price"] != 231.0 or aaa["exit_reason"] != "rebalance":
        _fail("AAA fill prices or reason")
    if abs(bbb["gross"]) > 1e-12 or abs(bbb["net"] + 0.0005) > 1e-12 or bbb["exit_reason"] != "sample_end":
        _fail(f"BBB trade {bbb}")
    if abs(sum(result["net"]) - 0.1485) > 1e-12:
        _fail(f"daily net sum {sum(result['net'])} path {result['net']}")
    if result["held"][4] != 0.0 or result["held"][5] != 1.0 or result["held"][7] != 1.0:
        _fail(f"held {result['held']}")

    flat_names = ["AAA", "BBB", "CCC", "DDD"]
    flat_symbols = flat_names + ["QQQ"]
    flat_cal = [date(2020, 2, 3) + timedelta(days=i) for i in range(8)]
    flat_open, flat_close, flat_time = _blank_panels(flat_symbols, 8, flat_cal)
    flat = simulate(
        flat_cal, flat_open, flat_time, flat_close, flat_names, "QQQ",
        lookback=2, skip=1, step=3, min_names=5, cost_rate=0.0005,
        fill_lag=1, eval_start=flat_cal[0], eval_end=flat_cal[-1],
    )
    if flat["trades"] or any(value != 0.0 for value in flat["net"]):
        _fail("N<5 book was not flat")

    delay_cal = [date(2020, 3, 2) + timedelta(days=i) for i in range(9)]
    delay_symbols = symbols
    d_open, d_close, d_time = _blank_panels(delay_symbols, 9, delay_cal)
    for symbol, end_px in (("AAA", 130.0), ("BBB", 120.0), ("CCC", 110.0), ("DDD", 105.0), ("EEE", 90.0)):
        d_close[symbol][2] = end_px
    d_close["FFF"][0] = math.nan
    d_close["BBB"][5] = 150.0
    d_open["AAA"][4] = 200.0
    d_open["AAA"][5] = 210.0
    d_open["AAA"][6] = 210.0
    d_open["AAA"][7] = math.nan
    d_open["AAA"][8] = 210.0
    d_open["BBB"][7] = 80.0
    d_open["BBB"][8] = 80.0
    delayed = simulate(
        delay_cal, d_open, d_time, d_close, names, "QQQ",
        lookback=2, skip=1, step=3, min_names=5, cost_rate=0.0005,
        fill_lag=1, eval_start=delay_cal[0], eval_end=delay_cal[-1],
    )
    by_symbol = {row["symbol"]: row for row in delayed["trades"]}
    if abs(delayed["gross"][7]) > 1e-12:
        _fail("missing open contributed a return")
    if by_symbol["AAA"]["exit_price"] != 210.0 or by_symbol["AAA"]["exit_reason"] != "rebalance":
        _fail("exit did not wait for the next open")
    if abs(by_symbol["AAA"]["gross"] - 0.05) > 1e-12:
        _fail(f"delayed AAA gross {by_symbol['AAA']['gross']}")
    if abs(delayed["held"][8] - 2.0) > 1e-12:
        _fail(f"overlap exposure {delayed['held'][8]}")
    empty = eligible_ranked(closes, names, "QQQ", 3, 2, 1)
    closes["QQQ"][0] = math.nan
    if eligible_ranked(closes, names, "QQQ", 3, 2, 1):
        _fail("missing QQQ endpoint left names eligible")
    closes["QQQ"][0] = 100.0
    if not empty:
        _fail("control ranking was empty")


def _load(md, symbols, calendar):
    index = {day: i for i, day in enumerate(calendar)}
    opens = {symbol: [math.nan] * len(calendar) for symbol in symbols}
    closes = {symbol: [math.nan] * len(calendar) for symbol in symbols}
    times = {symbol: [None] * len(calendar) for symbol in symbols}
    for symbol in symbols:
        for bar in md.bars(symbol, "1d", start=calendar[0], end=calendar[-1]):
            slot = index.get(bar.session)
            if slot is None:
                continue
            if _positive(bar.open):
                opens[symbol][slot] = float(bar.open)
            if _positive(bar.close):
                closes[symbol][slot] = float(bar.close)
            times[symbol][slot] = bar.time.isoformat(timespec="seconds")
    return opens, closes, times


def _path_summary(dates, net):
    return performance(dates, net, [], is_end=IS_END, oos_start=OOS_START, model="compound")


def _pearson(left, right) -> float | None:
    a = np.asarray(left, dtype=float)
    b = np.asarray(right, dtype=float)
    if len(a) < 2 or float(a.std(ddof=1)) == 0.0 or float(b.std(ddof=1)) == 0.0:
        return None
    return float(np.corrcoef(a, b)[0, 1])


def _spearman(left, right) -> float | None:
    if len(left) < 2:
        return None
    def ranks(values):
        order = np.argsort(np.asarray(values, dtype=float), kind="mergesort")
        out = np.empty(len(values), dtype=float)
        out[order] = np.arange(1, len(values) + 1)
        return out
    return _pearson(ranks(left), ranks(right))


def _prediction_one(calendar, opens, closes, symbols):
    signals = signal_indexes(
        calendar, closes, symbols, QQQ, LOOKBACK, SKIP, STEP, MIN_NAMES, EVAL_START, EVAL_END,
    )
    fills = []
    for signal in signals:
        fill = signal + 1
        if fill < len(calendar):
            fills.append((signal, fill))
    top_means = []
    bottom_means = []
    for (signal, fill), (_next_signal, next_fill) in zip(fills, fills[1:]):
        ranked = eligible_ranked(closes, symbols, QQQ, signal, LOOKBACK, SKIP)
        count = len(ranked)
        if count < MIN_NAMES:
            continue
        k = max(1, count // 5)
        def leg(names):
            values = []
            for symbol, _excess in names:
                entry = opens[symbol][fill]
                exit_ = opens[symbol][next_fill]
                if _positive(entry) and _positive(exit_):
                    values.append(exit_ / entry - 1.0)
            if not values:
                return None
            return float(sum(values) / len(values))
        top = leg(ranked[:k])
        bottom = leg(ranked[-k:])
        if top is None or bottom is None:
            continue
        top_means.append(top)
        bottom_means.append(bottom)
    if not top_means:
        return {"score": "not testable", "rebalances": 0, "mean_top": None, "mean_bottom": None}
    mean_top = float(sum(top_means) / len(top_means))
    mean_bottom = float(sum(bottom_means) / len(bottom_means))
    score = "consistent" if mean_top > mean_bottom else "not consistent"
    return {
        "score": score,
        "rebalances": len(top_means),
        "mean_top": mean_top,
        "mean_bottom": mean_bottom,
    }


def _prediction_three(dates, net, benchmark, held):
    kept = [
        (value, market, flag)
        for day, value, market, flag in zip(dates, net, benchmark, held)
        if day >= OOS_START
    ]
    if len(kept) < 2:
        return {"score": "not testable"}
    strategy = [row[0] for row in kept]
    market = [row[1] for row in kept]
    flags = [row[2] for row in kept]
    corr = _pearson(strategy, market)
    equity = 1.0
    peak = 1.0
    under = []
    other = []
    for value, flag in zip(market, flags):
        equity *= 1.0 + value
        peak = max(peak, equity)
        if equity < peak:
            under.append(flag)
        else:
            other.append(flag)
    if corr is None or not under or not other:
        return {
            "score": "not testable",
            "correlation": corr,
            "underwater_sessions": len(under),
            "other_sessions": len(other),
            "mean_held_underwater": float(np.mean(under)) if under else None,
            "mean_held_other": float(np.mean(other)) if other else None,
        }
    mean_under = float(np.mean(under))
    mean_other = float(np.mean(other))
    moves = corr > 0.0
    invested = mean_under >= 0.5 * mean_other
    return {
        "score": "consistent" if moves and invested else "not consistent",
        "correlation": corr,
        "underwater_sessions": len(under),
        "other_sessions": len(other),
        "mean_held_underwater": mean_under,
        "mean_held_other": mean_other,
    }


def _activity(dates, trades):
    nets = [float(row["net"]) for row in trades]
    winners = [value for value in nets if value > 0.0]
    losers = [value for value in nets if value < 0.0]
    holds = [int(row["holding_sessions"]) for row in trades]
    holds_sorted = sorted(holds)
    if not holds_sorted:
        median = None
    elif len(holds_sorted) % 2 == 1:
        median = float(holds_sorted[len(holds_sorted) // 2])
    else:
        mid = len(holds_sorted) // 2
        median = float(0.5 * (holds_sorted[mid - 1] + holds_sorted[mid]))
    reasons = {}
    for row in trades:
        reasons.setdefault(row["exit_reason"], []).append(float(row["net"]))
    by_reason = []
    for reason in sorted(reasons):
        values = reasons[reason]
        by_reason.append({
            "exit_reason": reason,
            "trades": len(values),
            "profit_factor": profit_factor(values),
            "avg_net": float(np.mean(values)),
        })
    sessions = len(dates)
    return {
        "trades_per_year": (len(trades) / (sessions / 252.0)) if sessions else None,
        "holding_sessions_mean": float(np.mean(holds)) if holds else None,
        "holding_sessions_median": median,
        "avg_winner": float(np.mean(winners)) if winners else None,
        "avg_loser": float(np.mean(losers)) if losers else None,
        "long_trades": sum(1 for row in trades if row["side"] == "long"),
        "short_trades": sum(1 for row in trades if row["side"] == "short"),
        "by_exit_reason": by_reason,
        "sum_trade_net": float(sum(nets)),
    }


def _timing(calendar, opens, times, closes, symbols, actual_gross):
    signals = signal_indexes(
        calendar, closes, symbols, QQQ, LOOKBACK, SKIP, STEP, MIN_NAMES, EVAL_START, EVAL_END,
    )
    prepared = []
    for signal in signals:
        fill = signal + 1
        if fill >= len(calendar):
            continue
        ranked = eligible_ranked(closes, symbols, QQQ, signal, LOOKBACK, SKIP)
        prepared.append((fill, ranked))
    generator = np.random.default_rng(SEED)
    samples = np.empty(TIMING_DRAWS, dtype=float)
    for draw in range(TIMING_DRAWS):
        targets = {}
        for fill, ranked in prepared:
            count = len(ranked)
            if count < MIN_NAMES:
                targets[fill] = {}
                continue
            k = max(1, count // 5)
            ordered = sorted(symbol for symbol, _excess in ranked)
            pick = generator.choice(len(ordered), size=k, replace=False)
            weight = 1.0 / k
            targets[fill] = {ordered[int(slot)]: weight for slot in pick}
        path = simulate(
            calendar, opens, times, closes, symbols, QQQ,
            lookback=LOOKBACK, skip=SKIP, step=STEP, min_names=MIN_NAMES,
            cost_rate=0.0, fill_lag=1, eval_start=EVAL_START, eval_end=EVAL_END,
            targets_by_fill=targets,
        )
        value = sharpe(path["gross"])
        samples[draw] = np.nan if value is None else value
    finite = samples[np.isfinite(samples)]
    if len(finite) == 0 or actual_gross is None:
        raise RuntimeError("timing placebo produced no finite Sharpe")
    return {
        "actual_gross_sharpe": float(actual_gross),
        "null_mean": float(finite.mean()),
        "null_p95": float(np.percentile(finite, 95)),
        "p": pvalue(actual_gross, samples),
        "draws": TIMING_DRAWS,
        "seed": SEED,
        "samples": samples,
    }


def _break_even(costs: list[dict]) -> float | None:
    rows = sorted(costs, key=lambda row: float(row["multiple"]))
    for left, right in zip(rows, rows[1:]):
        a = float(left["full_return"])
        b = float(right["full_return"])
        if a == 0.0:
            return float(left["multiple"])
        if a > 0.0 >= b or a < 0.0 <= b:
            span = b - a
            if span == 0.0:
                return float(left["multiple"])
            weight = (0.0 - a) / span
            return float(left["multiple"] + weight * (float(right["multiple"]) - float(left["multiple"])))
    return None


def _public_trades(trades: list[dict]) -> list[dict]:
    cleaned = []
    for trade in trades:
        row = {key: value for key, value in trade.items() if key != "_pieces"}
        cleaned.append(row)
    return cleaned


def _pieces(trades: list[dict]):
    return [list(trade["_pieces"]) for trade in trades]


def main() -> None:
    digest = assert_lock(HERE)
    self_test()
    print("self-test ok")
    from mdq import MarketData, nyse_sessions

    calendar = nyse_sessions(WARMUP_START, EVAL_END)
    evaluated = [day for day in calendar if EVAL_START <= day <= EVAL_END]
    if len(calendar) != 1255 or len(evaluated) != 1004:
        raise RuntimeError(f"calendar {len(calendar)} evaluated {len(evaluated)}")
    symbols = list(TICKERS)
    with MarketData() as md:
        for symbol in symbols + [QQQ]:
            md.resolve(symbol)
        opens, closes, times = _load(md, symbols + [QQQ], calendar)
    print("loaded")

    def run_book(names, lookback, skip, cost_rate, fill_lag, price_name):
        series = opens if price_name == "open" else closes
        return simulate(
            calendar, series, times, closes, names, QQQ,
            lookback=lookback, skip=skip, step=STEP, min_names=MIN_NAMES,
            cost_rate=cost_rate, fill_lag=fill_lag,
            eval_start=EVAL_START, eval_end=EVAL_END,
        )

    primary = run_book(symbols, LOOKBACK, SKIP, COST, 1, "open")
    dates = primary["dates"]
    piece_sum = [0.0] * len(dates)
    date_index = {day: i for i, day in enumerate(dates)}
    for trade in primary["trades"]:
        for day, piece in trade["_pieces"]:
            piece_sum[date_index[day]] += piece
    if any(abs(left - right) > 1e-8 for left, right in zip(piece_sum, primary["gross"])):
        raise RuntimeError("trade pieces do not sum to the daily gross path")
    if abs(sum(row["net"] for row in primary["trades"]) - sum(primary["net"])) > 1e-6:
        raise RuntimeError("trade nets do not sum to the daily net path")

    benchmark = [0.0] * len(dates)
    for pos in range(1, len(dates)):
        prev = closes[QQQ][calendar.index(dates[pos - 1])]
        cur = closes[QQQ][calendar.index(dates[pos])]
        if _positive(prev) and _positive(cur):
            benchmark[pos] = cur / prev - 1.0

    gross_sharpe = sharpe(primary["gross"])
    pieces = _pieces(primary["trades"])
    direction = direction_placebo(dates, pieces, seed=SEED, draws=DIRECTION_DRAWS)
    if gross_sharpe is None or abs(direction["actual_gross_sharpe"] - gross_sharpe) > 1e-9:
        raise RuntimeError("direction placebo gross Sharpe does not match the daily gross path")
    print("direction placebo done")
    timing = _timing(calendar, opens, times, closes, symbols, direction["actual_gross_sharpe"])
    print("timing placebo done")
    bootstrap = block_bootstrap(primary["net"], seed=SEED, block=BOOTSTRAP_BLOCK, draws=BOOTSTRAP_DRAWS)

    cost_rows = []
    for multiple in COST_MULTIPLES:
        net = [gross - multiple * cost for gross, cost in zip(primary["gross"], primary["cost"])]
        summary = _path_summary(dates, net)
        cost_rows.append({
            "multiple": multiple,
            "cost_bp": multiple * 5.0,
            "full_sharpe": summary["full"]["sharpe"],
            "oos_sharpe": summary["oos"]["sharpe"],
            "is_sharpe": summary["is"]["sharpe"],
            "full_return": summary["full"]["total_return"],
            "oos_return": summary["oos"]["total_return"],
            "is_return": summary["is"]["total_return"],
        })

    delayed = run_book(symbols, LOOKBACK, SKIP, COST, 2, "open")
    delay_summary = _path_summary(dates, delayed["net"])
    close_fill = run_book(symbols, LOOKBACK, SKIP, COST, 0, "close")
    close_summary = _path_summary(dates, close_fill["net"])

    grid = []
    for lookback in LOOKBACKS:
        for skip in SKIPS:
            path = run_book(symbols, lookback, skip, COST, 1, "open")
            summary = _path_summary(dates, path["net"])
            grid.append({
                "name": f"L{lookback}-S{skip}",
                "params": {"lookback": lookback, "skip": skip},
                "is_sharpe": summary["is"]["sharpe"],
                "oos_sharpe": summary["oos"]["sharpe"],
                "is_return": summary["is"]["total_return"],
                "oos_return": summary["oos"]["total_return"],
                "primary": lookback == LOOKBACK and skip == SKIP,
            })
    print("grid done")

    cross = []
    for label, names in (("HALF1", list(HALF1)), ("HALF2", list(HALF2))):
        path = run_book(names, LOOKBACK, SKIP, COST, 1, "open")
        summary = performance(
            path["dates"], path["net"], _public_trades(path["trades"]),
            is_end=IS_END, oos_start=OOS_START, model="compound",
        )
        cross.append({
            "symbol": label,
            "is_sharpe": summary["is"]["sharpe"],
            "oos_sharpe": summary["oos"]["sharpe"],
            "full_return": summary["full"]["total_return"],
            "full_profit_factor": summary["full"]["profit_factor"],
            "full_sharpe": summary["full"]["sharpe"],
            "oos_return": summary["oos"]["total_return"],
            "oos_trades": summary["oos"]["trades"],
        })
    print("halves done")

    prediction_one = _prediction_one(calendar, opens, closes, symbols)
    by_lookback = {}
    for row in grid:
        by_lookback[(row["params"]["lookback"], row["params"]["skip"])] = row["is_sharpe"]
    worse = []
    prediction_two_score = "consistent"
    for lookback in LOOKBACKS:
        skipped = by_lookback[(lookback, 21)]
        included = by_lookback[(lookback, 0)]
        if skipped is None or included is None:
            prediction_two_score = "not testable"
            break
        worse.append(skipped < included)
    else:
        if prediction_two_score != "not testable":
            prediction_two_score = "not consistent" if all(worse) else "consistent"
    prediction_three = _prediction_three(dates, primary["net"], benchmark, primary["held"])
    is_values = [row["is_sharpe"] for row in grid]
    oos_values = [row["oos_sharpe"] for row in grid]
    order = sorted(range(len(grid)), key=lambda i: (-(is_values[i] if is_values[i] is not None else -1e9), i))
    primary_rank = order.index(next(i for i, row in enumerate(grid) if row["primary"])) + 1

    public = _public_trades(primary["trades"])
    activity = _activity(dates, public)
    activity["sum_daily_net"] = float(sum(primary["net"]))
    study = {
        "predictions": [
            {"id": 1, "name": "top_versus_bottom", **prediction_one},
            {
                "id": 2,
                "name": "skip_not_uniformly_worse",
                "score": prediction_two_score,
                "paired_skip21_below_skip0": worse if prediction_two_score != "not testable" else None,
            },
            {"id": 3, "name": "moves_with_qqq", **prediction_three},
        ],
        "close_fill_upper_bound": {
            "full_sharpe": close_summary["full"]["sharpe"],
            "oos_sharpe": close_summary["oos"]["sharpe"],
            "full_return": close_summary["full"]["total_return"],
            "oos_return": close_summary["oos"]["total_return"],
            "is_sharpe": close_summary["is"]["sharpe"],
        },
        "activity": activity,
        "grid_spearman": _spearman(
            [0.0 if value is None else value for value in is_values],
            [0.0 if value is None else value for value in oos_values],
        ),
        "grid_primary_is_rank": primary_rank,
        "grid_best_is_name": grid[order[0]]["name"],
        "break_even_cost_multiple": _break_even(cost_rows),
        "signals": len(primary["signal_dates"]),
        "fills": len(primary["fill_dates"]),
        "first_signal": primary["signal_dates"][0].isoformat() if primary["signal_dates"] else None,
        "last_signal": primary["signal_dates"][-1].isoformat() if primary["signal_dates"] else None,
        "first_fill": primary["fill_dates"][0].isoformat() if primary["fill_dates"] else None,
        "last_fill": primary["fill_dates"][-1].isoformat() if primary["fill_dates"] else None,
        "gross_sharpe_full": gross_sharpe,
        "rules_sha256": digest,
    }
    doc = assemble(
        label="QQQ holdings 6-1",
        benchmark_name="QQQ close-to-close",
        model="compound",
        sessions=dates,
        strategy_net=primary["net"],
        benchmark=benchmark,
        trades=public,
        is_end=IS_END,
        oos_start=OOS_START,
        seeds={"direction": SEED, "timing": SEED, "bootstrap": SEED},
        costs=cost_rows,
        delay={
            "full_sharpe": delay_summary["full"]["sharpe"],
            "oos_sharpe": delay_summary["oos"]["sharpe"],
            "full_return": delay_summary["full"]["total_return"],
            "oos_return": delay_summary["oos"]["total_return"],
            "is_sharpe": delay_summary["is"]["sharpe"],
        },
        grid=grid,
        placebo_direction=direction,
        placebo_timing=timing,
        bootstrap=bootstrap,
        by_year=by_year(dates, primary["net"], benchmark, model="compound"),
        by_move_quintile=move_quintiles(dates, primary["net"], benchmark),
        cross_market=cross,
        study=study,
        held=primary["held"],
        study_dir=HERE,
        rules_sha256=digest,
    )
    write_results(HERE / "results.json", doc)
    write_daily(HERE / "daily.csv", dates, primary["net"], benchmark, held=primary["held"])
    write_trades(HERE / "trades.csv", public)
    append_runlog(HERE, doc, reason=REASON)
    print(doc["status"])
    print(
        "OOS Sharpe", doc["primary"]["oos"]["sharpe"],
        "OOS return", doc["primary"]["oos"]["total_return"],
        "OOS trades", doc["primary"]["oos"]["trades"],
    )


if __name__ == "__main__":
    main()
