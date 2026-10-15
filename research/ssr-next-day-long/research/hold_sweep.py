# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc hold sweep, 1 through 31 sessions.

Same fills, costs, and one-cohort book as RULES.md. The hold is the only
change. This does not rewrite results.json and does not change the verdict.
"""

from __future__ import annotations

import csv
import importlib.util
import json
import statistics
import sys
from datetime import date, datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from research.kit.metrics import avg_net_bp, profit_factor, win_rate  # noqa: E402

HERE = Path(__file__).resolve().parent
HOLDS = range(1, 32)
COST_BPS = 10.0


def load_engine():
    path = HERE / "backtest.py"
    spec = importlib.util.spec_from_file_location("ssr_backtest", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def name_outcome(sessions, bars, episode, hold: int, cost_bps: float):
    """One episode, weight 1, using the locked entry and missing-bar exit.

    Returns None when the fill bar is missing. The result is that name's round
    trip. It is not an account path, and it ignores whether another cohort was
    already open.
    """
    index = {day: i for i, day in enumerate(sessions)}
    entry = episode["entry"]
    if entry not in index:
        return None
    start = index[entry]
    last_i = len(sessions) - 1
    symbol = episode["symbol"]
    series = bars.get(symbol, {})
    opened = series.get(sessions[start])
    if opened is None or opened.open <= 0:
        return None
    entry_price = opened.open
    planned = start + hold - 1
    capped = planned > last_i
    planned = min(planned, last_i)
    cost = cost_bps / 10000.0
    last = entry_price
    sell_at_open = False
    sell_index = None
    exit_reason = "sample_end" if capped else "hold"
    i = start
    while i <= last_i:
        day = sessions[i]
        if sell_at_open and sell_index is not None and sell_index <= i:
            bar = series.get(day)
            if bar is None or bar.open <= 0:
                if i + 1 > last_i:
                    exit_price = last
                    reason = "sample_end"
                    break
                sell_index = i + 1
                i += 1
                continue
            exit_price = bar.open
            reason = exit_reason
            break
        bar = series.get(day)
        if bar is None or bar.close <= 0:
            if i + 1 > last_i:
                exit_price = last
                reason = "sample_end"
                break
            sell_at_open = True
            sell_index = i + 1
            exit_reason = "missing_bar"
            i += 1
            continue
        last = bar.close
        if planned == i:
            exit_price = bar.close
            reason = "sample_end" if capped else "hold"
            break
        i += 1
    else:
        raise RuntimeError(f"no exit for {symbol} {entry} hold {hold}")
    ratio = exit_price / entry_price
    gross = ratio - 1.0
    net = gross - cost - ratio * cost
    return {
        "symbol": symbol,
        "session": entry,
        "gross": gross,
        "net": net,
        "exit_reason": reason,
    }


def _window_names(rows, predicate) -> dict:
    chosen = [row for row in rows if predicate(row["session"])]
    nets = [row["net"] for row in chosen]
    gross = [row["gross"] for row in chosen]
    largest = max(chosen, key=lambda row: row["net"]) if chosen else None
    without = [row["net"] for row in chosen if row is not largest]
    return {
        "trades": len(chosen),
        "mean_net": (sum(nets) / len(nets) if nets else None),
        "median_net": (statistics.median(nets) if nets else None),
        "mean_gross": (sum(gross) / len(gross) if gross else None),
        "avg_net_bp": avg_net_bp(nets),
        "median_net_bp": (statistics.median(nets) * 1e4 if nets else None),
        "mean_net_bp_without_largest": (sum(without) / len(without) * 1e4 if without else None),
        "win_rate": win_rate(nets),
        "profit_factor": profit_factor(nets),
        "largest": (
            None if largest is None else {
                "symbol": largest["symbol"],
                "session": largest["session"].isoformat(),
                "net": largest["net"],
                "gross": largest["gross"],
                "exit_reason": largest["exit_reason"],
            }
        ),
    }


def name_summary(rows, is_end: date, oos_start: date) -> dict:
    return {
        "full": _window_names(rows, lambda day: True),
        "is": _window_names(rows, lambda day: day <= is_end),
        "oos": _window_names(rows, lambda day: day >= oos_start),
    }


def pack(window: dict) -> dict:
    keys = (
        "total_return",
        "sharpe",
        "max_dd",
        "trades",
        "profit_factor",
        "avg_net_bp",
        "win_rate",
        "exposure",
    )
    return {key: window[key] for key in keys}


def _close(left, right) -> bool:
    if left is None or right is None:
        return left is None and right is None
    if isinstance(left, int) and isinstance(right, int):
        return left == right
    return abs(float(left) - float(right)) <= 1e-9


def check_against_engine(engine, sessions, bars) -> None:
    d1, d2, d3, d4 = date(2024, 6, 3), date(2024, 6, 4), date(2024, 6, 5), date(2024, 6, 6)
    book = [d1, d2, d3, d4]

    def bar(open_, close):
        return type("B", (), {"open": open_, "close": close})()

    gap = {"AAA": {d1: bar(100.0, 110.0), d3: bar(90.0, 90.0), d4: bar(90.0, 90.0)}}
    one = [{"symbol": "AAA", "signal": date(2024, 5, 31), "entry": d1, "source": "nasdaq"}]
    got = name_outcome(book, gap, one[0], 3, 10.0)
    path = engine.run_book(book, gap, one, hold=3, cost_bps=10)
    if got is None or abs(got["net"] - path["trades"][0]["net"]) > 1e-12:
        raise AssertionError((got, path["trades"][0]))
    if got["exit_reason"] != "missing_bar" or got["net"] != path["trades"][0]["net"]:
        raise AssertionError(got)

    gap2 = {"AAA": {d1: bar(100.0, 110.0), d4: bar(80.0, 80.0)}}
    got = name_outcome(book, gap2, one[0], 3, 0.0)
    path = engine.run_book(book, gap2, one, hold=3, cost_bps=0)
    if abs(got["net"] - (80.0 / 100.0 - 1.0)) > 1e-12 or got["exit_reason"] != "missing_bar":
        raise AssertionError(got)
    if abs(got["net"] - path["trades"][0]["net"]) > 1e-12:
        raise AssertionError((got, path["trades"][0]["net"]))


def check_sample(engine, sessions, bars, episodes) -> None:
    for hold in (1, 2, 5, 15, 31):
        for episode in episodes[:80]:
            got = name_outcome(sessions, bars, episode, hold, COST_BPS)
            path = engine.run_book(sessions, bars, [episode], hold=hold, cost_bps=COST_BPS)
            trades = path["trades"]
            if got is None:
                if trades:
                    raise AssertionError((hold, episode, trades[0]))
                continue
            if len(trades) != 1 or abs(trades[0]["net"] - got["net"]) > 1e-9:
                raise AssertionError((hold, episode["symbol"], got, trades[:1]))
            if trades[0]["exit_reason"] != got["exit_reason"]:
                raise AssertionError((hold, got, trades[0]["exit_reason"]))


def match_locked_grid(portfolio_rows: list[dict]) -> bool:
    locked = json.loads((HERE / "results.json").read_text(encoding="utf-8"))["grid"]
    by_hold = {row["hold"]: row["portfolio"] for row in portfolio_rows}
    ok = True
    for row in locked:
        hold = row["params"]["hold"]
        got = by_hold[hold]
        pairs = {
            "is_sharpe": got["is"]["sharpe"],
            "oos_sharpe": got["oos"]["sharpe"],
            "full_return": got["full"]["total_return"],
            "is_return": got["is"]["total_return"],
            "oos_return": got["oos"]["total_return"],
            "oos_trades": got["oos"]["trades"],
            "full_profit_factor": got["full"]["profit_factor"],
        }
        for key, value in pairs.items():
            if not _close(row[key], value):
                print(f"grid mismatch hold {hold} {key}: locked {row[key]} sweep {value}")
                ok = False
    return ok


def _best(rows, value_of):
    chosen = None
    for row in rows:
        value = value_of(row)
        if value is None:
            continue
        if chosen is None or value > chosen[1]:
            chosen = (row["hold"], value)
    if chosen is None:
        return None
    return {"hold": chosen[0], "value": chosen[1]}


def draw(rows: list[dict], path: Path) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    holds = [row["hold"] for row in rows]
    sharpes = [row["portfolio"]["oos"]["sharpe"] for row in rows]
    medians = [row["name_level"]["oos"]["median_net_bp"] for row in rows]
    fig, axes = plt.subplots(2, 1, figsize=(8, 6.4), sharex=True)
    axes[0].bar(holds, sharpes, color="#1f4e79", width=0.8)
    axes[0].axhline(0, color="#666666", linewidth=1)
    axes[0].set_ylabel("OOS account Sharpe")
    axes[0].set_title("Post hoc hold sweep, 1 to 31 sessions")
    axes[1].bar(holds, medians, color="#8c4a2f", width=0.8)
    axes[1].axhline(0, color="#666666", linewidth=1)
    axes[1].set_ylabel("OOS median name net (bp)")
    axes[1].set_xlabel("Hold in NYSE sessions")
    axes[1].set_xticks(list(range(1, 32, 2)))
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, format="svg")
    plt.close(fig)


def write_csv(rows: list[dict], path: Path) -> None:
    fields = [
        "hold",
        "portfolio_oos_return",
        "portfolio_oos_sharpe",
        "portfolio_oos_trades",
        "portfolio_oos_avg_net_bp",
        "portfolio_full_return",
        "portfolio_full_sharpe",
        "portfolio_full_trades",
        "portfolio_is_sharpe",
        "name_oos_avg_net_bp",
        "name_oos_median_net_bp",
        "name_oos_mean_bp_without_largest",
        "name_oos_largest_symbol",
        "name_oos_largest_net",
        "name_oos_mean_gross",
        "name_oos_trades",
        "name_oos_win_rate",
        "name_full_avg_net_bp",
        "name_full_median_net_bp",
        "name_full_trades",
        "name_is_avg_net_bp",
        "portfolio_oos_max_trade_symbol",
        "portfolio_oos_max_trade_net",
        "portfolio_oos_max_day",
        "portfolio_oos_max_day_return",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            portfolio = row["portfolio"]
            names = row["name_level"]
            writer.writerow({
                "hold": row["hold"],
                "portfolio_oos_return": portfolio["oos"]["total_return"],
                "portfolio_oos_sharpe": portfolio["oos"]["sharpe"],
                "portfolio_oos_trades": portfolio["oos"]["trades"],
                "portfolio_oos_avg_net_bp": portfolio["oos"]["avg_net_bp"],
                "portfolio_full_return": portfolio["full"]["total_return"],
                "portfolio_full_sharpe": portfolio["full"]["sharpe"],
                "portfolio_full_trades": portfolio["full"]["trades"],
                "portfolio_is_sharpe": portfolio["is"]["sharpe"],
                "name_oos_avg_net_bp": names["oos"]["avg_net_bp"],
                "name_oos_median_net_bp": names["oos"]["median_net_bp"],
                "name_oos_mean_bp_without_largest": names["oos"]["mean_net_bp_without_largest"],
                "name_oos_largest_symbol": None if names["oos"]["largest"] is None else names["oos"]["largest"]["symbol"],
                "name_oos_largest_net": None if names["oos"]["largest"] is None else names["oos"]["largest"]["net"],
                "name_oos_mean_gross": names["oos"]["mean_gross"],
                "name_oos_trades": names["oos"]["trades"],
                "name_oos_win_rate": names["oos"]["win_rate"],
                "name_full_avg_net_bp": names["full"]["avg_net_bp"],
                "name_full_median_net_bp": names["full"]["median_net_bp"],
                "name_full_trades": names["full"]["trades"],
                "name_is_avg_net_bp": names["is"]["avg_net_bp"],
                "portfolio_oos_max_trade_symbol": portfolio["oos_max_trade"]["symbol"],
                "portfolio_oos_max_trade_net": portfolio["oos_max_trade"]["net"],
                "portfolio_oos_max_day": portfolio["oos_max_day"]["session"],
                "portfolio_oos_max_day_return": portfolio["oos_max_day"]["strategy_net"],
            })


def append_log(sha: str, matched: bool) -> None:
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    entry = (
        f"\n## {stamp}\n\n"
        "- Reason: post hoc hold sweep for holds 1 through 31, requested after the verdict. "
        "The locked primary stays hold 1. results.json was not rewritten. "
        "Appended by hold_sweep.py.\n"
        f"- Rules sha256: `{sha}`\n"
        "- Output: `hold_sweep.json` and `hold_sweep.csv`. Label: post hoc.\n"
        f"- Locked grid holds 1, 2, 3, and 5 match results.json: {matched}\n"
    )
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(entry)


def main() -> None:
    engine = load_engine()
    check_against_engine(engine, None, None)
    with engine.MarketData() as md:
        sessions, bars, episodes, _benchmark, dropped = engine._load(md)
    check_sample(engine, sessions, bars, episodes)

    rows = []
    for hold in HOLDS:
        path = engine.run_book(sessions, bars, episodes, hold=hold, cost_bps=COST_BPS)
        perf = engine.performance(
            sessions,
            path["net"],
            path["trades"],
            is_end=engine.IS_END,
            oos_start=engine.OOS_START,
            model="compound",
            held=path["held"],
        )
        names = [
            name_outcome(sessions, bars, episode, hold, COST_BPS)
            for episode in episodes
        ]
        names = [row for row in names if row is not None]
        oos_trades = [
            trade for trade in path["trades"]
            if trade["session"] >= engine.OOS_START
        ]
        largest_trade = max(oos_trades, key=lambda trade: trade["net"]) if oos_trades else None
        oos_days = [
            (day, value)
            for day, value in zip(sessions, path["net"])
            if day >= engine.OOS_START
        ]
        largest_day = max(oos_days, key=lambda item: item[1]) if oos_days else None
        rows.append({
            "hold": hold,
            "portfolio": {
                "cohorts": sum(1 for cohort in path["cohorts"] if cohort["sessions"]),
                "full": pack(perf["full"]),
                "is": pack(perf["is"]),
                "oos": pack(perf["oos"]),
                "oos_max_trade": None if largest_trade is None else {
                    "symbol": largest_trade["symbol"],
                    "session": largest_trade["session"].isoformat(),
                    "net": largest_trade["net"],
                    "weight": largest_trade["weight"],
                    "exit_reason": largest_trade["exit_reason"],
                },
                "oos_max_day": None if largest_day is None else {
                    "session": largest_day[0].isoformat(),
                    "strategy_net": largest_day[1],
                },
            },
            "name_level": name_summary(names, engine.IS_END, engine.OOS_START),
        })
        print(
            f"hold {hold:2d}  "
            f"OOS return {perf['oos']['total_return']:+.4f}  "
            f"Sharpe {perf['oos']['sharpe']}  "
            f"trades {perf['oos']['trades']}  "
            f"name OOS bp {rows[-1]['name_level']['oos']['avg_net_bp']}"
        )

    matched = match_locked_grid(rows)
    if not matched:
        raise SystemExit("sweep does not reproduce the locked grid")

    positive_account = [
        row["hold"] for row in rows
        if row["portfolio"]["oos"]["sharpe"] is not None and row["portfolio"]["oos"]["sharpe"] > 0
    ]
    positive_names = [
        row["hold"] for row in rows
        if row["name_level"]["oos"]["avg_net_bp"] is not None and row["name_level"]["oos"]["avg_net_bp"] > 0
    ]
    payload = {
        "label": "post hoc",
        "changes_verdict": False,
        "primary_hold": 1,
        "cost_bps_per_side": COST_BPS,
        "holds": list(HOLDS),
        "dropped_spy_sessions": dropped,
        "episodes_loaded": len(episodes),
        "portfolio_rule": (
            "Locked RULES.md book: equal weight, one cohort at a time, "
            "10 bp per side, next-open entry. A new entry session is skipped "
            "while any share is open."
        ),
        "name_level_rule": (
            "Every episode with a fill bar, held on its own at weight 1, "
            "same exit and cost. Overlapping episodes are each counted. "
            "The mean is not an account path."
        ),
        "matches_locked_grid": matched,
        "portfolio_oos_sharpe_positive_holds": positive_account,
        "name_oos_mean_positive_holds": positive_names,
        "highest_portfolio_oos_sharpe": _best(rows, lambda row: row["portfolio"]["oos"]["sharpe"]),
        "highest_portfolio_oos_return": _best(rows, lambda row: row["portfolio"]["oos"]["total_return"]),
        "highest_name_oos_avg_net_bp": _best(rows, lambda row: row["name_level"]["oos"]["avg_net_bp"]),
        "rows": rows,
    }
    (HERE / "hold_sweep.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    write_csv(rows, HERE / "hold_sweep.csv")
    draw(rows, HERE.parent / "report" / "figures" / "hold_sweep.svg")
    lock_line = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()[0]
    sha = lock_line.split()[1]
    append_log(sha, matched)
    print(f"matched locked grid: {matched}")
    print(f"positive OOS account Sharpe holds: {positive_account}")
    print(f"positive OOS name-mean holds: {positive_names}")


if __name__ == "__main__":
    main()
