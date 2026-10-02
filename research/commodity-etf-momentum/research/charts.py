# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the report. Reads results, daily, and posthoc files only."""

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

dates = [datetime.strptime(row["date"], "%Y-%m-%d") for row in daily]
net = np.array([float(row["strategy_net"]) for row in daily])
bench = np.array([float(row["benchmark"]) for row in daily])
SPLIT = datetime(2024, 7, 1)


def pct(x) -> str:
    return f"{100 * x:.1f}%"


def style(ax, title, ylab):
    ax.set_title(title, fontsize=10)
    ax.set_ylabel(ylab, fontsize=8)
    ax.tick_params(labelsize=7)
    ax.grid(alpha=0.3)
    ax.axvline(SPLIT, color="k", ls="--", lw=0.8)


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, format="svg")
    plt.close(fig)


full = res["full"]
oos = res["OOS"]
eq = np.cumprod(1.0 + net)
beq = np.cumprod(1.0 + bench)
end_note = "ends above $1" if eq[-1] >= 1.0 else "ends below $1"
fig, ax = plt.subplots(figsize=(7.2, 3.6))
ax.plot(dates, eq, label="strategy net", lw=1.2)
ax.plot(dates, beq, label="equal-weight long-only, uncosted", lw=1.0, alpha=0.85)
style(
    ax,
    f"Growth of $1, full sample (dashed = 2024-07-01). Strategy {end_note}; "
    f"OOS {pct(oos['total_return'])}",
    "value of $1",
)
ax.legend(fontsize=7)
save(fig, "equity.svg")

fig, ax = plt.subplots(figsize=(7.2, 3.2))
for series, label in ((net, "strategy"), (bench, "equal-weight six")):
    curve = np.cumprod(1.0 + series)
    ax.plot(dates, curve / np.maximum.accumulate(curve) - 1.0, label=label, lw=1.0)
style(
    ax,
    f"Drawdown from the running peak. Strategy full max {pct(full['max_drawdown'])}, "
    f"benchmark {pct(res['benchmark']['full']['max_drawdown'])}",
    "drawdown",
)
ax.legend(fontsize=7)
save(fig, "drawdown.svg")

years = sorted(res["breakdown"]["by_year"])
sr = [res["breakdown"]["by_year"][y]["return_"] for y in years]
br = [res["breakdown"]["by_year"][y]["benchmark"] for y in years]
neg_years = sum(1 for v in sr if v < 0.0)
fig, ax = plt.subplots(figsize=(7.2, 3.2))
x = np.arange(len(years))
w = 0.4
ax.bar(x - w / 2, sr, w, label="strategy")
ax.bar(x + w / 2, br, w, label="equal-weight six")
ax.set_xticks(x, years, rotation=45)
ax.axhline(0, color="k", lw=0.7)
ax.set_title(
    f"Calendar-year return. Strategy negative in {neg_years} of {len(years)} years",
    fontsize=10,
)
ax.set_ylabel("return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
ax.legend(fontsize=7)
save(fig, "by_year.svg")

ks = [int(k) for k in res["grid"]]
fig, ax = plt.subplots(figsize=(6.2, 3.2))
ax.plot(ks, [res["grid"][str(k)]["IS_sharpe"] for k in ks], "o-", label="IS Sharpe")
ax.plot(ks, [res["grid"][str(k)]["OOS_sharpe"] for k in ks], "s-", label="OOS Sharpe")
ax.axhline(0, color="k", lw=0.7)
ax.axvline(252, color="r", ls=":", lw=1, label="primary K=252")
pos = res["grid_cells_is_sharpe_positive"]
ax.set_title(f"Formation grid, book size fixed at 2. {pos} of 5 IS cells have Sharpe > 0", fontsize=10)
ax.set_xlabel("formation length (own sessions)", fontsize=8)
ax.set_ylabel("Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
ax.legend(fontsize=7)
save(fig, "grid.svg")

order = ["0", "0.5", "1", "2", "3"]
fig, ax = plt.subplots(figsize=(6.2, 3.2))
ax.plot([float(k) for k in order], [res["costs"][k]["full_sharpe"] for k in order], "o-", label="full Sharpe")
ax.plot([float(k) for k in order], [res["costs"][k]["oos_sharpe"] for k in order], "s-", label="OOS Sharpe")
ax.axhline(0, color="k", lw=0.7)
ax.set_title(
    f"Cost multiple of the pre-registered schedule. Full return at 2× is "
    f"{pct(res['costs']['2']['full_return'])}",
    fontsize=10,
)
ax.set_xlabel("cost multiple", fontsize=8)
ax.set_ylabel("Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
ax.legend(fontsize=7)
save(fig, "costs.svg")

direction = np.array(res["placebo"]["direction"]["null_sharpes"])
timing = np.array(res["placebo"]["timing"]["null_sharpes"])
actual = res["placebo"]["direction"]["actual_gross_sharpe"]
fig, axes = plt.subplots(1, 2, figsize=(7.4, 3.2))
axes[0].hist(direction, bins=40, color="lightgray", edgecolor="gray", lw=0.3)
axes[0].axvline(actual, color="r", lw=1.4, label=f"actual {actual:.2f}")
axes[0].axvline(direction.mean(), color="k", ls="--", lw=1, label=f"null mean {direction.mean():.2f}")
axes[0].set_title(f"Direction placebo p = {res['placebo']['direction']['p']:.3f}", fontsize=10)
axes[0].set_xlabel("gross Sharpe", fontsize=8)
axes[0].legend(fontsize=6)
axes[1].hist(timing, bins=30, color="lightgray", edgecolor="gray", lw=0.3)
axes[1].axvline(actual, color="r", lw=1.4, label=f"actual {actual:.2f}")
axes[1].axvline(timing.mean(), color="k", ls="--", lw=1, label=f"null mean {timing.mean():.2f}")
axes[1].set_title(f"Timing placebo p = {res['placebo']['timing']['p']:.3f}", fontsize=10)
axes[1].set_xlabel("gross Sharpe", fontsize=8)
axes[1].legend(fontsize=6)
for ax in axes:
    ax.tick_params(labelsize=7)
    ax.grid(alpha=0.3)
save(fig, "placebo.svg")

q = res["breakdown"]["quintile_month"]
labels = [f"Q{k}" for k in sorted(q, key=int)]
means = [q[k]["mean_strategy"] for k in sorted(q, key=int)]
fig, ax = plt.subplots(figsize=(6.2, 3.2))
ax.bar(labels, means, color=["tab:red" if v < 0 else "tab:green" for v in means])
ax.axhline(0, color="k", lw=0.7)
ax.set_title(
    "Mean strategy month by quintile of the equal-weight book's month. Q1 is the worst benchmark months",
    fontsize=10,
)
ax.set_ylabel("mean monthly return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
save(fig, "move_quintiles.svg")

rd = [datetime.strptime(d, "%Y-%m-%d") for d in ph["rolling_sharpe"]["dates"]]
fig, ax = plt.subplots(figsize=(7.2, 3.2))
ax.plot(rd, ph["rolling_sharpe"]["strategy"], lw=1.0, label="strategy")
ax.plot(rd, ph["rolling_sharpe"]["benchmark"], lw=0.8, alpha=0.75, label="equal-weight six")
ax.axhline(0, color="k", lw=0.7)
frac = ph["rolling_sharpe"]["fraction_positive"]
style(
    ax,
    f"Trailing 252-session Sharpe (post hoc). Positive in {100 * frac:.0f}% of windows",
    "Sharpe",
)
ax.legend(fontsize=7)
save(fig, "rolling_sharpe.svg")
print("figures written")
