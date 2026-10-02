# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/qqq-strategy-portfolio/report/. Run after backtest.py and posthoc.py.

    python research/qqq-strategy-portfolio/research/charts.py

Every figure is drawn from results.json, posthoc.json, or daily.csv. Nothing is recomputed.
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
PORTS = [("A", "A: IS max-Sharpe, 4 sleeves", BLUE), ("B", "B: IS max-Sharpe, QQQ + P1", ORANGE),
         ("N", "N: QQQ 1× + P1 1× (benchmark)", AQUA), ("Q", "QQQ buy & hold", YELLOW)]

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
    ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(), va="top", fontsize=9, color=TEXT_2)


def end_labels(ax, x_end, ys: list[tuple[float, str, str]], fmt) -> None:
    """Direct labels at the right edge, nudged apart so they never collide."""
    ys = sorted(ys)
    lo, hi = ax.get_ylim()
    gap = (hi - lo) * 0.06
    placed: list[float] = []
    for y, text, color in ys:
        y2 = max(y, placed[-1] + gap) if placed else y
        placed.append(y2)
        ax.annotate(fmt(y, text), (x_end, y), xytext=(x_end, y2), textcoords="data", fontsize=9, color=TEXT,
                    va="center", ha="left")
        ax.plot([x_end], [y], marker="o", markersize=5, color=color, markeredgecolor=SURFACE, markeredgewidth=1.5)


def main() -> None:
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    ph = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    with (HERE / "daily.csv").open(encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    days = [date.fromisoformat(r["session"]) for r in rows]
    s = {k: np.array([float(r[k]) for r in rows]) for k, _, _ in PORTS}

    # 1. growth of $1 (log scale)
    fig, ax = plt.subplots(figsize=(9, 4.4))
    ends = []
    for k, label, color in PORTS:
        eq = np.cumprod(1 + s[k])
        ax.plot(days, eq, color=color, label=label, linewidth=2)
        ends.append((float(eq[-1]), k, color))
    ax.set_yscale("log")
    ax.set_yticks([0.7, 1, 1.5, 2, 3])
    ax.set_yticklabels(["$0.70", "$1.00", "$1.50", "$2.00", "$3.00"])
    ax.minorticks_off()
    mark_oos(ax)
    ax.set_xlim(days[0], date(2027, 2, 15))
    end_labels(ax, days[-1], ends, lambda y, k: f"  {k} ${y:.2f}")
    ax.set_title("Growth of $1, net of costs (weights fitted on in-sample only)")
    ax.legend(loc="upper left", fontsize=9)
    save(fig, "equity.svg")

    # 2. drawdown
    fig, ax = plt.subplots(figsize=(9, 3.6))
    for k, label, color in PORTS:
        eq = np.cumprod(1 + s[k])
        dd = eq / np.maximum.accumulate(np.maximum(eq, 1.0)) - 1
        ax.plot(days, dd * 100, color=color, label=label, linewidth=1.6)
    mark_oos(ax)
    ax.set_ylabel("drawdown, %")
    ax.set_title("Drawdown from peak")
    ax.legend(loc="lower right", fontsize=9, ncol=2)
    save(fig, "drawdown.svg")

    # 3. overlay frontier (post hoc)
    fr = ph["overlay_frontier_q1x"]
    x = [f["w_p1"] for f in fr]
    fig, ax = plt.subplots(figsize=(8, 4.2))
    lines = [("is_sharpe", "in-sample", BLUE), ("oos_sharpe", "out of sample", ORANGE),
             ("oos_sharpe_2x", "out of sample, 2× cost", AQUA)]
    ends = []
    for key, label, color in lines:
        y = [f[key] for f in fr]
        ax.plot(x, y, color=color, label=label)
        ends.append((y[-1], label, color))
    ax.axhline(res["metrics"]["Q"]["OOS"]["sharpe"], color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(0.25, res["metrics"]["Q"]["OOS"]["sharpe"] - 0.015, "QQQ alone, OOS", color=TEXT_2, fontsize=9, va="top")
    ax.set_xlim(0, 2.55)
    end_labels(ax, x[-1], ends, lambda y, t: f"  {y:.2f}")
    ax.set_xlabel("P1 notional overlaid on a 1× QQQ holding (× equity)")
    ax.set_ylabel("Sharpe")
    ax.set_title("(post hoc) QQQ held at 1×, plus P1 intraday at 0–2×")
    ax.legend(loc="lower right", fontsize=9)
    save(fig, "frontier.svg")

    # 4. cost sweep, OOS Sharpe
    mults = ["0.0", "0.5", "1.0", "2.0", "3.0"]
    fig, ax = plt.subplots(figsize=(8, 4.0))
    ends = []
    for k, label, color in PORTS[:3]:
        y = [res["cost_sweep"][k][m]["oos_sharpe"] for m in mults]
        ax.plot([float(m) for m in mults], y, color=color, label=label, marker="o", markersize=6,
                markeredgecolor=SURFACE, markeredgewidth=1.5)
        ends.append((y[-1], k, color))
    q = res["metrics"]["Q"]["OOS"]["sharpe"]
    ax.axhline(q, color=YELLOW, linewidth=2, label="QQQ buy & hold (no strategy cost)")
    ax.set_xticks([0, 0.5, 1, 2, 3])
    ax.set_xticklabels(["0×", "0.5×", "1× (base)", "2×", "3×"])
    ax.set_xlim(-0.1, 3.45)
    end_labels(ax, 3.0, ends, lambda y, k: f"  {k} {y:.2f}")
    ax.set_xlabel("strategy cost, multiple of each sleeve's base cost")
    ax.set_ylabel("OOS Sharpe")
    ax.set_title("Out-of-sample Sharpe vs cost")
    ax.legend(loc="lower left", fontsize=9)
    save(fig, "costs.svg")
    print("figures written to", OUT)


if __name__ == "__main__":
    main()
