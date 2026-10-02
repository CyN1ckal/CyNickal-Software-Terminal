# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the SPY RSI(2) dip-buy report. Drawn only from saved outputs
(results.json, posthoc.json, daily.csv, trades.csv, rolling_sharpe.csv, placebo_*.npy).

    python research/spy-rsi2-dip-buy/research/charts.py
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
    fig.tight_layout()
    fig.savefig(OUT / name)
    plt.close(fig)


def mark_oos(ax) -> None:
    ax.axvline(OOS, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(),
            va="top", fontsize=9, color=TEXT_2)


def main() -> None:
    res = json.loads((HERE / "results.json").read_text())
    rows = list(csv.DictReader((HERE / "daily.csv").open()))
    trades = [t for t in csv.DictReader((HERE / "trades.csv").open()) if t["symbol"] == "SPY"]
    days = [date.fromisoformat(r["date"]) for r in rows]
    s = np.array([float(r["strategy_net"]) for r in rows])
    b = np.array([float(r["spy_bh"]) for r in rows])
    es, eb = np.cumprod(1 + s), np.cumprod(1 + b)

    # Equity.
    fig, ax = plt.subplots(figsize=(9, 4.4))
    ax.plot(days, eb, color=ORANGE, label="SPY buy and hold (uncosted)")
    ax.plot(days, es, color=BLUE, label="RSI(2) dip-buy, 1 bp per side")
    ax.text(days[-1], es[-1], f" {es[-1]:.2f}", color=TEXT, va="center", fontsize=9)
    ax.text(days[-1], eb[-1], f" {eb[-1]:.2f}", color=TEXT, va="center", fontsize=9)
    mark_oos(ax)
    ax.set_title("Growth of $1, SPY")
    ax.set_ylabel("equity")
    ax.legend(loc="upper left")
    save(fig, "equity.svg")

    # Drawdown.
    fig, ax = plt.subplots(figsize=(9, 3.6))
    for e, col, lab in ((eb, ORANGE, "SPY buy and hold"), (es, BLUE, "RSI(2) dip-buy")):
        pk = np.maximum.accumulate(np.concatenate([[1.0], e]))[1:]
        ax.plot(days, (e / pk - 1) * 100, color=col, label=lab, linewidth=1.5)
    mark_oos(ax)
    ax.set_title("Drawdown from running peak")
    ax.set_ylabel("%")
    ax.legend(loc="lower right")
    save(fig, "drawdown.svg")

    # Trade distribution and holding time.
    nets = np.array([float(t["net"]) for t in trades]) * 1e4
    holds = np.array([int(t["hold_sessions"]) for t in trades])
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(10, 4), gridspec_kw={"width_ratios": [1.3, 1]})
    bins = np.arange(-650, 350, 50)
    a1.hist(nets[nets > 0], bins=bins, color=BLUE, label=f"wins ({(nets > 0).sum()})", edgecolor=SURFACE, linewidth=1.5)
    a1.hist(nets[nets <= 0], bins=bins, color=ORANGE, label=f"losses ({(nets <= 0).sum()})", edgecolor=SURFACE, linewidth=1.5)
    a1.set_title("Net return per SPY trade")
    a1.set_xlabel("net trade return (bp)")
    a1.set_ylabel("trades")
    a1.legend(loc="upper left")
    hs = sorted(set(holds))
    avg = [nets[holds == h].mean() for h in hs]
    cnt = [(holds == h).sum() for h in hs]
    a2.bar([str(h) for h in hs], avg, color=[BLUE if v > 0 else ORANGE for v in avg], width=0.7)
    for k, (v, n) in enumerate(zip(avg, cnt)):
        a2.text(k, v + (12 if v > 0 else -12), f"n={n}", ha="center", va="bottom" if v > 0 else "top", fontsize=8, color=TEXT_2)
    a2.axhline(0, color=TEXT_2, linewidth=0.8)
    a2.set_title("Average net trade by sessions held")
    a2.set_xlabel("sessions held")
    a2.set_ylabel("bp")
    save(fig, "trade_distribution.svg")

    # By year.
    by = res["by_year"]
    yrs = [str(y["year"]) for y in by]
    x = np.arange(len(yrs))
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.bar(x - 0.18, [y["return"] * 100 for y in by], 0.34, color=BLUE, label="RSI(2) dip-buy")
    ax.bar(x + 0.18, [y["bh_return"] * 100 for y in by], 0.34, color=ORANGE, label="SPY buy and hold")
    ax.set_xticks(x, [f"{y}\n({n} trades)" for y, n in zip(yrs, [v["trades"] for v in by])])
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_title("Calendar-year return (2021 from 25 Oct, 2026 to 25 Sep)")
    ax.set_ylabel("%")
    ax.legend(loc="upper left", ncols=2)
    ax.set_ylim(-25, 32)
    save(fig, "by_year.svg")

    # Placebos.
    fig, axs = plt.subplots(1, 2, figsize=(10, 3.8))
    for ax, key, npy, title in ((axs[0], "direction_placebo", "placebo_direction.npy", "Direction placebo"),
                                (axs[1], "timing_placebo", "placebo_timing.npy", "Timing placebo")):
        d = np.load(HERE / npy)
        pl = res[key]
        ax.hist(d, bins=40, color=AQUA, edgecolor=SURFACE, linewidth=1)
        ax.axvline(pl["actual_gross_sharpe"], color=BLUE, linewidth=2)
        ax.axvline(pl["null_p95"], color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
        ax.text(pl["actual_gross_sharpe"], 1.0, f" actual {pl['actual_gross_sharpe']:.2f}\n p = {pl['p']:.3f}",
                transform=ax.get_xaxis_transform(), va="top", fontsize=9)
        ax.set_title(f"{title}: 2,000 draws")
        ax.set_xlabel("gross Sharpe (dashed: null 95th percentile)")
    save(fig, "placebo.svg")

    # Grid.
    g = res["grid"]
    rsis = sorted({c["entry_rsi"] for c in g})
    smas = sorted({c["exit_sma"] for c in g})
    fig, axs = plt.subplots(1, 2, figsize=(10, 3.6))
    for ax, key, title in ((axs[0], "is_sharpe", "In-sample Sharpe (plateau check)"),
                           (axs[1], "oos_sharpe", "Out-of-sample Sharpe (shown, not used)")):
        m = np.array([[next(c[key] for c in g if c["entry_rsi"] == r and c["exit_sma"] == sm) for r in rsis] for sm in smas])
        ax.imshow(m, cmap="Blues", vmin=0, vmax=1.6, aspect="auto")
        ax.grid(False)
        for i in range(len(smas)):
            for j in range(len(rsis)):
                prim = rsis[j] == 10 and smas[i] == 5
                ax.text(j, i, f"{m[i, j]:.2f}" + ("\nprimary" if prim else ""), ha="center", va="center", fontsize=9,
                        color="white" if m[i, j] > 1.0 else TEXT, fontweight="bold" if prim else "normal")
        ax.set_xticks(range(len(rsis)), [f"RSI<{r}" for r in rsis])
        ax.set_yticks(range(len(smas)), [f"SMA{sm}" for sm in smas])
        ax.set_title(title)
    save(fig, "grid.svg")

    # Costs.
    c = res["costs"]
    ks = sorted(c, key=float)
    fig, ax = plt.subplots(figsize=(7, 3.6))
    xs = [c[k]["cost_bps"] for k in ks]
    ax.plot(xs, [c[k]["full_sharpe"] for k in ks], color=BLUE, marker="o", markersize=8, label="full sample")
    ax.plot(xs, [c[k]["oos_sharpe"] for k in ks], color=ORANGE, marker="o", markersize=8, label="out of sample")
    ax.axhline(0.5, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(xs[-1], 0.5, "OOS threshold 0.5", ha="right", va="bottom", fontsize=9, color=TEXT_2)
    ax.set_ylim(0, 1.6)
    ax.set_xlabel("cost per side (bp)")
    ax.set_ylabel("Sharpe")
    ax.set_title("Sharpe against cost")
    ax.legend(loc="lower left")
    save(fig, "costs.svg")

    # Move quintiles.
    q = res["by_move_quintile"]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    x = np.arange(5)
    ax.bar(x - 0.18, [v["strategy_avg_bp"] for v in q], 0.34, color=BLUE, label="RSI(2) dip-buy")
    ax.bar(x + 0.18, [v["spy_avg_bp"] for v in q], 0.34, color=ORANGE, label="SPY")
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    ax.set_xticks(x, [f"Q{v['quintile']}" for v in q])
    ax.set_xlabel("quintile of SPY's close-to-close return that session (Q1 = worst)")
    ax.set_ylabel("average daily return (bp)")
    ax.set_title("Strategy return by SPY's daily move")
    ax.legend(loc="upper left")
    save(fig, "move_quintiles.svg")

    # Rolling Sharpe (post hoc).
    rr = list(csv.DictReader((HERE / "rolling_sharpe.csv").open()))
    rd = [date.fromisoformat(r["date"]) for r in rr]
    fig, ax = plt.subplots(figsize=(9, 3.6))
    ax.plot(rd, [float(r["spy"]) for r in rr], color=ORANGE, linewidth=1.5, label="SPY buy and hold")
    ax.plot(rd, [float(r["strategy"]) for r in rr], color=BLUE, linewidth=1.5, label="RSI(2) dip-buy")
    ax.axhline(0, color=TEXT_2, linewidth=0.8)
    mark_oos(ax)
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.legend(loc="upper left")
    save(fig, "rolling_sharpe.svg")


if __name__ == "__main__":
    main()
