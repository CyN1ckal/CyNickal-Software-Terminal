# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures from results.json, posthoc.json, daily.csv, and the placebo draw files.

Does not open the store and does not recompute the backtest.
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
with (HERE / "daily.csv").open(newline="", encoding="utf-8") as f:
    daily = list(csv.DictReader(f))

dates = [datetime.strptime(r["date"], "%Y-%m-%d") for r in daily]
strat = np.array([float(r["strategy_net"]) for r in daily])
bench = np.array([float(r["benchmark"]) for r in daily])
SPLIT = datetime(2024, 7, 1)


def pct(x):
    return f"{100.0 * x:+.1f}%"


def num(x):
    return f"{x:.2f}"


def style(ax, title, ylab):
    ax.set_title(title, fontsize=9)
    ax.set_ylabel(ylab, fontsize=8)
    ax.tick_params(labelsize=7)
    ax.grid(alpha=0.3)
    ax.axvline(SPLIT, color="k", ls="--", lw=0.8)


def save(fig, name):
    fig.tight_layout()
    fig.savefig(FIG / name, format="svg")
    plt.close(fig)


full = res["windows"]["full"]["strategy"]
oos = res["windows"]["oos"]["strategy"]
bf = res["windows"]["full"]["benchmark"]
bo = res["windows"]["oos"]["benchmark"]

fig, ax = plt.subplots(figsize=(7.2, 3.8))
ax.plot(dates, np.cumprod(1.0 + strat), label="strategy, 1 bp/side", lw=1.2, color="#1f4e79")
ax.plot(dates, np.cumprod(1.0 + bench), label="long-only TLT+IEF, uncosted", lw=1.0, color="#b85c38")
style(
    ax,
    "Growth of $1, full sample. Strategy ends "
    f"{pct(full['total_return'])}; the bond benchmark ends {pct(bf['total_return'])}. "
    f"Past the dashed line (2024-07-01) the strategy returns {pct(oos['total_return'])}.",
    "value of $1",
)
ax.legend(fontsize=7)
save(fig, "equity.svg")

fig, ax = plt.subplots(figsize=(7.2, 3.4))
for series, lab, color in (
    (strat, "strategy", "#1f4e79"),
    (bench, "long-only TLT+IEF", "#b85c38"),
):
    eq = np.cumprod(1.0 + series)
    ax.plot(dates, eq / np.maximum.accumulate(eq) - 1.0, label=lab, lw=1.0, color=color)
style(
    ax,
    "Drawdown from the running peak. Strategy max "
    f"{pct(full['max_dd'])} (in sample); out-of-sample max {pct(oos['max_dd'])}. "
    f"Benchmark max {pct(bf['max_dd'])}.",
    "drawdown",
)
ax.legend(fontsize=7)
save(fig, "drawdown.svg")

years = sorted(res["by_year"])
ys = np.arange(len(years))
sw = 0.36
fig, ax = plt.subplots(figsize=(7.2, 3.4))
ax.bar(ys - sw / 2, [res["by_year"][y]["strategy_return"] for y in years], sw,
       label="strategy", color="#1f4e79")
ax.bar(ys + sw / 2, [res["by_year"][y]["benchmark_return"] for y in years], sw,
       label="long-only TLT+IEF", color="#b85c38")
ax.axhline(0, color="k", lw=0.7)
ax.set_xticks(ys, years, rotation=45)
neg = sum(1 for y in years if res["by_year"][y]["strategy_return"] < 0)
ax.set_title(
    f"Calendar-year return. The strategy is negative in {neg} of {len(years)} years. "
    f"2022 is {pct(res['by_year']['2022']['strategy_return'])}; "
    f"2026 is partial, through 2026-10-01.",
    fontsize=9,
)
ax.set_ylabel("return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
ax.legend(fontsize=7)
save(fig, "by_year.svg")

costs = res["costs"]
fig, ax = plt.subplots(figsize=(7.2, 3.4))
x = [c["bps"] for c in costs]
ax.plot(x, [c["full_sharpe"] for c in costs], "o-", label="full sample", color="#1f4e79")
ax.plot(x, [c["oos_sharpe"] for c in costs], "o-", label="out of sample", color="#b85c38")
ax.axhline(0, color="k", lw=0.7)
ax.set_xticks(x)
ax.set_xlabel("cost, bp per side", fontsize=8)
ax.set_title(
    "Sharpe versus cost. Out of sample stays negative at 0 bp "
    f"({num(costs[0]['oos_sharpe'])}) through 3 bp ({num(costs[-1]['oos_sharpe'])}).",
    fontsize=9,
)
ax.set_ylabel("Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
ax.legend(fontsize=7)
save(fig, "costs.svg")

cells = res["grid"]["cells"]
fig, ax = plt.subplots(figsize=(7.2, 3.4))
lx = [c["lookback"] for c in cells]
ax.plot(lx, [c["is_sharpe"] for c in cells], "o-", label="in sample", color="#1f4e79")
ax.plot(lx, [c["oos_sharpe"] for c in cells], "o-", label="out of sample", color="#b85c38")
ax.axhline(0, color="k", lw=0.7)
ax.axvline(252, color="k", ls=":", lw=0.8)
ax.set_xticks(lx)
ax.set_xlabel("lookback, sessions", fontsize=8)
prim = next(c for c in cells if c["lookback"] == 252)
ax.set_title(
    "Lookback grid. All 5 in-sample Sharpes are positive. "
    f"The locked 252-session cell (dotted) is {num(prim['oos_sharpe'])} out of sample. "
    f"IS/OOS rank correlation {num(res['grid']['is_oos_spearman'])}. Nothing was selected.",
    fontsize=9,
)
ax.set_ylabel("Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
ax.legend(fontsize=7)
save(fig, "grid.svg")

q = res["monthly_quintiles"]
fig, ax = plt.subplots(figsize=(7.2, 3.4))
qx = np.arange(1, 6)
ax.bar(qx, [row["strategy_mean"] for row in q], color="#1f4e79", label="strategy")
ax.axhline(0, color="k", lw=0.7)
ax.set_xticks(qx, ["Q1 worst", "Q2", "Q3", "Q4", "Q5 best"])
ax.set_title(
    "Mean strategy return by SPY month quintile, full evaluation window. "
    f"Q1 {100.0 * q[0]['strategy_mean']:+.2f}% versus Q3 {100.0 * q[2]['strategy_mean']:+.2f}%; "
    f"Q5, the best equity months, is {100.0 * q[4]['strategy_mean']:+.2f}%.",
    fontsize=9,
)
ax.set_ylabel("mean monthly return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
save(fig, "move_quintiles.svg")

null = np.load(HERE / "placebo_direction.npy")
actual = res["direction_placebo"]["actual_gross_sharpe"]
fig, ax = plt.subplots(figsize=(7.2, 3.4))
ax.hist(null, bins=40, color="#9bb0c7", edgecolor="none")
ax.axvline(actual, color="#1f4e79", lw=1.4, label=f"actual gross Sharpe {num(actual)}")
ax.axvline(res["direction_placebo"]["null_p95"], color="#b85c38", ls="--", lw=1.0, label="null 95th percentile")
ax.set_title(
    f"Direction placebo, {res['direction_placebo']['draws']} draws, zero-cost book. "
    f"p = {res['direction_placebo']['p']:.3f}. The actual Sharpe sits inside the null.",
    fontsize=9,
)
ax.set_xlabel("full-sample gross Sharpe", fontsize=8)
ax.set_ylabel("draws", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
ax.legend(fontsize=7)
save(fig, "placebo.svg")

rd = [datetime.strptime(d, "%Y-%m-%d") for d in ph["rolling_252"]["dates"]]
rs = ph["rolling_252"]["sharpe"]
fig, ax = plt.subplots(figsize=(7.2, 3.4))
ax.plot(rd, rs, color="#1f4e79", lw=1.0)
ax.axhline(0, color="k", lw=0.7)
style(
    ax,
    "Trailing 252-session Sharpe (post hoc). "
    f"{100 * ph['rolling_252']['frac_positive']:.0f}% of windows are positive. "
    f"Range {num(ph['rolling_252']['min'])} to {num(ph['rolling_252']['max'])}; "
    f"the last window is {num(ph['rolling_252']['last'])}.",
    "Sharpe",
)
save(fig, "rolling_sharpe.svg")
print(f"wrote {len(list(FIG.glob('*.svg')))} svg files to {FIG}")
