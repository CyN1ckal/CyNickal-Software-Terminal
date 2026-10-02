# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/qqq-atr-scale-in/report/. Run after backtest.py and posthoc.py.

    python research/qqq-atr-scale-in/research/charts.py

Every figure is drawn from results.json, posthoc.json, daily.csv, or trades.csv.
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
OOS = date(2024, 7, 1)
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
    ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(),
            va="top", fontsize=9, color=TEXT_2)


def load():
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    posthoc = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    trades = [r for r in csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")) if r["symbol"] == "QQQ"]
    return results, posthoc, rows, trades


def series(rows, col):
    return np.array([float(r[col]) for r in rows])


def equity(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, np.cumprod(1 + series(rows, "bench_cc")), color=ORANGE, label="QQQ buy and hold")
    ax.plot(days, np.cumprod(1 + series(rows, "qqq_ret")), color=BLUE, label="ATR scale-in, 1 bp per side")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Growth of $1")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")


def drawdown(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]

    def dd(col):
        eq = np.cumprod(1 + series(rows, col))
        peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
        return eq / peak - 1.0

    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(days, dd("bench_cc"), color=ORANGE, label="QQQ buy and hold")
    ax.plot(days, dd("qqq_ret"), color=BLUE, label="ATR scale-in, 1 bp per side")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Drawdown from the running peak")
    ax.set_ylabel("Drawdown")
    ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.0%}"))
    save(fig, "drawdown.svg")


def by_year(results) -> None:
    rows = results["breakdowns"]["year"]
    years = [str(r["year"]) for r in rows]
    x = np.arange(len(years))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar(x - 0.18, [r["return"] for r in rows], width=0.36, color=BLUE, label="ATR scale-in")
    ax.bar(x + 0.18, [r["benchmark_return"] for r in rows], width=0.36, color=ORANGE, label="QQQ buy and hold")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, years)
    ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.0%}"))
    ax.legend(loc="upper left")
    ax.set_title("Calendar-year return")
    ax.set_ylabel("Return")
    save(fig, "by_year.svg")


def costs(results) -> None:
    order = ["0.0", "0.5", "1.0", "2.0", "3.0"]
    labels = ["0", "0.5", "1", "2", "3"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(labels, [results["costs"][k]["full_sharpe"] for k in order], color=BLUE, marker="o", label="Full sample")
    ax.plot(labels, [results["costs"][k]["oos_sharpe"] for k in order], color=ORANGE, marker="o", label="Out of sample")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axhline(0.5, color=AQUA, linewidth=1, linestyle=(0, (3, 3)), label="OOS Sharpe 0.5")
    ax.legend(loc="best")
    ax.set_title("Sharpe against cost per side")
    ax.set_xlabel("Basis points per side")
    ax.set_ylabel("Sharpe")
    save(fig, "costs.svg")


def grid(results) -> None:
    cells = results["grid"]
    spacings = sorted({c["spacing"] for c in cells})
    targets = sorted({c["target"] for c in cells})
    def panel(key):
        m = np.full((len(targets), len(spacings)), np.nan)
        for c in cells:
            m[targets.index(c["target"]), spacings.index(c["spacing"])] = c[key]
        return m
    both = np.concatenate([panel("is_sharpe").ravel(), panel("oos_sharpe").ravel()])
    lim = float(np.nanmax(np.abs(both))) if np.isfinite(both).any() else 1.0
    lim = max(lim, 0.05)
    fig, axes = plt.subplots(1, 2, figsize=(8.4, 3.6))
    for ax, key, title in (
        (axes[0], "is_sharpe", "In-sample Sharpe"),
        (axes[1], "oos_sharpe", "Out-of-sample Sharpe, not used"),
    ):
        m = panel(key)
        im = ax.imshow(m, cmap="RdBu", vmin=-lim, vmax=lim, aspect="auto")
        ax.set_xticks(range(len(spacings)), [str(s) for s in spacings])
        ax.set_yticks(range(len(targets)), [str(t) for t in targets])
        ax.set_xlabel("Spacing, ATR")
        ax.set_ylabel("Target, ATR")
        ax.set_title(title)
        for i in range(m.shape[0]):
            for j in range(m.shape[1]):
                ax.text(j, i, f"{m[i, j]:.2f}", ha="center", va="center", fontsize=8, color=TEXT)
        primary = (spacings.index(0.5), targets.index(0.25))
        ax.scatter([primary[0]], [primary[1]], s=180, facecolors="none", edgecolors=TEXT, linewidths=1.4)
        ax.grid(False)
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    save(fig, "grid.svg")


def quintiles(results) -> None:
    rows = results["predictions"]["2"]["quintiles"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    colors = [BLUE if (r["mean_strategy_return"] or 0) >= 0 else ORANGE for r in rows]
    ax.bar([str(r["quintile"]) for r in rows], [r["mean_strategy_return"] for r in rows], color=colors)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.3%}"))
    ax.set_xlabel("Quintile of |open to last close| / ATR  (1 = quiet)")
    ax.set_ylabel("Mean daily return")
    ax.set_title("Mean daily return by the day's excursion")
    save(fig, "move_quintiles.svg")


def placebo(results) -> None:
    null = np.array(results["placebo_null"]["direction"])
    actual = results["placebo"]["direction"]["actual_gross_sharpe"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.hist(null, bins=40, color="#c5d7f2", edgecolor=SURFACE)
    ax.axvline(actual, color=BLUE, linewidth=2, label=f"Actual gross Sharpe {actual:.2f}")
    ax.axvline(0, color=TEXT_2, linewidth=1)
    ax.legend(loc="upper left")
    ax.set_title("Direction placebo, full-sample gross Sharpe")
    ax.set_xlabel("Sharpe")
    ax.set_ylabel("Draws")
    save(fig, "placebo.svg")


def rolling(posthoc) -> None:
    block = posthoc["rolling_252"]
    if not block["dates"]:
        return
    days = [date.fromisoformat(d) for d in block["dates"]]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(days, block["sharpe"], color=BLUE, label="Trailing 252 sessions")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper right")
    ax.set_title("Trailing 252-session Sharpe")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def distribution(trades) -> None:
    nets = np.array([float(t["net_ret"]) * 1e4 for t in trades])
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.hist(nets, bins=60, color=BLUE, edgecolor=SURFACE)
    ax.axvline(0, color=TEXT_2, linewidth=1)
    ax.set_title("Campaign return, basis points per unit")
    ax.set_xlabel("Per-unit net return (bp)")
    ax.set_ylabel("Campaigns")
    save(fig, "trade_distribution.svg")


def main() -> None:
    results, posthoc, rows, trades = load()
    equity(rows)
    drawdown(rows)
    by_year(results)
    costs(results)
    grid(results)
    quintiles(results)
    placebo(results)
    rolling(posthoc)
    distribution(trades)
    print(f"wrote figures to {OUT}")


if __name__ == "__main__":
    main()
