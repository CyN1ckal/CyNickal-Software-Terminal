# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/qqq-intraday-trend/report/. Run after backtest.py and posthoc.py."""

from __future__ import annotations

import csv
import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("svg")
import matplotlib.dates as mdates  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap, TwoSlopeNorm  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"

# Reference palette (dataviz skill), light surface.
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"
RED = "#e34948"
NEUTRAL = "#f0efec"
IS_SHADE = "#f3f2ee"
OOS_START = date(2024, 7, 1)

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
    rows = list(csv.DictReader(open(HERE / "daily.csv", encoding="utf-8")))
    days = [date.fromisoformat(r["session"]) for r in rows]
    series = {k: np.array([float(r[k]) for r in rows]) for k in ("p1", "p2", "hold", "open_to_close")}
    return res, post, days, series


def mark_oos(ax, label=True) -> None:
    ax.axvline(OOS_START, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    if label:
        ax.text(OOS_START, 1.0, "  out-of-sample →", transform=ax.get_xaxis_transform(),
                va="top", ha="left", fontsize=9, color=TEXT_2)


def end_label(ax, x, y, text, color, dy=0) -> None:
    ax.annotate(text, (x, y), xytext=(6, dy), textcoords="offset points", va="center",
                fontsize=9, color=TEXT, fontweight="bold")
    ax.plot([x], [y], "o", color=color, markersize=6, markeredgecolor=SURFACE, markeredgewidth=2)


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT / f"{name}.svg", bbox_inches="tight")
    plt.close(fig)


def equity(days, s) -> None:
    fig, ax = plt.subplots(figsize=(9, 4.6))
    curves = [("QQQ buy & hold", s["hold"], ORANGE), ("P2 vol-targeted", s["p2"], AQUA),
              ("P1 noise-boundary, 1×", s["p1"], BLUE)]
    ends = {name: np.cumprod(1 + r)[-1] for name, r, _ in curves}
    for name, r, color in curves:
        eq = np.cumprod(1 + r)
        ax.plot(days, eq, color=color, label=name)
        others = [v for k, v in ends.items() if k != name and abs(np.log(v / eq[-1])) < 0.05]
        dy = 0 if not others else (7 if eq[-1] > max(others) else -7)
        end_label(ax, days[-1], eq[-1], f"{eq[-1]:.2f}×", color, dy)
    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:.1f}×"))
    ax.yaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    ax.set_yticks([0.7, 1.0, 1.5, 2.0])
    mark_oos(ax)
    ax.set_title("Growth of $1, after 1 bp per side (log scale)")
    ax.legend(loc="lower right", ncols=1, fontsize=9)
    ax.xaxis.set_major_locator(mdates.YearLocator())
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
    ax.set_xlim(days[0], days[-1] + (days[-1] - days[0]) * 0.07)
    save(fig, "equity")


def drawdown(days, s) -> None:
    fig, ax = plt.subplots(figsize=(9, 2.8))
    for name, r, color in (("QQQ buy & hold", s["hold"], ORANGE), ("P1", s["p1"], BLUE)):
        eq = np.cumprod(1 + r)
        dd = eq / np.maximum.accumulate(eq) - 1
        ax.plot(days, dd * 100, color=color, label=name, linewidth=1.6)
        i = int(np.argmin(dd))
        ax.annotate(f"{dd[i]:.1%}", (days[i], dd[i] * 100), xytext=(6, -2), textcoords="offset points",
                    fontsize=9, color=TEXT, va="top")
    mark_oos(ax, label=False)
    ax.set_title("Drawdown from peak (%)")
    ax.legend(loc="lower right", ncols=2, fontsize=9)
    ax.xaxis.set_major_locator(mdates.YearLocator())
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
    save(fig, "drawdown")


def rolling(post) -> None:
    roll = post["rolling_252"]
    d = [date.fromisoformat(x["session"]) for x in roll]
    v = np.array([x["p1"] for x in roll])
    fig, ax = plt.subplots(figsize=(9, 2.8))
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.plot(d, v, color=BLUE)
    i = int(np.argmin(v))
    ax.annotate(f"{v[i]:+.2f} (12 months to {d[i]:%b %Y})", (d[i], v[i]), xytext=(8, 0),
                textcoords="offset points", fontsize=9, va="center")
    ax.plot([d[i]], [v[i]], "o", color=BLUE, markersize=6, markeredgecolor=SURFACE, markeredgewidth=2)
    mark_oos(ax)
    ax.set_title("P1 trailing 252-session Sharpe")
    ax.xaxis.set_major_locator(mdates.YearLocator())
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
    save(fig, "rolling_sharpe")


def quintiles(res) -> None:
    rows = res["P1_by_move_quintile"]
    labels = [f"Q{r['quintile']}\n{r['abs_open_to_close_bps_lo']:.0f}–{r['abs_open_to_close_bps_hi']:.0f} bp"
              for r in rows]
    labels[-1] = f"Q5\n≥{rows[-1]['abs_open_to_close_bps_lo']:.0f} bp"
    v = [r["avg_strategy_bps"] for r in rows]
    fig, ax = plt.subplots(figsize=(7.5, 3.4))
    ax.bar(labels, v, color=[BLUE if x > 0 else RED for x in v], width=0.6, edgecolor=SURFACE, linewidth=2)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for i, x in enumerate(v):
        ax.annotate(f"{x:+.1f}", (i, x), xytext=(0, 4 if x > 0 else -4), textcoords="offset points",
                    ha="center", va="bottom" if x > 0 else "top", fontsize=9)
    ax.set_title("P1 average day (bp) by size of QQQ's open-to-close move")
    ax.set_xlabel("Quintile of |open-to-close| move, full sample")
    ax.grid(axis="x", visible=False)
    save(fig, "move_quintiles")


def costs(res) -> None:
    rows = res["P1_costs"]
    x = [r["bps"] for r in rows]
    fig, ax = plt.subplots(figsize=(7.5, 3.2))
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for key, name, color in (("full_sharpe", "Full sample", BLUE), ("oos_sharpe", "Out-of-sample", ORANGE)):
        y = [r[key] for r in rows]
        ax.plot(x, y, color=color, marker="o", markersize=7, markeredgecolor=SURFACE, markeredgewidth=2, label=name)
        ax.annotate(name, (x[-1], y[-1]), xytext=(8, 0), textcoords="offset points", va="center",
                    fontsize=9, fontweight="bold")
    ax.axvline(1.0, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(1.0, 1.0, " assumed", transform=ax.get_xaxis_transform(), va="top", fontsize=9, color=TEXT_2)
    ax.set_xticks(x)
    ax.set_xlabel("Cost per side (bp of notional)")
    ax.set_title("P1 Sharpe as trading costs rise")
    ax.set_xlim(-0.2, 3.9)
    ax.legend(loc="upper right", fontsize=9)
    save(fig, "costs")


def grid(res) -> None:
    g = [x for x in res["grid"] if x["spacing"] == 30]
    lbs = sorted({x["lookback"] for x in g})
    bands = sorted({x["band"] for x in g})
    cmap = LinearSegmentedColormap.from_list("div", [RED, NEUTRAL, BLUE])
    fig, axes = plt.subplots(1, 2, figsize=(9.5, 3.4), sharey=True)
    lim = max(abs(x[k]) for x in g for k in ("is_sharpe", "oos_sharpe"))
    norm = TwoSlopeNorm(0, -lim, lim)
    for ax, key, title in ((axes[0], "is_sharpe", "In-sample Sharpe"), (axes[1], "oos_sharpe", "Out-of-sample Sharpe")):
        m = np.array([[next(x[key] for x in g if x["lookback"] == lb and x["band"] == b) for b in bands] for lb in lbs])
        ax.imshow(m, cmap=cmap, norm=norm, aspect="auto")
        for i in range(len(lbs)):
            for j in range(len(bands)):
                default = lbs[i] == 14 and bands[j] == 1.0
                ax.text(j, i, f"{m[i, j]:.2f}", ha="center", va="center", fontsize=9,
                        fontweight="bold" if default else "normal", color=TEXT)
                if default:
                    ax.add_patch(plt.Rectangle((j - 0.5, i - 0.5), 1, 1, fill=False, edgecolor=TEXT, linewidth=2))
        ax.set_xticks(range(len(bands)), [f"{b:g}" for b in bands])
        ax.set_yticks(range(len(lbs)), [str(x) for x in lbs])
        ax.set_xlabel("Band multiplier V")
        ax.set_title(title)
        ax.grid(False)
    axes[0].set_ylabel("Lookback L (sessions)")
    fig.text(0.01, -0.06, "30-minute decisions; outlined cell = pre-registered default (L 14, V 1).",
             ha="left", fontsize=9, color=TEXT_2)
    save(fig, "grid")


def placebo(res) -> None:
    p = res["P1_placebo"]
    edges = np.array(p["hist_edges"])
    fig, ax = plt.subplots(figsize=(7.5, 3.0))
    ax.bar(edges[:-1], p["hist"], width=np.diff(edges), align="edge", color="#b9b8b2", edgecolor=SURFACE, linewidth=1)
    ax.axvline(p["actual_sharpe"], color=BLUE, linewidth=2)
    ax.annotate(f"actual {p['actual_sharpe']:.2f}", (p["actual_sharpe"], 1.0), xycoords=("data", "axes fraction"),
                xytext=(-6, -4), textcoords="offset points", ha="right", va="top", fontsize=9, fontweight="bold")
    ax.set_title(f"Same trade times, random direction ({p['draws']:,} draws)")
    ax.set_xlabel("Full-sample Sharpe after 1 bp per side")
    ax.set_ylabel("Draws")
    ax.grid(axis="x", visible=False)
    save(fig, "placebo")


def by_entry(res) -> None:
    rows = res["P1_by_entry"]
    labels = [r["entry"] for r in rows]
    v = [r["sum_net"] * 100 for r in rows]
    fig, ax = plt.subplots(figsize=(8.5, 3.0))
    ax.bar(labels, v, color=[BLUE if x > 0 else RED for x in v], width=0.6, edgecolor=SURFACE, linewidth=2)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for i, (x, r) in enumerate(zip(v, rows)):
        ax.annotate(f"{r['trades']}", (i, x), xytext=(0, 3 if x > 0 else -3), textcoords="offset points",
                    ha="center", va="bottom" if x > 0 else "top", fontsize=8, color=TEXT_2)
    ax.set_title("P1 summed net return by entry time (% of equity; label = trades)")
    ax.grid(axis="x", visible=False)
    save(fig, "by_entry")


def by_year(res) -> None:
    rows = res["P1_by_year"]
    years = [str(r["year"]) + ("*" if r["year"] in (2021, 2026) else "") for r in rows]
    a = [r["total_return"] * 100 for r in rows]
    b = [r["hold_return"] * 100 for r in rows]
    x = np.arange(len(years))
    fig, ax = plt.subplots(figsize=(8, 3.2))
    ax.bar(x - 0.19, a, 0.36, color=BLUE, label="P1", edgecolor=SURFACE, linewidth=2)
    ax.bar(x + 0.19, b, 0.36, color=ORANGE, label="QQQ buy & hold", edgecolor=SURFACE, linewidth=2)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for i, v in enumerate(a):
        ax.annotate(f"{v:+.1f}", (i - 0.19, v), xytext=(0, 3 if v > 0 else -3), textcoords="offset points",
                    ha="center", va="bottom" if v > 0 else "top", fontsize=8)
    ax.set_xticks(x, years)
    ax.set_title("Calendar-year return (%)  * partial year")
    ax.legend(loc="lower right", ncols=2, fontsize=9)
    ax.grid(axis="x", visible=False)
    save(fig, "by_year")


def main() -> None:
    res, post, days, s = load()
    equity(days, s)
    drawdown(days, s)
    rolling(post)
    quintiles(res)
    costs(res)
    grid(res)
    placebo(res)
    by_entry(res)
    by_year(res)
    print("wrote", sorted(p.name for p in OUT.glob("*.svg")))


if __name__ == "__main__":
    main()
