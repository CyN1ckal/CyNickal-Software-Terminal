# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Placebo, bootstrap, and the pre-registered acceptance lines.

The study still builds the gross pieces and re-simulates the cost and grid
rows. This module flips signs, resamples the daily path, and scores the lines.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np

from research.kit.metrics import as_date, as_dates, sharpe

PLACEBO_FILE = "placebo_direction.npy"
TIMING_FILE = "placebo_timing.npy"


class _Sentinel:
    def __repr__(self) -> str:
        return "NO_CROSS_MARKET"


NO_CROSS_MARKET = _Sentinel()


@dataclass
class Thresholds:
    oos_sharpe_min: float = 0.5
    oos_profit_factor_min: float = 1.10
    placebo_p_max: float = 0.05
    is_sharpe_min_exclusive: float = 0.0
    grid_positive_fraction: float = 0.60
    full_return_2x_min_exclusive: float = 0.0
    cross_market_oos_sharpe_min_exclusive: float = 0.0
    min_oos_trades: int = 100


@dataclass
class Criterion:
    """One extra acceptance line. `passed` is computed from op and threshold."""

    name: str
    required: str
    actual: float
    op: str
    threshold: float


def pvalue(actual: float, draws) -> float:
    """(1 + count of draws >= actual) / (len(draws) + 1)."""
    series = np.asarray(list(draws), dtype=float)
    if len(series) == 0:
        raise ValueError("pvalue needs at least one draw")
    count = int(np.sum(series >= actual))
    return (1 + count) / (len(series) + 1)


def _finite(series: np.ndarray) -> np.ndarray:
    return series[np.isfinite(series)]


def direction_placebo(sessions, pieces, *, seed: int, draws: int = 2000) -> dict:
    """Flip each trade's gross pieces by one sign and recompute gross Sharpe.

    `pieces` is a list of trades. Each trade is a list of (session, gross simple
    return). Every piece of a trade gets the same sign. Days with no piece are 0.
    """
    dates = as_dates(sessions)
    index = {day: i for i, day in enumerate(dates)}
    trades = []
    for trade in pieces:
        rows = []
        for session, gross in trade:
            day = as_date(session)
            if day not in index:
                raise ValueError(f"placebo piece session {day.isoformat()} is not in the daily path")
            rows.append((index[day], float(gross)))
        trades.append(rows)

    def path(signs: np.ndarray | None) -> np.ndarray:
        out = np.zeros(len(dates), dtype=float)
        for trade_i, rows in enumerate(trades):
            sign = 1.0 if signs is None else float(signs[trade_i])
            for bar, gross in rows:
                out[bar] += sign * gross
        return out

    actual = sharpe(path(None))
    if actual is None:
        raise ValueError("direction placebo actual gross Sharpe is undefined")
    generator = np.random.default_rng(seed)
    samples = np.empty(draws, dtype=float)
    for draw in range(draws):
        signs = generator.choice(np.array([-1.0, 1.0]), size=len(trades))
        value = sharpe(path(signs))
        samples[draw] = np.nan if value is None else value
    finite = _finite(samples)
    if len(finite) == 0:
        raise ValueError("direction placebo produced no finite Sharpe draws")
    return {
        "actual_gross_sharpe": actual,
        "null_mean": float(finite.mean()),
        "null_p95": float(np.percentile(finite, 95)),
        "p": pvalue(actual, samples),
        "draws": int(draws),
        "seed": int(seed),
        "samples": samples,
        "samples_file": PLACEBO_FILE,
    }


def placebo_summary(result: dict) -> dict:
    """Drop the ndarray. Keep samples_file for the chart."""
    summary = {key: value for key, value in result.items() if key != "samples"}
    if "samples" in result and "samples_file" not in summary:
        summary["samples_file"] = PLACEBO_FILE
    return summary


def save_samples(path, samples) -> None:
    np.save(path, np.asarray(samples, dtype=float))


def block_bootstrap(daily_net, *, seed: int, block: int = 20, draws: int = 2000) -> dict:
    """Circular block bootstrap of the net daily path. Not an acceptance line."""
    series = np.asarray(list(daily_net), dtype=float)
    count = len(series)
    if count == 0:
        raise ValueError("block bootstrap needs a daily path")
    if block < 1:
        raise ValueError("bootstrap block must be at least 1")
    generator = np.random.default_rng(seed)
    blocks = int(math.ceil(count / block))
    samples = np.empty(draws, dtype=float)
    for draw in range(draws):
        starts = generator.integers(0, count, size=blocks)
        pieces = []
        for start in starts:
            for offset in range(block):
                pieces.append(series[(int(start) + offset) % count])
        value = sharpe(pieces[:count])
        samples[draw] = np.nan if value is None else value
    finite = _finite(samples)
    if len(finite) == 0:
        raise ValueError("block bootstrap produced no finite Sharpe draws")
    return {
        "block": int(block),
        "draws": int(draws),
        "seed": int(seed),
        "sharpe_lo": float(np.percentile(finite, 2.5)),
        "sharpe_hi": float(np.percentile(finite, 97.5)),
        "share_le_0": float(np.mean(finite <= 0.0)),
    }


def _fmt(value, digits: int) -> str:
    if value is None or (isinstance(value, float) and not math.isfinite(value)):
        return "null"
    return f"{float(value):.{digits}f}"


def _compare(actual: float, op: str, threshold: float) -> bool:
    if actual is None or (isinstance(actual, float) and not math.isfinite(actual)):
        return False
    if op == ">=":
        return actual >= threshold
    if op == ">":
        return actual > threshold
    if op == "<=":
        return actual <= threshold
    if op == "<":
        return actual < threshold
    raise ValueError(f"unknown comparison {op!r}")


def _cost_2x(costs: list[dict]) -> dict:
    for row in costs:
        if abs(float(row["multiple"]) - 2.0) < 1e-9:
            return row
    raise ValueError("costs are missing the 2× row")


def evaluate(
    *,
    oos_sharpe,
    oos_profit_factor,
    placebo_p,
    is_sharpe,
    grid,
    costs,
    oos_trades,
    cross_market,
    thresholds: Thresholds | None = None,
    extra: list[Criterion] | None = None,
) -> list[dict]:
    """The six default lines, plus any extra Criterion rows. The kit sets `passed`."""
    limits = thresholds or Thresholds()
    extra = extra or []
    if cross_market is not NO_CROSS_MARKET and not cross_market:
        raise ValueError("cross_market must be a non-empty list or NO_CROSS_MARKET")

    positive = 0
    for row in grid:
        value = row.get("is_sharpe")
        if value is not None and float(value) > 0.0:
            positive += 1
    fraction = positive / len(grid) if grid else 0.0
    doubled = _cost_2x(costs)
    doubled_return = doubled.get("full_return")

    sharpe_ok = oos_sharpe is not None and float(oos_sharpe) >= limits.oos_sharpe_min
    factor_ok = oos_profit_factor is not None and float(oos_profit_factor) >= limits.oos_profit_factor_min
    is_ok = is_sharpe is not None and float(is_sharpe) > limits.is_sharpe_min_exclusive
    grid_ok = fraction >= limits.grid_positive_fraction
    cost_ok = doubled_return is not None and float(doubled_return) > limits.full_return_2x_min_exclusive
    sample_ok = int(oos_trades) >= limits.min_oos_trades

    lines = [
        {
            "id": "1",
            "name": "oos_edge",
            "required": (
                f"OOS Sharpe >= {_fmt(limits.oos_sharpe_min, 2)} and "
                f"OOS profit factor >= {_fmt(limits.oos_profit_factor_min, 2)}"
            ),
            "actual": f"Sharpe {_fmt(oos_sharpe, 3)}, profit factor {_fmt(oos_profit_factor, 3)}",
            "passed": bool(sharpe_ok and factor_ok),
        },
        {
            "id": "2",
            "name": "direction_placebo",
            "required": f"direction placebo p <= {_fmt(limits.placebo_p_max, 2)}",
            "actual": None if placebo_p is None else float(placebo_p),
            "passed": placebo_p is not None and float(placebo_p) <= limits.placebo_p_max,
        },
        {
            "id": "3",
            "name": "is_plateau",
            "required": (
                f"IS Sharpe > {_fmt(limits.is_sharpe_min_exclusive, 2)} and "
                f"at least {limits.grid_positive_fraction:.0%} of IS grid cells have Sharpe > 0"
            ),
            "actual": f"IS Sharpe {_fmt(is_sharpe, 3)}, grid {_fmt(fraction, 3)}",
            "passed": bool(is_ok and grid_ok),
        },
        {
            "id": "4",
            "name": "cost_2x",
            "required": "full-sample total return > 0 at 2× cost",
            "actual": None if doubled_return is None else float(doubled_return),
            "passed": bool(cost_ok),
        },
        {
            "id": "6",
            "name": "oos_sample",
            "required": f"at least {limits.min_oos_trades} OOS trades",
            "actual": int(oos_trades),
            "passed": bool(sample_ok),
        },
    ]
    # Line 5 sits between the cost line and the sample line when it applies.
    if cross_market is not NO_CROSS_MARKET:
        named = []
        passed = False
        for row in cross_market:
            value = row.get("oos_sharpe")
            named.append(f"{row.get('symbol', '?')}={_fmt(value, 3)}")
            if value is not None and float(value) > limits.cross_market_oos_sharpe_min_exclusive:
                passed = True
        lines.insert(4, {
            "id": "5",
            "name": "cross_market",
            "required": "OOS Sharpe > 0 on at least one cross-market instrument",
            "actual": ", ".join(named),
            "passed": passed,
        })

    next_id = 7
    for item in extra:
        if not isinstance(item, Criterion):
            raise TypeError("extra acceptance lines must be Criterion values")
        lines.append({
            "id": str(next_id),
            "name": item.name,
            "required": item.required,
            "actual": float(item.actual),
            "passed": _compare(float(item.actual), item.op, float(item.threshold)),
        })
        next_id += 1
    return lines


def status_from(lines, *, void_reason: str | None = None) -> str:
    """Void, then a short out-of-sample sample, then any other failure, then a pass."""
    if void_reason is not None and void_reason.strip():
        return "Void"
    for line in lines:
        if line.get("name") == "oos_sample" and not line.get("passed"):
            return "Inconclusive"
    if any(not line.get("passed") for line in lines):
        return "Rejected"
    return "Paper-trading candidate"
