# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Daily-path and trade-list statistics for a pre-registered study.

Sharpe, drawdown, and profit factor live here so a study does not reimplement
them. The daily path and the trade list stay separate: flat sessions are part
of the path, and sizing stays in the study.
"""

from __future__ import annotations

import math
from datetime import date

import numpy as np

ANNUAL = 252

_TRADE_KEYS = (
    "sessions",
    "total_return",
    "cagr",
    "ann_vol",
    "sharpe",
    "max_dd",
    "t_stat",
    "trades",
    "win_rate",
    "profit_factor",
    "avg_net_bp",
)
_BENCH_KEYS = (
    "sessions",
    "total_return",
    "cagr",
    "ann_vol",
    "sharpe",
    "max_dd",
    "t_stat",
)


def as_date(value) -> date:
    if isinstance(value, date):
        return value
    return date.fromisoformat(str(value)[:10])


def as_dates(sessions) -> list[date]:
    return [as_date(session) for session in sessions]


def _model(model: str) -> str:
    if model not in ("compound", "additive"):
        raise ValueError(f"return model must be compound or additive, got {model!r}")
    return model


def _floats(values) -> np.ndarray:
    return np.asarray(list(values), dtype=float)


def _py(value):
    if value is None:
        return None
    number = float(value)
    if not math.isfinite(number):
        return None
    return number


def sharpe(values) -> float | None:
    """Mean / sample standard deviation * sqrt(252). None if n < 2 or sd == 0."""
    series = _floats(values)
    if len(series) < 2:
        return None
    deviation = float(series.std(ddof=1))
    if deviation == 0.0 or not math.isfinite(deviation):
        return None
    return _py(float(series.mean()) / deviation * math.sqrt(ANNUAL))


def ann_vol(values) -> float | None:
    series = _floats(values)
    if len(series) < 2:
        return None
    deviation = float(series.std(ddof=1))
    if deviation == 0.0 or not math.isfinite(deviation):
        return None
    return _py(deviation * math.sqrt(ANNUAL))


def total_return(values, model: str = "compound") -> float:
    model = _model(model)
    series = _floats(values)
    if len(series) == 0:
        return 0.0
    if model == "additive":
        return float(series.sum())
    return float(np.prod(1.0 + series) - 1.0)


def cagr(values, model: str = "compound") -> float | None:
    model = _model(model)
    if model == "additive":
        return None
    series = _floats(values)
    count = len(series)
    if count == 0:
        return None
    end = float(np.prod(1.0 + series))
    if end <= 0.0:
        return None
    return float(end ** (ANNUAL / count) - 1.0)


def wealth(values, model: str = "compound") -> np.ndarray:
    """Equity path. Compound is cumprod(1+r). Additive is 1+cumsum(r)."""
    model = _model(model)
    series = _floats(values)
    if model == "additive":
        return 1.0 + np.cumsum(series)
    return np.cumprod(1.0 + series)


def max_drawdown(values, model: str = "compound") -> float:
    """Most negative equity/peak - 1. The peak is seeded at 1.

    A loss on the first session is inside the drawdown. An empty series is 0.
    """
    model = _model(model)
    series = _floats(values)
    if len(series) == 0:
        return 0.0
    equity = wealth(series, model)
    peak = np.maximum.accumulate(np.concatenate([np.array([1.0]), equity]))[1:]
    return float((equity / peak - 1.0).min())


def t_stat(values) -> float | None:
    series = _floats(values)
    if len(series) < 2:
        return None
    deviation = float(series.std(ddof=1))
    if deviation == 0.0 or not math.isfinite(deviation):
        return None
    return _py(float(series.mean()) / (deviation / math.sqrt(len(series))))


def profit_factor(nets) -> float | None:
    """Sum of nets strictly above 0 over the absolute sum of nets strictly below 0.

    A zero net is in neither bucket. None when there is no trade or no loser.
    """
    series = _floats(nets)
    if len(series) == 0:
        return None
    wins = float(series[series > 0].sum())
    losses = float(-series[series < 0].sum())
    if losses == 0.0:
        return None
    return wins / losses


def win_rate(nets) -> float | None:
    series = _floats(nets)
    if len(series) == 0:
        return None
    return float((series > 0).mean())


def avg_net_bp(nets) -> float | None:
    series = _floats(nets)
    if len(series) == 0:
        return None
    return float(series.mean() * 1e4)


def _mask(dates: list[date], which: str, is_end: date, oos_start: date) -> list[bool]:
    if which == "full":
        return [True] * len(dates)
    if which == "is":
        return [day <= is_end for day in dates]
    if which == "oos":
        return [day >= oos_start for day in dates]
    raise ValueError(which)


def _window_returns(dates, values, kept: list[bool]):
    return [value for value, keep in zip(values, kept) if keep]


def _trade_day(trade: dict) -> date:
    if "session" not in trade:
        raise ValueError("trade is missing session")
    return as_date(trade["session"])


def _window_metrics(dates, values, trades, kept, model: str, held) -> dict:
    chosen_days = {day for day, keep in zip(dates, kept) if keep}
    series = _window_returns(dates, values, kept)
    chosen_trades = [trade for trade in trades if _trade_day(trade) in chosen_days]
    nets = [float(trade["net"]) for trade in chosen_trades]
    out = {
        "sessions": len(series),
        "total_return": total_return(series, model),
        "cagr": cagr(series, model),
        "ann_vol": ann_vol(series),
        "sharpe": sharpe(series),
        "max_dd": max_drawdown(series, model),
        "t_stat": t_stat(series),
        "trades": len(chosen_trades),
        "win_rate": win_rate(nets),
        "profit_factor": profit_factor(nets),
        "avg_net_bp": avg_net_bp(nets),
    }
    if held is not None:
        flags = _window_returns(dates, held, kept)
        out["exposure"] = float(np.mean(_floats(flags))) if flags else None
    return out


def _check_split(dates: list[date], is_end: date, oos_start: date) -> None:
    if is_end >= oos_start:
        raise ValueError(f"is_end {is_end.isoformat()} must be before oos_start {oos_start.isoformat()}")


def performance(
    sessions,
    strategy_net,
    trades,
    *,
    is_end,
    oos_start,
    model: str = "compound",
    held=None,
) -> dict:
    """Full, in-sample, and out-of-sample metrics for one daily path and its trades.

    A trade counts in a window when its session is one of the daily rows in that
    window. In-sample is session <= is_end. Out-of-sample is session >= oos_start.
    A gap between those dates stays in the full sample only.
    """
    model = _model(model)
    dates = as_dates(sessions)
    net = list(strategy_net)
    if len(dates) != len(net):
        raise ValueError("sessions and strategy_net have different lengths")
    if held is not None and len(held) != len(dates):
        raise ValueError("held and sessions have different lengths")
    is_end = as_date(is_end)
    oos_start = as_date(oos_start)
    _check_split(dates, is_end, oos_start)
    out = {}
    for name in ("full", "is", "oos"):
        kept = _mask(dates, name, is_end, oos_start)
        out[name] = _window_metrics(dates, net, trades, kept, model, held)
    return out


def benchmark_performance(sessions, benchmark, *, is_end, oos_start, model: str = "compound") -> dict:
    """The same windows as performance(), without trade statistics."""
    model = _model(model)
    dates = as_dates(sessions)
    series = list(benchmark)
    if len(dates) != len(series):
        raise ValueError("sessions and benchmark have different lengths")
    is_end = as_date(is_end)
    oos_start = as_date(oos_start)
    _check_split(dates, is_end, oos_start)
    out = {}
    for name in ("full", "is", "oos"):
        kept = _mask(dates, name, is_end, oos_start)
        chosen = _window_returns(dates, series, kept)
        out[name] = {
            "sessions": len(chosen),
            "total_return": total_return(chosen, model),
            "cagr": cagr(chosen, model),
            "ann_vol": ann_vol(chosen),
            "sharpe": sharpe(chosen),
            "max_dd": max_drawdown(chosen, model),
            "t_stat": t_stat(chosen),
        }
    return out


def by_year(sessions, strategy_net, benchmark, *, model: str = "compound") -> list[dict]:
    model = _model(model)
    dates = as_dates(sessions)
    net = list(strategy_net)
    bench = list(benchmark)
    if not (len(dates) == len(net) == len(bench)):
        raise ValueError("by_year inputs have different lengths")
    years = sorted({day.year for day in dates})
    rows = []
    for year in years:
        kept = [day.year == year for day in dates]
        strategy = _window_returns(dates, net, kept)
        market = _window_returns(dates, bench, kept)
        rows.append({
            "year": year,
            "strategy_return": total_return(strategy, model),
            "strategy_sharpe": sharpe(strategy),
            "strategy_max_dd": max_drawdown(strategy, model),
            "benchmark_return": total_return(market, model),
        })
    return rows


def move_quintiles(sessions, strategy_net, market_move) -> list[dict]:
    """Equal-count quintiles of the full-sample market move. Quintile 1 is the lowest.

    Ranks use a stable mergesort, so a tie stays with the earlier session and the
    lower quintile. The edges are a reporting cut, not a signal input.
    """
    dates = as_dates(sessions)
    strategy = _floats(strategy_net)
    move = _floats(market_move)
    if not (len(dates) == len(strategy) == len(move)):
        raise ValueError("move_quintiles inputs have different lengths")
    count = len(dates)
    quintile = np.ones(count, dtype=int)
    if count:
        order = np.argsort(move, kind="mergesort")
        for rank, index in enumerate(order):
            quintile[index] = min(5, int(rank) * 5 // count + 1)
    rows = []
    for bucket in range(1, 6):
        chosen = strategy[quintile == bucket]
        rows.append({
            "quintile": bucket,
            "sessions": int(len(chosen)),
            "mean_strategy_net": float(chosen.mean()) if len(chosen) else None,
        })
    return rows


def trade_keys() -> tuple[str, ...]:
    return _TRADE_KEYS


def bench_keys() -> tuple[str, ...]:
    return _BENCH_KEYS


def deflated_sharpe_ratio(
    daily_returns,
    trials_count: int = 1,
    trials_variance: float = 0.25,
) -> float | None:
    """Bailey & Lopez de Prado (2014) Deflated Sharpe Ratio (DSR).

    Adjusts observed Sharpe ratio for sample size, skewness, kurtosis,
    and the number of strategy trials sharing the evaluation window.
    Returns the probability that the true Sharpe > 0 given multiple testing history.
    """
    series = _floats(daily_returns)
    n = len(series)
    if n < 30:
        return None
    mean = float(series.mean())
    std = float(series.std(ddof=1))
    if std <= 0.0 or not math.isfinite(std):
        return None
    sr_ann = (mean / std) * math.sqrt(ANNUAL)

    diff = series - mean
    m2 = float(np.mean(diff ** 2))
    if m2 <= 0.0:
        return None
    skew = float(np.mean(diff ** 3) / (m2 ** 1.5))
    kurt = float(np.mean(diff ** 4) / (m2 ** 2.0))

    var_sr = (1.0 - skew * sr_ann + ((kurt - 1.0) / 4.0) * (sr_ann ** 2)) / (n / ANNUAL)
    if var_sr <= 0.0 or not math.isfinite(var_sr):
        return None
    se_sr = math.sqrt(var_sr)

    euler_mascheroni = 0.5772156649
    m = max(1, int(trials_count))
    try:
        from scipy import stats
        z1 = float(stats.norm.ppf(1.0 - 1.0 / m)) if m > 1 else 0.0
        z2 = float(stats.norm.ppf(1.0 - 1.0 / (m * math.e))) if m > 1 else 0.0
        sr_star = math.sqrt(trials_variance) * ((1.0 - euler_mascheroni) * z1 + euler_mascheroni * z2) if m > 1 else 0.0
        dsr = float(stats.norm.cdf((sr_ann - sr_star) / se_sr))
    except Exception:
        sr_star = math.sqrt(trials_variance) * math.sqrt(2.0 * math.log(m)) if m > 1 else 0.0
        z = (sr_ann - sr_star) / se_sr
        dsr = 0.5 * (1.0 + math.erf(z / math.sqrt(2.0)))
    return _py(dsr)
