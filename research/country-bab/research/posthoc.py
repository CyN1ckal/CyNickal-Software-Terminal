# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post-hoc checks for country-bab.

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


def sharpe(rets: np.ndarray) -> float | None:
    r = np.asarray(rets, dtype=float)
    if len(r) < 2:
        return None
    sd = float(r.std(ddof=1))
    if sd == 0.0:
        return None
    return float(r.mean()) / sd * math.sqrt(252.0)


def total_return(rets: np.ndarray) -> float:
    r = np.asarray(rets, dtype=float)
    if len(r) == 0:
        return 0.0
    return float(np.prod(1.0 + r) - 1.0)


def zero_tails(rets: np.ndarray, k: int) -> np.ndarray:
    out = np.asarray(rets, dtype=float).copy()
    order = np.argsort(out, kind="mergesort")
    out[order[:k]] = 0.0
    out[order[-k:]] = 0.0
    return out


def side_block(rows: list[dict]) -> dict:
    out = {}
    for side in ("long", "short"):
        picked = [r for r in rows if r["side"] == side]
        nets = [float(r["net_pnl"]) for r in picked]
        out[side] = {
            "trades": len(picked),
            "net_pnl": float(sum(nets)),
            "gross_pnl": float(sum(float(r["gross_pnl"]) for r in picked)),
            "wins": sum(1 for n in nets if n > 0.0),
            "win_rate": (sum(1 for n in nets if n > 0.0) / len(picked)) if picked else None,
        }
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
    oos = np.array([d >= OOS for d in dates])

    window = 252
    roll_s = [sharpe(strat[i : i + window]) for i in range(len(strat) - window + 1)]
    roll_b = [sharpe(spy[i : i + window]) for i in range(len(spy) - window + 1)]
    roll_e = [sharpe(ew[i : i + window]) for i in range(len(ew) - window + 1)]
    roll_end = dates[window - 1 :]
    roll_s_a = np.asarray(roll_s, dtype=float)
    roll_b_a = np.asarray(roll_b, dtype=float)
    roll_e_a = np.asarray(roll_e, dtype=float)
    imin = int(np.argmin(roll_s_a))
    imax = int(np.argmax(roll_s_a))

    tail = slice(-window, None)
    concentration = {}
    for k in (10, 20):
        full_z = zero_tails(strat, k)
        oos_z = zero_tails(strat[oos], k)
        concentration[str(k)] = {
            "k": k,
            "method": "set the k highest and k lowest strategy_net days to 0 and keep the day in the sample",
            "full": {"sharpe": sharpe(full_z), "total_return": total_return(full_z)},
            "oos": {"sharpe": sharpe(oos_z), "total_return": total_return(oos_z)},
        }

    oos_rows = [tr for tr in trades if tr["entry_date"] >= OOS]
    carried = [tr for tr in trades if tr["entry_date"] < OOS <= tr["exit_date"]]

    def trip_row(tr: dict) -> dict:
        return {
            "symbol": tr["symbol"],
            "side": tr["side"],
            "entry_date": tr["entry_date"],
            "exit_date": tr["exit_date"],
            "exit_reason": tr["exit_reason"],
            "holding_sessions": int(tr["holding_sessions"]),
            "net_pnl": float(tr["net_pnl"]),
            "gross_pnl": float(tr["gross_pnl"]),
            "note": "full-trip net_pnl, not an out-of-sample slice of the daily path",
        }

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
            "ew": roll_e,
            "n": len(roll_s),
            "share_positive_strategy": float(np.mean(roll_s_a > 0.0)),
            "min_strategy": float(roll_s_a[imin]),
            "min_strategy_end": roll_end[imin],
            "max_strategy": float(roll_s_a[imax]),
            "max_strategy_end": roll_end[imax],
            "last_strategy": float(roll_s_a[-1]),
            "share_positive_spy": float(np.mean(roll_b_a > 0.0)),
            "min_spy": float(np.min(roll_b_a)),
            "max_spy": float(np.max(roll_b_a)),
            "last_spy": float(roll_b_a[-1]),
            "share_positive_ew": float(np.mean(roll_e_a > 0.0)),
            "min_ew": float(np.min(roll_e_a)),
            "max_ew": float(np.max(roll_e_a)),
            "last_ew": float(roll_e_a[-1]),
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
        "concentration": concentration,
        "oos_entry_by_side": side_block(oos_rows),
        "oos_entry_trips": [trip_row(tr) for tr in oos_rows],
        "carried_into_oos": [trip_row(tr) for tr in carried],
        "trades_per_year": trades_per_year,
        "trades_per_year_inputs": {"trades": n_trades, "sessions": n_sessions, "sessions_per_year": 252},
        "rebalance_sessions": int(sum(int(float(r["rebalance"])) for r in daily)),
        "terminal_equity": {
            "strategy": float(equity[-1]),
            "spy": float(daily[-1]["spy_equity"]),
            "ew": float(daily[-1]["ew_equity"]),
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    brief = {
        "trailing_252": out["trailing_252"],
        "concentration": out["concentration"],
        "oos_entry_by_side": out["oos_entry_by_side"],
        "oos_entry_count": len(out["oos_entry_trips"]),
        "carried_into_oos": out["carried_into_oos"],
        "trades_per_year": out["trades_per_year"],
        "rebalance_sessions": out["rebalance_sessions"],
        "terminal_equity": out["terminal_equity"],
        "rolling_summary": {
            k: out["rolling_sharpe"][k]
            for k in (
                "n",
                "share_positive_strategy",
                "min_strategy",
                "min_strategy_end",
                "max_strategy",
                "max_strategy_end",
                "last_strategy",
                "share_positive_spy",
                "last_spy",
                "share_positive_ew",
                "last_ew",
            )
        },
    }
    print(json.dumps(brief, indent=2))


if __name__ == "__main__":
    main()
