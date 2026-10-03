# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures from results.json, daily.csv, posthoc.json, and placebo_direction.npy.

Does not read the store and does not recompute the book.
"""

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
with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
    daily = list(csv.DictReader(f))
with (HERE / "rolling_sharpe.csv").open(encoding="utf-8", newline="") as f:
    roll = list(csv.DictReader(f))

dates = [datetime.strptime(r["date"], "%Y-%m-%d") for r in daily]
net = np.array([float(r["strategy_net"]) for r in daily])
eem = np.array([float(r["eem_otc"]) for r in daily])
spy = np.array([float(r["spy_c2c"]) for r in daily])
SPLIT = datetime(2024, 7, 1)
STRAT = "#1f4e79"
EEM = "#548235"
SPY = "#c65911"


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, format="svg")
    plt.close(fig)


def mark(ax):
    ax.axvline(SPLIT, color="black", ls="--", lw=0.8)
    ax.grid(alpha=0.3)
    ax.tick_params(labelsize=8)


eq_s = np.cumprod(1.0 + net)
eq_e = np.cumprod(1.0 + eem)
eq_p = np.cumprod(1.0 + spy)
fig, ax = plt.subplots(figsize=(8.2, 4.2))
ax.plot(dates, eq_s, label="strategy, net, 1 bp/side", lw=1.3, color=STRAT)
ax.plot(dates, eq_e, label="EEM open to close, uncosted", lw=1.0, color=EEM)
ax.plot(dates, eq_p, label="SPY close to close, uncosted", lw=1.0, color=SPY)
mark(ax)
ax.set_ylabel("growth of $1")
ax.set_title(
    f"Growth of $1, full sample. Dashed line is 2024-07-01. "
    f"Strategy ends at {eq_s[-1]:.2f}; EEM open-to-close at {eq_e[-1]:.2f}; "
    f"SPY close-to-close at {eq_p[-1]:.2f}.",
    fontsize=9,
)
ax.legend(fontsize=8, loc="upper left")
save(fig, "equity.svg")

fig, ax = plt.subplots(figsize=(8.2, 3.8))
for series, lab, color in (
    (net, "strategy", STRAT),
    (eem, "EEM open to close", EEM),
    (spy, "SPY close to close", SPY),
):
    eq = np.cumprod(1.0 + series)
    ax.plot(dates, eq / np.maximum.accumulate(eq) - 1.0, label=lab, lw=1.0, color=color)
mark(ax)
ax.set_ylabel("drawdown")
p, b = res["primary"], res["benchmarks"]
ax.set_title(
    f"Drawdown, full sample. Strategy {p['full']['max_dd']:.1%}, "
    f"EEM open-to-close {b['eem_otc']['full']['max_dd']:.1%}, "
    f"SPY close-to-close {b['spy_c2c']['full']['max_dd']:.1%}.",
    fontsize=9,
)
ax.legend(fontsize=8)
save(fig, "drawdown.svg")

years = [row["year"] for row in res["by_year"]]
x = np.arange(len(years))
width = 0.26
fig, ax = plt.subplots(figsize=(8.2, 4.2))
ax.bar(x - width, [row["total_return"] for row in res["by_year"]], width=width, color=STRAT, label="strategy")
ax.bar(x, [row["eem_otc_return"] for row in res["by_year"]], width=width, color=EEM, label="EEM open to close")
ax.bar(x + width, [row["spy_c2c_return"] for row in res["by_year"]], width=width, color=SPY, label="SPY close to close")
ax.axhline(0, color="black", lw=0.6)
ax.set_xticks(x)
ax.set_xticklabels([str(y) for y in years], rotation=45, fontsize=8)
ax.set_ylabel("calendar-year return")
n_neg = sum(row["total_return"] < 0 for row in res["by_year"])
ax.set_title(
    "Calendar-year compounded return. 2011 starts 2011-01-06. 2026 ends 2026-10-01. "
    f"2024 contains both samples. The strategy is negative in {n_neg} of {len(years)} years.",
    fontsize=9,
)
ax.grid(axis="y", alpha=0.3)
ax.legend(fontsize=8)
save(fig, "by_year.svg")

order = ["0", "0.5", "1", "2", "3"]
bps = [res["costs"][k]["bps_side"] for k in order]
fig, ax = plt.subplots(figsize=(8.2, 4.0))
ax.plot(bps, [res["costs"][k]["full_sharpe"] for k in order], marker="o", color=STRAT, label="full sample")
ax.plot(bps, [res["costs"][k]["oos_sharpe"] for k in order], marker="o", color="#a31f34", label="out of sample")
ax.axhline(0, color="black", lw=0.6)
ax.axvline(1, color="black", ls="--", lw=0.8)
ax.set_xlabel("cost, bp per side")
ax.set_ylabel("Sharpe")
ax.set_title(
    "Sharpe at 0, 0.5, 1, 2, and 3 bp per side. The dashed line is the locked 1 bp cost. "
    f"At 0 bp, OOS Sharpe is {res['costs']['0']['oos_sharpe']:.2f}.",
    fontsize=9,
)
ax.grid(alpha=0.3)
ax.legend(fontsize=8)
save(fig, "costs.svg")

fig, ax = plt.subplots(figsize=(8.2, 4.0))
z = [c["deadzone"] for c in res["grid"]]
ax.plot(z, [c["is_sharpe"] for c in res["grid"]], marker="o", color=STRAT, label="in sample")
ax.plot(z, [c["oos_sharpe"] for c in res["grid"]], marker="o", color="#a31f34", label="out of sample")
ax.scatter([0], [res["grid"][0]["is_sharpe"]], s=60, facecolors="none", edgecolors="black", zorder=3, label="primary")
ax.axhline(0, color="black", lw=0.6)
ax.set_xlabel("deadzone on |prior SPY return|")
ax.set_ylabel("Sharpe")
ax.set_title(
    f"Deadzone grid. {res['grid_is_positive']} of {res['grid_is_cells']} in-sample Sharpes are positive. "
    "Nothing was selected. Out of sample is shown for selection bias only.",
    fontsize=9,
)
ax.grid(alpha=0.3)
ax.legend(fontsize=8)
save(fig, "grid.svg")

fig, ax = plt.subplots(figsize=(8.2, 4.0))
qx = [q["quintile"] for q in res["quintiles"]]
ax.bar(qx, [q["mean_net"] * 10000 for q in res["quintiles"]], color=STRAT)
ax.axhline(0, color="black", lw=0.6)
ax.set_xlabel("quintile of prior SPY return (1 = most negative)")
ax.set_ylabel("mean net trade, bp")
ax.set_title(
    "Mean net trade by quintile of the signed prior SPY close-to-close return. "
    "Quintile 1 is the large down days. This split was not used as a filter.",
    fontsize=9,
)
ax.grid(axis="y", alpha=0.3)
save(fig, "move_quintiles.svg")

draws = np.load(HERE / "placebo_direction.npy")
fig, ax = plt.subplots(figsize=(8.2, 4.0))
ax.hist(draws, bins=40, color="#8aa0b4", edgecolor="white")
actual = res["direction_placebo"]["actual_gross_sharpe"]
ax.axvline(actual, color="#a31f34", lw=1.4, label=f"actual gross Sharpe {actual:.2f}")
ax.set_xlabel("gross Sharpe")
ax.set_ylabel("draws")
ax.set_title(
    f"Direction placebo, {res['direction_placebo']['n']} draws, seed {res['direction_placebo']['seed']}. "
    f"Null mean {res['direction_placebo']['null_mean']:.3f}. p = {res['direction_placebo']['p']:.3f}.",
    fontsize=9,
)
ax.legend(fontsize=8)
ax.grid(axis="y", alpha=0.3)
save(fig, "placebo.svg")

rdates = [datetime.strptime(r["date"], "%Y-%m-%d") for r in roll]
rsharpe = np.array([float(r["sharpe"]) for r in roll])
fig, ax = plt.subplots(figsize=(8.2, 3.8))
ax.plot(rdates, rsharpe, color=STRAT, lw=1.0)
ax.axhline(0, color="black", lw=0.6)
mark(ax)
roll_s = ph["rolling_252"]
ax.set_ylabel("252-session Sharpe")
ax.set_title(
    f"Trailing 252-session net Sharpe (post hoc). "
    f"Range {roll_s['min']:.3f} to {roll_s['max']:.3f}. "
    f"Positive in {roll_s['fraction_positive']:.1%} of windows. Last window {roll_s['last']:.3f}.",
    fontsize=9,
)
save(fig, "rolling_sharpe.svg")
print(f"wrote {len(list(FIG.glob('*.svg')))} figures to {FIG}")
