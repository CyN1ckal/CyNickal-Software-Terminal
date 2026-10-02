# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/qqq-bollinger-adding/report/. Run after backtest.py and posthoc.py.

    python research/qqq-bollinger-adding/research/charts.py

Every figure is drawn from results.json, posthoc.json, daily.csv, rolling_sharpe.csv,
or placebo_draws.npy. Nothing is recomputed from the store.
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
MUTED = "#8a8984"
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


def load():
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    post = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    with open(HERE / "daily.csv", newline="", encoding="utf-8") as f:
        daily = list(csv.DictReader(f))
    return res, post, daily


def oos_line(ax, label=True):
    ax.axvline(OOS, color=MUTED, linestyle="--", linewidth=1)
    if label:
        ax.annotate("out of sample →", (OOS, 1), xycoords=("data", "axes fraction"),
                    xytext=(4, -4), textcoords="offset points", va="top", color=TEXT_2, fontsize=9)


def save(fig, name):
    OUT.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(OUT / name, format="svg")
    plt.close(fig)


def equity_and_drawdown(daily):
    d = [date.fromisoformat(x["session"]) for x in daily]
    series = [("Bollinger reversion with adding (1 bp)", "qqq_net", BLUE),
              ("No adding, diagnostic D1 (1 bp)", "d1_no_adding_net", AQUA),
              ("QQQ buy and hold", "buy_and_hold", ORANGE)]
    fig, ax = plt.subplots(figsize=(9, 4.6))
    for label, col, color in series:
        eq = np.cumprod(1 + np.array([float(x[col]) for x in daily]))
        ax.plot(d, eq, color=color, linewidth=2 if col != "d1_no_adding_net" else 1.5, label=label)
        ax.annotate(f"{eq[-1]:.2f}", (d[-1], eq[-1]), xytext=(4, 0), textcoords="offset points",
                    va="center", color=TEXT_2, fontsize=9)
    ax.axhline(1, color=MUTED, linewidth=0.8)
    oos_line(ax)
    ax.set_title("Growth of 1: QQQ, 27 Sep 2021 to 25 Sep 2026")
    ax.set_ylabel("Equity")
    ax.legend(loc="upper left")
    save(fig, "equity.svg")

    fig, ax = plt.subplots(figsize=(9, 3.4))
    for label, col, color in (series[0], series[2]):
        eq = np.cumprod(1 + np.array([float(x[col]) for x in daily]))
        dd = eq / np.maximum.accumulate(eq) - 1
        ax.plot(d, dd * 100, color=color, label=label, linewidth=1.5)
    oos_line(ax)
    ax.set_title("Drawdown from the running peak")
    ax.set_ylabel("%")
    ax.legend(loc="lower left")
    save(fig, "drawdown.svg")


def by_year(res):
    years = sorted(res["by_year"])
    s = [res["by_year"][y]["return"] * 100 for y in years]
    b = [res["by_year"][y]["buy_and_hold"] * 100 for y in years]
    x = np.arange(len(years))
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.bar(x - 0.2, s, 0.38, color=BLUE, label="Strategy (1 bp)")
    ax.bar(x + 0.2, b, 0.38, color=ORANGE, label="QQQ buy and hold")
    for i, v in enumerate(s):
        ax.annotate(f"{v:+.1f}%", (x[i] - 0.2, v), xytext=(0, 3 if v >= 0 else -11),
                    textcoords="offset points", ha="center", fontsize=8, color=TEXT_2)
    ax.axhline(0, color=MUTED, linewidth=0.8)
    ax.set_xticks(x, [f"{y}{' (part)' if y in ('2021', '2026') else ''}" for y in years])
    ax.set_title("Calendar-year return")
    ax.set_ylabel("%")
    ax.legend(loc="upper left")
    save(fig, "by_year.svg")


def placebo(res):
    draws = np.load(HERE / "placebo_draws.npy")
    dirn, timing = draws[0], draws[1][~np.isnan(draws[1])]
    actual = res["direction_placebo"]["actual_gross_sharpe"]
    fig, axes = plt.subplots(1, 2, figsize=(10, 3.6), sharey=False)
    for ax, x, title, p in ((axes[0], dirn, "Direction placebo (2,000 sign flips)", res["direction_placebo"]["p"]),
                            (axes[1], timing, "Timing placebo (500 random starts)", res["timing_placebo"]["p"])):
        ax.hist(x, bins=40, color=AQUA, edgecolor=SURFACE, linewidth=0.5)
        ax.axvline(actual, color=BLUE, linewidth=2)
        ax.annotate(f"actual {actual:.2f}\np = {p:.3f}", (actual, 1), xycoords=("data", "axes fraction"),
                    xytext=(5, -4), textcoords="offset points", va="top", fontsize=9, color=TEXT)
        ax.set_title(title, fontsize=11)
        ax.set_xlabel("Full-sample gross Sharpe")
    axes[0].set_ylabel("Draws")
    save(fig, "placebo.svg")


def grid(res):
    cells = res["grid"]["cells"]
    fig, ax = plt.subplots(figsize=(6.4, 5))
    markers = {10: "o", 20: "s", 30: "^"}
    for n in (10, 20, 30):
        cs = [c for c in cells if c["n"] == n and not c["primary"]]
        ax.scatter([c["is_sharpe"] for c in cs], [c["oos_sharpe"] for c in cs], s=46, marker=markers[n],
                   color=AQUA, edgecolor=SURFACE, linewidth=1, label=f"N = {n}")
    p = next(c for c in cells if c["primary"])
    ax.scatter([p["is_sharpe"]], [p["oos_sharpe"]], s=90, marker="s", color=BLUE, edgecolor=SURFACE,
               linewidth=1.5, label="Primary (20, 2.0, 1.0)", zorder=3)
    ax.axhline(0, color=MUTED, linewidth=0.8)
    ax.axvline(0, color=MUTED, linewidth=0.8)
    ax.set_xlim(right=0.3)
    ax.set_ylim(top=0.3)
    ax.set_xlabel("In-sample Sharpe")
    ax.set_ylabel("Out-of-sample Sharpe (shown for selection bias only)")
    ax.set_title("27 grid cells, 1 bp: none above zero")
    ax.legend(loc="lower right")
    save(fig, "grid.svg")


def costs(res):
    cs = ["0.0", "0.5", "1.0", "2.0", "3.0"]
    x = [float(c) for c in cs]
    fig, ax = plt.subplots(figsize=(7, 3.6))
    for w, color, label in (("full", BLUE, "Full sample"), ("oos", ORANGE, "Out of sample")):
        y = [res["costs"][c][w]["sharpe"] for c in cs]
        ax.plot(x, y, color=color, marker="o", markersize=7, label=label)
    ax.axhline(0, color=MUTED, linewidth=0.8)
    ax.axhline(0.5, color=MUTED, linewidth=0.8, linestyle=":")
    ax.annotate("acceptance line 1: OOS 0.5", (3, 0.5), xytext=(0, 4), textcoords="offset points",
                ha="right", fontsize=8, color=TEXT_2)
    ax.axvline(1, color=GRID, linewidth=6, zorder=0)
    ax.set_xticks(x, [f"{c:g} bp" for c in x])
    ax.set_title("Sharpe by cost per side (base 1 bp)")
    ax.set_ylabel("Sharpe")
    ax.legend(loc="lower left")
    save(fig, "costs.svg")


def quintiles(res):
    q = res["by_move_quintile"]
    keys = ["1", "2", "3", "4", "5"]
    v = [q[k]["mean_net_bp"] for k in keys]
    fig, ax = plt.subplots(figsize=(7, 3.6))
    ax.bar(range(5), v, 0.6, color=BLUE)
    for i, val in enumerate(v):
        ax.annotate(f"{val:+.1f}", (i, val), xytext=(0, 3 if val >= 0 else -11), textcoords="offset points",
                    ha="center", fontsize=9, color=TEXT_2)
    ax.axhline(0, color=MUTED, linewidth=0.8)
    ax.set_xticks(range(5), [f"Q{k}\n|move| ~{q[k]['median_abs_move_bp']:.0f} bp" for k in keys])
    ax.set_title("Mean net daily return by quintile of |open to close|")
    ax.set_ylabel("bp per session")
    save(fig, "move_quintiles.svg")


def units(res):
    u = res["by_units"]["full"]
    keys = ["1", "2", "3"]
    v = [u[k]["net_dollars"] for k in keys]
    fig, ax = plt.subplots(figsize=(6.4, 3.6))
    ax.bar(range(3), v, 0.55, color=[BLUE, BLUE, BLUE])
    for i, k in enumerate(keys):
        ax.annotate(f"{v[i]:+.3f}\n{u[k]['trades']} campaigns", (i, v[i]), xytext=(0, 4 if v[i] >= 0 else -26),
                    textcoords="offset points", ha="center", fontsize=9, color=TEXT_2)
    ax.axhline(0, color=MUTED, linewidth=0.8)
    ax.set_ylim(min(v) * 1.35, max(v) * 1.35)
    ax.set_xticks(range(3), ["1 unit", "2 units", "3 units"])
    ax.set_title("Net P&L by units in the campaign (starting equity 1)")
    ax.set_ylabel("Net P&L")
    save(fig, "units.svg")


def rolling():
    with open(HERE / "rolling_sharpe.csv", newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    d = [date.fromisoformat(r["session"]) for r in rows]
    v = [float(r["rolling_252_sharpe"]) for r in rows]
    fig, ax = plt.subplots(figsize=(9, 3.4))
    ax.plot(d, v, color=BLUE, linewidth=1.5)
    ax.axhline(0, color=MUTED, linewidth=0.8)
    oos_line(ax)
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def main():
    res, post, daily = load()
    equity_and_drawdown(daily)
    by_year(res)
    placebo(res)
    grid(res)
    costs(res)
    quintiles(res)
    units(res)
    rolling()
    print("figures written to", OUT)


if __name__ == "__main__":
    main()
