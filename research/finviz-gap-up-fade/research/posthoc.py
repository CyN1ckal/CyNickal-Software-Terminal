# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Analyses added after the locked run. None of these enter the verdict.

    python research/finviz-gap-up-fade/research/posthoc.py
"""

from __future__ import annotations

import csv
import json
import math
import sys
from collections import defaultdict
from datetime import date
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData  # noqa: E402

HERE = Path(__file__).resolve().parent
ANNUAL = 252


def sharpe(r: np.ndarray) -> float:
    if len(r) < 2:
        return float("nan")
    sd = float(r.std(ddof=1))
    if sd <= 0:
        return float("nan")
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def total_return(r: np.ndarray) -> float:
    return float(np.prod(1.0 + r) - 1.0) if len(r) else float("nan")


def pack(r: np.ndarray) -> dict:
    return {"sharpe": sharpe(r), "total_return": total_return(r), "sessions": int(len(r))}


def zero_best(r: np.ndarray, n: int) -> np.ndarray:
    out = r.copy()
    if n <= 0 or len(out) == 0:
        return out
    idx = np.argsort(out)[-n:]
    out[idx] = 0.0
    return out


def main() -> None:
    daily = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    trades = list(csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")))
    ret = np.array([float(row["ret"]) for row in daily])
    sessions = [date.fromisoformat(row["session"]) for row in daily]
    oos = np.array([s >= date(2024, 1, 2) for s in sessions])

    eq = np.cumprod(1.0 + ret)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    dd = eq / peak - 1.0
    trough_i = int(np.argmin(dd))
    peak_i = int(np.argmax(eq[:trough_i + 1])) if trough_i >= 0 else 0

    roll = []
    for i in range(ANNUAL - 1, len(ret)):
        window = ret[i - ANNUAL + 1:i + 1]
        roll.append({"session": sessions[i].isoformat(), "sharpe": sharpe(window)})
    roll_s = np.array([row["sharpe"] for row in roll])

    by_day: dict[str, list[dict]] = defaultdict(list)
    for t in trades:
        by_day[t["entry_session"]].append(t)
    counts = [len(v) for v in by_day.values()]

    symbols = sorted({t["symbol"] for t in trades})
    volume: dict[tuple[str, str], float] = {}
    with MarketData() as md:
        for sym in symbols:
            for bar in md.bars(sym, "1d", start="2016-01-04", end="2026-09-25"):
                volume[(sym, bar.session.isoformat())] = bar.open * bar.volume

    enriched = []
    for t in trades:
        dv = volume.get((t["symbol"], t["entry_session"]))
        enriched.append({
            "symbol": t["symbol"], "session": t["entry_session"], "sample": t["sample"],
            "gap": float(t["gap"]), "gross_ret": float(t["gross_ret"]),
            "dollar_volume": None if dv is None else float(dv),
        })
    enriched.sort(key=lambda row: row["gross_ret"], reverse=True)

    def dust_mask(threshold: float) -> np.ndarray:
        bad_days = set()
        for day, rows in by_day.items():
            dvs = [volume.get((row["symbol"], day)) for row in rows]
            if any(dv is not None and dv < threshold for dv in dvs):
                bad_days.add(day)
        return np.array([s.isoformat() in bad_days for s in sessions])

    dust = {}
    for threshold in (25_000, 100_000, 250_000):
        mask = dust_mask(threshold)
        kept = ret.copy()
        kept[mask] = 0.0
        dust[str(threshold)] = {
            "days_zeroed": int(mask.sum()),
            "oos_days_zeroed": int((mask & oos).sum()),
            "full": pack(kept),
            "OOS": pack(kept[oos]),
        }

    by_symbol: dict[str, list[float]] = defaultdict(list)
    for row in enriched:
        by_symbol[row["symbol"]].append(row["gross_ret"])
    symbol_rows = []
    gross_sum = float(sum(row["gross_ret"] for row in enriched))
    for sym, vals in by_symbol.items():
        symbol_rows.append({
            "symbol": sym, "trades": len(vals), "mean_gross": float(np.mean(vals)),
            "sum_gross": float(np.sum(vals)),
            "share_of_sum_gross": float(np.sum(vals) / gross_sum) if gross_sum else None,
        })
    symbol_rows.sort(key=lambda row: row["sum_gross"], reverse=True)

    order = np.argsort(ret)
    best10 = order[-10:]
    # A session at -100% makes log growth undefined. Leave the share blank.
    if np.any(ret <= -1.0):
        best10_share = None
    else:
        log_growth = np.log1p(ret)
        total = float(log_growth.sum())
        best10_share = None if total == 0.0 or not np.isfinite(total) else float(log_growth[best10].sum() / total)

    out = {
        "label": "post hoc",
        "rolling_sharpe": roll,
        "rolling_sharpe_min": float(np.nanmin(roll_s)) if len(roll_s) else None,
        "rolling_sharpe_max": float(np.nanmax(roll_s)) if len(roll_s) else None,
        "rolling_sharpe_last": float(roll_s[-1]) if len(roll_s) else None,
        "max_drawdown": {
            "depth": float(dd[trough_i]),
            "peak_session": sessions[peak_i].isoformat(),
            "trough_session": sessions[trough_i].isoformat(),
            "peak_equity": float(peak[trough_i]),
            "trough_equity": float(eq[trough_i]),
        },
        "signal_days": len(counts),
        "single_name_days": int(sum(v == 1 for v in counts)),
        "max_names_in_a_day": int(max(counts) if counts else 0),
        "drop_best_days": {
            "full_drop_10": pack(zero_best(ret, 10)),
            "full_drop_20": pack(zero_best(ret, 20)),
            "oos_drop_10_of_oos": pack(zero_best(ret[oos], 10)),
            "oos_after_dropping_10_best_full_sample_days": pack(zero_best(ret, 10)[oos]),
        },
        "best_10_days_share_of_log_growth": best10_share,
        "best_10_sessions": [sessions[i].isoformat() for i in sorted(best10)],
        "largest_gross_trades": enriched[:15],
        "dust_days_zeroed": dust,
        "by_symbol_gross": symbol_rows[:10],
        "trades_with_dollar_volume_under": {
            "25000": int(sum(1 for row in enriched if row["dollar_volume"] is not None and row["dollar_volume"] < 25_000)),
            "100000": int(sum(1 for row in enriched if row["dollar_volume"] is not None and row["dollar_volume"] < 100_000)),
            "250000": int(sum(1 for row in enriched if row["dollar_volume"] is not None and row["dollar_volume"] < 250_000)),
            "missing": int(sum(1 for row in enriched if row["dollar_volume"] is None)),
        },
    }
    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(
        f"posthoc wrote rolling {len(roll)} "
        f"dd {out['max_drawdown']['depth']:.3f} "
        f"{out['max_drawdown']['peak_session']} -> {out['max_drawdown']['trough_session']} "
        f"single {out['single_name_days']}/{out['signal_days']} "
        f"dust100k days {dust['100000']['days_zeroed']} "
        f"full sharpe after {dust['100000']['full']['sharpe']:.3f}"
    )


if __name__ == "__main__":
    main()
