# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc checks for spdr-sector-momentum.

Reads results.json, daily.csv, and trades.csv. Does not open the store.
Nothing here changes the verdict. Writes posthoc.json.
"""
from __future__ import annotations

import csv
import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
OOS = "2024-07-01"


def sharpe(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return 0.0
    sd = float(r.std(ddof=1))
    mu = float(r.mean())
    if sd == 0.0:
        return 0.0 if mu == 0.0 else float("nan")
    return mu / sd * math.sqrt(252.0)


def total_return(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) == 0:
        return 0.0
    return float(np.prod(1.0 + r) - 1.0)


def zero_tails(rets: np.ndarray, k: int) -> np.ndarray:
    out = np.asarray(rets, dtype=float).copy()
    order = np.argsort(out)
    out[order[:k]] = 0.0
    out[order[-k:]] = 0.0
    return out


def main() -> None:
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
        daily = list(csv.DictReader(f))
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as f:
        trades = list(csv.DictReader(f))

    dates = [r["date"] for r in daily]
    strat = np.array([float(r["strategy_net"]) for r in daily])
    spy = np.array([float(r["spy"]) for r in daily])
    ew = np.array([float(r["ew"]) for r in daily])
    equity = np.array([float(r["equity"]) for r in daily])
    long_pnl = np.array([float(r["long_pnl"]) for r in daily])
    short_pnl = np.array([float(r["short_pnl"]) for r in daily])
    oos = np.array([d >= OOS for d in dates])

    window = 252
    roll_s = [sharpe(strat[i : i + window]) for i in range(len(strat) - window + 1)]
    roll_b = [sharpe(spy[i : i + window]) for i in range(len(spy) - window + 1)]
    roll_end = dates[window - 1 :]
    roll_s_a = np.asarray(roll_s, dtype=float)
    roll_b_a = np.asarray(roll_b, dtype=float)
    imin = int(np.argmin(roll_s_a))
    imax = int(np.argmax(roll_s_a))

    tail = slice(-window, None)
    k = 20
    full_z = zero_tails(strat, k)
    oos_z = zero_tails(strat[oos], k)

    by_symbol: dict[str, dict] = {}
    for tr in trades:
        row = by_symbol.setdefault(
            tr["symbol"],
            {"symbol": tr["symbol"], "trades": 0, "long": 0, "short": 0, "net_pnl": 0.0, "gross_pnl": 0.0},
        )
        row["trades"] += 1
        row[tr["side"]] += 1
        row["net_pnl"] += float(tr["net_pnl"])
        row["gross_pnl"] += float(tr["gross_pnl"])
    symbols = sorted(by_symbol.values(), key=lambda r: r["net_pnl"])

    years: dict[str, dict] = {}
    for i, d in enumerate(dates):
        y = d[:4]
        bucket = years.setdefault(y, {"year": int(y), "sessions": 0, "long_pnl": 0.0, "short_pnl": 0.0})
        bucket["sessions"] += 1
        bucket["long_pnl"] += float(long_pnl[i])
        bucket["short_pnl"] += float(short_pnl[i])
    year_rows = [years[y] for y in sorted(years)]
    long_sum = float(long_pnl.sum())
    short_sum = float(short_pnl.sum())
    match = abs(long_sum - res["long_gross_pnl"]) < 1e-6 and abs(short_sum - res["short_gross_pnl"]) < 1e-6

    last_2021 = max(i for i, d in enumerate(dates) if d.startswith("2021"))
    last_2022 = max(i for i, d in enumerate(dates) if d.startswith("2022"))
    dollar_2022 = float(equity[last_2022] - equity[last_2021])
    dollar_full = float(equity[-1] - 1.0)

    oos_exit: dict[str, int] = {}
    for tr in trades:
        if tr["entry_date"] >= OOS:
            oos_exit[tr["exit_reason"]] = oos_exit.get(tr["exit_reason"], 0) + 1

    n_trades = int(res["strategy"]["full"]["trades"])
    n_sessions = int(res["strategy"]["full"]["sessions"])
    trades_per_year = n_trades / (n_sessions / 252.0)

    out = {
        "note": "post hoc; does not feed the verdict",
        "rules_sha256": res["rules_sha256"],
        "rolling_sharpe": {
            "window": window,
            "end_dates": roll_end,
            "strategy": roll_s,
            "spy": roll_b,
            "n": len(roll_s),
            "share_positive_strategy": float(np.mean(roll_s_a > 0.0)),
            "min_strategy": float(roll_s_a[imin]),
            "min_strategy_end": roll_end[imin],
            "max_strategy": float(roll_s_a[imax]),
            "max_strategy_end": roll_end[imax],
            "last_strategy": float(roll_s_a[-1]),
            "share_positive_spy": float(np.mean(roll_b_a > 0.0)),
            "min_spy": float(roll_b_a.min()),
            "max_spy": float(roll_b_a.max()),
            "last_spy": float(roll_b_a[-1]),
        },
        "trailing_252": {
            "start": dates[-window],
            "end": dates[-1],
            "sessions": window,
            "strategy_return": total_return(strat[tail]),
            "strategy_sharpe": sharpe(strat[tail]),
            "spy_return": total_return(spy[tail]),
            "spy_sharpe": sharpe(spy[tail]),
            "ew_return": total_return(ew[tail]),
            "ew_sharpe": sharpe(ew[tail]),
        },
        "concentration": {
            "k": k,
            "method": "set the k highest and k lowest strategy_net days to 0 and keep the day in the sample",
            "full": {"sharpe": sharpe(full_z), "total_return": total_return(full_z)},
            "oos": {"sharpe": sharpe(oos_z), "total_return": total_return(oos_z)},
        },
        "correlation_spy": {
            "full": float(np.corrcoef(strat, spy)[0, 1]),
            "oos": float(np.corrcoef(strat[oos], spy[oos])[0, 1]),
        },
        "by_symbol": symbols,
        "long_short_by_year": year_rows,
        "long_pnl_sum": long_sum,
        "short_pnl_sum": short_sum,
        "long_short_sums_match_results": match,
        "trades_per_year": trades_per_year,
        "trades_per_year_inputs": {"trades": n_trades, "sessions": n_sessions, "sessions_per_year": 252},
        "rebalance_sessions": int(sum(int(r["rebalance"]) for r in daily)),
        "terminal_equity": {
            "strategy": float(equity[-1]),
            "spy": float(daily[-1]["spy_equity"]),
            "ew": float(daily[-1]["ew_equity"]),
        },
        "year_2022": {
            "equity_2021_12_31": float(equity[last_2021]),
            "date_2021_end": dates[last_2021],
            "equity_2022_end": float(equity[last_2022]),
            "date_2022_end": dates[last_2022],
            "dollar_change": dollar_2022,
            "full_dollar_change": dollar_full,
            "share_of_full_dollar_change": dollar_2022 / dollar_full,
        },
        "oos_trades_by_exit": oos_exit,
        "first_day": {
            "date": daily[0]["date"],
            "strategy_net": float(daily[0]["strategy_net"]),
            "cost": float(daily[0]["cost"]),
            "n_long": int(daily[0]["n_long"]),
            "n_short": int(daily[0]["n_short"]),
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    brief = {
        k: out[k]
        for k in (
            "trailing_252",
            "concentration",
            "correlation_spy",
            "trades_per_year",
            "rebalance_sessions",
            "terminal_equity",
            "year_2022",
            "oos_trades_by_exit",
            "long_short_sums_match_results",
            "by_symbol",
        )
    }
    brief["rolling_summary"] = {
        k: out["rolling_sharpe"][k]
        for k in (
            "n",
            "share_positive_strategy",
            "min_strategy",
            "min_strategy_end",
            "max_strategy",
            "max_strategy_end",
            "last_strategy",
            "last_spy",
        )
    }
    print(json.dumps(brief, indent=2))


if __name__ == "__main__":
    main()
