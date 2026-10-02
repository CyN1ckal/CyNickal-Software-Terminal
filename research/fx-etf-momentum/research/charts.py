# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the report. Reads only results.json, daily.csv, and posthoc.json
in this folder. Does not open the store and does not recompute the backtest.
Writes SVG to ../report/figures/."""

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
daily = [ln.split(",") for ln in (HERE / "daily.csv").read_text(encoding="utf-8").splitlines()[1:]]
dates = [datetime.strptime(row[0], "%Y-%m-%d") for row in daily]
equity = np.array([float(row[4]) for row in daily])
bench_eq = np.array([float(row[5]) for row in daily])
SPLIT = datetime(2024, 7, 1)


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


def drawdown(eq):
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return eq / peak - 1.0


# 1. growth of $1, continuous full-sample path
fig, ax = plt.subplots(figsize=(7.2, 3.6))
ax.plot(dates, equity, label="strategy (net, 5 bp/side)", lw=1.2)
ax.plot(dates, bench_eq, label="equal-weight long basket, uncosted", lw=1.0, alpha=0.85)
style(ax,
      f"63-session FX ETF momentum: growth of $1 "
      f"(dashed = 2024-07-01; path ends at {equity[-1]:.3f}, long basket at {bench_eq[-1]:.3f})",
      "value of $1")
ax.legend(fontsize=7)
save(fig, "equity.svg")

# 2. drawdown of the continuous path, peak starting at 1
fig, ax = plt.subplots(figsize=(7.2, 3.2))
ax.plot(dates, drawdown(equity), label="strategy", lw=1.0)
ax.plot(dates, drawdown(bench_eq), label="equal-weight long basket", lw=1.0, alpha=0.85)
style(ax,
      f"Drawdown of the continuous path "
      f"(strategy max {res['full']['max_dd']:.1%}, long basket {res['full']['benchmark_max_dd']:.1%})",
      "drawdown")
ax.legend(fontsize=7)
save(fig, "drawdown.svg")

# 3. calendar year
years = res["breakdown"]["by_year"]
yrs = sorted(years, key=int)
sr = [years[y]["return"] for y in yrs]
br = [years[y]["benchmark_return"] for y in yrs]
fig, ax = plt.subplots(figsize=(7.4, 3.2))
w = 0.4
i = np.arange(len(yrs))
ax.bar(i - w / 2, sr, w, label="strategy")
ax.bar(i + w / 2, br, w, label="equal-weight long basket")
ax.set_xticks(i, yrs, rotation=45)
ax.axhline(0, color="k", lw=0.7)
ax.set_title(
    f"Calendar-year return: strategy negative in {ph['negative_calendar_years']} of "
    f"{ph['calendar_years']} years (2011 and 2026 are partial)",
    fontsize=10)
ax.set_ylabel("return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
ax.legend(fontsize=7)
save(fig, "by_year.svg")

# 4. lookback grid
g = res["grid"]
ks = sorted(g, key=lambda k: int(k))
fig, ax = plt.subplots(figsize=(5.8, 3.2))
ax.plot([int(k) for k in ks], [g[k]["IS_sharpe"] for k in ks], "o-", label="IS Sharpe")
ax.plot([int(k) for k in ks], [g[k]["OOS_sharpe"] for k in ks], "s-", label="OOS Sharpe")
ax.axhline(0, color="k", lw=0.7)
ax.axvline(63, color="r", ls=":", lw=1, label="primary 63")
best = max(ks, key=lambda k: g[k]["IS_sharpe"])
ax.set_title(
    f"Lookback grid: {res['grid_is_positive']} of 5 IS Sharpes > 0. "
    f"IS-best cell {best} has OOS Sharpe {g[best]['OOS_sharpe']:.2f}. Nothing selected.",
    fontsize=9)
ax.set_xlabel("lookback (sessions)", fontsize=8)
ax.set_ylabel("Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
ax.legend(fontsize=7)
save(fig, "grid.svg")

# 5. costs
cs = res["costs"]
order = ["0", "2.5", "5", "10", "15"]
cx = [cs[k]["bp"] for k in order]
fig, ax = plt.subplots(figsize=(5.8, 3.2))
ax.plot(cx, [cs[k]["full_sharpe"] for k in order], "o-", label="full Sharpe")
ax2 = ax.twinx()
ax2.plot(cx, [cs[k]["full_return"] for k in order], "s-", color="tab:orange", label="full return")
ax.axhline(0, color="k", lw=0.7)
ax2.axhline(0, color="k", lw=0.4)
ax.set_title(
    f"Cost sweep: full-sample Sharpe is {cs['0']['full_sharpe']:.3f} at 0 bp. "
    "The loss is present before costs.",
    fontsize=9)
ax.set_xlabel("cost per side (bp)", fontsize=8)
ax.set_ylabel("Sharpe", fontsize=8)
ax2.set_ylabel("total return", fontsize=8)
ax.tick_params(labelsize=7)
ax2.tick_params(labelsize=7)
ax.grid(alpha=0.3)
fig.legend(fontsize=7, loc="lower left")
save(fig, "costs.svg")

# 6. direction placebo
null = np.array(res["placebo"]["direction_draws"], dtype=float)
act = res["placebo"]["direction_actual_gross_sharpe"]
fig, ax = plt.subplots(figsize=(5.8, 3.2))
ax.hist(null, bins=40, color="lightgray", edgecolor="gray", lw=0.4)
ax.axvline(act, color="r", lw=1.5, label=f"actual gross Sharpe {act:.3f}")
ax.axvline(res["placebo"]["direction_null_mean"], color="k", ls="--", lw=1,
           label=f"null mean {res['placebo']['direction_null_mean']:.3f}")
ax.set_title(
    f"Direction placebo: p = {res['placebo']['direction_p']:.3f}. "
    "Actual gross Sharpe is inside the null, and negative.",
    fontsize=9)
ax.set_xlabel("gross Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.legend(fontsize=7)
save(fig, "placebo.svg")

# 7. benchmark-move quintiles
qb = res["breakdown"]["quintile_benchmark"]
labels = [f"Q{row['quintile']}" for row in qb]
means = [row["mean_strategy"] for row in qb]
fig, ax = plt.subplots(figsize=(5.8, 3.2))
ax.bar(labels, means, color=["tab:green" if v > 0 else "tab:red" for v in means])
ax.axhline(0, color="k", lw=0.7)
ax.set_title(
    "Mean strategy return by quintile of the long basket "
    "(Q1 worst day … Q5 best): positive in Q1, negative in Q5",
    fontsize=9)
ax.set_ylabel("mean daily return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
save(fig, "move_quintiles.svg")

# 8. rolling Sharpe (post hoc series)
rs = ph["rolling_252"]
rd = [datetime.strptime(pt["date"], "%Y-%m-%d") for pt in rs["series"]]
rv = [pt["sharpe"] for pt in rs["series"]]
fig, ax = plt.subplots(figsize=(7.2, 3.2))
ax.plot(rd, rv, lw=1.0, label="strategy")
ax.axhline(0, color="k", lw=0.7)
style(ax,
      f"Rolling 252-session Sharpe (post hoc): {rs['share_le_0']:.1%} of windows ≤ 0; "
      f"min {rs['min']:.2f} on {rs['min_date']}",
      "Sharpe")
ax.legend(fontsize=7)
save(fig, "rolling_sharpe.svg")

print("wrote", sorted(p.name for p in FIG.glob("*.svg")))
