# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the opening-gap fade. Run after backtest.py and posthoc.py.

    python research/small-cap-gap-up-fade/research/charts.py

Every figure is drawn from results.json, posthoc.json, or daily.csv.
"""

from __future__ import annotations

import csv
import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("svg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"
OOS = date(2024, 1, 2)
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
BLUE, ORANGE, AQUA, GRAY = "#2a78d6", "#eb6834", "#1baf7a", "#8a8884"

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "font.family": "DejaVu Sans", "font.size": 10, "text.color": TEXT,
    "axes.edgecolor": GRID, "axes.labelcolor": TEXT_2, "axes.titlesize": 12,
    "axes.titleweight": "bold", "axes.titlelocation": "left", "axes.titlecolor": TEXT,
    "xtick.color": TEXT_2, "ytick.color": TEXT_2, "axes.grid": True, "grid.color": GRID,
    "grid.linewidth": 0.8, "axes.spines.top": False, "axes.spines.right": False,
    "axes.spines.left": False, "legend.frameon": False, "lines.linewidth": 2,
    "svg.fonttype": "path", "axes.titlepad": 14, "axes.axisbelow": True, "axes.ymargin": 0.12,
})


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    if fig.get_layout_engine() is None:
        fig.tight_layout()
    fig.savefig(OUT / name)
    plt.close(fig)


def mark_oos(ax) -> None:
    ax.axvline(OOS, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(), va="top", fontsize=9, color=TEXT_2)


def load():
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    posthoc = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    return results, posthoc, rows


def series(rows, col):
    return np.array([float(r[col]) for r in rows])


def equity(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.semilogy(days, np.cumprod(1 + series(rows, "bench_intraday_short_ret")), color=GRAY, label="Short every open, uncosted")
    ax.semilogy(days, np.cumprod(1 + series(rows, "bench_spy_ret")), color=AQUA, label="SPY close to close")
    ax.semilogy(days, np.cumprod(1 + series(rows, "bench_ew_ret")), color=ORANGE, label="Equal-weight screen, uncosted")
    ax.semilogy(days, np.cumprod(1 + series(rows, "ret")), color=BLUE, label="Gap-up fade, 20 bp")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Growth of $1, log scale")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")


def drawdown(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.4))
    for col, color, label in (
        ("bench_ew_ret", ORANGE, "Equal-weight screen"),
        ("ret", BLUE, "Gap-up fade"),
    ):
        eq = np.cumprod(1 + series(rows, col))
        peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
        ax.plot(days, eq / peak - 1.0, color=color, label=label)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Drawdown")
    ax.set_ylabel("Drawdown")
    save(fig, "drawdown.svg")


def by_year(results) -> None:
    rows = results["breakdowns"]["by_year"]
    years = [r["year"] for r in rows]
    x = np.arange(len(years))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar(x - 0.18, [r["return"] for r in rows], width=0.36, color=BLUE, label="Gap-up fade")
    ax.bar(x + 0.18, [r["bench_ew_return"] for r in rows], width=0.36, color=ORANGE, label="Equal-weight screen")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, [str(y) for y in years])
    ax.legend(loc="upper left")
    ax.set_title("Calendar-year return")
    ax.set_ylabel("Return")
    save(fig, "by_year.svg")


def costs(results) -> None:
    keys = ["0.0", "0.5", "1.0", "2.0", "3.0"]
    labels = ["0", "0.5×", "1×", "2×", "3×"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(labels, [results["cost_sweep"][k]["full_sharpe"] for k in keys], color=BLUE, marker="o", label="Full sample")
    ax.plot(labels, [results["cost_sweep"][k]["oos_sharpe"] for k in keys], color=ORANGE, marker="o", label="Out of sample")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axhline(0.5, color=GRAY, linewidth=1, linestyle=(0, (3, 3)))
    ax.legend(loc="best")
    ax.set_title("Sharpe against the spread")
    ax.set_xlabel("Cost, as a multiple of 20 bp per side")
    ax.set_ylabel("Sharpe")
    save(fig, "costs.svg")


def placebo(results) -> None:
    null = np.array(results["placebo_null"], float)
    null = null[np.isfinite(null)]
    actual = results["placebo"]["actual_gross_sharpe"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.hist(null, bins=40, color=GRAY, label="Sign-flipped gross paths")
    ax.axvline(actual, color=BLUE, linewidth=2, label="Actual gross Sharpe")
    ax.legend(loc="best")
    ax.set_title("Direction placebo")
    ax.set_xlabel("Full-sample gross Sharpe")
    ax.set_ylabel("Draws")
    save(fig, "placebo.svg")


def grid(results) -> None:
    rows = results["grid"]
    labels = [f"{int(r['gap_min'] * 100)}%" for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(labels, [r["is_sharpe"] for r in rows], color=BLUE, marker="o", label="In sample")
    ax.plot(labels, [r["oos_sharpe"] for r in rows], color=ORANGE, marker="o", label="Out of sample, not used to choose")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.legend(loc="best")
    ax.set_title("Gap threshold, primary at 5%")
    ax.set_xlabel("Minimum opening gap")
    ax.set_ylabel("Sharpe")
    save(fig, "grid.svg")


def quintiles(results) -> None:
    rows = results["breakdowns"]["spy_quintiles"]
    labels = [str(r["quintile"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    vals = [r["strategy_mean"] * 1e4 for r in rows]
    colors = [ORANGE if v < 0 else BLUE for v in vals]
    ax.bar(labels, vals, color=colors)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_title("Mean daily return by SPY-move quintile")
    ax.set_xlabel("Quintile of the same-day SPY return, low to high")
    ax.set_ylabel("Strategy, basis points per day")
    save(fig, "move_quintiles.svg")


def rolling(posthoc) -> None:
    rows = posthoc["rolling_sharpe"]
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.4))
    ax.plot(days, [r["sharpe"] for r in rows], color=BLUE, label="252-session Sharpe")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axhline(0.5, color=GRAY, linewidth=1, linestyle=(0, (3, 3)))
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Rolling Sharpe")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def main() -> None:
    results, posthoc, rows = load()
    equity(rows)
    drawdown(rows)
    by_year(results)
    costs(results)
    placebo(results)
    grid(results)
    quintiles(results)
    rolling(posthoc)
    print(f"wrote figures to {OUT}")


if __name__ == "__main__":
    main()
