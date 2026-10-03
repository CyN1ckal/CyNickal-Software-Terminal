# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the country-bab report.

Drawn only from results.json, posthoc.json, and daily.csv.

    python research/country-bab/research/charts.py
"""
from __future__ import annotations

import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("svg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"
OOS = date(2024, 7, 1)
SURFACE, TEXT, TEXT_2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
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
    fig.tight_layout()
    fig.savefig(OUT / name)
    plt.close(fig)


def mark_oos(ax) -> None:
    ax.axvline(OOS, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(),
            va="top", fontsize=9, color=TEXT_2)


def drawdown(equity: np.ndarray) -> np.ndarray:
    eq = np.concatenate([[1.0], equity])
    peak = np.maximum.accumulate(eq)
    return eq / peak - 1.0


def main() -> None:
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    post = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    raw = np.genfromtxt(HERE / "daily.csv", delimiter=",", names=True, dtype=None, encoding="utf-8")
    days = [date.fromisoformat(str(d)) for d in raw["date"]]
    strat_eq = np.asarray(raw["equity"], dtype=float)
    spy_eq = np.asarray(raw["spy_equity"], dtype=float)
    ew_eq = np.asarray(raw["ew_equity"], dtype=float)

    fig, ax = plt.subplots(figsize=(9, 4.6))
    ax.plot(days, spy_eq, color=ORANGE, label="SPY close-to-close")
    ax.plot(days, ew_eq, color=AQUA, label="Equal-weight eligible countries")
    ax.plot(days, strat_eq, color=BLUE, label="Low-minus-high beta, 5 bp per side")
    ax.text(days[-1], strat_eq[-1], f" {strat_eq[-1]:.2f}", color=BLUE, va="center", fontsize=9)
    ax.text(days[-1], ew_eq[-1], f" {ew_eq[-1]:.2f}", color=AQUA, va="center", fontsize=9)
    ax.text(days[-1], spy_eq[-1], f" {spy_eq[-1]:.2f}", color=ORANGE, va="center", fontsize=9)
    mark_oos(ax)
    ax.set_title("Growth of $1")
    ax.set_ylabel("equity")
    ax.legend(loc="upper left")
    save(fig, "equity.svg")

    fig, ax = plt.subplots(figsize=(9, 3.6))
    for eq, col, lab in (
        (spy_eq, ORANGE, "SPY close-to-close"),
        (ew_eq, AQUA, "Equal-weight eligible countries"),
        (strat_eq, BLUE, "Low-minus-high beta"),
    ):
        ax.plot(days, drawdown(eq)[1:] * 100, color=col, label=lab, linewidth=1.5)
    mark_oos(ax)
    ax.set_title("Drawdown from the running peak")
    ax.set_ylabel("%")
    ax.legend(loc="lower left")
    save(fig, "drawdown.svg")

    by = res["by_year"]
    yrs = [str(y["year"]) for y in by]
    x = np.arange(len(yrs))
    fig, ax = plt.subplots(figsize=(10, 4.0))
    ax.bar(x - 0.18, [y["strategy_return"] * 100 for y in by], 0.36, color=BLUE, label="Low-minus-high beta")
    ax.bar(x + 0.18, [y["ew_return"] * 100 for y in by], 0.36, color=AQUA, label="Equal-weight countries")
    ax.set_xticks(x, yrs)
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_title("Calendar-year return (2012 from 1 Feb, 2026 through 1 Oct)")
    ax.set_ylabel("%")
    ax.legend(loc="upper left", ncols=2)
    save(fig, "by_year.svg")

    sweep = res["cost_sweep"]
    fig, ax = plt.subplots(figsize=(7.2, 3.8))
    xs = [row["bp"] for row in sweep]
    ax.plot(xs, [row["full_sharpe"] for row in sweep], color=BLUE, marker="o", markersize=7, label="full sample")
    ax.plot(xs, [row["oos_sharpe"] for row in sweep], color=ORANGE, marker="o", markersize=7, label="out of sample")
    ax.axhline(0.5, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(xs[-1], 0.5, " OOS bar 0.5", va="bottom", ha="right", fontsize=9, color=TEXT_2)
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_xlabel("cost per side (bp)")
    ax.set_ylabel("Sharpe")
    ax.set_title("Sharpe against cost")
    ax.legend(loc="best")
    save(fig, "costs.svg")

    grid = res["grid"]
    labels = [str(c["window"]) for c in grid]
    x = np.arange(len(grid))
    fig, ax = plt.subplots(figsize=(8, 4.0))
    is_bars = ax.bar(x - 0.18, [c["is_sharpe"] for c in grid], 0.36, color=BLUE, label="in sample")
    oos_bars = ax.bar(x + 0.18, [c["oos_sharpe"] for c in grid], 0.36, color=ORANGE, label="out of sample")
    for bars in (is_bars, oos_bars):
        for bar, cell in zip(bars, grid):
            if cell["window"] == 252:
                bar.set_edgecolor(TEXT)
                bar.set_linewidth(1.4)
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_xticks(x, [f"{lab}\nprimary" if lab == "252" else lab for lab in labels])
    ax.set_xlabel("beta window (sessions), book size fixed at 3")
    ax.set_ylabel("Sharpe")
    ax.set_title("Beta-window grid. Nothing was selected from it.")
    ax.legend(loc="lower left")
    save(fig, "grid.svg")

    fig, axs = plt.subplots(1, 2, figsize=(10, 3.8))
    panels = (
        (axs[0], res["direction_placebo"], "Direction placebo: 2,000 draws"),
        (axs[1], res["timing_placebo"], "Timing placebo: 500 draws"),
    )
    for ax, block, title in panels:
        draws = np.asarray(block["draws"], dtype=float)
        ax.hist(draws, bins=40, color=AQUA, edgecolor=SURFACE, linewidth=0.6)
        ax.axvline(block["actual_gross_sharpe"], color=BLUE, linewidth=2)
        ax.axvline(block["null_p95"], color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
        ax.text(
            block["actual_gross_sharpe"], 1.0,
            f"  actual {block['actual_gross_sharpe']:.2f}\n  p = {block['p']:.3f}",
            transform=ax.get_xaxis_transform(), va="top", fontsize=9, color=TEXT,
        )
        ax.set_title(title)
        ax.set_xlabel("gross Sharpe")
    save(fig, "placebo.svg")

    q = res["move_quintiles"]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    x = np.arange(len(q))
    ax.bar(x - 0.18, [v["mean_strategy"] * 1e4 for v in q], 0.36, color=BLUE, label="Low-minus-high beta")
    ax.bar(x + 0.18, [v["mean_spy"] * 1e4 for v in q], 0.36, color=ORANGE, label="SPY")
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_xticks(x, [f"Q{v['quintile']}" for v in q])
    ax.set_xlabel("quintile of SPY's close-to-close return (Q1 = worst)")
    ax.set_ylabel("average daily return (bp)")
    ax.set_title("Strategy return by the size of SPY's day")
    ax.legend(loc="upper right")
    save(fig, "move_quintiles.svg")

    roll = post["rolling_sharpe"]
    rd = [date.fromisoformat(d) for d in roll["end_dates"]]
    fig, ax = plt.subplots(figsize=(9, 3.6))
    ax.plot(rd, roll["spy"], color=ORANGE, linewidth=1.5, label="SPY close-to-close")
    ax.plot(rd, roll["ew"], color=AQUA, linewidth=1.5, label="Equal-weight countries")
    ax.plot(rd, roll["strategy"], color=BLUE, linewidth=1.5, label="Low-minus-high beta")
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    mark_oos(ax)
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.legend(loc="upper left")
    save(fig, "rolling_sharpe.svg")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
