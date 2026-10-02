# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the ATR martingale report. Drawn only from saved outputs.

    python research/qqq-atr-martingale/research/charts.py
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
    return results, posthoc, rows


def equity(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    eq = np.array([float(r["equity"]) for r in rows])
    bh = np.cumprod(1.0 + np.array([float(r["bench_cc"]) for r in rows]))
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, bh, color=ORANGE, label="QQQ buy and hold")
    ax.plot(days, eq, color=BLUE, label="Martingale, 1 bp per side")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Equity per 1 of starting capital")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")


def drawdown(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]

    def dd(path):
        peak = np.maximum.accumulate(np.concatenate([[1.0], path]))[1:]
        return path / peak - 1.0

    eq = np.array([float(r["equity"]) for r in rows])
    bh = np.cumprod(1.0 + np.array([float(r["bench_cc"]) for r in rows]))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(days, dd(bh), color=ORANGE, label="QQQ buy and hold")
    ax.plot(days, dd(eq), color=BLUE, label="Martingale")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Drawdown of marked equity")
    ax.set_ylabel("Drawdown")
    ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.0%}"))
    save(fig, "drawdown.svg")


def notional(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    n = np.array([float(r["gross_notional"]) for r in rows])
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(days, n, color=BLUE)
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.set_title("Gross notional at the session close")
    ax.set_ylabel("Multiples of starting capital")
    save(fig, "notional.svg")


def by_year(results) -> None:
    rows = results["breakdowns"]["year"]
    x = np.arange(len(rows))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar(x - 0.18, [r["pnl"] for r in rows], width=0.36, color=BLUE, label="Martingale P&L")
    ax.bar(x + 0.18, [r["benchmark_return"] for r in rows], width=0.36, color=ORANGE, label="QQQ return")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, [str(r["year"]) for r in rows])
    ax.legend(loc="upper left")
    ax.set_title("Year P&L per 1 of starting capital, and QQQ's return")
    ax.set_ylabel("P&L or return")
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
    mults = sorted({c["multiplier"] for c in cells})

    def panel(key):
        m = np.full((len(mults), len(spacings)), np.nan)
        for c in cells:
            m[mults.index(c["multiplier"]), spacings.index(c["spacing"])] = c[key]
        return m

    both = np.concatenate([panel("is_sharpe").ravel(), panel("oos_sharpe").ravel()])
    lim = max(float(np.nanmax(np.abs(both))), 0.05)
    fig, axes = plt.subplots(1, 2, figsize=(8.4, 3.6))
    for ax, key, title in (
        (axes[0], "is_sharpe", "In-sample Sharpe"),
        (axes[1], "oos_sharpe", "Out-of-sample Sharpe, not used"),
    ):
        m = panel(key)
        im = ax.imshow(m, cmap="RdBu", vmin=-lim, vmax=lim, aspect="auto")
        ax.set_xticks(range(len(spacings)), [str(s) for s in spacings])
        ax.set_yticks(range(len(mults)), [str(t) for t in mults])
        ax.set_xlabel("Spacing, ATR")
        ax.set_ylabel("Multiplier")
        ax.set_title(title)
        for i in range(m.shape[0]):
            for j in range(m.shape[1]):
                ax.text(j, i, f"{m[i, j]:.2f}", ha="center", va="center", fontsize=8, color=TEXT)
        ax.scatter([spacings.index(0.5)], [mults.index(2.0)], s=180, facecolors="none",
                   edgecolors=TEXT, linewidths=1.4)
        ax.grid(False)
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    save(fig, "grid.svg")


def placebo(results) -> None:
    null = np.array(results["placebo_null"]["direction"], dtype=float)
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
    days = [date.fromisoformat(d) for d in block["dates"]]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(days, block["sharpe"], color=BLUE, label="Trailing 252 sessions")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Trailing 252-session Sharpe")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def units(results) -> None:
    rows = results["breakdowns"]["units"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar([str(r["units"]) for r in rows], [r["realized"] for r in rows], color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xlabel("Units in the campaign")
    ax.set_ylabel("Realized P&L per 1 of starting capital")
    ax.set_title("Realized P&L by how many times the book added")
    save(fig, "units.svg")


def main() -> None:
    results, posthoc, rows = load()
    equity(rows)
    drawdown(rows)
    notional(rows)
    by_year(results)
    costs(results)
    grid(results)
    placebo(results)
    rolling(posthoc)
    units(results)
    print(f"wrote figures to {OUT}")


if __name__ == "__main__":
    main()
