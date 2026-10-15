# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Next-open long after an SSR episode. Rules are locked in RULES.md.

The self-test runs before the store opens. A store run refuses a hash mismatch.
"""

from __future__ import annotations

import sys
from collections import defaultdict
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import EARLY_CLOSES, MarketData, nyse_sessions, ymd_from_int, ymd_int  # noqa: E402
from research.kit import (  # noqa: E402
    Criterion,
    append_runlog,
    assemble,
    assert_lock,
    asset_adjusted_direction_placebo,
    block_bootstrap,
    by_year,
    concentration_stress_test,
    deflated_sharpe_ratio,
    move_quintiles,
    performance,
    standardized_timing_placebo,
    write_daily,
    write_results,
    write_trades,
)
from research.kit.metrics import sharpe  # noqa: E402

HERE = Path(__file__).resolve().parent

SAMPLE_START = date(2015, 3, 25)
SAMPLE_END = date(2026, 10, 1)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
PRIMARY_HOLD = 1
HOLDS = (1, 2, 3, 5)
COST_BPS = 10.0
COST_MULTIPLES = (0.0, 0.5, 1.0, 2.0, 3.0)
EXCLUDE = frozenset({"VIXY", "UVXY"})
SEED_DIRECTION = 20261014
SEED_TIMING = 20261015
SEED_BOOTSTRAP = 20261016
TIMING_DRAWS = 500
DIRECTION_DRAWS = 2000
BOOTSTRAP_DRAWS = 2000


def cover_symbol(rows: list[dict], sessions: list[date]) -> dict[date, str]:
    """Restricted sessions for one symbol. A Nasdaq row wins over NYSE Group."""
    index = {day: i for i, day in enumerate(sessions)}
    covered: dict[date, str] = {}
    last = len(sessions)
    for row in rows:
        if row["source"] == "nasdaq":
            day = ymd_from_int(row["file_session"])
            if day in index:
                covered[day] = "nasdaq"
            continue
        start = ymd_from_int(row["trigger_session"])
        stop = ymd_from_int(row["deactivate_session"])
        cursor = index.get(start, 0)
        if start not in index:
            while cursor < last and sessions[cursor] < start:
                cursor += 1
        while cursor < last and sessions[cursor] < stop:
            day = sessions[cursor]
            if covered.get(day) != "nasdaq":
                covered[day] = "nyse_group"
            cursor += 1
    return covered


def build_episodes(covered: dict[date, str], sessions: list[date], symbol: str) -> list[dict]:
    """Episode start: restricted today, previous session not restricted. Entry is the next session."""
    previous = {sessions[i]: sessions[i - 1] for i in range(1, len(sessions))}
    nxt = {sessions[i]: sessions[i + 1] for i in range(len(sessions) - 1)}
    out = []
    for day, source in covered.items():
        prior = previous.get(day)
        if prior is not None and prior in covered:
            continue
        entry = nxt.get(day)
        if entry is None:
            continue
        out.append({"symbol": symbol, "signal": day, "entry": entry, "source": source})
    return out


def _clock(day: date, kind: str) -> str:
    if kind == "open":
        stamp = "09:30:00"
    else:
        stamp = "13:00:00" if day in EARLY_CLOSES else "16:00:00"
    return f"{day.isoformat()}T{stamp}"


def _bar_on(bars: dict, symbol: str, day: date):
    return bars.get(symbol, {}).get(day)


def run_book(
    sessions: list[date],
    bars: dict,
    episodes: list[dict],
    *,
    hold: int,
    cost_bps: float,
    delay: int = 0,
    fill: str = "next_open",
) -> dict:
    """Equal-weight long cohorts. `fill` is `next_open` or `signal_close`.

    Gross exposure at the fill is 1. Costs are `cost_bps` per side of the
    dollars bought and of the dollars sold. One cohort at a time: a new entry
    date is skipped while any share is still open. A name with no bar at its
    fill is left out. A later missing bar is sold at the next session open.
    """
    if fill not in ("next_open", "signal_close"):
        raise ValueError(fill)
    if hold < 1:
        raise ValueError("hold must be at least 1")
    if delay < 0:
        raise ValueError("delay must be >= 0")
    if fill == "signal_close" and delay:
        raise ValueError("the same-bar upper bound is not combined with a delay")

    index = {day: i for i, day in enumerate(sessions)}
    n = len(sessions)
    cost = cost_bps / 10000.0
    pending: dict[int, list[dict]] = defaultdict(list)
    for ep in episodes:
        signal = ep["signal"]
        entry = ep["entry"]
        if entry not in index:
            continue
        entry_i = index[entry]
        if fill == "next_open":
            fill_i = entry_i + delay
            if fill_i >= n:
                continue
            exit_i = fill_i + hold - 1
            fill_at = "open"
        else:
            if signal not in index:
                continue
            fill_i = index[signal]
            exit_i = entry_i + hold - 1
            fill_at = "close"
        capped = exit_i > n - 1
        exit_i = min(exit_i, n - 1)
        if exit_i < fill_i:
            continue
        pending[fill_i].append({
            "symbol": ep["symbol"],
            "signal": signal,
            "source": ep["source"],
            "fill_at": fill_at,
            "exit_i": exit_i,
            "capped": capped,
        })

    cash = 1.0
    positions: list[dict] = []
    nets: list[float] = []
    held: list[float] = []
    trades: list[dict] = []
    cohorts: list[dict] = []
    min_equity = 1.0
    max_intraday = 0.0
    max_overnight = 0.0
    cohort_id = 0

    def mark() -> float:
        total = cash
        for pos in positions:
            total += pos["shares"] * pos["last"]
        return total

    def position_value() -> float:
        return sum(pos["shares"] * pos["last"] for pos in positions)

    def note_leverage(overnight: bool) -> None:
        nonlocal max_intraday, max_overnight
        equity_now = mark()
        gross = position_value()
        if equity_now <= 0:
            lev = 999.0 if gross > 0 else 0.0
        else:
            lev = gross / equity_now
        if overnight:
            max_overnight = max(max_overnight, lev)
        else:
            max_intraday = max(max_intraday, lev)

    def close_position(pos: dict, price: float, day: date, kind: str, reason: str) -> None:
        nonlocal cash
        proceeds = pos["shares"] * price
        cash += proceeds
        cash -= cost * proceeds
        ratio = price / pos["entry_price"]
        weight = pos["weight"]
        gross = weight * (ratio - 1.0)
        net = gross - weight * cost - weight * ratio * cost
        trades.append({
            "session": pos["entry_day"],
            "side": "long",
            "entry_time": _clock(pos["entry_day"], pos["entry_kind"]),
            "entry_price": pos["entry_price"],
            "exit_time": _clock(day, kind),
            "exit_price": price,
            "gross": gross,
            "net": net,
            "exit_reason": reason,
            "symbol": pos["symbol"],
            "signal": pos["signal"].isoformat(),
            "source": pos["source"],
            "weight": weight,
            "cohort": pos["cohort"],
            "prior_return": "",
        })

    for i, day in enumerate(sessions):
        start = mark()
        # Sells discovered on an earlier missing bar, filled at this open.
        staying = []
        for pos in positions:
            if pos.get("sell_at") == "open" and pos.get("sell_index") <= i:
                bar = _bar_on(bars, pos["symbol"], day)
                if bar is None or bar.open <= 0:
                    if i + 1 >= n:
                        close_position(pos, pos["last"], day, "close", "sample_end")
                    else:
                        pos["sell_index"] = i + 1
                        staying.append(pos)
                    continue
                close_position(pos, bar.open, day, "open", pos["exit_reason"])
            else:
                staying.append(pos)
        positions = staying

        def enter(fill_at: str) -> None:
            nonlocal cash, cohort_id
            if positions or i not in pending:
                return
            specs = [spec for spec in pending[i] if spec["fill_at"] == fill_at]
            entrants = []
            for spec in specs:
                bar = _bar_on(bars, spec["symbol"], day)
                if bar is None:
                    continue
                price = bar.open if fill_at == "open" else bar.close
                if price <= 0:
                    continue
                entrants.append((spec, price))
            if not entrants:
                return
            equity_now = mark()
            if equity_now <= 0:
                return
            count = len(entrants)
            cohort_id += 1
            cohorts.append({"id": cohort_id, "sessions": []})
            for spec, price in entrants:
                dollars = equity_now / count
                positions.append({
                    "symbol": spec["symbol"],
                    "shares": dollars / price,
                    "last": price,
                    "entry_price": price,
                    "entry_day": day,
                    "entry_kind": fill_at,
                    "signal": spec["signal"],
                    "source": spec["source"],
                    "weight": 1.0 / count,
                    "planned_exit": spec["exit_i"],
                    "capped": spec["capped"],
                    "cohort": cohort_id,
                    "exit_reason": "sample_end" if spec["capped"] else "hold",
                })
                cash -= dollars
            cash -= cost * equity_now

        enter("open")
        note_leverage(False)

        staying = []
        for pos in positions:
            if pos.get("sell_at") == "open":
                staying.append(pos)
                continue
            bar = _bar_on(bars, pos["symbol"], day)
            if bar is None or bar.close <= 0:
                if i + 1 >= n:
                    close_position(pos, pos["last"], day, "close", "sample_end")
                else:
                    pos["sell_at"] = "open"
                    pos["sell_index"] = i + 1
                    pos["exit_reason"] = "missing_bar"
                    staying.append(pos)
                continue
            pos["last"] = bar.close
            if pos["planned_exit"] == i:
                reason = "sample_end" if pos["capped"] else "hold"
                close_position(pos, bar.close, day, "close", reason)
            else:
                staying.append(pos)
        positions = staying

        enter("close")
        if positions:
            note_leverage(True)
        equity = mark()
        min_equity = min(min_equity, equity)
        nets.append(0.0 if start == 0 else equity / start - 1.0)
        held.append(0.0)

    _assign_cohort_sessions(sessions, cohorts, trades, held)

    return {
        "net": nets,
        "held": held,
        "trades": trades,
        "cohorts": cohorts,
        "min_equity": min_equity,
        "max_intraday_leverage": max_intraday,
        "max_overnight_leverage": max_overnight,
    }


def _assign_cohort_sessions(sessions, cohorts, trades, held) -> None:
    """Each cohort spans its earliest fill through its latest exit, inclusive.

    The primary hold exits at the close, so the next cohort cannot start until
    the next session and the spans do not share a day.
    """
    index = {day: i for i, day in enumerate(sessions)}
    by_id = {cohort["id"]: cohort for cohort in cohorts}
    for cohort in cohorts:
        cohort["sessions"] = []
    spans: dict[int, list[int]] = {}
    for trade in trades:
        start = index[date.fromisoformat(trade["entry_time"][:10])]
        end = index[date.fromisoformat(trade["exit_time"][:10])]
        span = spans.get(trade["cohort"])
        if span is None:
            spans[trade["cohort"]] = [start, end]
        else:
            span[0] = min(span[0], start)
            span[1] = max(span[1], end)
    flagged = set()
    for cohort_id, (start, end) in spans.items():
        days = sessions[start:end + 1]
        by_id[cohort_id]["sessions"] = days
        flagged.update(days)
    for i, day in enumerate(sessions):
        held[i] = 1.0 if day in flagged else 0.0


def self_test() -> None:
    d1, d2, d3, d4 = date(2024, 6, 3), date(2024, 6, 4), date(2024, 6, 5), date(2024, 6, 6)
    sessions = [d1, d2, d3, d4]

    def bar(open_, close):
        return type("B", (), {"open": open_, "close": close})()

    def book(bars, episodes, **kwargs):
        return run_book(sessions, bars, episodes, **kwargs)

    # One name, hold 1, 100 -> 110, 10 bp per side.
    bars = {"AAA": {d1: bar(100.0, 110.0), d2: bar(110.0, 110.0), d3: bar(110.0, 110.0), d4: bar(110.0, 110.0)}}
    episodes = [{"symbol": "AAA", "signal": date(2024, 5, 31), "entry": d1, "source": "nasdaq"}]
    result = book(bars, episodes, hold=1, cost_bps=10)
    if abs(result["net"][0] - 0.0979) > 1e-12:
        raise AssertionError(result["net"][0])
    if abs(result["trades"][0]["net"] - 0.0979) > 1e-12:
        raise AssertionError(result["trades"][0]["net"])
    if result["trades"][0]["entry_price"] != 100.0 or result["trades"][0]["exit_price"] != 110.0:
        raise AssertionError(result["trades"][0])
    if result["held"][0] != 1.0 or result["held"][1] != 0.0:
        raise AssertionError(result["held"])

    # Two names, hold 1.
    bars["BBB"] = {d1: bar(50.0, 40.0), d2: bar(40.0, 40.0), d3: bar(40.0, 40.0), d4: bar(40.0, 40.0)}
    episodes.append({"symbol": "BBB", "signal": date(2024, 5, 31), "entry": d1, "source": "nyse_group"})
    result = book(bars, episodes, hold=1, cost_bps=10)
    if abs(result["net"][0] - (-0.05195)) > 1e-12:
        raise AssertionError(result["net"][0])
    if len(result["trades"]) != 2 or abs(result["trades"][0]["weight"] - 0.5) > 1e-12:
        raise AssertionError(result["trades"])

    # A name with no entry bar is dropped. The other name is the whole book.
    thin = {"AAA": bars["AAA"]}
    result = book(thin, episodes, hold=1, cost_bps=10)
    if len(result["trades"]) != 1 or result["trades"][0]["symbol"] != "AAA":
        raise AssertionError(result["trades"])
    if abs(result["net"][0] - 0.0979) > 1e-12:
        raise AssertionError(result["net"][0])

    # Hold 2 compounds close to close. 100, 110, then 121.
    path = {"AAA": {d1: bar(100.0, 110.0), d2: bar(110.0, 121.0), d3: bar(121.0, 121.0), d4: bar(121.0, 121.0)}}
    one = [{"symbol": "AAA", "signal": date(2024, 5, 31), "entry": d1, "source": "nasdaq"}]
    result = book(path, one, hold=2, cost_bps=10)
    if abs(result["net"][0] - 0.099) > 1e-12:
        raise AssertionError(result["net"][0])
    if abs(mark_end(result) - 1.20779) > 1e-9:
        raise AssertionError(mark_end(result))
    if result["trades"][0]["exit_reason"] != "hold" or result["held"][1] != 1.0:
        raise AssertionError(result["trades"][0])

    # A second episode whose entry falls inside the hold is skipped.
    second = one + [{"symbol": "BBB", "signal": d1, "entry": d2, "source": "nasdaq"}]
    result = book(path, second, hold=2, cost_bps=10)
    if len(result["trades"]) != 1:
        raise AssertionError(len(result["trades"]))

    # Two missing sessions, then an open. The sale waits for that open.
    gap2 = {"AAA": {d1: bar(100.0, 110.0), d4: bar(80.0, 80.0)}}
    result = book(gap2, one, hold=3, cost_bps=0)
    if result["trades"][0]["exit_price"] != 80.0 or result["trades"][0]["session"] != d1:
        raise AssertionError(result["trades"][0])
    if result["held"][1] != 1.0 or result["held"][3] != 1.0:
        raise AssertionError(result["held"])

    # Missing day 2 sells at the next open. Planned hold was 3.
    gap = {"AAA": {d1: bar(100.0, 110.0), d3: bar(90.0, 90.0), d4: bar(90.0, 90.0)}}
    result = book(gap, one, hold=3, cost_bps=10)
    if result["trades"][0]["exit_reason"] != "missing_bar":
        raise AssertionError(result["trades"][0])
    if result["trades"][0]["exit_price"] != 90.0 or abs(result["net"][1]) > 1e-12:
        raise AssertionError((result["trades"][0], result["net"]))
    if abs(mark_end(result) - 0.8981) > 1e-9:
        raise AssertionError(mark_end(result))

    # Delay by one session.
    result = book(path, one, hold=1, cost_bps=0, delay=1)
    if result["trades"][0]["session"] != d2 or result["trades"][0]["entry_price"] != 110.0:
        raise AssertionError(result["trades"][0])
    if abs(result["trades"][0]["gross"] - (121.0 / 110.0 - 1.0)) > 1e-12:
        raise AssertionError(result["trades"][0]["gross"])

    # Same-bar upper bound buys the signal close.
    upper_sessions = [d1, d2, d3, d4]
    upper_bars = {"AAA": {d1: bar(90.0, 100.0), d2: bar(100.0, 110.0), d3: bar(110.0, 110.0), d4: bar(110.0, 110.0)}}
    upper_eps = [{"symbol": "AAA", "signal": d1, "entry": d2, "source": "nasdaq"}]
    result = run_book(upper_sessions, upper_bars, upper_eps, hold=1, cost_bps=10, fill="signal_close")
    if result["trades"][0]["entry_price"] != 100.0 or result["trades"][0]["exit_price"] != 110.0:
        raise AssertionError(result["trades"][0])
    if abs(result["net"][0] - (-0.001)) > 1e-12:
        raise AssertionError(result["net"][0])
    if abs(mark_end(result) - 1.0979) > 1e-9:
        raise AssertionError(mark_end(result))

    # Hold longer than the sample flattens on the last close.
    short = [d1, d2]
    result = run_book(short, path, one, hold=5, cost_bps=0)
    if result["trades"][0]["exit_reason"] != "sample_end" or result["trades"][0]["exit_price"] != 121.0:
        raise AssertionError(result["trades"][0])

    # Episode builder: a second restricted session is not a new episode.
    covered = {d1: "nyse_group", d2: "nasdaq", d4: "nyse_group"}
    built = build_episodes(covered, [d1, d2, d3, d4], "AAA")
    got = [(row["signal"], row["entry"], row["source"]) for row in built]
    if got != [(d1, d2, "nyse_group")]:
        raise AssertionError(got)

    # Zero cost is the price ratio.
    result = book(bars, [{"symbol": "AAA", "signal": date(2024, 5, 31), "entry": d1, "source": "nasdaq"}], hold=1, cost_bps=0)
    if abs(result["net"][0] - 0.10) > 1e-12:
        raise AssertionError(result["net"][0])


def mark_end(result: dict) -> float:
    equity = 1.0
    for value in result["net"]:
        equity *= 1.0 + value
    return equity


def _load(md: MarketData):
    calendar = nyse_sessions(date(2015, 3, 20), SAMPLE_END)
    book = [day for day in nyse_sessions(SAMPLE_START, SAMPLE_END)]
    instruments = [row for row in md.instruments() if row["symbol"] not in EXCLUDE and row["1d_sessions"]]
    symbols = {row["symbol"].strip().upper() for row in instruments}
    rows_by_symbol: dict[str, list[dict]] = defaultdict(list)
    for row in md.sql(
        "SELECT source, symbol, file_session, trigger_session, deactivate_session "
        "FROM short_sale_restriction"
    ):
        symbol = str(row["symbol"]).strip().upper()
        if symbol in symbols:
            rows_by_symbol[symbol].append(row)

    bars: dict[str, dict[date, object]] = {}
    episodes: list[dict] = []
    for inst in instruments:
        symbol = inst["symbol"].strip().upper()
        series = {}
        for bar in md.bars(inst["id"], "1d", start=date(2015, 3, 1), end=SAMPLE_END, adjust=True):
            series[bar.session] = bar
        if not series:
            continue
        bars[symbol] = series
        covered = cover_symbol(rows_by_symbol.get(symbol, []), calendar)
        episodes.extend(build_episodes(covered, calendar, symbol))

    spy = {}
    for bar in md.bars("SPY", "1d", start=SAMPLE_START, end=SAMPLE_END, adjust=True):
        spy[bar.session] = bar
    kept = [day for day in book if day in spy and spy[day].open > 0 and spy[day].close > 0]
    benchmark = [spy[day].close / spy[day].open - 1.0 for day in kept]
    return kept, bars, episodes, benchmark, len(book) - len(kept)


def _attach_prior(trades, bars, calendar) -> None:
    previous = {calendar[i]: calendar[i - 1] for i in range(1, len(calendar))}
    for trade in trades:
        signal = date.fromisoformat(trade["signal"])
        prev = previous.get(signal)
        today = _bar_on(bars, trade["symbol"], signal)
        before = _bar_on(bars, trade["symbol"], prev) if prev else None
        if today is None or before is None or before.close <= 0 or today.close <= 0:
            trade["prior_return"] = ""
            continue
        trade["prior_return"] = f"{today.close / before.close - 1.0:.10f}"


def _prediction_event(sessions, bars, trades) -> dict:
    book = set(sessions)
    entries: dict[str, set[date]] = defaultdict(set)
    for trade in trades:
        entries[trade["symbol"]].add(trade["session"] if isinstance(trade["session"], date) else date.fromisoformat(str(trade["session"])))
    event = []
    other = []
    for symbol, days in entries.items():
        for day, bar in bars.get(symbol, {}).items():
            if day not in book or bar.open <= 0:
                continue
            value = bar.close / bar.open - 1.0
            if day in days:
                event.append(value)
            else:
                other.append(value)
    event_mean = sum(event) / len(event) if event else None
    other_mean = sum(other) / len(other) if other else None
    return {
        "event_days": len(event),
        "other_days": len(other),
        "event_mean_open_to_close": event_mean,
        "other_mean_open_to_close": other_mean,
        "consistent": event_mean is not None and other_mean is not None and event_mean > other_mean,
    }


def _prediction_prior(trades) -> dict:
    rows = []
    for trade in trades:
        text = trade.get("prior_return")
        if text == "" or text is None:
            continue
        rows.append((float(text), float(trade["net"])))
    rows.sort(key=lambda item: item[0])
    buckets = {q: [] for q in range(1, 6)}
    count = len(rows)
    for rank, (prior, net) in enumerate(rows):
        quintile = min(5, rank * 5 // count + 1) if count else 1
        buckets[quintile].append(net)
    means = {
        str(q): (sum(values) / len(values) if values else None)
        for q, values in buckets.items()
    }
    low = means["1"]
    high = means["5"]
    return {
        "trades": count,
        "mean_net_by_quintile": means,
        "consistent": low is not None and high is not None and low > high,
    }


def _cost_row(sessions, nets, multiple: float) -> dict:
    perf = performance(sessions, nets, [], is_end=IS_END, oos_start=OOS_START, model="compound")
    return {
        "multiple": multiple,
        "full_sharpe": perf["full"]["sharpe"],
        "oos_sharpe": perf["oos"]["sharpe"],
        "full_return": perf["full"]["total_return"],
        "oos_return": perf["oos"]["total_return"],
    }


def _grid_row(sessions, nets, trades, hold: int) -> dict:
    perf = performance(sessions, nets, trades, is_end=IS_END, oos_start=OOS_START, model="compound")
    return {
        "params": {"hold": hold},
        "name": f"hold={hold}",
        "primary": hold == PRIMARY_HOLD,
        "is_sharpe": perf["is"]["sharpe"],
        "oos_sharpe": perf["oos"]["sharpe"],
        "full_return": perf["full"]["total_return"],
        "is_return": perf["is"]["total_return"],
        "oos_return": perf["oos"]["total_return"],
        "oos_trades": perf["oos"]["trades"],
        "full_profit_factor": perf["full"]["profit_factor"],
    }


def _cross_row(sessions, nets, trades, label: str) -> dict:
    perf = performance(sessions, nets, trades, is_end=IS_END, oos_start=OOS_START, model="compound")
    return {
        "symbol": label,
        "is_sharpe": perf["is"]["sharpe"],
        "oos_sharpe": perf["oos"]["sharpe"],
        "full_return": perf["full"]["total_return"],
        "full_profit_factor": perf["full"]["profit_factor"],
        "oos_trades": perf["oos"]["trades"],
    }


def run_store() -> None:
    assert_lock(HERE)
    with MarketData() as md:
        sessions, bars, episodes, benchmark, dropped_spy = _load(md)
    calendar = nyse_sessions(date(2015, 3, 20), SAMPLE_END)

    def simulate(eps, **kwargs):
        return run_book(sessions, bars, eps, **kwargs)

    primary = simulate(episodes, hold=PRIMARY_HOLD, cost_bps=COST_BPS)
    gross = simulate(episodes, hold=PRIMARY_HOLD, cost_bps=0.0)
    _attach_prior(primary["trades"], bars, calendar)

    costs = []
    for multiple in COST_MULTIPLES:
        path = primary if multiple == 1.0 else simulate(episodes, hold=PRIMARY_HOLD, cost_bps=COST_BPS * multiple)
        costs.append(_cost_row(sessions, path["net"], multiple))

    grid = []
    grid_paths = {}
    for hold in HOLDS:
        path = primary if hold == PRIMARY_HOLD else simulate(episodes, hold=hold, cost_bps=COST_BPS)
        grid_paths[hold] = path
        grid.append(_grid_row(sessions, path["net"], path["trades"], hold))

    delay = simulate(episodes, hold=PRIMARY_HOLD, cost_bps=COST_BPS, delay=1)
    delay_perf = performance(sessions, delay["net"], delay["trades"], is_end=IS_END, oos_start=OOS_START)
    upper = simulate(episodes, hold=PRIMARY_HOLD, cost_bps=COST_BPS, fill="signal_close")
    upper_perf = performance(sessions, upper["net"], upper["trades"], is_end=IS_END, oos_start=OOS_START)

    cross = []
    for source, label in (("nasdaq", "NASDAQ"), ("nyse_group", "NYSE_GROUP")):
        subset = [ep for ep in episodes if ep["source"] == source]
        path = simulate(subset, hold=PRIMARY_HOLD, cost_bps=COST_BPS)
        cross.append(_cross_row(sessions, path["net"], path["trades"], label))

    pieces = []
    gross_by_day = {day: value for day, value in zip(sessions, gross["net"])}
    for cohort in gross["cohorts"]:
        if not cohort["sessions"]:
            continue
        pieces.append([(day, gross_by_day[day]) for day in cohort["sessions"]])
    placebo = asset_adjusted_direction_placebo(
        sessions, pieces, benchmark, seed=SEED_DIRECTION, draws=DIRECTION_DRAWS,
    )
    held_mask_returns = [bench if flag else 0.0 for bench, flag in zip(benchmark, primary["held"])]
    durations = [len(cohort["sessions"]) for cohort in primary["cohorts"] if cohort["sessions"]]
    try:
        timing = standardized_timing_placebo(
            sessions,
            durations,
            benchmark,
            sharpe(held_mask_returns) or 0.0,
            seed=SEED_TIMING,
            draws=TIMING_DRAWS,
        )
    except ValueError as exc:
        timing = None
        timing_error = str(exc)
    else:
        timing_error = None

    bootstrap = block_bootstrap(primary["net"], seed=SEED_BOOTSTRAP, block=20, draws=BOOTSTRAP_DRAWS)
    years = by_year(sessions, primary["net"], benchmark, model="compound")
    quintiles = move_quintiles(sessions, primary["net"], benchmark)
    stress = concentration_stress_test(sessions, primary["net"])
    dsr = deflated_sharpe_ratio(primary["net"], trials_count=len(HOLDS))

    hold_means = []
    for hold in HOLDS:
        nets = [float(trade["net"]) for trade in grid_paths[hold]["trades"]]
        hold_means.append({
            "hold": hold,
            "trades": len(nets),
            "mean_net": (sum(nets) / len(nets) if nets else None),
        })
    decreasing = all(
        hold_means[i]["mean_net"] is not None
        and hold_means[i + 1]["mean_net"] is not None
        and hold_means[i]["mean_net"] > hold_means[i + 1]["mean_net"]
        for i in range(len(hold_means) - 1)
    )
    nasdaq_return = cross[0]["full_return"]
    nyse_return = cross[1]["full_return"]
    same_sign = (
        nasdaq_return is not None
        and nyse_return is not None
        and nasdaq_return != 0
        and nyse_return != 0
        and (nasdaq_return > 0) == (nyse_return > 0)
    )

    primary_perf = performance(
        sessions, primary["net"], primary["trades"],
        is_end=IS_END, oos_start=OOS_START, model="compound", held=primary["held"],
    )
    extra = [
        Criterion(
            "max_drawdown",
            "full-sample max drawdown >= -0.35",
            float(primary_perf["full"]["max_dd"]),
            ">=",
            -0.35,
        ),
        Criterion(
            "min_equity",
            "minimum mark-to-market equity > 0",
            float(primary["min_equity"]),
            ">",
            0.0,
        ),
        Criterion(
            "intraday_leverage",
            "max intraday gross leverage <= 4",
            float(primary["max_intraday_leverage"]),
            "<=",
            4.0,
        ),
        Criterion(
            "overnight_leverage",
            "max overnight gross leverage <= 2",
            float(primary["max_overnight_leverage"]),
            "<=",
            2.0,
        ),
    ]
    # performance() inside assemble recomputes the primary block. Pass held.
    doc = assemble(
        label="SSR next-session long, hold 1",
        benchmark_name="SPY open-to-close",
        model="compound",
        sessions=sessions,
        strategy_net=primary["net"],
        benchmark=benchmark,
        trades=primary["trades"],
        is_end=IS_END,
        oos_start=OOS_START,
        seeds={"direction": SEED_DIRECTION, "timing": SEED_TIMING, "bootstrap": SEED_BOOTSTRAP},
        costs=costs,
        delay={
            "full_sharpe": delay_perf["full"]["sharpe"],
            "oos_sharpe": delay_perf["oos"]["sharpe"],
            "full_return": delay_perf["full"]["total_return"],
            "oos_return": delay_perf["oos"]["total_return"],
        },
        grid=grid,
        placebo_direction=placebo,
        placebo_timing=timing,
        bootstrap=bootstrap,
        by_year=years,
        by_move_quintile=quintiles,
        cross_market=cross,
        extra=extra,
        held=primary["held"],
        universe_type="static_snapshot",
        study_dir=HERE,
        study={
            "dropped_spy_sessions": dropped_spy,
            "episodes": len(episodes),
            "cohorts": len(durations),
            "excluded_symbols": sorted(EXCLUDE),
            "min_equity": primary["min_equity"],
            "max_intraday_leverage": primary["max_intraday_leverage"],
            "max_overnight_leverage": primary["max_overnight_leverage"],
            "timing_error": timing_error,
            "timing_actual_sharpe": None if timing is None else timing.get("actual_sharpe"),
            "timing_p": None if timing is None else timing.get("p"),
            "deflated_sharpe_ratio": dsr,
            "deflated_sharpe_trials": len(HOLDS),
            "concentration": {
                "residual_sharpe": stress.get("residual_sharpe"),
                "max_session_influence": stress.get("max_session_influence"),
                "worst_session": stress.get("worst_session"),
                "outlier_concentration_ratio": stress.get("outlier_concentration_ratio"),
                "k_dropped": stress.get("k_dropped"),
                "passed": stress.get("passed"),
            },
            "upper_bound": {
                "label": "signal close to the planned exit close; not a legal fill",
                "full_sharpe": upper_perf["full"]["sharpe"],
                "oos_sharpe": upper_perf["oos"]["sharpe"],
                "full_return": upper_perf["full"]["total_return"],
                "oos_return": upper_perf["oos"]["total_return"],
            },
            "predictions": {
                "event_vs_nonevent": _prediction_event(sessions, bars, primary["trades"]),
                "prior_decline_quintile": _prediction_prior(primary["trades"]),
                "hold_decay": {"by_hold": hold_means, "consistent": decreasing},
                "sources_same_sign": {
                    "nasdaq_full_return": nasdaq_return,
                    "nyse_full_return": nyse_return,
                    "consistent": same_sign,
                },
            },
        },
    )
    # Grid rows carry oos_trades for the report. assemble keeps the dict as given.
    write_daily(HERE / "daily.csv", sessions, primary["net"], benchmark, primary["held"])
    write_trades(HERE / "trades.csv", primary["trades"])
    write_results(HERE / "results.json", doc)
    append_runlog(
        HERE,
        doc,
        reason=(
            "code did not do what RULES.md says: a missing-bar exit that found "
            "a second session with no bar left sell_index unchanged, so the book "
            "could stay open and later sessions earned 0. The open-sell loop now "
            "waits for the next session that has a bar, or flattens at the last "
            "marked close if the sample ends. The primary hold exits the same "
            "session and was not on that path. Before this rerun the primary "
            "Sharpe full / IS / OOS was -0.915 / -0.781 / -1.360 and total return "
            "was -1.000 / -0.995 / -0.941, with 4131 / 1362 trades."
        ),
    )
    print(f"status: {doc['status']}")
    print(
        "OOS Sharpe / return / trades: "
        f"{doc['primary']['oos']['sharpe']} / {doc['primary']['oos']['total_return']} / {doc['primary']['oos']['trades']}"
    )


def main() -> None:
    self_test()
    if "--self-test" in sys.argv:
        print("self-test ok")
        return
    run_store()


if __name__ == "__main__":
    main()
