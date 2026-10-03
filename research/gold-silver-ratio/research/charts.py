# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures from results.json, daily.csv, posthoc.json, and placebo_direction.npy.

Does not read the store and does not rerun the book.
"""

from __future__ import annotations

import csv
import json
from datetime import datetime
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
FIG = HERE.parent / "report" / "figures"
FIG.mkdir(parents=True, exist_ok=True)

res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
ph = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
    daily = list(csv.DictReader(handle))
with (HERE / "rolling_sharpe.csv").open(encoding="utf-8", newline="") as handle:
    rolling = list(csv.DictReader(handle))

dates = [datetime.strptime(row["date"], "%Y-%m-%d") for row in daily]
net = np.array([float(row["strategy_net"]) for row in daily])
gld = np.array([float(row["gld_bh"]) for row in daily])
slv = np.array([float(row["slv_bh"]) for row in daily])
SPLIT = datetime(2024, 7, 1)
PRIMARY = res["primary"]
BENCH = res["benchmarks"]


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, format="svg")
    plt.close(fig)


def mark(ax):
    ax.axvline(SPLIT, color="black", ls="--", lw=0.8)
    ax.grid(alpha=0.3)
    ax.tick_params(labelsize=8)


eq_s = np.cumprod(1.0 + net)
eq_g = np.cumprod(1.0 + gld)
eq_v = np.cumprod(1.0 + slv)
fig, ax = plt.subplots(figsize=(8.2, 4.2))
ax.plot(dates, eq_s, label="strategy, net, 1 bp/side", lw=1.3, color="#1f4e79")
ax.plot(dates, eq_g, label="GLD buy and hold, uncosted", lw=1.0, color="#c65911")
ax.plot(dates, eq_v, label="SLV buy and hold, uncosted", lw=1.0, color="#548235")
mark(ax)
ax.set_ylabel("growth of $1")
ax.set_title(
    f"Growth of $1, full evaluation sample. Dashed line is 2024-07-01. "
    f"Strategy ends at {eq_s[-1]:.3f}; GLD at {eq_g[-1]:.3f}; SLV at {eq_v[-1]:.3f}. "
    f"OOS strategy return {PRIMARY['oos']['total_return']:.1%}.",
    fontsize=9,
)
ax.legend(fontsize=8, loc="upper left")
save(fig, "equity.svg")

fig, ax = plt.subplots(figsize=(8.2, 3.8))
for series, lab, color in (
    (net, "strategy", "#1f4e79"),
    (gld, "GLD", "#c65911"),
    (slv, "SLV", "#548235"),
):
    equity = np.cumprod(1.0 + series)
    ax.plot(dates, equity / np.maximum.accumulate(equity) - 1.0, label=lab, lw=1.0, color=color)
mark(ax)
ax.set_ylabel("drawdown")
ax.set_title(
    f"Drawdown from each series' own peak, full sample. "
    f"Strategy {PRIMARY['full']['max_dd']:.1%}, "
    f"GLD {BENCH['gld']['full']['max_dd']:.1%}, "
    f"SLV {BENCH['slv']['full']['max_dd']:.1%}. "
    f"OOS strategy drawdown {PRIMARY['oos']['max_dd']:.1%}.",
    fontsize=9,
)
ax.legend(fontsize=8)
save(fig, "drawdown.svg")

years = res["by_year"]
labels = [str(row["year"]) for row in years]
x = np.arange(len(years))
width = 0.25
fig, ax = plt.subplots(figsize=(8.6, 4.2))
ax.bar(x - width, [row["total_return"] for row in years], width, label="strategy", color="#1f4e79")
ax.bar(x, [row["gld_return"] for row in years], width, label="GLD", color="#c65911")
ax.bar(x + width, [row["slv_return"] for row in years], width, label="SLV", color="#548235")
ax.axhline(0, color="black", lw=0.6)
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=45, fontsize=8)
ax.set_ylabel("compounded return")
ax.set_title(
    "Calendar-year return. 2024 straddles 2024-07-01. "
    f"2025 strategy {years[-2]['total_return']:.1%} against SLV {years[-2]['slv_return']:.1%}.",
    fontsize=9,
)
ax.legend(fontsize=8)
ax.grid(axis="y", alpha=0.3)
save(fig, "by_year.svg")

draws = np.load(HERE / "placebo_direction.npy")
finite = draws[np.isfinite(draws)]
actual = res["direction_placebo"]["actual"]
fig, ax = plt.subplots(figsize=(8.2, 4.0))
ax.hist(finite, bins=40, color="#8fa6bf", edgecolor="white")
ax.axvline(actual, color="#1f4e79", lw=1.4, label=f"actual gross Sharpe {actual:.3f}")
ax.set_xlabel("full-sample gross Sharpe")
ax.set_ylabel("draws")
timing = res["timing_placebo"]
ax.set_title(
    f"Direction placebo, 2,000 sign flips, seed 20261081. "
    f"p {res['direction_placebo']['p']:.3f}. "
    f"Timing placebo not testable: {timing['accepted']} of {timing['attempts']} draws accepted.",
    fontsize=9,
)
ax.legend(fontsize=8)
ax.grid(axis="y", alpha=0.3)
save(fig, "placebo.svg")

grid = res["grid"]
fig, ax = plt.subplots(figsize=(7.4, 4.0))
xs = [row["entry"] for row in grid]
ax.plot(xs, [row["is_sharpe"] for row in grid], marker="o", color="#1f4e79", label="IS Sharpe")
ax.plot(xs, [row["oos_sharpe"] for row in grid], marker="o", color="#c65911", label="OOS Sharpe")
ax.axhline(0, color="black", lw=0.6)
ax.axvline(2.0, color="black", ls="--", lw=0.8)
ax.set_xlabel("entry |z| threshold")
ax.set_ylabel("Sharpe")
ax.set_title(
    f"Entry grid, window 60, exit 0. Dashed line is the primary E=2. "
    f"{res['grid_positive_is_cells']} of 5 IS cells have Sharpe > 0. Nothing was selected.",
    fontsize=9,
)
ax.legend(fontsize=8)
ax.grid(alpha=0.3)
save(fig, "grid.svg")

costs = res["costs"]
fig, ax = plt.subplots(figsize=(7.4, 4.0))
m = [row["multiple"] for row in costs]
ax.plot(m, [row["full_sharpe"] for row in costs], marker="o", color="#1f4e79", label="full Sharpe")
ax.plot(m, [row["oos_sharpe"] for row in costs], marker="o", color="#c65911", label="OOS Sharpe")
ax.axhline(0, color="black", lw=0.6)
ax.set_xlabel("cost multiple of 1 bp per side")
ax.set_ylabel("Sharpe")
ax.set_title(
    f"Cost sweep. 2× full return {res['costs'][3]['full_return']:.1%}. "
    f"3× full return {res['costs'][4]['full_return']:.1%}.",
    fontsize=9,
)
ax.legend(fontsize=8)
ax.grid(alpha=0.3)
save(fig, "costs.svg")

q = res["quintiles"]
fig, ax = plt.subplots(figsize=(7.4, 4.0))
idx = np.arange(1, 6)
width = 0.35
ax.bar(idx - width / 2, [row["mean_strategy"] for row in q], width, label="strategy", color="#1f4e79")
ax.bar(idx + width / 2, [row["mean_gld"] for row in q], width, label="GLD", color="#c65911")
ax.axhline(0, color="black", lw=0.6)
ax.set_xticks(idx)
ax.set_xlabel("GLD close-to-close quintile, 1 = worst")
ax.set_ylabel("mean daily return")
ax.set_title(
    "Mean daily return by GLD-move quintile. "
    f"Strategy mean is {q[0]['mean_strategy']:+.4f} in the worst GLD quintile "
    f"and {q[4]['mean_strategy']:+.4f} in the best.",
    fontsize=9,
)
ax.legend(fontsize=8)
ax.grid(axis="y", alpha=0.3)
save(fig, "move_quintiles.svg")

rdates = [datetime.strptime(row["date"], "%Y-%m-%d") for row in rolling]
fig, ax = plt.subplots(figsize=(8.2, 3.8))
ax.plot(rdates, [float(row["strategy"]) for row in rolling], label="strategy", lw=1.1, color="#1f4e79")
ax.plot(rdates, [float(row["gld"]) for row in rolling], label="GLD", lw=1.0, color="#c65911")
ax.plot(rdates, [float(row["slv"]) for row in rolling], label="SLV", lw=1.0, color="#548235")
ax.axhline(0, color="black", lw=0.6)
mark(ax)
roll = ph["rolling_252"]
ax.set_ylabel("252-session Sharpe")
ax.set_title(
    f"Trailing 252-session Sharpe (post hoc). "
    f"{roll['fraction_positive']:.1%} of strategy windows are positive. "
    f"Range {roll['min']:.2f} to {roll['max']:.2f}.",
    fontsize=9,
)
ax.legend(fontsize=8)
save(fig, "rolling_sharpe.svg")
print(f"wrote {len(list(FIG.glob('*.svg')))} figures")
