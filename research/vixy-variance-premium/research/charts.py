# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""SVG figures from the saved study files. Does not open the store or rerun the backtest.

The locked results file is not kit_schema 1, so this script draws the eight
report figures itself. Equity is the stored equity column, on a linear axis,
over the full sample.
"""

import csv
import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("svg", force=True)
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"
OOS = date(2024, 7, 1)
SPLIT = date(2013, 6, 10)

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
BLUE = "#2a78d6"
ORANGE = "#eb6834"

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "text.color": TEXT,
    "axes.edgecolor": GRID,
    "axes.labelcolor": TEXT_2,
    "axes.titlesize": 12,
    "axes.titleweight": "bold",
    "axes.titlelocation": "left",
    "axes.titlecolor": TEXT,
    "xtick.color": TEXT_2,
    "ytick.color": TEXT_2,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.spines.left": False,
    "legend.frameon": False,
    "lines.linewidth": 2,
    "svg.fonttype": "path",
    "axes.titlepad": 14,
    "axes.axisbelow": True,
    "axes.ymargin": 0.12,
})


def save(fig, name):
    OUT.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    path = OUT / name
    fig.savefig(path)
    plt.close(fig)
    return path


def mark(ax):
    ax.axvline(OOS, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))


def wealth(series):
    eq = np.cumprod(1.0 + np.asarray(series, float))
    return eq


def drawdown(equity):
    equity = np.asarray(equity, float)
    peak = np.maximum.accumulate(equity)
    return equity / peak - 1.0


def main():
    doc = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    days = [date.fromisoformat(row["date"]) for row in rows]
    strategy_eq = np.array([float(row["equity"]) for row in rows], float)
    bench_r = np.array([float(row["benchmark_long"]) for row in rows], float)
    bench_eq = wealth(bench_r)

    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, bench_eq, color=ORANGE, label="Uncosted long VIXY")
    ax.plot(days, strategy_eq, color=BLUE, label="Short VIXY, weight -1")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axvline(SPLIT, color=ORANGE, linewidth=1, linestyle=(0, (1, 2)))
    mark(ax)
    ax.legend(loc="lower left")
    ax.set_title("Growth of $1, linear scale, full sample")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")

    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, drawdown(bench_eq), color=ORANGE, label="Uncosted long VIXY")
    ax.plot(days, drawdown(strategy_eq), color=BLUE, label="Short VIXY, weight -1")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axvline(SPLIT, color=ORANGE, linewidth=1, linestyle=(0, (1, 2)))
    mark(ax)
    ax.legend(loc="lower left")
    ax.set_title("Drawdown from the running peak")
    ax.set_ylabel("Drawdown")
    save(fig, "drawdown.svg")

    years = list(doc["by_year"])
    index = np.arange(len(years))
    width = 0.38
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.bar(index - width / 2, [doc["by_year"][y]["strategy_return"] for y in years], width, color=BLUE, label="Short VIXY")
    ax.bar(index + width / 2, [doc["by_year"][y]["benchmark_return"] for y in years], width, color=ORANGE, label="Uncosted long VIXY")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(index, years, rotation=45, ha="right")
    ax.legend(loc="lower left")
    ax.set_title("Calendar-year return")
    ax.set_ylabel("Return")
    save(fig, "by_year.svg")

    samples = np.load(HERE / "placebo_direction.npy")
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.hist(samples[np.isfinite(samples)], bins=40, color=BLUE)
    ax.axvline(doc["placebo"]["actual_gross_sharpe"], color=ORANGE, linewidth=2, label="actual gross Sharpe")
    ax.legend(loc="upper left")
    ax.set_title("Direction placebo")
    ax.set_xlabel("Sharpe")
    ax.set_ylabel("Draws")
    save(fig, "placebo.svg")

    cells = doc["grid"]["cells"]
    xs = [row["weight"] for row in cells]
    fig, axes = plt.subplots(1, 2, figsize=(8, 3.8))
    axes[0].plot(xs, [row["is_sharpe"] for row in cells], color=BLUE, marker="o")
    primary = [row for row in cells if row["weight"] == -1.0][0]
    axes[0].scatter([-1.0], [primary["is_sharpe"]], s=80, color=ORANGE, zorder=3, label="primary -1")
    axes[0].axhline(0, color=TEXT_2, linewidth=1)
    axes[0].legend(loc="best")
    axes[0].set_title("In-sample Sharpe")
    axes[0].set_xlabel("Weight")
    axes[0].set_ylabel("Sharpe")
    oos = [np.nan if row["oos_sharpe"] is None else row["oos_sharpe"] for row in cells]
    axes[1].plot(xs, oos, color=BLUE, marker="o")
    axes[1].axhline(0, color=TEXT_2, linewidth=1)
    axes[1].set_title("OOS Sharpe, not used for selection")
    axes[1].set_xlabel("Weight")
    axes[1].set_ylabel("Sharpe")
    save(fig, "grid.svg")

    costs = sorted(doc["costs"], key=lambda row: row["multiple"])
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot([row["multiple"] for row in costs], [row["full_sharpe"] for row in costs], color=BLUE, marker="o", label="Full sample")
    oos_c = [np.nan if row["oos_sharpe"] is None else row["oos_sharpe"] for row in costs]
    ax.plot([row["multiple"] for row in costs], oos_c, color=ORANGE, marker="o", label="Out of sample")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.legend(loc="best")
    ax.set_title("Cost sensitivity")
    ax.set_xlabel("Cost multiple")
    ax.set_ylabel("Sharpe")
    save(fig, "costs.svg")

    fig, axes = plt.subplots(1, 2, figsize=(8, 3.8))
    for ax, key, title in (
        (axes[0], "quintiles_spy", "SPY close-to-close quintile"),
        (axes[1], "quintiles_product", "VIXY close-to-close quintile"),
    ):
        qrows = doc[key]
        ax.bar([str(row["quintile"]) for row in qrows], [row["strategy_mean"] for row in qrows], color=BLUE)
        ax.axhline(0, color=TEXT_2, linewidth=1)
        ax.set_title(title)
        ax.set_xlabel("Quintile (1 = lowest)")
        ax.set_ylabel("Mean strategy return")
    save(fig, "move_quintiles.svg")

    with (HERE / "rolling_sharpe.csv").open(encoding="utf-8", newline="") as f:
        roll = list(csv.DictReader(f))
    rd = [date.fromisoformat(row["date"]) for row in roll]
    rv = [np.nan if row["sharpe"] == "" else float(row["sharpe"]) for row in roll]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(rd, rv, color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark(ax)
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")
    print(f"wrote 8 figures under {OUT}")


if __name__ == "__main__":
    main()
