# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/qqq-15m-turtle-overnight/report/. Run after backtest.py and posthoc.py.

    python research/qqq-15m-turtle-overnight/research/charts.py

Every figure is drawn from results.json, posthoc.json, daily.csv, or trades.csv. Nothing is recomputed.
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
BLUE, ORANGE, AQUA, YELLOW = "#2a78d6", "#eb6834", "#1baf7a", "#eda100"

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
    ax.plot(days, np.cumprod(1 + series(rows, "bench_qqq_ret")), color=ORANGE, label="QQQ buy and hold (price only)")
    ax.plot(days, np.cumprod(1 + series(rows, "qqq_ret")), color=BLUE, label="Turtle 55/20 on 15m, 1 bp per side")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Growth of $1, QQQ")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")


def drawdown(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.2))
    for col, color, label in (("bench_qqq_ret", ORANGE, "QQQ buy and hold"), ("qqq_ret", BLUE, "Strategy")):
        eq = np.cumprod(1 + series(rows, col))
        dd = eq / np.maximum.accumulate(eq) - 1
        ax.plot(days, dd * 100, color=color, label=label, linewidth=1.6)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Drawdown from peak")
    ax.set_ylabel("%")
    save(fig, "drawdown.svg")


def cross_market(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    for col, color, label in (("qqq_ret", BLUE, "QQQ (primary, 1 bp)"), ("spy_ret", ORANGE, "SPY (1 bp)"),
                              ("igv_ret", AQUA, "IGV (2 bp)")):
        ax.plot(days, np.cumprod(1 + series(rows, col)), color=color, label=label)
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Identical rules on three ETFs")
    ax.set_ylabel("Equity")
    save(fig, "cross_market.svg")


def overnight_vs_flat(rows) -> None:
    days = [date.fromisoformat(r["session"]) for r in rows]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, np.cumprod(1 + series(rows, "qqq_forced_flat_ret")), color=AQUA,
            label="Same rules, flat at every close (mechanism check)")
    ax.plot(days, np.cumprod(1 + series(rows, "qqq_ret")), color=BLUE, label="Primary, held overnight")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Holding overnight vs flattening at the close, QQQ, 1 bp")
    ax.set_ylabel("Equity")
    save(fig, "overnight_vs_flat.svg")


def decomposition(results) -> None:
    d = results["decomposition"]
    labels = ["Long", "Short", "All"]
    intr = [d["long"]["intraday_total"], d["short"]["intraday_total"], d["all"]["intraday_total"]]
    ovn = [d["long"]["overnight_total"], d["short"]["overnight_total"], d["all"]["overnight_total"]]
    x = np.arange(3)
    fig, ax = plt.subplots(figsize=(6.5, 3.6))
    ax.bar(x - 0.19, np.array(intr) * 100, width=0.36, color=BLUE, label="Regular hours")
    ax.bar(x + 0.19, np.array(ovn) * 100, width=0.36, color=ORANGE, label="Overnight gaps held")
    for xi, v in zip(x - 0.19, intr):
        ax.text(xi, v * 100, f"{v * 100:+.0f}", ha="center", va="bottom" if v >= 0 else "top", fontsize=9, color=TEXT)
    for xi, v in zip(x + 0.19, ovn):
        ax.text(xi, v * 100, f"{v * 100:+.0f}", ha="center", va="bottom" if v >= 0 else "top", fontsize=9, color=TEXT)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, labels)
    ax.legend(loc="upper center")
    ax.set_title("Where the gross P&L came from, full sample")
    ax.set_ylabel("Sum of trade returns, % of entry notional")
    save(fig, "decomposition.svg")


def by_year(results) -> None:
    yrs = list(results["by_year"])
    s = [results["by_year"][y]["return"] * 100 for y in yrs]
    b = [results["by_year"][y]["benchmark"] * 100 for y in yrs]
    x = np.arange(len(yrs))
    fig, ax = plt.subplots(figsize=(7.5, 3.6))
    ax.bar(x - 0.19, s, width=0.36, color=BLUE, label="Strategy")
    ax.bar(x + 0.19, b, width=0.36, color=ORANGE, label="QQQ buy and hold")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, [y if y not in ("2021", "2026") else y + ("\n(from Oct 25)" if y == "2021" else "\n(to Sep 25)")
                      for y in yrs])
    ax.legend(loc="upper left")
    ax.set_title("Calendar-year return")
    ax.set_ylabel("%")
    save(fig, "by_year.svg")


def costs(results) -> None:
    cs = results["cost_sweep"]
    bp = [c["cost_bps"] for c in cs]
    fig, ax = plt.subplots(figsize=(6.5, 3.6))
    for key, color, label in (("full", TEXT_2, "Full"), ("IS", AQUA, "In-sample"), ("OOS", BLUE, "Out-of-sample")):
        ax.plot(bp, [c[key]["sharpe"] for c in cs], color=color, label=label, marker="o", markersize=6)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axhline(0.5, color=ORANGE, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(2.5, 0.51, "OOS threshold 0.5", ha="center", va="bottom", fontsize=9, color=TEXT_2)
    ax.axvline(1, color=TEXT_2, linewidth=0.8, linestyle=":")
    ax.set_xticks(bp)
    ax.set_xlabel("Cost per side, bp (base = 1)")
    ax.set_ylabel("Sharpe")
    ax.legend(loc="upper right")
    ax.set_title("Cost sensitivity, QQQ")
    save(fig, "costs.svg")


def placebo(results) -> None:
    p = results["placebo"]
    edges = np.array(p["hist_edges"])
    counts = np.array(p["hist"])
    fig, ax = plt.subplots(figsize=(6.5, 3.4))
    ax.bar(edges[:-1], counts, width=np.diff(edges) * 0.92, align="edge", color=TEXT_2, alpha=0.55,
           label="2,000 random-direction books")
    a = p["actual_gross_sharpe_fixed_notional"]
    ax.axvline(a, color=BLUE, linewidth=2)
    ax.text(a, counts.max(), f"  actual gross {a:.2f}\n  p = {p['p']:.3f}", color=TEXT, va="top", fontsize=9)
    ax.set_xlabel("Full-sample gross Sharpe (fixed notional)")
    ax.set_ylabel("Draws")
    ax.legend(loc="upper left")
    ax.set_title("Direction placebo")
    save(fig, "placebo.svg")


def grid(results) -> None:
    cells = results["grid"]["cells"]
    nin = sorted({c["n_in"] for c in cells})
    nout = sorted({c["n_out"] for c in cells})
    fig, axes = plt.subplots(1, 2, figsize=(9, 3.8), layout="constrained")
    for ax, key, title in ((axes[0], "is_sharpe", "In-sample Sharpe (plateau check)"),
                           (axes[1], "oos_sharpe", "Out-of-sample Sharpe (not used)")):
        m = np.array([[next(c[key] for c in cells if c["n_in"] == i and c["n_out"] == o) for i in nin] for o in nout])
        im = ax.imshow(m, cmap="RdBu", vmin=-1.2, vmax=1.2, aspect="auto")
        for yi in range(len(nout)):
            for xi in range(len(nin)):
                v = m[yi, xi]
                bold = nin[xi] == 55 and nout[yi] == 20
                ax.text(xi, yi, f"{v:+.2f}", ha="center", va="center", fontsize=9,
                        color=TEXT if abs(v) < 0.8 else "#ffffff", fontweight="bold" if bold else "normal")
        ax.set_xticks(range(len(nin)), nin)
        ax.set_yticks(range(len(nout)), nout)
        ax.set_xlabel("Entry channel, bars")
        ax.set_ylabel("Exit channel, bars")
        ax.set_title(title, fontsize=10)
        ax.grid(False)
    fig.colorbar(im, ax=axes, shrink=0.8)
    fig.suptitle("Parameter grid, QQQ, 1 bp (bold = primary 55/20)", x=0.02, ha="left", fontweight="bold")
    save(fig, "grid.svg")


def quintiles(results) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(9, 3.8), sharey=True, layout="constrained")
    for ax, key, title, lab in ((axes[0], "quintiles_abs_move", "By size of QQQ's day", "|close-to-close| quintile"),
                                (axes[1], "quintiles_signed_move", "By direction of QQQ's day",
                                 "signed close-to-close quintile")):
        q = results[key]
        v = [x["mean_strategy"] * 1e4 for x in q]
        ax.bar(range(1, 6), v, color=[BLUE if y >= 0 else ORANGE for y in v], width=0.7)
        for xi, y in zip(range(1, 6), v):
            ax.text(xi, y, f"{y:+.0f}", ha="center", va="bottom" if y >= 0 else "top", fontsize=9)
        ax.axhline(0, color=TEXT_2, linewidth=1)
        ax.set_xticks(range(1, 6), ["1\nsmallest" if key == "quintiles_abs_move" else "1\nworst", "2", "3", "4",
                                    "5\nlargest" if key == "quintiles_abs_move" else "5\nbest"])
        ax.set_xlabel(lab)
        ax.set_title(title, fontsize=10)
    axes[0].set_ylabel("Mean strategy day, bp")
    fig.suptitle("Strategy return by QQQ's daily move, full sample", x=0.02, ha="left", fontweight="bold")
    save(fig, "quintiles.svg")


def rolling(posthoc) -> None:
    fig, ax = plt.subplots(figsize=(8, 3.4))
    for col, color, label in (("qqq_ret", BLUE, "QQQ"), ("spy_ret", ORANGE, "SPY"), ("igv_ret", AQUA, "IGV")):
        r = posthoc[col]["rolling_252_sharpe"]
        ax.plot([date.fromisoformat(d) for d in r["series_end_dates"]], r["series"], color=color, label=label,
                linewidth=1.6)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def main() -> None:
    results, posthoc, rows = load()
    equity(rows)
    drawdown(rows)
    cross_market(rows)
    overnight_vs_flat(rows)
    decomposition(results)
    by_year(results)
    costs(results)
    placebo(results)
    grid(results)
    quintiles(results)
    rolling(posthoc)
    print(f"figures written to {OUT}")


if __name__ == "__main__":
    main()
