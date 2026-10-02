# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the weekly small-cap reversal report. Run after backtest.py and posthoc.py.

    python research/low-liq-high-vol-mean-reversion/research/charts.py

Every figure is drawn from results.json, posthoc.json, or daily.csv. Nothing is recomputed
beyond the cumulative product and the drawdown of those stored daily returns.
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
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"

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
    ax.plot(days, np.cumprod(1 + series(rows, "bench_ew_ret")), color=ORANGE, label="Equal-weight screen, uncosted")
    ax.plot(days, np.cumprod(1 + series(rows, "bench_spy_ret")), color=AQUA, label="SPY close to close (zero before the store's history)")
    ax.plot(days, np.cumprod(1 + series(rows, "ret")), color=BLUE, label="Weekly quintile reversal, 20 bp and 5% borrow")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Growth of $1")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")


def drawdown(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.4))
    for col, color, label in (
        ("bench_ew_ret", ORANGE, "Equal-weight screen"),
        ("ret", BLUE, "Reversal book"),
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


def by_year(posthoc) -> None:
    rows = posthoc["by_year"]
    years = [r["year"] for r in rows]
    x = np.arange(len(years))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar(x - 0.18, [r["strategy_return"] for r in rows], width=0.36, color=BLUE, label="Reversal book")
    ax.bar(x + 0.18, [r["equal_weight_return"] for r in rows], width=0.36, color=ORANGE, label="Equal-weight screen")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, [str(y) for y in years], rotation=0)
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
    ax.legend(loc="lower left")
    ax.set_title("Sharpe against the cost multiplier")
    ax.set_xlabel("Multiplier on the 20 bp spread and the 5% borrow")
    ax.set_ylabel("Sharpe")
    save(fig, "costs.svg")


def grid(results) -> None:
    formations = [1, 2, 4]
    ks = [4, 5, 8, 10]
    mat = np.full((len(formations), len(ks)), np.nan)
    for cell in results["grid"]:
        mat[formations.index(cell["formation_weeks"]), ks.index(cell["k"])] = cell["is_sharpe"] if cell["is_sharpe"] is not None else np.nan
    fig, ax = plt.subplots(figsize=(7.2, 3.4))
    display = np.where(np.isnan(mat), 0.0, mat)
    im = ax.imshow(display, cmap="coolwarm", vmin=-0.6, vmax=0.6, aspect="auto")
    ax.set_xticks(range(len(ks)), [str(k) for k in ks])
    ax.set_yticks(range(len(formations)), [str(f) for f in formations])
    ax.set_xlabel("K, the quintile divisor")
    ax.set_ylabel("Formation, weeks")
    for i in range(len(formations)):
        for j in range(len(ks)):
            text = "flat" if np.isnan(mat[i, j]) else f"{mat[i, j]:.2f}"
            ax.text(j, i, text, ha="center", va="center", color=TEXT, fontsize=9)
    fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    ax.set_title("In-sample Sharpe. The primary is formation 1, K 5")
    save(fig, "grid.svg")


def placebo(results) -> None:
    null = np.array(results["placebo_null"], float)
    actual = results["placebo"]["actual_gross_sharpe"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.hist(null, bins=40, color="#d9d6cf", edgecolor=SURFACE)
    ax.axvline(actual, color=BLUE, linewidth=2, label=f"Actual gross Sharpe {actual:.2f}")
    ax.legend(loc="upper left")
    ax.set_title("Direction placebo, gross Sharpe")
    ax.set_xlabel("Sharpe")
    ax.set_ylabel("Draws")
    save(fig, "placebo.svg")


def quintiles(results) -> None:
    rows = results["breakdowns"]["spy_quintiles"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    means = [0.0 if r["mean_strategy"] is None else r["mean_strategy"] for r in rows]
    ax.bar([r["bin"] for r in rows], means, color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks([r["bin"] for r in rows])
    ax.set_title("Mean daily book return by SPY-move quintile")
    ax.set_xlabel("Quintile of the same-day SPY return, 1 is the worst")
    ax.set_ylabel("Mean daily return")
    save(fig, "move_quintiles.svg")


def rolling(posthoc, rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    vals = [np.nan if v is None else v for v in posthoc["rolling_sharpe_252"]]
    fig, ax = plt.subplots(figsize=(8, 3.4))
    ax.plot(days, vals, color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.set_title("Trailing 252-session Sharpe")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def main() -> None:
    results, posthoc, rows = load()
    equity(rows)
    drawdown(rows)
    by_year(posthoc)
    costs(results)
    grid(results)
    placebo(results)
    quintiles(results)
    rolling(posthoc, rows)
    print(f"wrote figures to {OUT}")


if __name__ == "__main__":
    main()
