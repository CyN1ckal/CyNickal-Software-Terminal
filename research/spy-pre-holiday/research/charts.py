# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures from results.json, daily.csv, posthoc.json, and the placebo arrays.

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
with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
    daily = list(csv.DictReader(handle))
with (HERE / "rolling_sharpe.csv").open(encoding="utf-8", newline="") as handle:
    rolling = list(csv.DictReader(handle))

dates = [datetime.strptime(r["date"], "%Y-%m-%d") for r in daily]
net = np.array([float(r["strategy_net"]) for r in daily])
c2c = np.array([float(r["close_to_close"]) for r in daily])
otc = np.array([float(r["open_to_close"]) for r in daily])
SPLIT = datetime(2024, 7, 1)
spy = res["spy"]


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, format="svg")
    plt.close(fig)


def mark(ax):
    ax.axvline(SPLIT, color="black", ls="--", lw=0.8)
    ax.grid(alpha=0.3)
    ax.tick_params(labelsize=8)


eq_s = np.cumprod(1.0 + net)
eq_b = np.cumprod(1.0 + c2c)
eq_o = np.cumprod(1.0 + otc)
if abs(eq_s[-1] - spy["full"]["ending_equity"]) > 1e-9:
    raise SystemExit("equity curve does not match results.json")

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

fig, ax = plt.subplots(figsize=(8.2, 3.8))
for series, lab, color in (
    (net, "strategy", "#1f4e79"),
    (c2c, "close to close", "#c65911"),
    (otc, "open to close", "#548235"),
):
    eq = np.cumprod(1.0 + series)
    ax.plot(dates, eq / np.maximum.accumulate(eq) - 1.0, label=lab, lw=1.0, color=color)
mark(ax)
ax.set_ylabel("drawdown")
ax.set_title(
    f"Drawdown, full sample. Strategy {spy['full']['max_dd']:.1%}. "
    f"Close-to-close {spy['benchmark_c2c']['full']['max_dd']:.1%}. "
    f"The strategy is invested on {spy['full']['exposure']:.1%} of sessions.",
    fontsize=9,
)
ax.legend(fontsize=8)
save(fig, "drawdown.svg")

years = res["years"]
labels = [str(y["year"]) for y in years]
x = np.arange(len(years))
width = 0.27
fig, ax = plt.subplots(figsize=(8.6, 4.0))
ax.bar(x - width, [y["return"] for y in years], width, label="strategy", color="#1f4e79")
ax.bar(x, [y["otc_return"] for y in years], width, label="open to close", color="#548235")
ax.bar(x + width, [y["c2c_return"] for y in years], width, label="close to close", color="#c65911")
ax.axhline(0, color="black", lw=0.6)
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=45, fontsize=7)
ax.set_ylabel("compound return")
neg = [y["year"] for y in years if y["return"] < 0]
ax.set_title(
    f"Calendar-year compound return. Strategy years below zero: {', '.join(str(y) for y in neg)}.",
    fontsize=9,
)
ax.grid(axis="y", alpha=0.3)
ax.legend(fontsize=8)
save(fig, "by_year.svg")

costs = res["costs"]
mult = [c["multiplier"] for c in costs]
fig, ax = plt.subplots(figsize=(7.2, 3.8))
ax.plot(mult, [c["full_sharpe"] for c in costs], marker="o", label="full Sharpe", color="#1f4e79")
ax.plot(mult, [c["oos_sharpe"] for c in costs], marker="o", label="OOS Sharpe", color="#c65911")
ax.axhline(0.5, color="gray", ls=":", lw=0.8, label="OOS Sharpe bar 0.5")
ax.axhline(0.0, color="black", lw=0.6)
ax.set_xlabel("cost multiplier (1 = 1 bp per side)")
ax.set_ylabel("Sharpe")
base = next(c for c in costs if c["multiplier"] == 1.0)
zero = next(c for c in costs if c["multiplier"] == 0.0)
ax.set_title(
    f"Cost sweep. OOS Sharpe is {base['oos_sharpe']:.3f} at 1 bp/side "
    f"and {zero['oos_sharpe']:.3f} at zero cost.",
    fontsize=9,
)
ax.grid(alpha=0.3)
ax.legend(fontsize=8)
save(fig, "costs.svg")

cells = res["grid"]["cells"]
fig, axes = plt.subplots(1, 2, figsize=(8.2, 3.6))
xs = [c["notional"] for c in cells]
axes[0].plot(xs, [c["is_sharpe"] for c in cells], marker="o", color="#1f4e79")
axes[0].axhline(0, color="black", lw=0.6)
axes[0].set_xlabel("notional")
axes[0].set_ylabel("IS Sharpe")
axes[0].set_title("IS Sharpe is tied across the five notionals", fontsize=9)
axes[1].plot(xs, [c["is_return"] for c in cells], marker="o", color="#548235", label="IS return")
axes[1].plot(xs, [c["oos_return"] for c in cells], marker="o", color="#c65911", label="OOS return")
axes[1].axhline(0, color="black", lw=0.6)
axes[1].set_xlabel("notional")
axes[1].set_ylabel("compound return")
axes[1].set_title("Return compounds. Nothing was selected.", fontsize=9)
axes[1].legend(fontsize=8)
for ax in axes:
    ax.grid(alpha=0.3)
save(fig, "grid.svg")

q = res["quintiles"]
fig, ax = plt.subplots(figsize=(7.2, 3.8))
ax.bar([str(row["quintile"]) for row in q], [row["mean_strategy_net"] for row in q], color="#1f4e79")
ax.axhline(0, color="black", lw=0.6)
ax.set_xlabel("close-to-close quintile, 1 = worst")
ax.set_ylabel("mean strategy net")
ax.set_title(
    f"Mean strategy return by SPY close-to-close quintile. "
    f"Quintile 1 mean {q[0]['mean_strategy_net']:.5f}, {q[0]['trades']} trades. "
    f"Quintile 5 has {q[-1]['trades']} trades.",
    fontsize=9,
)
ax.grid(axis="y", alpha=0.3)
save(fig, "move_quintiles.svg")

direction = np.load(HERE / "placebo_direction.npy")
timing = np.load(HERE / "placebo_timing.npy")
actual = res["placebo_direction"]["actual_gross_sharpe"]
fig, axes = plt.subplots(1, 2, figsize=(8.4, 3.6))
axes[0].hist(direction, bins=40, color="#1f4e79")
axes[0].axvline(actual, color="#c65911", lw=1.2)
axes[0].set_title(f"Direction placebo. p = {res['placebo_direction']['p']:.3f}", fontsize=9)
axes[0].set_xlabel("gross Sharpe")
axes[1].hist(timing, bins=30, color="#548235")
axes[1].axvline(actual, color="#c65911", lw=1.2)
axes[1].set_title(
    f"Timing placebo. p = {res['placebo_timing']['p']:.3f}. Not an acceptance line.",
    fontsize=9,
)
axes[1].set_xlabel("gross Sharpe")
for ax in axes:
    ax.grid(alpha=0.3)
save(fig, "placebo.svg")

rdates = [datetime.strptime(r["date"], "%Y-%m-%d") for r in rolling]
rsharpe = [float(r["sharpe"]) for r in rolling]
fig, ax = plt.subplots(figsize=(8.2, 3.6))
ax.plot(rdates, rsharpe, color="#1f4e79", lw=1.0)
ax.axhline(0, color="black", lw=0.6)
mark(ax)
ax.set_ylabel("252-session Sharpe")
last = ph["rolling_252"]
ax.set_title(
    f"Rolling 252-session Sharpe of the net daily series (post hoc). "
    f"Last {last['last']:.2f}. Share of windows below zero: {last['fraction_negative']:.0%}.",
    fontsize=9,
)
save(fig, "rolling_sharpe.svg")
print(f"wrote {len(list(FIG.glob('*.svg')))} figures")
