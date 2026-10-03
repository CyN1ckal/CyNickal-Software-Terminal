# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered 150/250 moving-average bounce on the end-of-sample QQQ holdings.

Run from the repo root:

    python research/qqq-holdings-ma-bounce/research/backtest.py
"""

from __future__ import annotations

import sys
from datetime import date, timedelta
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from research.kit import (  # noqa: E402
    append_runlog,
    assemble,
    assert_lock,
    block_bootstrap,
    direction_placebo,
    write_daily,
    write_results,
    write_trades,
)
from research.kit.metrics import by_year, move_quintiles, performance, profit_factor, sharpe  # noqa: E402

FIRST = date(2021, 10, 4)
LAST = date(2026, 10, 2)
EVAL_START = date(2022, 10, 3)
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
SMA_FAST = 150
SMA_SLOW = 250
HOLD = 20
COST = 0.0005
CROSS_COST = 0.0001
SEED = 20261003
N_DIRECTION = 2000
N_TIMING = 500
N_BOOT = 2000
BOOT_BLOCK = 20
HOLDS = (10, 15, 20, 30, 40)
COST_MULTIPLES = (0.0, 0.5, 1.0, 2.0, 3.0)

NAMES = (
    "NVDA AAPL MSFT MU AMD AMZN META GOOGL TSLA SPCX GOOG INTC AVGO WMT CSCO LRCX "
    "PLTR AMAT COST PANW NFLX CRWD KLAC TXN SNDK MRVL LIN AMGN ADI QCOM STX SHOP GILD "
    "ASML TMUS PEP WDC ISRG ARM FTNT VRTX BKNG SBUX ADP LITE CDNS ADBE SNPS MAR DDOG "
    "CEG CSX MELI MNST APP WBD DASH CTAS INTU CMCSA MDLZ REGN ROST MPWR TER ORLY ABNB "
    "HON AEP NXPI ALAB MSTR FAST NBIS PCAR BKR FANG PDD HONA PYPL XEL ADSK RKLB MCHP "
    "CCEP EXC KDP CRWV IDXX FER TTWO ODFL TRI WDAY PAYX ROP AXON DXCM ALNY GEHC CPRT"
).split()


def build_sma(has: np.ndarray, close: np.ndarray, window: int) -> np.ndarray:
    """Mean of the last `window` closes that exist, ending at a session that has a bar."""
    sma = np.full(len(close), np.nan)
    prefix = [0.0]
    idxs: list[int] = []
    for i in range(len(close)):
        if not has[i]:
            continue
        prefix.append(prefix[-1] + float(close[i]))
        idxs.append(i)
    for k, i in enumerate(idxs):
        if k + 1 < window:
            continue
        sma[i] = (prefix[k + 1] - prefix[k + 1 - window]) / window
    return sma


def walk_name(
    has: np.ndarray,
    open_: np.ndarray,
    close: np.ndarray,
    low: np.ndarray,
    sma_fast: np.ndarray,
    sma_slow: np.ndarray,
    first_eval: int,
    hold: int,
    fill_lag: int,
    mark: str,
    forced_entries: set[int] | None = None,
) -> list[tuple]:
    """One name. Returns (entry_i, exit_i or None, reason or None, entry_price, exit_price or None)."""
    held = False
    entry_i = -1
    entry_px = 0.0
    pending_entry: int | None = None
    pending_exit: tuple[int, str] | None = None
    out: list[tuple] = []

    def px(i: int) -> float:
        return float(open_[i] if mark == "open" else close[i])

    def signal(i: int) -> bool:
        fast = float(sma_fast[i])
        slow = float(sma_slow[i])
        if not (np.isfinite(fast) and np.isfinite(slow)):
            return False
        c = float(close[i])
        lo = float(low[i])
        lower = fast if fast < slow else slow
        upper = slow if fast < slow else fast
        return c > slow and fast > slow and lower <= lo <= upper and c > fast

    def try_fill(i: int) -> None:
        nonlocal held, entry_i, entry_px, pending_entry, pending_exit
        if not has[i]:
            return
        price = px(i)
        if held:
            if pending_exit is not None and pending_exit[0] <= i:
                out.append((entry_i, i, pending_exit[1], entry_px, price))
                held = False
                pending_exit = None
            elif i >= entry_i + hold:
                out.append((entry_i, i, "time", entry_px, price))
                held = False
                pending_exit = None
        if not held and pending_entry is not None and pending_entry <= i:
            held = True
            entry_i = i
            entry_px = price
            pending_entry = None
            pending_exit = None

    def try_decide(i: int) -> None:
        nonlocal pending_entry, pending_exit
        if not has[i]:
            return
        slow = float(sma_slow[i])
        if held and np.isfinite(slow) and float(close[i]) < slow:
            nxt = i + fill_lag
            if pending_exit is None or nxt < pending_exit[0]:
                pending_exit = (nxt, "trend")
        if (not held) and pending_entry is None and forced_entries is None and signal(i):
            pending_entry = i + fill_lag

    for i in range(len(has)):
        if i < first_eval:
            continue
        if mark == "open":
            try_fill(i)
            try_decide(i)
        else:
            try_decide(i)
            try_fill(i)
        if forced_entries is not None and (not held) and pending_entry is None and i in forced_entries:
            pending_entry = i
            try_fill(i)
    if held:
        out.append((entry_i, None, None, entry_px, None))
    return out


def exit_if_entered(
    has: np.ndarray,
    close: np.ndarray,
    sma_slow: np.ndarray,
    entry_i: int,
    hold: int,
    fill_lag: int,
) -> tuple[int, str] | None:
    """Exit under the locked rules if a long was filled at entry_i. None if it does not finish."""
    if not has[entry_i]:
        return None
    pending: tuple[int, str] | None = None
    n = len(has)
    for i in range(entry_i, n):
        if i > entry_i and has[i]:
            if pending is not None and pending[0] <= i:
                return i, pending[1]
            if i >= entry_i + hold:
                return i, "time"
        if not has[i]:
            continue
        slow = float(sma_slow[i])
        if not np.isfinite(slow) or float(close[i]) >= slow:
            continue
        exited = i > entry_i and (
            (pending is not None and pending[0] <= i) or i >= entry_i + hold
        )
        if exited:
            continue
        nxt = i + fill_lag
        if pending is None or nxt < pending[0]:
            pending = (nxt, "trend")
    return None


def account_book(
    dates: list[date],
    names: list[str],
    mark_px: np.ndarray,
    has: np.ndarray,
    intervals: dict[str, list[tuple]],
    first_eval: int,
    cost: float,
) -> dict:
    """Equal-weight open-to-open (or close-to-close) book. `mark_px` is the fill series."""
    eval_idx = [i for i in range(len(dates)) if i >= first_eval]
    t_count = len(eval_idx)
    n_count = len(names)
    held_after = np.zeros((t_count, n_count), dtype=bool)
    for j, name in enumerate(names):
        for entry_i, exit_i, *_rest in intervals[name]:
            for t, i in enumerate(eval_idx):
                if i >= entry_i and (exit_i is None or i < exit_i):
                    held_after[t, j] = True

    gross = np.zeros(t_count)
    net = np.zeros(t_count)
    exposure = np.zeros(t_count)
    contrib = np.zeros((t_count, n_count))
    name_cost = np.zeros((t_count, n_count))
    last = np.full(n_count, np.nan)
    w_prev = np.zeros(n_count)
    for t, i in enumerate(eval_idx):
        active = int(held_after[t].sum())
        w = np.zeros(n_count) if active == 0 else held_after[t].astype(float) / active
        if t > 0:
            exposure[t] = 1.0 if w_prev.sum() > 0 else 0.0
        for j in range(n_count):
            if not has[j, i]:
                continue
            price = float(mark_px[j, i])
            if w_prev[j] != 0.0 and np.isfinite(last[j]):
                piece = w_prev[j] * (price / float(last[j]) - 1.0)
                contrib[t, j] = piece
                gross[t] += piece
            last[j] = price
        delta = np.abs(w - w_prev)
        name_cost[t] = cost * delta
        net[t] = gross[t] - float(name_cost[t].sum())
        w_prev = w

    trades: list[dict] = []
    pieces: list[list[tuple[date, float]]] = []
    open_positions = 0
    for j, name in enumerate(names):
        for entry_i, exit_i, reason, entry_px, exit_px in intervals[name]:
            if exit_i is None:
                open_positions += 1
                continue
            gross_acc = 0.0
            cost_acc = 0.0
            trade_pieces: list[tuple[date, float]] = []
            for t, i in enumerate(eval_idx):
                if entry_i < i <= exit_i and contrib[t, j] != 0.0:
                    gross_acc += float(contrib[t, j])
                    trade_pieces.append((dates[i], float(contrib[t, j])))
                if entry_i <= i <= exit_i:
                    cost_acc += float(name_cost[t, j])
            entry_day = dates[entry_i]
            exit_day = dates[exit_i]
            trades.append({
                "session": entry_day,
                "side": "long",
                "entry_time": f"{entry_day.isoformat()}T09:30:00",
                "entry_price": float(entry_px),
                "exit_time": f"{exit_day.isoformat()}T09:30:00",
                "exit_price": float(exit_px),
                "gross": gross_acc,
                "net": gross_acc - cost_acc,
                "exit_reason": reason,
                "symbol": name,
                "hold_sessions": int(exit_i - entry_i),
            })
            pieces.append(trade_pieces)
    trades.sort(key=lambda row: (row["session"], row["symbol"], row["exit_time"]))
    return {
        "gross": gross,
        "net": net,
        "held": exposure,
        "trades": trades,
        "pieces": pieces,
        "open_positions": open_positions,
        "eval_idx": eval_idx,
    }


def _run_names(bundle, names, first_eval, hold, cost, fill_lag, mark, forced=None):
    intervals = {}
    mark_px = np.full((len(names), len(bundle["dates"])), np.nan)
    has = np.zeros((len(names), len(bundle["dates"])), dtype=bool)
    for j, name in enumerate(names):
        series = bundle["series"][name]
        has[j] = series["has"]
        mark_px[j] = series["open"] if mark == "open" else series["close"]
        # None uses the signal. A set, including empty, forces those entries and ignores signals.
        entries = None if forced is None else forced.get(name, set())
        intervals[name] = walk_name(
            series["has"], series["open"], series["close"], series["low"],
            series["sma_fast"], series["sma_slow"], first_eval, hold, fill_lag, mark, entries,
        )
    book = account_book(bundle["dates"], names, mark_px, has, intervals, first_eval, cost)
    book["intervals"] = intervals
    return book


def _blank(n: int, fill: float = 100.0):
    return {
        "open": np.full(n, fill),
        "high": np.full(n, fill),
        "low": np.full(n, fill),
        "close": np.full(n, fill),
        "has": np.ones(n, dtype=bool),
    }


def _prepare(raw: dict[str, dict]) -> dict:
    n = len(next(iter(raw.values()))["close"])
    dates = [date(2020, 1, 1) + timedelta(days=i) for i in range(n)]
    series = {}
    for name, cols in raw.items():
        series[name] = {
            "open": np.array(cols["open"], dtype=float),
            "high": np.array(cols["high"], dtype=float),
            "low": np.array(cols["low"], dtype=float),
            "close": np.array(cols["close"], dtype=float),
            "has": np.array(cols["has"], dtype=bool),
        }
        series[name]["sma_fast"] = build_sma(series[name]["has"], series[name]["close"], SMA_FAST)
        series[name]["sma_slow"] = build_sma(series[name]["has"], series[name]["close"], SMA_SLOW)
    return {"dates": dates, "series": series}


def _fail(name: str, detail: str) -> None:
    raise SystemExit(f"self-test {name} failed: {detail}")


def _signal_band() -> tuple[float, float]:
    """Hand SMA at the flat-100 history plus a 110 close. 249 prior closes of 100."""
    slow = (249 * 100 + 110) / 250
    fast = (149 * 100 + 110) / 150
    return fast, slow


def self_test() -> None:
    """Hand-built sessions. Expected prices and reasons are literals. Does not open the store."""
    fast, slow = _signal_band()
    if not (slow <= 100.05 <= fast):
        _fail("band", f"100.05 not inside [{slow}, {fast}]")
    n = 300
    first = 249

    def base_signal(cols: dict, close_after: float) -> None:
        cols["close"][249] = 110.0
        cols["high"][249] = 110.0
        cols["low"][249] = 100.05
        cols["open"][249] = 100.0
        cols["open"][250:] = close_after
        cols["high"][250:] = close_after
        cols["low"][250:] = close_after
        cols["close"][250:] = close_after

    # 1. Entry at the next open, trend exit at the open after that.
    trend = _blank(n)
    base_signal(trend, 200.0)
    trend["open"][250] = 105.0
    trend["high"][250] = 105.0
    trend["low"][250] = 90.0
    trend["close"][250] = 90.0
    trend["open"][251] = 95.0
    book = _run_names(_prepare({"AAA": trend}), ["AAA"], first, HOLD, COST, 1, "open")
    trades = book["trades"]
    if len(trades) != 1:
        _fail("trend", f"expected 1 trade, got {len(trades)}")
    trade = trades[0]
    if trade["entry_price"] != 105.0 or trade["exit_price"] != 95.0 or trade["exit_reason"] != "trend":
        _fail("trend", f"prices/reason {trade['entry_price']} {trade['exit_price']} {trade['exit_reason']}")
    if trade["entry_time"] != "2020-09-07T09:30:00":
        # 2020-01-01 + 250 days. Checked against the literal index, not a calendar library guess.
        entry_day = date(2020, 1, 1) + timedelta(days=250)
        if trade["entry_time"] != f"{entry_day.isoformat()}T09:30:00":
            _fail("trend", f"entry time {trade['entry_time']}")
    gross = 95.0 / 105.0 - 1.0
    if abs(trade["gross"] - gross) > 1e-12 or abs(trade["net"] - (gross - 0.001)) > 1e-12:
        _fail("trend", f"pnl gross {trade['gross']} net {trade['net']}")
    # Entry session is index 250, the second evaluated session (index 249 is eval 0).
    if abs(book["net"][1] - (-COST)) > 1e-12:
        _fail("trend", f"entry-session net {book['net'][1]}")
    if abs(book["net"][2] - (gross - COST)) > 1e-12:
        _fail("trend", f"exit-session net {book['net'][2]}")
    scanned = exit_if_entered(trend["has"], trend["close"], build_sma(trend["has"], trend["close"], SMA_SLOW), 250, HOLD, 1)
    if scanned != (251, "trend"):
        _fail("trend-scan", str(scanned))

    # 2. Time exit. Closes stay at 200, above any blend with the flat history.
    timed = _blank(n)
    base_signal(timed, 200.0)
    timed["open"][250] = 200.0
    book = _run_names(_prepare({"BBB": timed}), ["BBB"], first, HOLD, COST, 1, "open")
    if len(book["trades"]) != 1:
        _fail("time", f"expected 1 trade, got {len(book['trades'])}")
    trade = book["trades"][0]
    if trade["exit_reason"] != "time" or trade["entry_price"] != 200.0 or trade["exit_price"] != 200.0:
        _fail("time", f"{trade['exit_reason']} {trade['entry_price']} {trade['exit_price']}")
    if trade["hold_sessions"] != 20:
        _fail("time", f"hold {trade['hold_sessions']}")
    if abs(trade["gross"]) > 1e-12 or abs(trade["net"] - (-0.001)) > 1e-12:
        _fail("time", f"pnl {trade['gross']} {trade['net']}")
    scanned = exit_if_entered(timed["has"], timed["close"], build_sma(timed["has"], timed["close"], SMA_SLOW), 250, HOLD, 1)
    if scanned != (270, "time"):
        _fail("time-scan", str(scanned))

    # 3. Missing held session contributes 0. Gap prints on the next open. Missing time-stop waits.
    gap = _blank(n)
    base_signal(gap, 200.0)
    gap["open"][250] = 110.0
    gap["open"][259] = 100.0
    gap["has"][260] = False
    gap["open"][261] = 150.0
    gap["has"][270] = False
    gap["open"][271] = 180.0
    book = _run_names(_prepare({"CCC": gap}), ["CCC"], first, HOLD, COST, 1, "open")
    if len(book["trades"]) != 1:
        _fail("gap", f"expected 1 trade, got {len(book['trades'])}")
    trade = book["trades"][0]
    if trade["exit_reason"] != "time" or trade["entry_price"] != 110.0 or trade["exit_price"] != 180.0:
        _fail("gap", f"{trade['exit_reason']} {trade['entry_price']} {trade['exit_price']}")
    if trade["hold_sessions"] != 21:
        _fail("gap", f"hold {trade['hold_sessions']}")
    # eval index = global index - 249.
    if abs(book["gross"][259 - 249]) - 0.5 > 1e-12 and abs(book["gross"][10] + 0.5) > 1e-12:
        _fail("gap", f"session 259 gross {book['gross'][10]}")
    if abs(book["gross"][10] + 0.5) > 1e-12:
        _fail("gap", f"session 259 gross {book['gross'][10]}")
    if book["gross"][11] != 0.0:
        _fail("gap", f"missing session gross {book['gross'][11]}")
    if abs(book["gross"][12] - 0.5) > 1e-12:
        _fail("gap", f"gap session gross {book['gross'][12]}")

    # 4. Trend fill session missing: wait for the next open.
    wait = _blank(n)
    base_signal(wait, 200.0)
    wait["open"][250] = 105.0
    wait["low"][250] = 90.0
    wait["close"][250] = 90.0
    wait["has"][251] = False
    wait["open"][252] = 88.0
    book = _run_names(_prepare({"WAIT": wait}), ["WAIT"], first, HOLD, COST, 1, "open")
    if len(book["trades"]) != 1 or book["trades"][0]["exit_price"] != 88.0 or book["trades"][0]["exit_reason"] != "trend":
        _fail("wait", str(book["trades"]))
    if book["trades"][0]["hold_sessions"] != 2:
        _fail("wait", f"hold {book['trades'][0]['hold_sessions']}")

    # 5. A valid second signal while long is ignored. Hand SMA at index 255 on the 200-path:
    # 243 closes of 100, one 110, six 200s. Band [102.44, 15610/150]. Low 103 is inside.
    ddd = _blank(n)
    base_signal(ddd, 200.0)
    ddd["low"][255] = 103.0
    ddd["close"][255] = 200.0
    hand_slow = (243 * 100 + 110 + 6 * 200) / 250
    hand_fast = (143 * 100 + 110 + 6 * 200) / 150
    if not (hand_slow <= 103.0 <= hand_fast and 200.0 > hand_fast > hand_slow):
        _fail("ignore-band", f"{hand_slow} {hand_fast}")
    book = _run_names(_prepare({"DDD": ddd}), ["DDD"], first, HOLD, COST, 1, "open")
    if len(book["trades"]) != 1 or book["trades"][0]["exit_reason"] != "time":
        _fail("ignore", str([(t["exit_reason"], t["hold_sessions"]) for t in book["trades"]]))

    # 6. Fewer than 250 closes: no trade. The last bar is signal-shaped.
    short = _blank(n)
    short["has"][:150] = False
    short["close"][249] = 110.0
    short["low"][249] = 100.05
    short["high"][249] = 110.0
    book = _run_names(_prepare({"EEE": short}), ["EEE"], first, HOLD, COST, 1, "open")
    if book["trades"] or book["open_positions"]:
        _fail("short", f"trades {len(book['trades'])} open {book['open_positions']}")

    # 7. Low outside the band: no trade.
    outside = _blank(n)
    base_signal(outside, 200.0)
    outside["low"][249] = 90.0
    book = _run_names(_prepare({"NOPE": outside}), ["NOPE"], first, HOLD, COST, 1, "open")
    if book["trades"] or book["open_positions"]:
        _fail("outside", "entered")

    # 8. Trend and time due at the same open: reason is trend.
    both = _blank(n)
    base_signal(both, 200.0)
    both["open"][250] = 130.0
    both["close"][269] = 50.0
    both["low"][269] = 50.0
    both["open"][270] = 80.0
    book = _run_names(_prepare({"PRI": both}), ["PRI"], first, HOLD, COST, 1, "open")
    if len(book["trades"]) != 1 or book["trades"][0]["exit_reason"] != "trend" or book["trades"][0]["exit_price"] != 80.0:
        _fail("priority", str(book["trades"]))
    if book["trades"][0]["entry_price"] != 130.0 or book["trades"][0]["hold_sessions"] != 20:
        _fail("priority", f"entry {book['trades'][0]['entry_price']} hold {book['trades'][0]['hold_sessions']}")

    # 9. Two names, equal weight, one open-to-open step.
    left = _blank(n)
    right = _blank(n)
    for cols in (left, right):
        base_signal(cols, 200.0)
        cols["open"][250] = 105.0
        cols["low"][250] = 90.0
        cols["close"][250] = 90.0
        cols["open"][251] = 95.0
    book = _run_names(_prepare({"FFF": left, "GGG": right}), ["FFF", "GGG"], first, HOLD, COST, 1, "open")
    if len(book["trades"]) != 2:
        _fail("pair", f"{len(book['trades'])} trades")
    half = 0.5 * (95.0 / 105.0 - 1.0)
    for trade in book["trades"]:
        if abs(trade["gross"] - half) > 1e-12 or abs(trade["net"] - (half - COST)) > 1e-12:
            _fail("pair", f"{trade['symbol']} {trade['gross']} {trade['net']}")
    if abs(book["net"][1] - (-COST)) > 1e-12 or abs(book["net"][2] - ((95.0 / 105.0 - 1.0) - COST)) > 1e-12:
        _fail("pair", f"daily {book['net'][1]} {book['net'][2]}")

    # 10. Same-bar close fill is a separate path: enter and exit at the closes.
    book = _run_names(_prepare({"AAA": trend}), ["AAA"], first, HOLD, COST, 0, "close")
    if len(book["trades"]) != 1:
        _fail("upper", f"{len(book['trades'])} trades")
    trade = book["trades"][0]
    if trade["entry_price"] != 110.0 or trade["exit_price"] != 90.0 or trade["exit_reason"] != "trend":
        _fail("upper", f"{trade['entry_price']} {trade['exit_price']} {trade['exit_reason']}")


def _load():
    sys.path.insert(0, str(ROOT / "agent-data"))
    from mdq import MarketData, nyse_sessions

    if len(NAMES) != 101 or len(set(NAMES)) != 101:
        raise SystemExit("universe is not 101 unique names")
    dates = nyse_sessions(FIRST, LAST)
    eval_idx = [i for i, day in enumerate(dates) if day >= EVAL_START]
    if dates[eval_idx[0]] != EVAL_START or dates[eval_idx[-1]] != LAST or len(eval_idx) != 1004:
        raise SystemExit("evaluated calendar does not match the locked 1,004 sessions")
    if IS_END not in dates or OOS_START not in dates:
        raise SystemExit("IS end or OOS start is not an NYSE session")
    between = [day for day in dates if IS_END < day < OOS_START]
    if between:
        raise SystemExit(f"sessions between IS and OOS: {between}")
    symbols = NAMES + ["QQQ", "SPY"]
    raw: dict[str, dict] = {}
    with MarketData() as md:
        for symbol in symbols:
            bars = md.bars(symbol, "1d", start=FIRST, end=LAST, adjust=True)
            seen: dict[date, tuple[float, float, float, float]] = {}
            for bar in bars:
                if bar.open <= 0 or bar.high <= 0 or bar.low <= 0 or bar.close <= 0:
                    raise SystemExit(f"{symbol} {bar.session} has a non-positive price")
                if bar.session in seen:
                    raise SystemExit(f"{symbol} has a duplicate bar on {bar.session}")
                seen[bar.session] = (float(bar.open), float(bar.high), float(bar.low), float(bar.close))
            n = len(dates)
            cols = {
                "open": np.full(n, np.nan),
                "high": np.full(n, np.nan),
                "low": np.full(n, np.nan),
                "close": np.full(n, np.nan),
                "has": np.zeros(n, dtype=bool),
            }
            for i, day in enumerate(dates):
                row = seen.get(day)
                if row is None:
                    continue
                cols["open"][i], cols["high"][i], cols["low"][i], cols["close"][i] = row
                cols["has"][i] = True
            cols["sma_fast"] = build_sma(cols["has"], cols["close"], SMA_FAST)
            cols["sma_slow"] = build_sma(cols["has"], cols["close"], SMA_SLOW)
            raw[symbol] = cols
    for symbol in ("QQQ", "SPY"):
        missing = [dates[i] for i in eval_idx if not raw[symbol]["has"][i]]
        if missing != [LAST]:
            raise SystemExit(f"{symbol} missing evaluated sessions {missing}, expected only {LAST}")
    return {"dates": dates, "series": raw, "first_eval": eval_idx[0]}


def _benchmark(bundle) -> np.ndarray:
    dates = bundle["dates"]
    qqq = bundle["series"]["QQQ"]
    first = bundle["first_eval"]
    out = []
    for i in range(first, len(dates)):
        if i == 0 or not qqq["has"][i] or not qqq["has"][i - 1]:
            out.append(0.0)
        else:
            out.append(float(qqq["close"][i]) / float(qqq["close"][i - 1]) - 1.0)
    return np.array(out, dtype=float)


def _window_mean(trades: list[dict], reason: str, start: date | None, end: date | None, field: str) -> tuple[int, float | None]:
    chosen = []
    for trade in trades:
        if trade["exit_reason"] != reason:
            continue
        day = trade["session"]
        if start is not None and day < start:
            continue
        if end is not None and day > end:
            continue
        chosen.append(float(trade[field]))
    if not chosen:
        return 0, None
    return len(chosen), float(sum(chosen) / len(chosen))


def _side_stats(trades: list[dict]) -> dict:
    nets = [float(trade["net"]) for trade in trades]
    wins = [value for value in nets if value > 0]
    losses = [value for value in nets if value < 0]
    return {
        "trades": len(nets),
        "win_rate": None if not nets else float(sum(value > 0 for value in nets) / len(nets)),
        "avg_winner_bp": None if not wins else float(sum(wins) / len(wins) * 1e4),
        "avg_loser_bp": None if not losses else float(sum(losses) / len(losses) * 1e4),
        "profit_factor": profit_factor(nets),
    }


def _score_predictions(trades, quintiles, primary_oos_return, ex_nvda_oos_return) -> list[dict]:
    n_time, mean_time = _window_mean(trades, "time", None, None, "gross")
    n_trend, mean_trend = _window_mean(trades, "trend", None, None, "gross")
    if n_time == 0 or n_trend == 0 or mean_time is None or mean_trend is None:
        score_1 = "not testable"
    elif mean_time < mean_trend:
        score_1 = "consistent"
    else:
        score_1 = "not consistent"
    by_q = {int(row["quintile"]): row["mean_strategy_net"] for row in quintiles}
    upper = [by_q.get(4), by_q.get(5)]
    lower = [by_q.get(1), by_q.get(2)]
    if any(value is None for value in upper + lower):
        score_2 = "not testable"
        upper_mean = None
        lower_mean = None
    else:
        upper_mean = float(sum(upper) / 2)
        lower_mean = float(sum(lower) / 2)
        score_2 = "consistent" if upper_mean > lower_mean else "not consistent"
    if primary_oos_return is None or not (primary_oos_return > 0):
        score_3 = "not testable"
    elif ex_nvda_oos_return is not None and ex_nvda_oos_return > 0:
        score_3 = "consistent"
    else:
        score_3 = "not consistent"
    return [
        {
            "id": 1,
            "score": score_1,
            "time_trades": n_time,
            "time_mean_gross": mean_time,
            "trend_trades": n_trend,
            "trend_mean_gross": mean_trend,
        },
        {
            "id": 2,
            "score": score_2,
            "upper_mean": upper_mean,
            "lower_mean": lower_mean,
        },
        {
            "id": 3,
            "score": score_3,
            "primary_oos_total_return": primary_oos_return,
            "ex_nvda_oos_total_return": ex_nvda_oos_return,
        },
    ]


def _candidates(series, first_eval: int, hold: int) -> tuple[np.ndarray, np.ndarray]:
    entries: list[int] = []
    exits: list[int] = []
    n = len(series["has"])
    for i in range(first_eval, n):
        if not series["has"][i]:
            continue
        found = exit_if_entered(series["has"], series["close"], series["sma_slow"], i, hold, 1)
        if found is not None:
            entries.append(i)
            exits.append(found[0])
    return np.array(entries, dtype=int), np.array(exits, dtype=int)


def _return_matrix(bundle, names: list[str], first_eval: int) -> np.ndarray:
    """Open-to-open simple return on each evaluated session, 0 when the session has no bar."""
    n_dates = len(bundle["dates"])
    width = n_dates - first_eval
    out = np.zeros((len(names), width))
    for j, name in enumerate(names):
        has = bundle["series"][name]["has"]
        opens = bundle["series"][name]["open"]
        last = np.nan
        for i in range(n_dates):
            if not has[i]:
                continue
            if i >= first_eval and np.isfinite(last):
                out[j, i - first_eval] = float(opens[i]) / float(last) - 1.0
            last = float(opens[i])
    return out


def _gross_from_spans(spans: list[list[tuple[int, int]]], rets: np.ndarray, first_eval: int) -> np.ndarray:
    """Zero-cost equal-weight gross path. Span end is exclusive, in full-calendar index."""
    width = rets.shape[1]
    held = np.zeros((width, len(spans)), dtype=bool)
    for j, intervals in enumerate(spans):
        for entry_i, exit_i in intervals:
            start = entry_i - first_eval
            stop = exit_i - first_eval
            if start < 0:
                start = 0
            if stop > width:
                stop = width
            if start < stop:
                held[start:stop, j] = True
    counts = held.sum(axis=1).astype(float)
    weights = np.zeros_like(held, dtype=float)
    active = counts > 0
    weights[active] = held[active] / counts[active, None]
    gross = np.zeros(width)
    if width > 1:
        gross[1:] = np.sum(weights[:-1] * rets[:, 1:].T, axis=1)
    return gross


def _pack_once(
    entries: np.ndarray,
    exits: np.ndarray,
    needed: int,
    rng: np.random.Generator,
    horizon: int,
) -> list[tuple[int, int]] | None:
    """One shuffled candidate list, packed so [entry, exit) intervals do not overlap."""
    if needed == 0:
        return []
    if len(entries) < needed:
        return None
    order = rng.permutation(len(entries))
    busy = np.zeros(horizon, dtype=bool)
    chosen: list[tuple[int, int]] = []
    for idx in order:
        entry = int(entries[idx])
        exit_i = int(exits[idx])
        if busy[entry:exit_i].any():
            continue
        busy[entry:exit_i] = True
        chosen.append((entry, exit_i))
        if len(chosen) == needed:
            return chosen
    return None


def _timing(bundle, names, first_eval, primary_pieces, intervals, primary_gross) -> dict:
    per_name: dict[str, int] = {}
    check_spans: list[list[tuple[int, int]]] = []
    horizon = len(bundle["dates"])
    for name in names:
        spans: list[tuple[int, int]] = []
        for entry, exit_i, reason, *_rest in intervals[name]:
            if exit_i is None:
                spans.append((entry, horizon))
                continue
            found = exit_if_entered(
                bundle["series"][name]["has"], bundle["series"][name]["close"],
                bundle["series"][name]["sma_slow"], entry, HOLD, 1,
            )
            if found != (exit_i, reason):
                raise SystemExit(f"exit scanner mismatch on {name} {entry}: {found} vs {(exit_i, reason)}")
            spans.append((entry, exit_i))
        check_spans.append(spans)
        done = sum(1 for entry, exit_i, *_rest in intervals[name] if exit_i is not None)
        if done:
            per_name[name] = done
    rets = _return_matrix(bundle, names, first_eval)
    check = _gross_from_spans(check_spans, rets, first_eval)
    if len(check) != len(primary_gross) or float(np.max(np.abs(check - primary_gross))) > 1e-8:
        raise SystemExit("fast gross path does not match the account book")
    pools = {name: _candidates(bundle["series"][name], first_eval, HOLD) for name in per_name}
    actual = sharpe(_piece_path(bundle, primary_pieces))
    if actual is None:
        raise SystemExit("timing placebo actual gross Sharpe is undefined")
    rng = np.random.default_rng(SEED)
    samples = []
    attempts = 0
    width = rets.shape[1]
    while len(samples) < N_TIMING and attempts < 20000:
        attempts += 1
        if attempts % 1000 == 0:
            print(f"timing attempts {attempts} kept {len(samples)}", flush=True)
        spans: list[list[tuple[int, int]]] = []
        ok = True
        for name in names:
            needed = per_name.get(name, 0)
            if needed == 0:
                spans.append([])
                continue
            entries, exits = pools[name]
            packed = _pack_once(entries, exits, needed, rng, horizon)
            if packed is None:
                ok = False
                break
            spans.append(packed)
        if not ok:
            continue
        # Names that failed to pack left `spans` short. Rebuild only on success,
        # aligned to every name: the loop above appends in `names` order and breaks
        # without a partial tail, so a success has one list per name.
        if len(spans) != len(names):
            continue
        value = sharpe(_gross_from_spans(spans, rets, first_eval))
        samples.append(np.nan if value is None else value)
    if len(samples) < N_TIMING:
        raise SystemExit(f"timing placebo produced {len(samples)} draws in {attempts} attempts")
    arr = np.array(samples, dtype=float)
    finite = arr[np.isfinite(arr)]
    count = int(np.sum(arr >= actual))
    return {
        "actual_gross_sharpe": float(actual),
        "null_mean": float(finite.mean()),
        "null_p95": float(np.percentile(finite, 95)),
        "p": (1 + count) / (len(arr) + 1),
        "draws": N_TIMING,
        "seed": SEED,
        "samples": arr,
        "attempts": attempts,
        "width": width,
    }


def _piece_path(bundle, pieces) -> np.ndarray:
    dates = [bundle["dates"][i] for i in range(bundle["first_eval"], len(bundle["dates"]))]
    index = {day: i for i, day in enumerate(dates)}
    out = np.zeros(len(dates))
    for trade in pieces:
        for day, value in trade:
            out[index[day]] += value
    return out


def _cost_row(bundle, names, intervals, multiple: float, bench_sessions) -> dict:
    mark = np.vstack([bundle["series"][name]["open"] for name in names])
    has = np.vstack([bundle["series"][name]["has"] for name in names])
    book = account_book(bundle["dates"], names, mark, has, intervals, bundle["first_eval"], COST * multiple)
    perf = performance(
        bench_sessions, book["net"], book["trades"],
        is_end=IS_END, oos_start=OOS_START, model="compound",
    )
    return {
        "multiple": multiple,
        "full_sharpe": perf["full"]["sharpe"],
        "oos_sharpe": perf["oos"]["sharpe"],
        "full_return": perf["full"]["total_return"],
        "book": book,
        "perf": perf,
    }


def _summarize_hold(bundle, names, sessions, hold: int) -> dict:
    book = _run_names(bundle, names, bundle["first_eval"], hold, COST, 1, "open")
    perf = performance(sessions, book["net"], book["trades"], is_end=IS_END, oos_start=OOS_START, model="compound")
    return {
        "params": {"hold": hold},
        "is_sharpe": perf["is"]["sharpe"],
        "oos_sharpe": perf["oos"]["sharpe"],
        "full_return": perf["full"]["total_return"],
        "primary": hold == HOLD,
        "book": book,
        "perf": perf,
    }


def run(digest: str) -> None:
    bundle = _load()
    names = list(NAMES)
    sessions = [bundle["dates"][i] for i in range(bundle["first_eval"], len(bundle["dates"]))]
    bench = _benchmark(bundle)
    if len(bench) != len(sessions):
        raise SystemExit("benchmark length mismatch")

    primary_walk = _run_names(bundle, names, bundle["first_eval"], HOLD, COST, 1, "open")
    # Cost multiples reprice the same intervals. The 1x book must match the walk.
    costs = []
    primary = None
    for multiple in COST_MULTIPLES:
        row = _cost_row(bundle, names, primary_walk["intervals"], multiple, sessions)
        if multiple == 1.0:
            primary = row
        costs.append({key: row[key] for key in ("multiple", "full_sharpe", "oos_sharpe", "full_return")})
    if primary is None:
        raise SystemExit("missing 1x cost row")
    book = primary["book"]
    if len(book["trades"]) != len(primary_walk["trades"]):
        raise SystemExit("1x reprice trade count does not match the walk")

    grid = []
    for hold in HOLDS:
        if hold == HOLD:
            perf = primary["perf"]
            grid.append({
                "params": {"hold": hold},
                "is_sharpe": perf["is"]["sharpe"],
                "oos_sharpe": perf["oos"]["sharpe"],
                "full_return": perf["full"]["total_return"],
                "primary": True,
            })
        else:
            row = _summarize_hold(bundle, names, sessions, hold)
            grid.append({key: row[key] for key in ("params", "is_sharpe", "oos_sharpe", "full_return", "primary")})

    delay_book = _run_names(bundle, names, bundle["first_eval"], HOLD, COST, 2, "open")
    delay_perf = performance(
        sessions, delay_book["net"], delay_book["trades"],
        is_end=IS_END, oos_start=OOS_START, model="compound",
    )
    upper_book = _run_names(bundle, names, bundle["first_eval"], HOLD, COST, 0, "close")
    upper_perf = performance(
        sessions, upper_book["net"], upper_book["trades"],
        is_end=IS_END, oos_start=OOS_START, model="compound",
    )
    cross = []
    for symbol in ("QQQ", "SPY"):
        cross_book = _run_names(bundle, [symbol], bundle["first_eval"], HOLD, CROSS_COST, 1, "open")
        cross_perf = performance(
            sessions, cross_book["net"], cross_book["trades"],
            is_end=IS_END, oos_start=OOS_START, model="compound",
        )
        cross.append({
            "symbol": symbol,
            "is_sharpe": cross_perf["is"]["sharpe"],
            "oos_sharpe": cross_perf["oos"]["sharpe"],
            "full_return": cross_perf["full"]["total_return"],
            "full_profit_factor": cross_perf["full"]["profit_factor"],
        })
    ex_names = [name for name in names if name != "NVDA"]
    ex_book = _run_names(bundle, ex_names, bundle["first_eval"], HOLD, COST, 1, "open")
    ex_perf = performance(
        sessions, ex_book["net"], ex_book["trades"],
        is_end=IS_END, oos_start=OOS_START, model="compound",
    )

    pieces = book["pieces"]
    direction = direction_placebo(sessions, pieces, seed=SEED, draws=N_DIRECTION)
    timing = _timing(
        bundle, names, bundle["first_eval"], pieces, primary_walk["intervals"], book["gross"],
    )
    bootstrap = block_bootstrap(book["net"], seed=SEED, block=BOOT_BLOCK, draws=N_BOOT)
    years = by_year(sessions, book["net"], bench, model="compound")
    quintiles = move_quintiles(sessions, book["net"], bench)

    holds = [int(trade["hold_sessions"]) for trade in book["trades"]]
    by_exit = []
    for sample, start, end in (
        ("full", None, None),
        ("is", None, IS_END),
        ("oos", OOS_START, None),
    ):
        chosen = []
        for trade in book["trades"]:
            day = trade["session"]
            if start is not None and day < start:
                continue
            if end is not None and day > end:
                continue
            chosen.append(trade)
        for reason in ("trend", "time"):
            group = [trade for trade in chosen if trade["exit_reason"] == reason]
            nets = [float(trade["net"]) for trade in group]
            grosses = [float(trade["gross"]) for trade in group]
            by_exit.append({
                "sample": sample,
                "reason": reason,
                "trades": len(group),
                "mean_gross": None if not grosses else float(sum(grosses) / len(grosses)),
                "mean_net": None if not nets else float(sum(nets) / len(nets)),
                "profit_factor": profit_factor(nets),
            })
    predictions = _score_predictions(
        book["trades"], quintiles, primary["perf"]["oos"]["total_return"], ex_perf["oos"]["total_return"],
    )
    is_trades = [trade for trade in book["trades"] if trade["session"] <= IS_END]
    oos_trades = [trade for trade in book["trades"] if trade["session"] >= OOS_START]
    study = {
        "open_positions": book["open_positions"],
        "sessions_held": int(np.sum(book["held"] > 0)),
        "hold_mean": None if not holds else float(np.mean(holds)),
        "hold_median": None if not holds else float(np.median(holds)),
        "gross_sharpe_path": sharpe(book["gross"]),
        "gross_sharpe_completed": sharpe(_piece_path(bundle, pieces)),
        "by_exit": by_exit,
        "full_side": _side_stats(book["trades"]),
        "is_side": _side_stats(is_trades),
        "oos_side": _side_stats(oos_trades),
        "predictions": predictions,
        "ex_nvda": {
            "oos_sharpe": ex_perf["oos"]["sharpe"],
            "oos_total_return": ex_perf["oos"]["total_return"],
            "oos_trades": ex_perf["oos"]["trades"],
            "full_sharpe": ex_perf["full"]["sharpe"],
            "full_total_return": ex_perf["full"]["total_return"],
        },
        "same_bar_close_upper_bound": {
            "label": "upper bound, not an acceptance input",
            "full_sharpe": upper_perf["full"]["sharpe"],
            "oos_sharpe": upper_perf["oos"]["sharpe"],
            "full_return": upper_perf["full"]["total_return"],
            "oos_return": upper_perf["oos"]["total_return"],
        },
        "timing_attempts": timing["attempts"],
        "cost_bp_per_side": 5.0,
        "cross_cost_bp_per_side": 1.0,
    }
    doc = assemble(
        label="MA bounce, 5 bp",
        benchmark_name="QQQ close-to-close",
        model="compound",
        sessions=sessions,
        strategy_net=book["net"],
        benchmark=bench,
        trades=book["trades"],
        is_end=IS_END,
        oos_start=OOS_START,
        seeds={"direction": SEED, "timing": SEED, "bootstrap": SEED},
        costs=costs,
        delay={
            "full_sharpe": delay_perf["full"]["sharpe"],
            "oos_sharpe": delay_perf["oos"]["sharpe"],
            "full_return": delay_perf["full"]["total_return"],
            "oos_return": delay_perf["oos"]["total_return"],
        },
        grid=grid,
        placebo_direction=direction,
        placebo_timing=timing,
        bootstrap=bootstrap,
        by_year=years,
        by_move_quintile=quintiles,
        cross_market=cross,
        study=study,
        held=book["held"],
        study_dir=HERE,
        rules_sha256=digest,
    )
    if doc["rules_sha256"] != digest:
        raise SystemExit("results hash does not match the lock")
    write_results(HERE / "results.json", doc)
    write_daily(HERE / "daily.csv", sessions, book["net"], bench, book["held"])
    write_trades(HERE / "trades.csv", book["trades"])
    append_runlog(HERE, doc, reason="initial pre-registered store run")
    oos = doc["primary"]["oos"]
    print(
        f"status {doc['status']} OOS sharpe {oos['sharpe']} "
        f"return {oos['total_return']} trades {oos['trades']}"
    )


def main() -> None:
    self_test()
    digest = assert_lock(HERE)
    run(digest)


if __name__ == "__main__":
    main()
