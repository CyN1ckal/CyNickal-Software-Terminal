# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Cross-sectional 12-month momentum on six commodity ETFs.

Refuses to run unless RULES.md hashes to RULES.lock. The synthetic self-test
runs before the store is opened. A store run appends to RUNLOG.md.
"""

import bisect
import datetime as dt
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))

# Constants mirror RULES.md.
NAMES = ("GLD", "SLV", "USO", "UNG", "DBA", "DBB")
COST_BP = {"GLD": 1.0, "SLV": 1.0, "USO": 5.0, "UNG": 5.0, "DBA": 5.0, "DBB": 5.0}
K_PRIMARY = 252
BOOK_K = 2
GRID_K = (63, 126, 189, 252, 315)
COST_SWEEP = (0.0, 0.5, 1.0, 2.0, 3.0)
SKIP = {dt.date(2012, 10, 29), dt.date(2012, 10, 30), dt.date(2018, 12, 5)}
FIRST = dt.date(2011, 1, 4)
LAST = dt.date(2026, 10, 1)
IS_END = dt.date(2024, 6, 28)
OOS_START = dt.date(2024, 7, 1)
EXPECTED_BARS = 3959
EXPECTED_SPLITS = {
    "USO": ((dt.date(2020, 4, 29), 0.125),),
    "UNG": ((dt.date(2018, 1, 5), 0.25), (dt.date(2024, 1, 24), 0.25)),
}
SEED_DIRECTION = 20261021
SEED_BOOTSTRAP = 20261022
SEED_TIMING = 20261023
SEED_VERIFY = 20261024
N_DIRECTION = 2000
N_BOOTSTRAP = 2000
N_TIMING = 500
BLOCK = 20
ANNUAL = 252


def rules_hash() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(raw).hexdigest()


def require_lock() -> str:
    have = rules_hash()
    lock_line = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()[0]
    want = lock_line.split()[1]
    if have != want:
        sys.exit(f"RULES.md hash {have} != RULES.lock {want}; refusing to run")
    return have


def sign(x: float) -> int:
    if x > 0.0:
        return 1
    if x < 0.0:
        return -1
    return 0


def month_ends(sessions: list[dt.date]) -> list[dt.date]:
    last: dict[tuple[int, int], dt.date] = {}
    for day in sessions:
        last[(day.year, day.month)] = day
    return [last[k] for k in sorted(last)]


def book_sessions_from(sessions: list[dt.date]) -> list[dt.date]:
    return [d for d in sessions if d not in SKIP]


def formation_at(dates: list[dt.date], closes: list[float], day: dt.date, lookback: int):
    i = bisect.bisect_right(dates, day) - 1
    if i < lookback:
        return None
    return closes[i] / closes[i - lookback] - 1.0


def weights_from_formation(form: dict[str, float], names: tuple[str, ...], book_k: int):
    if any(form.get(s) is None for s in names):
        return None
    order = sorted(names, key=lambda s: (-form[s], s))
    weights = {s: 0.0 for s in names}
    n = len(order)
    unit = 1.0 / book_k
    for i in range(book_k):
        weights[order[i]] = unit
        weights[order[n - 1 - i]] = -unit
    return weights


class Trade:
    def __init__(self, symbol, side, entry_date, entry_price, entry_shares):
        self.symbol = symbol
        self.side = side
        self.entry_date = entry_date
        self.entry_price = entry_price
        self.entry_shares = entry_shares
        self.exit_date = None
        self.exit_price = None
        self.exit_reason = None
        self.exit_shares = entry_shares
        self.gross = 0.0
        self.cost = 0.0
        self.daily: dict[dt.date, float] = {}

    def as_dict(self):
        den = abs(self.entry_shares) * self.entry_price
        net = self.gross - self.cost
        return {
            "symbol": self.symbol,
            "side": self.side,
            "entry_date": self.entry_date,
            "entry_price": self.entry_price,
            "exit_date": self.exit_date,
            "exit_price": self.exit_price,
            "exit_reason": self.exit_reason,
            "entry_shares": self.entry_shares,
            "exit_shares": self.exit_shares,
            "gross_pnl": self.gross,
            "cost": self.cost,
            "net_pnl": net,
            "return_bp": net / den * 10000.0 if den else None,
            "daily": self.daily,
        }


def run_book(sessions, bars, names, signal_dates, lookback, cost_bp, cost_mult,
             book_k, fill_mode="next_open", weight_fn=None):
    """Simulate one book. `bars[symbol][date] = (open, close)`.

    Evaluation rows start on the first rebalance fill. The signal on the last
    session is computed by the caller's weight function only when applied;
    this function discards a signal dated on the final session.
    """
    if list(sessions) != sorted(sessions):
        raise RuntimeError("sessions are not sorted")
    names = tuple(names)
    signal_set = set(signal_dates)
    series_dates = {}
    series_closes = {}
    for s in names:
        items = sorted(bars[s].items())
        series_dates[s] = [d for d, _ in items]
        series_closes[s] = [px[1] for _, px in items]

    cash = 1.0
    shares = {s: 0.0 for s in names}
    last = {s: None for s in names}
    pending: dict[str, dict] = {}
    open_trade = {s: None for s in names}
    closed: list[Trade] = []
    rate = {s: cost_bp[s] * cost_mult / 10000.0 for s in names}
    final = sessions[-1]
    rows = []
    started = False

    def current_equity():
        equity = cash
        for s in names:
            if shares[s] != 0.0:
                if last[s] is None:
                    raise RuntimeError(f"held {s} with no mark")
                equity += shares[s] * last[s]
        return equity

    def trade_to(sym, new_shares, price, day, force_reason):
        old = shares[sym]
        if old == new_shares:
            return 0.0
        delta = new_shares - old
        cost = abs(delta) * price * rate[sym]
        nonlocal cash
        cash -= delta * price + cost
        shares[sym] = new_shares
        old_sign = sign(old)
        new_sign = sign(new_shares)
        if old_sign != 0 and open_trade[sym] is None:
            raise RuntimeError(f"shares without a trade on {sym}")
        if old_sign == 0 and new_sign != 0:
            tr = Trade(sym, "long" if new_sign > 0 else "short", day, price, new_shares)
            tr.cost += cost
            open_trade[sym] = tr
        elif old_sign != 0 and new_sign == 0:
            tr = open_trade[sym]
            tr.cost += cost
            tr.exit_date = day
            tr.exit_price = price
            tr.exit_reason = force_reason or "flat"
            tr.exit_shares = old
            closed.append(tr)
            open_trade[sym] = None
        elif old_sign == new_sign:
            tr = open_trade[sym]
            tr.cost += cost
            tr.exit_shares = new_shares
        else:
            closed_notional = abs(old) * price
            opened_notional = abs(new_shares) * price
            split = closed_notional + opened_notional
            tr = open_trade[sym]
            tr.cost += cost * (closed_notional / split)
            tr.exit_date = day
            tr.exit_price = price
            tr.exit_reason = "flip"
            tr.exit_shares = old
            closed.append(tr)
            nt = Trade(sym, "long" if new_sign > 0 else "short", day, price, new_shares)
            nt.cost += cost * (opened_notional / split)
            open_trade[sym] = nt
        return cost

    for day in sessions:
        eq_prev = current_equity()
        has_bar = {}
        opens = {}
        gaps = {}
        for s in names:
            bar = bars[s].get(day)
            if bar is None:
                has_bar[s] = False
                gaps[s] = 0.0
            else:
                has_bar[s] = True
                opens[s] = bar[0]
                if last[s] is not None and shares[s] != 0.0:
                    gaps[s] = shares[s] * (opens[s] - last[s])
                else:
                    gaps[s] = 0.0
                tr = open_trade[s]
                if tr is not None and gaps[s] != 0.0:
                    tr.gross += gaps[s]
                    tr.daily[day] = tr.daily.get(day, 0.0) + gaps[s]

        eq_open = cash
        for s in names:
            if shares[s] == 0.0:
                continue
            eq_open += shares[s] * (opens[s] if has_bar[s] else last[s])
        if abs((eq_open - eq_prev) - sum(gaps.values())) > 1e-8:
            raise RuntimeError(f"gap identity failed on {day}")

        cost_today = 0.0
        rebalance_fill = False
        if fill_mode != "close":
            for s in list(pending):
                if not has_bar[s]:
                    continue
                pending[s]["seen"] += 1
                if pending[s]["seen"] < pending[s]["need"]:
                    continue
                info = pending.pop(s)
                rebalance_fill = True
                cost_today += trade_to(s, info["weight"] * eq_open / opens[s], opens[s], day, None)

        price_pnl = sum(gaps.values())
        for s in names:
            if not has_bar[s]:
                continue
            close = bars[s][day][1]
            oc = shares[s] * (close - opens[s])
            price_pnl += oc
            tr = open_trade[s]
            if tr is not None and oc != 0.0:
                tr.gross += oc
                tr.daily[day] = tr.daily.get(day, 0.0) + oc
            last[s] = close

        exposed = any(shares[s] != 0.0 for s in names)

        if day in signal_set and day != final:
            form = {s: formation_at(series_dates[s], series_closes[s], day, lookback) for s in names}
            weights = weight_fn(day, form) if weight_fn else weights_from_formation(form, names, book_k)
            if weights is not None:
                if fill_mode == "close":
                    eq_close = current_equity()
                    new_pending = {}
                    for s in names:
                        if has_bar[s]:
                            rebalance_fill = True
                            close = bars[s][day][1]
                            cost_today += trade_to(
                                s, weights[s] * eq_close / close, close, day, None)
                        else:
                            new_pending[s] = {"weight": weights[s], "seen": 0, "need": 1}
                    pending = new_pending
                else:
                    need = 2 if fill_mode == "delay" else 1
                    pending = {s: {"weight": weights[s], "seen": 0, "need": need} for s in names}

        if day == final:
            for s in names:
                if shares[s] == 0.0:
                    continue
                price = last[s]
                if price is None:
                    raise RuntimeError(f"cannot flatten {s}")
                cost_today += trade_to(s, 0.0, price, day, "end")

        eq_after = current_equity()
        if abs((eq_after - eq_prev) - (price_pnl - cost_today)) > 1e-8:
            raise RuntimeError(
                f"equity identity failed on {day}: dE={eq_after - eq_prev} "
                f"price-cost={price_pnl - cost_today}")
        if rebalance_fill:
            started = True
        if started:
            rows.append({
                "date": day,
                "ret_net": eq_after / eq_prev - 1.0,
                "ret_gross": price_pnl / eq_prev,
                "equity": eq_after,
                "cost": cost_today,
                "price_pnl": price_pnl,
                "exposed": exposed,
                "rebalance_fill": rebalance_fill,
            })

    if any(open_trade[s] is not None for s in names):
        raise RuntimeError("a trade was still open after the last session")
    trade_rows = [t.as_dict() for t in closed]
    attrib = 0.0
    for tr in trade_rows:
        attrib += sum(tr["daily"].values())
    price_sum = sum(row["price_pnl"] for row in rows)
    if abs(attrib - price_sum) > 1e-8:
        raise RuntimeError(f"trade attribution {attrib} != price pnl {price_sum}")
    return rows, trade_rows


def sharpe(values) -> float | None:
    r = np.asarray(values, dtype=float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0 or not math.isfinite(sd):
        return None
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total_return(values) -> float:
    return float(np.prod(1.0 + np.asarray(values, dtype=float)) - 1.0)


def cagr(values):
    r = np.asarray(values, dtype=float)
    eq = float(np.prod(1.0 + r))
    if eq <= 0.0 or len(r) == 0:
        return None
    return float(eq ** (ANNUAL / len(r)) - 1.0)


def max_drawdown(values) -> float:
    eq = np.cumprod(1.0 + np.asarray(values, dtype=float))
    peak = np.maximum.accumulate(eq)
    return float((eq / peak - 1.0).min())


def t_stat(values):
    r = np.asarray(values, dtype=float)
    n = len(r)
    if n < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0:
        return None
    return float(r.mean() / (sd / math.sqrt(n)))


def profit_factor(trades) -> float | None:
    pos = sum(t["net_pnl"] for t in trades if t["net_pnl"] > 0.0)
    neg = sum(t["net_pnl"] for t in trades if t["net_pnl"] < 0.0)
    if neg == 0.0:
        return None
    return float(pos / abs(neg))


def holding_sessions(trade, index) -> int:
    span = index[trade["exit_date"]] - index[trade["entry_date"]]
    if trade["exit_reason"] == "end":
        return span + 1
    return span


def pack_metrics(rows, trades, session_index) -> dict:
    rets = [row["ret_net"] for row in rows]
    gross = [row["ret_gross"] for row in rows]
    winners = [t for t in trades if t["net_pnl"] > 0.0]
    losers = [t for t in trades if t["net_pnl"] < 0.0]
    holds = [holding_sessions(t, session_index) for t in trades]
    bps = [t["return_bp"] for t in trades]
    return {
        "start": rows[0]["date"].isoformat() if rows else None,
        "end": rows[-1]["date"].isoformat() if rows else None,
        "sessions": len(rows),
        "total_return": total_return(rets) if rets else None,
        "cagr": cagr(rets) if rets else None,
        "annual_volatility": float(np.std(rets, ddof=1) * math.sqrt(ANNUAL)) if len(rets) > 1 else None,
        "sharpe": sharpe(rets),
        "gross_sharpe": sharpe(gross),
        "max_drawdown": max_drawdown(rets) if rets else None,
        "t_stat": t_stat(rets),
        "trades": len(trades),
        "win_rate": (len(winners) / len(trades)) if trades else None,
        "profit_factor": profit_factor(trades),
        "avg_net_trade_bp": float(np.mean(bps)) if bps else None,
        "avg_winner": float(np.mean([t["net_pnl"] for t in winners])) if winners else None,
        "avg_loser": float(np.mean([t["net_pnl"] for t in losers])) if losers else None,
        "exposure": float(np.mean([row["exposed"] for row in rows])) if rows else None,
        "holding_mean": float(np.mean(holds)) if holds else None,
        "holding_median": float(np.median(holds)) if holds else None,
        "sessions_with_fill": int(sum(1 for row in rows if row["rebalance_fill"])),
    }


def slice_rows(rows, start, end):
    return [row for row in rows if start <= row["date"] <= end]


def trades_entering(trades, start, end):
    return [t for t in trades if start <= t["entry_date"] <= end]


def window_bundle(rows, trades, session_index, start, end):
    kept = slice_rows(rows, start, end)
    return pack_metrics(kept, trades_entering(trades, start, end), session_index)


def block_bootstrap(values, n_draws, block, seed):
    r = np.asarray(values, dtype=float)
    n = len(r)
    rng = np.random.default_rng(seed)
    n_blocks = (n + block - 1) // block
    starts = rng.integers(0, n, size=(n_draws, n_blocks))
    out = np.empty(n_draws)
    offset = np.arange(block)
    for i in range(n_draws):
        pieces = [r[(int(s) + offset) % n] for s in starts[i]]
        sample = np.concatenate(pieces)[:n]
        out[i] = sharpe(sample)
    return out


def direction_placebo(rows, trades, seed, n_draws):
    dates = [row["date"] for row in rows]
    index = {day: i for i, day in enumerate(dates)}
    mat = np.zeros((len(trades), len(dates)))
    for i, tr in enumerate(trades):
        for day, pnl in tr["daily"].items():
            mat[i, index[day]] += pnl
    prev = np.empty(len(rows))
    prev[0] = 1.0
    for i in range(1, len(rows)):
        prev[i] = rows[i - 1]["equity"]
    actual = sharpe(mat.sum(axis=0) / prev)
    rng = np.random.default_rng(seed)
    signs = rng.choice(np.array([-1.0, 1.0]), size=(n_draws, len(trades)))
    draw_rets = (signs @ mat) / prev
    mu = draw_rets.mean(axis=1)
    sd = draw_rets.std(axis=1, ddof=1)
    draws = mu / sd * math.sqrt(ANNUAL)
    p = (1 + int(np.sum(draws >= actual))) / (n_draws + 1)
    return actual, draws, p


def prepare_series(bars):
    out = {}
    for s, items in bars.items():
        ordered = sorted(items.items())
        out[s] = ([d for d, _ in ordered], [px[1] for _, px in ordered])
    return out


def signal_weights(series, names, day, lookback, book_k):
    form = {s: formation_at(series[s][0], series[s][1], day, lookback) for s in names}
    return weights_from_formation(form, names, book_k), form


def assert_close(actual, expected, label, tol=1e-9):
    if actual is None or abs(actual - expected) > tol:
        raise SystemExit(f"self-test {label}: {actual} != {expected}")


def self_test() -> None:
    names = NAMES
    d0, d1, d2, d3, d4, d5 = (dt.date(2020, 1, day) for day in (2, 3, 6, 7, 8, 9))

    # Month-end helper and the three closures.
    oct_days = [
        dt.date(2012, 10, 26), dt.date(2012, 10, 29), dt.date(2012, 10, 30),
        dt.date(2012, 10, 31), dt.date(2012, 11, 1),
    ]
    if month_ends(book_sessions_from(oct_days)) != [dt.date(2012, 10, 31), dt.date(2012, 11, 1)]:
        raise SystemExit("self-test month-end failed")
    sandy = [dt.date(2012, 10, 26), dt.date(2012, 10, 29), dt.date(2012, 10, 30)]
    if month_ends(book_sessions_from(sandy)) != [dt.date(2012, 10, 26)]:
        raise SystemExit("self-test skipped month-end failed")

    # Formation ignores a hole instead of inserting a flat bar.
    hole = formation_at([d0, d1, d3], [100.0, 100.0, 110.0], d3, 1)
    assert_close(hole, 0.10, "hole formation")

    def panel(spec):
        out = {s: {} for s in names}
        for day, row in spec.items():
            for s in names:
                if row[s] is not None:
                    out[s][day] = row[s]
        return out

    def flat(px):
        return {s: (px, px) for s in names}

    # 1. Clean rank, then a last-session flatten on a unchanged day.
    clean_px = {
        d0: flat(100.0),
        d1: flat(100.0),
        d2: {
            "GLD": (100.0, 130.0), "SLV": (100.0, 120.0), "USO": (100.0, 110.0),
            "UNG": (100.0, 100.0), "DBA": (100.0, 90.0), "DBB": (100.0, 80.0),
        },
        d3: {
            "GLD": (130.0, 143.0), "SLV": (120.0, 120.0), "USO": (110.0, 110.0),
            "UNG": (100.0, 100.0), "DBA": (90.0, 81.0), "DBB": (80.0, 80.0),
        },
        d4: {
            "GLD": (143.0, 143.0), "SLV": (120.0, 120.0), "USO": (110.0, 110.0),
            "UNG": (100.0, 100.0), "DBA": (81.0, 81.0), "DBB": (80.0, 80.0),
        },
    }
    rows, trades = run_book(
        [d0, d1, d2, d3, d4], panel(clean_px), names, [d2], 2, COST_BP, 1.0, 2)
    if [row["date"] for row in rows] != [d3, d4]:
        raise SystemExit("self-test clean rank evaluation window")
    assert_close(rows[0]["equity"], 1.0994, "clean equity")
    assert_close(rows[0]["ret_net"], 0.0994, "clean return")
    by = {t["symbol"]: t for t in trades}
    if set(by) != {"GLD", "SLV", "DBA", "DBB"}:
        raise SystemExit(f"self-test clean names {set(by)}")
    if by["GLD"]["side"] != "long" or by["SLV"]["side"] != "long":
        raise SystemExit("self-test clean longs")
    if by["DBA"]["side"] != "short" or by["DBB"]["side"] != "short":
        raise SystemExit("self-test clean shorts")
    assert_close(by["GLD"]["entry_price"], 130.0, "GLD entry")
    assert_close(by["GLD"]["exit_price"], 143.0, "GLD exit")
    assert_close(by["GLD"]["gross_pnl"], 0.05, "GLD gross")
    if any(t["exit_reason"] != "end" for t in trades):
        raise SystemExit("self-test clean exit reason")
    assert_close(rows[1]["equity"], 1.09882, "clean final equity", tol=1e-9)

    # 2. Exact tie at the boundary. Alphabetically earlier symbol ranks higher.
    tie_px = {
        d1: flat(100.0),
        d2: {
            "DBA": (100.0, 120.0), "GLD": (100.0, 120.0), "SLV": (100.0, 120.0),
            "DBB": (100.0, 100.0), "UNG": (100.0, 90.0), "USO": (100.0, 80.0),
        },
        d3: {
            "DBA": (120.0, 120.0), "GLD": (120.0, 120.0), "SLV": (120.0, 120.0),
            "DBB": (100.0, 100.0), "UNG": (90.0, 90.0), "USO": (80.0, 80.0),
        },
    }
    _, tie_trades = run_book([d1, d2, d3], panel(tie_px), names, [d2], 1, COST_BP, 1.0, 2)
    tie_side = {t["symbol"]: t["side"] for t in tie_trades}
    if tie_side.get("DBA") != "long" or tie_side.get("GLD") != "long":
        raise SystemExit(f"self-test tie longs {tie_side}")
    if "SLV" in tie_side or "DBB" in tie_side:
        raise SystemExit(f"self-test tie middle entered {tie_side}")
    if tie_side.get("UNG") != "short" or tie_side.get("USO") != "short":
        raise SystemExit(f"self-test tie shorts {tie_side}")

    # 3. Missing bar: stale formation, delayed fill sized off later equity, a held name earns 0.
    miss = {s: {} for s in names}
    for s in names:
        miss[s][d0] = (100.0, 100.0)
        miss[s][d1] = (100.0, 100.0)
    miss["GLD"][d2] = (100.0, 120.0)
    miss["USO"][d2] = (100.0, 110.0)
    miss["UNG"][d2] = (100.0, 101.0)
    miss["DBA"][d2] = (100.0, 90.0)
    miss["DBB"][d2] = (100.0, 80.0)
    # SLV has no d2 bar.
    for s in ("SLV", "USO", "UNG", "DBA", "DBB"):
        prev = 100.0 if s == "SLV" else miss[s][d2][1]
        miss[s][d3] = (prev, prev)
    # GLD has no d3 bar. USO has no d4 bar.
    miss["GLD"][d4] = (120.0, 120.0)
    for s in ("SLV", "UNG", "DBA", "DBB"):
        prev = miss[s][d3][1]
        miss[s][d4] = (prev, prev)
    m_rows, m_trades = run_book(
        [d0, d1, d2, d3, d4], miss, names, [d2], 1, COST_BP, 1.0, 2)
    gld = [t for t in m_trades if t["symbol"] == "GLD"]
    if len(gld) != 1 or gld[0]["entry_date"] != d4 or gld[0]["side"] != "long":
        raise SystemExit("self-test missing-bar GLD fill day")
    assert_close(gld[0]["entry_shares"], 0.5 * 0.99925 / 120.0, "GLD delayed shares")
    if any(t["symbol"] == "SLV" for t in m_trades):
        raise SystemExit("self-test missing signal bar ranked SLV in")
    uso = [t for t in m_trades if t["symbol"] == "USO"][0]
    d4_row = next(row for row in m_rows if row["date"] == d4)
    assert_close(d4_row["price_pnl"], 0.0, "missing held name pnl")
    assert_close(uso["exit_price"], 110.0, "USO flattened at last close")
    if uso["exit_reason"] != "end" or uso["exit_date"] != d4:
        raise SystemExit("self-test USO end exit")

    # 4. Smooth path across a labelled split. Identical moves net to zero price P&L.
    smooth = {
        d1: flat(100.0),
        d2: flat(100.0),
        d3: flat(100.0) | {s: (100.0, 101.0) for s in names},
        d4: {s: (101.0, 101.0) for s in names},
    }
    # The dict union above overwrites flat. Build d3 explicitly.
    smooth[d3] = {s: (100.0, 101.0) for s in names}
    s_rows, s_trades = run_book([d1, d2, d3, d4], panel(smooth), names, [d2], 1, COST_BP, 1.0, 2)
    assert_close(s_rows[0]["price_pnl"], 0.0, "smooth price pnl")
    assert_close(s_rows[0]["equity"], 0.999, "smooth equity")
    if abs(s_rows[0]["ret_net"]) > 0.01:
        raise SystemExit("self-test smooth path behaved like an unadjusted split")
    if {t["symbol"] for t in s_trades} != {"DBA", "DBB", "UNG", "USO"}:
        raise SystemExit("self-test smooth tie-break names")

    # 5 and 6. Flip, and last-session exit. The final session is also a signal and must be discarded.
    flip = {
        d1: flat(100.0),
        d2: {
            "GLD": (100.0, 150.0), "SLV": (100.0, 140.0), "USO": (100.0, 100.0),
            "UNG": (100.0, 100.0), "DBA": (100.0, 90.0), "DBB": (100.0, 80.0),
        },
        d3: {
            "GLD": (150.0, 150.0), "SLV": (140.0, 140.0), "USO": (100.0, 100.0),
            "UNG": (100.0, 100.0), "DBA": (90.0, 90.0), "DBB": (80.0, 80.0),
        },
        d4: {
            "GLD": (150.0, 100.0), "SLV": (140.0, 140.0), "USO": (100.0, 130.0),
            "UNG": (100.0, 120.0), "DBA": (90.0, 90.0), "DBB": (80.0, 80.0),
        },
        d5: {
            "GLD": (100.0, 100.0), "SLV": (140.0, 140.0), "USO": (130.0, 130.0),
            "UNG": (120.0, 120.0), "DBA": (90.0, 90.0), "DBB": (80.0, 80.0),
        },
    }
    _, f_trades = run_book(
        [d1, d2, d3, d4, d5], panel(flip), names, [d2, d4, d5], 1, COST_BP, 1.0, 2)
    gld_f = [t for t in f_trades if t["symbol"] == "GLD"]
    if len(gld_f) != 2:
        raise SystemExit(f"self-test flip count {len(gld_f)}")
    if gld_f[0]["side"] != "long" or gld_f[0]["exit_reason"] != "flip":
        raise SystemExit("self-test flip first leg")
    assert_close(gld_f[0]["entry_price"], 150.0, "flip entry")
    assert_close(gld_f[0]["exit_price"], 100.0, "flip exit")
    assert_close(gld_f[0]["gross_pnl"], -1.0 / 6.0, "flip gross")
    if gld_f[1]["side"] != "short" or gld_f[1]["exit_reason"] != "end":
        raise SystemExit("self-test last-session exit")
    assert_close(gld_f[1]["entry_price"], 100.0, "second entry")
    assert_close(gld_f[1]["exit_price"], 100.0, "second exit")
    assert_close(gld_f[1]["gross_pnl"], 0.0, "second gross")
    uso_f = [t for t in f_trades if t["symbol"] == "USO"]
    if len(uso_f) != 1 or uso_f[0]["side"] != "long" or uso_f[0]["exit_reason"] != "end":
        raise SystemExit("self-test discarded last signal")
    dba_f = [t for t in f_trades if t["symbol"] == "DBA"]
    if len(dba_f) != 1 or dba_f[0]["side"] != "short" or dba_f[0]["exit_reason"] != "flat":
        raise SystemExit("self-test flat exit")
    slv_f = [t for t in f_trades if t["symbol"] == "SLV"]
    if len(slv_f) != 2 or slv_f[1]["exit_reason"] != "end":
        raise SystemExit("self-test SLV end rather than a discarded rebalance")

    # Unfilled target is replaced by the next signal.
    sup = {s: {} for s in names}
    for s in names:
        sup[s][d1] = (100.0, 100.0)
    sup["GLD"][d2] = (100.0, 150.0)
    sup["SLV"][d2] = (100.0, 140.0)
    sup["USO"][d2] = (100.0, 110.0)
    sup["UNG"][d2] = (100.0, 100.0)
    sup["DBA"][d2] = (100.0, 90.0)
    sup["DBB"][d2] = (100.0, 80.0)
    sup["SLV"][d3] = (140.0, 140.0)
    sup["USO"][d3] = (110.0, 220.0)
    sup["UNG"][d3] = (100.0, 180.0)
    sup["DBA"][d3] = (90.0, 90.0)
    sup["DBB"][d3] = (80.0, 80.0)
    for s, prev in (("GLD", 150.0), ("SLV", 140.0), ("USO", 220.0), ("UNG", 180.0),
                    ("DBA", 90.0), ("DBB", 80.0)):
        sup[s][d4] = (prev, prev)
    _, sup_trades = run_book([d1, d2, d3, d4], sup, names, [d2, d3], 1, COST_BP, 1.0, 2)
    if any(t["symbol"] == "GLD" for t in sup_trades):
        raise SystemExit("self-test superseded pending still traded GLD")

    print("self-test passed")


def load_store():
    from mdq import MarketData, nyse_sessions

    bars = {s: {} for s in NAMES}
    raw = {}
    actions = {}
    with MarketData() as md:
        for s in NAMES:
            loaded = md.bars(s, "1d", start=FIRST, end=LAST)
            raw[s] = md.bars(s, "1d", start=FIRST, end=LAST, adjust=False)
            actions[s] = md.corporate_actions(s)
            if len(loaded) != EXPECTED_BARS:
                raise SystemExit(f"{s} has {len(loaded)} bars, expected {EXPECTED_BARS}")
            days = [b.session for b in loaded]
            if days[0] != FIRST or days[-1] != LAST:
                raise SystemExit(f"{s} range {days[0]} {days[-1]}")
            if len(days) != len(set(days)):
                raise SystemExit(f"{s} duplicate sessions")
            if any(min(b.open, b.high, b.low, b.close) <= 0.0 for b in loaded):
                raise SystemExit(f"{s} non-positive price")
            for b in loaded:
                bars[s][b.session] = (b.open, b.close)
        spy = md.bars("SPY", "1d", start=FIRST, end=LAST)
        spy_days = [b.session for b in spy]
        calendar = book_sessions_from(nyse_sessions(FIRST, LAST))
    if calendar != spy_days or calendar != sorted(bars["GLD"]):
        raise SystemExit(
            f"calendar {len(calendar)} spy {len(spy_days)} GLD {len(bars['GLD'])} disagree")
    if any(d in calendar for d in SKIP):
        raise SystemExit("skipped closure still in the book calendar")
    for s in NAMES:
        got = [(a["type"], a["ex_date"], a["split_ratio"], a["amount"]) for a in actions[s]]
        expect = [("split", d.isoformat(), ratio, None) for d, ratio in EXPECTED_SPLITS.get(s, ())]
        if got != expect:
            raise SystemExit(f"{s} corporate actions {got} != {expect}")
        raw_by = {b.session: b.close for b in raw[s]}
        for ex, _ratio in EXPECTED_SPLITS.get(s, ()):
            prev = max(d for d in raw_by if d < ex)
            raw_ratio = raw_by[ex] / raw_by[prev]
            adj_ratio = bars[s][ex][1] / bars[s][prev][1]
            if not (raw_ratio > 3.0 and 0.5 < adj_ratio < 1.5):
                raise SystemExit(f"{s} {ex} split continuity raw {raw_ratio} adj {adj_ratio}")
    return calendar, bars


def first_signal(calendar, bars, names, lookback):
    series = prepare_series(bars)
    for day in month_ends(calendar):
        form = [formation_at(series[s][0], series[s][1], day, lookback) for s in names]
        if all(v is not None for v in form):
            return day
    raise RuntimeError(f"no rankable month-end at K={lookback}")


def simulate(calendar, bars, names, lookback, cost_mult, fill_mode="next_open", weight_fn=None,
             start_signal=None):
    start = start_signal or first_signal(calendar, bars, names, lookback)
    sessions = [d for d in calendar if d >= start]
    signals = [d for d in month_ends(calendar) if d >= start]
    rows, trades = run_book(
        sessions, bars, names, signals, lookback, COST_BP, cost_mult, BOOK_K, fill_mode, weight_fn)
    return rows, trades, start


def constant_weights(names):
    unit = 1.0 / len(names)

    def fn(_day, _form):
        return {s: unit for s in names}

    return fn


def timing_weights(series, names, signals, lookback, rng):
    """One permutation of formation returns at each signal, including the discarded last one."""
    out = {}
    for day in signals:
        form = {s: formation_at(series[s][0], series[s][1], day, lookback) for s in names}
        values = [form[s] for s in names]
        order = rng.permutation(len(names))
        shuffled = {names[i]: values[int(order[i])] for i in range(len(names))}
        out[day] = weights_from_formation(shuffled, names, BOOK_K)

    def fn(day, _form):
        return out[day]

    return fn


def yearly(rows, bench_by_date):
    by = {}
    for row in rows:
        by.setdefault(row["date"].year, []).append(row)
    out = {}
    for year, chunk in by.items():
        rets = [row["ret_net"] for row in chunk]
        brets = [bench_by_date[row["date"]] for row in chunk]
        out[str(year)] = {
            "return_": total_return(rets),
            "sharpe": sharpe(rets),
            "max_drawdown": max_drawdown(rets),
            "benchmark": total_return(brets),
            "sessions": len(chunk),
        }
    return out


def quintiles(rows, bench_by_date):
    buckets: dict[tuple[int, int], list] = {}
    for row in rows:
        key = (row["date"].year, row["date"].month)
        buckets.setdefault(key, []).append(row)
    months = []
    for key, chunk in buckets.items():
        sret = total_return([row["ret_net"] for row in chunk])
        bret = total_return([bench_by_date[row["date"]] for row in chunk])
        months.append((bret, key, sret))
    months.sort()
    n = len(months)
    groups = {q: {"n": 0, "strategy": [], "benchmark": []} for q in range(1, 6)}
    for i, (bret, _key, sret) in enumerate(months):
        q = i * 5 // n + 1
        groups[q]["n"] += 1
        groups[q]["strategy"].append(sret)
        groups[q]["benchmark"].append(bret)
    return {
        str(q): {
            "n": groups[q]["n"],
            "mean_strategy": float(np.mean(groups[q]["strategy"])),
            "mean_benchmark": float(np.mean(groups[q]["benchmark"])),
        }
        for q in range(1, 6)
    }


def side_block(trades, side):
    kept = [t for t in trades if t["side"] == side]
    return {
        "trades": len(kept),
        "gross": float(sum(t["gross_pnl"] for t in kept)),
        "net": float(sum(t["net_pnl"] for t in kept)),
        "profit_factor": profit_factor(kept),
        "win_rate": (sum(1 for t in kept if t["net_pnl"] > 0.0) / len(kept)) if kept else None,
        "avg_net_trade_bp": float(np.mean([t["return_bp"] for t in kept])) if kept else None,
    }


def convert(value):
    if isinstance(value, dict):
        return {str(k): convert(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [convert(v) for v in value]
    if isinstance(value, np.ndarray):
        return convert(value.tolist())
    if isinstance(value, np.floating):
        value = float(value)
    elif isinstance(value, np.integer):
        return int(value)
    if isinstance(value, float):
        if not math.isfinite(value):
            return None
        return value
    if isinstance(value, dt.date):
        return value.isoformat()
    return value


def write_daily(path, rows, bench_rows):
    bench = {row["date"]: row for row in bench_rows}
    lines = ["date,strategy_net,strategy_gross,benchmark,equity_net,benchmark_equity,cost,price_pnl,exposed"]
    for row in rows:
        b = bench[row["date"]]
        lines.append(
            f"{row['date'].isoformat()},{row['ret_net']:.12g},{row['ret_gross']:.12g},"
            f"{b['ret_net']:.12g},{row['equity']:.12g},{b['equity']:.12g},"
            f"{row['cost']:.12g},{row['price_pnl']:.12g},{int(row['exposed'])}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def write_trades(path, trades, session_index):
    header = (
        "symbol,side,entry_date,entry_price,exit_date,exit_price,exit_reason,"
        "entry_shares,exit_shares,gross_pnl,cost,net_pnl,return_bp,holding_sessions,sample"
    )
    lines = [header]
    ordered = sorted(trades, key=lambda t: (t["entry_date"], t["symbol"], t["side"]))
    for t in ordered:
        sample = "OOS" if t["entry_date"] >= OOS_START else "IS"
        lines.append(
            ",".join([
                t["symbol"], t["side"], t["entry_date"].isoformat(), f"{t['entry_price']:.10g}",
                t["exit_date"].isoformat(), f"{t['exit_price']:.10g}", t["exit_reason"],
                f"{t['entry_shares']:.12g}", f"{t['exit_shares']:.12g}",
                f"{t['gross_pnl']:.12g}", f"{t['cost']:.12g}", f"{t['net_pnl']:.12g}",
                f"{t['return_bp']:.12g}", str(holding_sessions(t, session_index)), sample,
            ])
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def append_runlog(when, digest, head, dirty, reason, headline) -> None:
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text(
            "# Run log\n\nAppend-only. One entry per store run.\n",
            encoding="utf-8", newline="\n")
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(
            f"\n## {when}\n"
            f"- rules_sha256 {digest}\n"
            f"- git_head {head} dirty={'yes' if dirty else 'no'}\n"
            f"- reason: {reason}\n"
            f"- {headline}\n"
        )


def status_of(oos_sharpe, oos_pf, placebo_p, is_sharpe, grid_positive, ret_2x, oos_trips) -> str:
    if oos_trips < 24:
        return "Inconclusive"
    line1 = oos_sharpe is not None and oos_sharpe >= 0.5 and oos_pf is not None and oos_pf >= 1.10
    line2 = placebo_p is not None and placebo_p <= 0.05
    line3 = is_sharpe is not None and is_sharpe > 0.0 and grid_positive >= 3
    line4 = ret_2x is not None and ret_2x > 0.0
    if line1 and line2 and line3 and line4:
        return "Paper-trading candidate"
    return "Rejected"


def main() -> None:
    digest = require_lock()
    self_test()
    reason = "initial pre-registered run"
    if len(sys.argv) > 1:
        reason = " ".join(sys.argv[1:])

    calendar, bars = load_store()
    session_index = {day: i for i, day in enumerate(calendar)}
    rows, trades, start = simulate(calendar, bars, NAMES, K_PRIMARY, 1.0)
    bench_rows, _bench_trades, _bench_start = simulate(
        calendar, bars, NAMES, K_PRIMARY, 0.0, weight_fn=constant_weights(NAMES), start_signal=start)
    if [row["date"] for row in rows] != [row["date"] for row in bench_rows]:
        raise SystemExit("benchmark sessions do not match the primary")

    full = pack_metrics(rows, trades, session_index)
    is_m = window_bundle(rows, trades, session_index, rows[0]["date"], IS_END)
    oos_m = window_bundle(rows, trades, session_index, OOS_START, LAST)
    bfull = pack_metrics(bench_rows, [], session_index)
    bis = window_bundle(bench_rows, [], session_index, bench_rows[0]["date"], IS_END)
    boos = window_bundle(bench_rows, [], session_index, OOS_START, LAST)

    grid = {}
    for k in GRID_K:
        g_rows, g_trades, g_start = simulate(calendar, bars, NAMES, k, 1.0)
        g_is = window_bundle(g_rows, g_trades, session_index, g_rows[0]["date"], IS_END)
        g_oos = window_bundle(g_rows, g_trades, session_index, OOS_START, LAST)
        g_full = pack_metrics(g_rows, g_trades, session_index)
        grid[str(k)] = {
            "first_signal": g_start.isoformat(),
            "first_fill": g_rows[0]["date"].isoformat(),
            "IS_sharpe": g_is["sharpe"],
            "OOS_sharpe": g_oos["sharpe"],
            "IS_return": g_is["total_return"],
            "OOS_return": g_oos["total_return"],
            "full_sharpe": g_full["sharpe"],
            "IS_sessions": g_is["sessions"],
            "OOS_sessions": g_oos["sessions"],
        }
    if abs(grid[str(K_PRIMARY)]["IS_sharpe"] - is_m["sharpe"]) > 1e-12:
        raise SystemExit("K=252 grid cell does not match the primary IS Sharpe")

    costs = {}
    for mult in COST_SWEEP:
        if mult == 1.0:
            c_rows, c_trades = rows, trades
        else:
            c_rows, c_trades, _ = simulate(calendar, bars, NAMES, K_PRIMARY, mult, start_signal=start)
        c_full = pack_metrics(c_rows, c_trades, session_index)
        c_oos = window_bundle(c_rows, c_trades, session_index, OOS_START, LAST)
        costs[f"{mult:g}"] = {
            "full_sharpe": c_full["sharpe"],
            "oos_sharpe": c_oos["sharpe"],
            "full_return": c_full["total_return"],
            "oos_return": c_oos["total_return"],
            "full_gross_sharpe": c_full["gross_sharpe"],
        }

    d_rows, d_trades, _ = simulate(
        calendar, bars, NAMES, K_PRIMARY, 1.0, fill_mode="delay", start_signal=start)
    delay = {
        "full_sharpe": pack_metrics(d_rows, d_trades, session_index)["sharpe"],
        "oos_sharpe": window_bundle(d_rows, d_trades, session_index, OOS_START, LAST)["sharpe"],
        "full_return": pack_metrics(d_rows, d_trades, session_index)["total_return"],
    }
    u_rows, u_trades, _ = simulate(
        calendar, bars, NAMES, K_PRIMARY, 1.0, fill_mode="close", start_signal=start)
    upper = {
        "full_sharpe": pack_metrics(u_rows, u_trades, session_index)["sharpe"],
        "oos_sharpe": window_bundle(u_rows, u_trades, session_index, OOS_START, LAST)["sharpe"],
        "full_return": pack_metrics(u_rows, u_trades, session_index)["total_return"],
    }

    actual_gross, dir_draws, dir_p = direction_placebo(rows, trades, SEED_DIRECTION, N_DIRECTION)
    if abs(actual_gross - full["gross_sharpe"]) > 1e-9:
        raise SystemExit("placebo actual gross Sharpe does not match the book")
    boot = block_bootstrap([row["ret_net"] for row in rows], N_BOOTSTRAP, BLOCK, SEED_BOOTSTRAP)

    series = prepare_series(bars)
    signals = [d for d in month_ends(calendar) if d >= start]
    timing_rng = np.random.default_rng(SEED_TIMING)
    timing_draws = np.empty(N_TIMING)
    for i in range(N_TIMING):
        fn = timing_weights(series, NAMES, signals, K_PRIMARY, timing_rng)
        t_rows, _t_trades, _ = simulate(
            calendar, bars, NAMES, K_PRIMARY, 1.0, weight_fn=fn, start_signal=start)
        timing_draws[i] = pack_metrics(t_rows, [], session_index)["gross_sharpe"]
    timing_p = (1 + int(np.sum(timing_draws >= actual_gross))) / (N_TIMING + 1)

    leave = {}
    for dropped in NAMES:
        kept = tuple(s for s in NAMES if s != dropped)
        l_rows, l_trades, _ = simulate(calendar, bars, kept, K_PRIMARY, 1.0, start_signal=start)
        gross = float(sum(row["price_pnl"] for row in l_rows))
        leave[dropped] = {"gross_pnl": gross, "positive": gross > 0.0}

    bench_by = {row["date"]: row["ret_net"] for row in bench_rows}
    by_exit = {}
    for reason_name in ("flip", "flat", "end"):
        kept = [t for t in trades if t["exit_reason"] == reason_name]
        by_exit[reason_name] = {
            "trades": len(kept),
            "gross": float(sum(t["gross_pnl"] for t in kept)),
            "net": float(sum(t["net_pnl"] for t in kept)),
        }
    by_name = {}
    for s in NAMES:
        kept = [t for t in trades if t["symbol"] == s]
        by_name[s] = {
            "trades": len(kept),
            "gross": float(sum(t["gross_pnl"] for t in kept)),
            "net": float(sum(t["net_pnl"] for t in kept)),
            "long_trades": sum(1 for t in kept if t["side"] == "long"),
            "short_trades": sum(1 for t in kept if t["side"] == "short"),
        }

    long_gross = float(sum(t["gross_pnl"] for t in trades if t["side"] == "long"))
    short_gross = float(sum(t["gross_pnl"] for t in trades if t["side"] == "short"))
    oos_trips = oos_m["trades"]
    grid_positive = sum(1 for cell in grid.values() if cell["IS_sharpe"] is not None and cell["IS_sharpe"] > 0.0)
    verdict = status_of(
        oos_m["sharpe"], oos_m["profit_factor"], dir_p, is_m["sharpe"], grid_positive,
        costs["2"]["full_return"], oos_trips)
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True).stdout.strip())
    when = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")

    results = {
        "rules_sha256": digest,
        "git_head": head,
        "git_dirty": dirty,
        "run_utc": when,
        "seeds": {
            "direction": SEED_DIRECTION,
            "bootstrap": SEED_BOOTSTRAP,
            "timing": SEED_TIMING,
            "verify": SEED_VERIFY,
        },
        "params": {
            "K": K_PRIMARY,
            "book_k": BOOK_K,
            "names": list(NAMES),
            "cost_bp": COST_BP,
            "first_signal": start.isoformat(),
            "first_fill": rows[0]["date"].isoformat(),
            "is_end": IS_END.isoformat(),
            "oos_start": OOS_START.isoformat(),
            "last": LAST.isoformat(),
        },
        "full": full,
        "IS": is_m,
        "OOS": oos_m,
        "benchmark": {"full": bfull, "IS": bis, "OOS": boos},
        "predictions": {
            "long_gross": long_gross,
            "short_gross": short_gross,
            "long_positive": long_gross > 0.0,
            "short_positive": short_gross > 0.0,
            "leave_one_out": leave,
            "all_leave_one_out_positive": all(v["positive"] for v in leave.values()),
        },
        "placebo": {
            "direction": {
                "actual_gross_sharpe": actual_gross,
                "null_mean": float(dir_draws.mean()),
                "null_sd": float(dir_draws.std(ddof=1)),
                "p": dir_p,
                "n": N_DIRECTION,
                "null_p95": float(np.quantile(dir_draws, 0.95)),
                "null_sharpes": dir_draws.tolist(),
            },
            "timing": {
                "actual_gross_sharpe": actual_gross,
                "null_mean": float(timing_draws.mean()),
                "null_sd": float(timing_draws.std(ddof=1)),
                "p": timing_p,
                "n": N_TIMING,
                "null_sharpes": timing_draws.tolist(),
            },
        },
        "bootstrap": {
            "p2_5": float(np.quantile(boot, 0.025)),
            "p97_5": float(np.quantile(boot, 0.975)),
            "median": float(np.quantile(boot, 0.50)),
            "n": N_BOOTSTRAP,
            "block": BLOCK,
        },
        "grid": grid,
        "grid_cells_is_sharpe_positive": grid_positive,
        "costs": costs,
        "fill_delay": delay,
        "close_fill_upper_bound": upper,
        "breakdown": {
            "by_year": yearly(rows, bench_by),
            "by_side": {"long": side_block(trades, "long"), "short": side_block(trades, "short")},
            "by_exit": by_exit,
            "by_name": by_name,
            "quintile_month": quintiles(rows, bench_by),
        },
        "activity": {
            "trades_per_year": full["trades"] / (full["sessions"] / ANNUAL),
            "exposure": full["exposure"],
            "holding_mean": full["holding_mean"],
            "holding_median": full["holding_median"],
            "sessions_with_fill": full["sessions_with_fill"],
            "win_rate": full["win_rate"],
            "avg_winner": full["avg_winner"],
            "avg_loser": full["avg_loser"],
        },
        "acceptance": {
            "oos_sharpe": oos_m["sharpe"],
            "oos_profit_factor": oos_m["profit_factor"],
            "direction_p": dir_p,
            "is_sharpe": is_m["sharpe"],
            "grid_positive": grid_positive,
            "grid_required": 3,
            "full_return_2x": costs["2"]["full_return"],
            "oos_round_trips": oos_trips,
            "line_5": "not applicable",
        },
        "status": verdict,
    }
    (HERE / "results.json").write_text(
        json.dumps(convert(results), indent=2) + "\n", encoding="utf-8", newline="\n")
    write_daily(HERE / "daily.csv", rows, bench_rows)
    write_trades(HERE / "trades.csv", trades, session_index)
    headline = (
        f"full Sharpe {full['sharpe']:.6f} return {full['total_return']:.6f} | "
        f"IS Sharpe {is_m['sharpe']:.6f} | "
        f"OOS Sharpe {oos_m['sharpe']:.6f} return {oos_m['total_return']:.6f} | "
        f"round trips full/IS/OOS {full['trades']}/{is_m['trades']}/{oos_m['trades']} | "
        f"status {verdict}"
    )
    append_runlog(when, digest, head, dirty, reason, headline)
    print(headline)


if __name__ == "__main__":
    main()
