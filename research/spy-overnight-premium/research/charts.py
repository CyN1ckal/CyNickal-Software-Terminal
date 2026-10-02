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
dates = [datetime.strptime(r["date"], "%Y-%m-%d") for r in daily]
net = np.array([float(r["strategy_net"]) for r in daily])
bh = np.array([float(r["bh_c2c"]) for r in daily])
oc = np.array([float(r["open_to_close"]) for r in daily])
SPLIT = datetime(2024, 7, 1)


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, format="svg")
    plt.close(fig)


def mark(ax):
    ax.axvline(SPLIT, color="black", ls="--", lw=0.8)
    ax.grid(alpha=0.3)
    ax.tick_params(labelsize=8)


# 1. Growth of $1. Linear scale, full sample, three series.
eq_s = np.cumprod(1.0 + net)
eq_b = np.cumprod(1.0 + bh)
eq_o = np.cumprod(1.0 + oc)
fig, ax = plt.subplots(figsize=(8.2, 4.2))
ax.plot(dates, eq_s, label="strategy, net, 1 bp/side", lw=1.3, color="#1f4e79")
ax.plot(dates, eq_b, label="SPY close to close, uncosted", lw=1.0, color="#c65911")
ax.plot(dates, eq_o, label="SPY open to close, uncosted", lw=1.0, color="#548235")
mark(ax)
ax.set_ylabel("growth of $1")
ax.set_title(
    f"Growth of $1, full sample. Dashed line is 2024-07-01. "
    f"Strategy ends at {eq_s[-1]:.2f}; close-to-close at {eq_b[-1]:.2f}; "
    f"open-to-close at {eq_o[-1]:.2f}.",
    fontsize=9,
)
ax.legend(fontsize=8, loc="upper left")
save(fig, "equity.svg")

# 2. Drawdown.
fig, ax = plt.subplots(figsize=(8.2, 3.8))
for series, lab, color in (
    (net, "strategy", "#1f4e79"),
    (bh, "close to close", "#c65911"),
    (oc, "open to close", "#548235"),
):
    eq = np.cumprod(1.0 + series)
    ax.plot(dates, eq / np.maximum.accumulate(eq) - 1.0, label=lab, lw=1.0, color=color)
mark(ax)
spy = res["spy"]
ax.set_ylabel("drawdown")
ax.set_title(
    f"Drawdown, full sample. Strategy {spy['full']['max_dd']:.1%}, "
    f"close-to-close {spy['benchmark_c2c']['full']['max_dd']:.1%}, "
    f"open-to-close {spy['benchmark_otc']['full']['max_dd']:.1%}.",
    fontsize=9,
)
ax.legend(fontsize=8)
save(fig, "drawdown.svg")

# 3. Calendar year.
years = res["by_year"]
labels = [str(y["year"]) for y in years]
x = np.arange(len(years))
fig, ax = plt.subplots(figsize=(8.6, 4.0))
w = 0.26
ax.bar(x - w, [y["return"] * 100 for y in years], w, label="strategy", color="#1f4e79")
ax.bar(x, [y["bh_return"] * 100 for y in years], w, label="close to close", color="#c65911")
ax.bar(x + w, [y["otc_return"] * 100 for y in years], w, label="open to close", color="#548235")
ax.set_xticks(x, labels, fontsize=7)
ax.axhline(0, color="black", lw=0.7)
ax.set_ylabel("compound return (%)")
neg = ", ".join(str(y) for y in ph["negative_compound_years"])
ax.set_title(
    f"Calendar-year compound return. Strategy negative in {neg}. "
    f"2011 starts 2011-01-05. 2026 ends 2026-10-01 and has {years[-1]['sessions']} sessions.",
    fontsize=8,
)
ax.legend(fontsize=7, ncol=3)
ax.grid(alpha=0.3, axis="y")
save(fig, "by_year.svg")

# 4. Costs.
costs = res["costs"]
xs = [c["cost_bps_side"] for c in costs]
fig, ax = plt.subplots(figsize=(7.4, 3.8))
ax.plot(xs, [c["full_sharpe"] for c in costs], marker="o", color="#1f4e79", label="full sample")
ax.plot(xs, [c["oos_sharpe"] for c in costs], marker="o", color="#c65911", label="out of sample")
ax.axhline(0.5, color="gray", ls="--", lw=0.8)
ax.axhline(0.0, color="black", lw=0.6)
ax.set_xlabel("cost per side (bp)")
ax.set_ylabel("Sharpe")
two = next(c for c in costs if c["multiplier"] == 2.0)
ax.set_title(
    f"Sharpe against cost. At 2 bp per side the full-sample return is {two['full_return']:.1%} "
    f"and the Sharpe is {two['full_sharpe']:.2f}. Dashed line is the OOS Sharpe hurdle of 0.5.",
    fontsize=8,
)
ax.legend(fontsize=8)
ax.grid(alpha=0.3)
save(fig, "costs.svg")

# 5. Notional grid. Sharpe does not move. Show return as well so the cells are not blank copies.
grid = res["grid"]
fig, axes = plt.subplots(1, 2, figsize=(8.6, 3.8))
xx = np.arange(len(grid))
labs = [f"{c['notional']:.2f}" + (" *" if c["primary"] else "") for c in grid]
for ax, key, title in (
    (axes[0], "is_sharpe", "In-sample Sharpe"),
    (axes[1], "oos_sharpe", "Out-of-sample Sharpe, not used"),
):
    vals = [c[key] for c in grid]
    colors = ["#1f4e79" if c["primary"] else "#8fa8c4" for c in grid]
    ax.bar(xx, vals, color=colors)
    ax.set_xticks(xx, labs, fontsize=8)
    ax.axhline(0, color="black", lw=0.6)
    ax.set_title(title, fontsize=9)
    ax.grid(alpha=0.3, axis="y")
    ax.set_ylabel("Sharpe")
fig.suptitle(
    "Notional grid. * is the primary, 1.00, and nothing was selected. "
    "The five Sharpes match: scaling the book does not change a zero-rate Sharpe.",
    fontsize=8,
)
save(fig, "grid.svg")

# 6. Quintiles of the close-to-close move.
q = res["by_quintile"]
fig, ax = plt.subplots(figsize=(7.6, 3.8))
xx = np.arange(len(q))
ax.bar(xx - 0.18, [v["mean_strategy_net"] * 1e4 for v in q], 0.36, label="strategy net", color="#1f4e79")
ax.bar(xx + 0.18, [v["mean_bh_c2c"] * 1e4 for v in q], 0.36, label="close to close", color="#c65911")
ax.axhline(0, color="black", lw=0.6)
ax.set_xticks(xx, [f"Q{v['quintile']}" for v in q])
ax.set_xlabel("quintile of that session's close-to-close return (Q1 = worst)")
ax.set_ylabel("mean return (bp)")
ax.set_title(
    "The overnight book is negative in the worst close-to-close quintile "
    f"({q[0]['mean_strategy_net'] * 1e4:.1f} bp a session) and positive in the best.",
    fontsize=8,
)
ax.legend(fontsize=8)
ax.grid(alpha=0.3, axis="y")
save(fig, "move_quintiles.svg")

# 7. Direction placebo, gross.
draws = np.load(HERE / "placebo_direction.npy")
pl = res["direction_placebo"]
fig, ax = plt.subplots(figsize=(7.6, 3.8))
ax.hist(draws, bins=40, color="#8fa8c4", edgecolor="white")
ax.axvline(pl["actual_gross_sharpe"], color="#1f4e79", lw=1.6, label="actual gross Sharpe")
ax.axvline(pl["null_p95"], color="gray", ls="--", lw=0.9, label="null 95th percentile")
ax.set_xlabel("gross Sharpe")
ax.set_ylabel("draws")
ax.set_title(
    f"Direction placebo, 2,000 draws, gross returns before cost. "
    f"Actual {pl['actual_gross_sharpe']:.2f}, p = {pl['p']:.3f}. "
    f"The costed Sharpe is {spy['full']['sharpe']:.2f} and is not this line.",
    fontsize=8,
)
ax.legend(fontsize=8)
ax.grid(alpha=0.3, axis="y")
save(fig, "placebo.svg")

# 8. Rolling Sharpe, post hoc.
with (HERE / "rolling_sharpe.csv").open(encoding="utf-8", newline="") as f:
    roll = list(csv.DictReader(f))
rd = [datetime.strptime(r["date"], "%Y-%m-%d") for r in roll]
fig, ax = plt.subplots(figsize=(8.2, 3.8))
ax.plot(rd, [float(r["strategy"]) for r in roll], color="#1f4e79", lw=1.1, label="strategy")
ax.plot(rd, [float(r["bh_c2c"]) for r in roll], color="#c65911", lw=1.0, label="close to close")
ax.plot(rd, [float(r["open_to_close"]) for r in roll], color="#548235", lw=1.0, label="open to close")
ax.axhline(0, color="black", lw=0.6)
mark(ax)
st = ph["rolling_252"]["strategy"]
ax.set_ylabel("Sharpe")
ax.set_title(
    f"(post hoc) Trailing 252-session Sharpe. Strategy ranges from {st['min']:.2f} to {st['max']:.2f}. "
    f"Share above zero: {st['frac_positive']:.0%}. Last window: {st['last']:.2f}.",
    fontsize=8,
)
ax.legend(fontsize=8)
save(fig, "rolling_sharpe.svg")

print(f"wrote figures to {FIG}")
