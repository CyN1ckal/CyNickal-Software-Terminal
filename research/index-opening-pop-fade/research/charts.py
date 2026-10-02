# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the index opening-pop fade report. Drawn only from saved outputs
(results.json, posthoc.json, daily.csv). Nothing is recomputed from the store.

    python research/index-opening-pop-fade/research/charts.py
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
from matplotlib.colors import LinearSegmentedColormap, TwoSlopeNorm  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"
OOS = date(2024, 7, 1)
SURFACE, TEXT, TEXT_2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
BLUE, ORANGE, AQUA, YELLOW = "#2a78d6", "#eb6834", "#1baf7a", "#eda100"
RED, MID = "#e34948", "#f0efec"

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


def end_label(ax, x, y, text, color, dy: float = 0) -> None:
    ax.annotate(text, (x, y), xytext=(6, dy), textcoords="offset points", va="center",
                fontsize=9, color=TEXT)
    ax.plot([x], [y], "o", color=color, markersize=5, markeredgecolor=SURFACE, markeredgewidth=1.5)


def dd(eq):
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return eq / peak - 1


def main() -> None:
    res = json.loads((HERE / "results.json").read_text())
    post = json.loads((HERE / "posthoc.json").read_text())
    rows = list(csv.DictReader((HERE / "daily.csv").open()))
    days = [date.fromisoformat(r["session"]) for r in rows]
    col = lambda k: np.array([float(r[k]) for r in rows])  # noqa: E731
    series = [("Primary (open → 10:00 pop)", col("primary_net"), BLUE),
              ("S1 (prior close → 10:00 pop)", col("s1_net"), ORANGE),
              ("SPY buy and hold", col("spy_buy_hold"), AQUA),
              ("Short SPY 10:00 → close every day", col("spy_uncond_short_gross"), YELLOW)]
    P, S1 = res["primary"], res["s1"]

    # Equity
    fig, ax = plt.subplots(figsize=(10, 5))
    for name, r, c in series:
        eq = np.cumprod(1 + r)
        ax.plot(days, eq, color=c, label=name, linewidth=2 if c in (BLUE, ORANGE) else 1.5)
        end_label(ax, days[-1], eq[-1], f"{eq[-1]:.2f}", c, dy=-7 if c == YELLOW else (5 if c == BLUE else 0))
    ax.axhline(1, color=TEXT_2, linewidth=0.8)
    mark_oos(ax)
    ax.set_title("Growth of $1: SPY opening-pop short vs. benchmarks (1 bp per side)")
    ax.set_ylabel("Equity (start = 1)")
    ax.legend(loc="upper left", fontsize=9)
    ax.set_xlim(days[0], date(2027, 1, 20))
    save(fig, "equity.svg")

    # Drawdown
    fig, ax = plt.subplots(figsize=(10, 4))
    for name, r, c in series[:3]:
        ax.plot(days, dd(np.cumprod(1 + r)) * 100, color=c, label=name, linewidth=1.5)
    mark_oos(ax)
    ax.set_title("Drawdown of compounded equity")
    ax.set_ylabel("%")
    ax.legend(loc="lower left", fontsize=9)
    save(fig, "drawdown.svg")

    # By year
    by = P["breakdowns"]["by_year"]
    by1 = S1["breakdowns"]["by_year"]
    years = [y for y in by if by[y]["trades"]]
    x = np.arange(len(years))
    w = 0.26
    fig, ax = plt.subplots(figsize=(9, 4.2))
    for k, (lab, vals, c) in enumerate([
            ("Primary", [by[y]["return"] * 100 for y in years], BLUE),
            ("S1", [by1[y]["return"] * 100 for y in years], ORANGE),
            ("SPY buy and hold", [by[y]["bench_return"] * 100 for y in years], AQUA)]):
        ax.bar(x + (k - 1) * w, vals, w - 0.03, color=c, label=lab)
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_xticks(x, [f"{y}\n({by[y]['trades']} trades)" for y in years])
    ax.set_title("Calendar-year return (2026 through 25 Sep)")
    ax.set_ylabel("%")
    ax.legend(fontsize=9, loc="upper right")
    save(fig, "by_year.svg")

    # Placebos
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=True)
    for ax, key, title in ((axes[0], "direction_placebo", "Direction placebo (random side)"),
                           (axes[1], "timing_placebo", "Timing placebo (random sessions)")):
        d = np.array(P[key]["draws"])
        ax.hist(d, bins=50, color=BLUE, alpha=0.55, label="2,000 null draws")
        ax.axvline(P[key]["actual_gross_sharpe"], color=TEXT, linewidth=2)
        ax.text(P[key]["actual_gross_sharpe"], 0.95, f" actual {P[key]['actual_gross_sharpe']:.2f}\n p = {P[key]['p']:.3f}",
                transform=ax.get_xaxis_transform(), va="top", fontsize=9,
                bbox=dict(boxstyle="square,pad=0.2", facecolor=SURFACE, edgecolor="none"))
        ax.set_title(title, fontsize=11)
        ax.set_xlabel("Full-sample gross Sharpe")
    axes[0].set_ylabel("Draws")
    save(fig, "placebo.svg")

    # Grid heatmaps
    cmap = LinearSegmentedColormap.from_list("div", [RED, MID, BLUE])
    fig, axes = plt.subplots(2, 2, figsize=(10, 6.2))
    zs = sorted({g["z"] for g in P["grid"]})
    ts = sorted({g["t"] for g in P["grid"]})
    for i, (cand, nm) in enumerate(((P, "Primary"), (S1, "S1"))):
        for j, key in enumerate(("is_sharpe", "oos_sharpe")):
            ax = axes[i, j]
            M = np.array([[next(g[key] for g in cand["grid"] if g["t"] == t and g["z"] == z) for z in zs] for t in ts])
            ax.imshow(M, cmap=cmap, norm=TwoSlopeNorm(0, -1.6, 1.6), aspect="auto")
            for a in range(len(ts)):
                for b in range(len(zs)):
                    star = "*" if ts[a] == "10:00" and zs[b] == 1.0 else ""
                    ax.text(b, a, f"{M[a, b]:.2f}{star}", ha="center", va="center", fontsize=9, color=TEXT)
            ax.set_xticks(range(len(zs)), [str(z) for z in zs])
            ax.set_yticks(range(len(ts)), ts)
            ax.grid(False)
            ax.set_title(f"{nm}: {'in-sample' if j == 0 else 'out-of-sample (not used to select)'}", fontsize=10)
            ax.set_xlabel("Threshold Z (trailing RMS units)")
            ax.set_ylabel("Decision time")
    fig.suptitle("Sharpe by grid cell, 1 bp per side (* = locked rule)", x=0.01, ha="left",
                 fontweight="bold", fontsize=12)
    save(fig, "grid.svg")

    # Costs
    mults = ["0.0", "0.5", "1.0", "2.0", "3.0"]
    fig, ax = plt.subplots(figsize=(8, 4))
    for cand, nm, c in ((P, "Primary", BLUE), (S1, "S1", ORANGE)):
        for wdw, ls in (("full", "-"), ("OOS", (0, (4, 2)))):
            ax.plot([float(m) for m in mults], [cand["costs"][m][wdw]["sharpe"] for m in mults], color=c,
                    linestyle=ls, marker="o", markersize=5, label=f"{nm}, {'full sample' if wdw == 'full' else 'OOS'}")
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.axhline(0.5, color=TEXT_2, linewidth=0.8, linestyle=(0, (1, 2)))
    ax.text(3.0, 0.5, "OOS bar 0.5 ", ha="right", va="bottom", fontsize=8, color=TEXT_2)
    ax.set_xticks([0, 0.5, 1, 2, 3], ["0", "0.5", "1 (base)", "2", "3"])
    ax.set_xlabel("Cost, bp per side")
    ax.set_ylabel("Sharpe")
    ax.set_title("Cost sensitivity")
    ax.legend(fontsize=9, ncol=2, loc="center")
    save(fig, "costs.svg")

    # Move quintiles
    q = P["breakdowns"]["by_open_close_quintile"]
    q1 = S1["breakdowns"]["by_open_close_quintile"]
    ks = list(q)
    x = np.arange(len(ks))
    fig, ax = plt.subplots(figsize=(9, 4.2))
    ax.bar(x - 0.2, [q[k]["mean_strategy_day_bp"] for k in ks], 0.37, color=BLUE, label="Primary")
    ax.bar(x + 0.2, [q1[k]["mean_strategy_day_bp"] for k in ks], 0.37, color=ORANGE, label="S1")
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_xticks(x, [f"{k}\n{q[k]['range_bp'][0]:.0f} to {q[k]['range_bp'][1]:.0f} bp\n{q[k]['trades']} / {q1[k]['trades']} trades"
                      for k in ks], fontsize=8)
    ax.set_ylabel("Mean strategy return per session, bp")
    ax.set_title("By quintile of SPY's open-to-close move (all sessions, flat days = 0)")
    ax.legend(fontsize=9)
    save(fig, "move_quintiles.svg")

    # Rolling Sharpe (post hoc)
    fig, ax = plt.subplots(figsize=(10, 4))
    for key, nm, c in (("primary_net", "Primary SPY", BLUE), ("s1_net", "S1 SPY", ORANGE),
                       ("qqq_net", "Primary on QQQ", AQUA)):
        rs = post[key]["rolling_252"]
        dts = [date.fromisoformat(d) for d in rs["sessions"]]
        v = np.array([np.nan if s is None else s for s in rs["sharpe"]])
        ax.plot(dts, v, color=c, label=nm, linewidth=1.5)
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    mark_oos(ax)
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.legend(fontsize=9, loc="lower left")
    save(fig, "rolling_sharpe.svg")
    print("figures written to", OUT)


if __name__ == "__main__":
    main()
