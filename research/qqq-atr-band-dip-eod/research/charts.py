# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for the report. Reads only results.json, daily.csv, trades.csv and
posthoc.json in this folder; never recomputes from the store. Writes SVG to
../report/figures/."""

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

res = json.loads((HERE / "results.json").read_text())
ph = json.loads((HERE / "posthoc.json").read_text())
daily = [ln.split(",") for ln in (HERE / "daily.csv").read_text().splitlines()[1:]]
dates = [datetime.strptime(d[0], "%Y-%m-%d") for d in daily]
r_net = np.array([float(d[1]) for d in daily])
bh = np.array([float(d[3]) for d in daily])
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


# 1. growth of $1
fig, ax = plt.subplots(figsize=(7, 3.6))
ax.plot(dates, np.cumprod(1 + r_net), label="strategy (net, 1 bp/side)", lw=1.2)
ax.plot(dates, np.cumprod(1 + bh), label="QQQ buy & hold", lw=1.0, alpha=0.8)
style(ax, "QQQ open−1×ATR(14) band dip buy, flat at close: growth of $1 "
           "(dashed = IS/OOS; whole window, strategy ends below $1)", "value of $1")
ax.legend(fontsize=7)
save(fig, "equity.svg")

# 2. drawdowns
fig, ax = plt.subplots(figsize=(7, 3.2))
for x, lab in ((r_net, "strategy"), (bh, "QQQ buy & hold")):
    eq = np.cumprod(1 + x)
    ax.plot(dates, eq / np.maximum.accumulate(eq) - 1, label=lab, lw=1.0)
style(ax, "Drawdown, full sample (strategy max −17.2%, benchmark −35.6%)", "drawdown")
ax.legend(fontsize=7)
save(fig, "drawdown.svg")

# 3. calendar year
yrs = sorted({d.year for d in dates})
sr = [float(res["breakdown"]["by_year"][str(y)]["return_"]) for y in yrs]
br = [float(res["breakdown"]["by_year"][str(y)]["bh"]) for y in yrs]
fig, ax = plt.subplots(figsize=(7, 3.0))
w = 0.4
i = np.arange(len(yrs))
ax.bar(i - w / 2, sr, w, label="strategy")
ax.bar(i + w / 2, br, w, label="QQQ buy & hold")
ax.set_xticks(i, yrs)
ax.axhline(0, color="k", lw=0.7)
ax.set_title("Calendar-year return: strategy negative in 5 of 6 years "
             "(2026 is the only positive year)", fontsize=10)
ax.set_ylabel("return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
ax.legend(fontsize=7)
save(fig, "by_year.svg")

# 4. plateau grid
g = res["symbols"]["QQQ"]["grid"]
ks = [float(k) for k in g]
fig, ax = plt.subplots(figsize=(5.5, 3.0))
ax.plot(ks, [g[f"{k:g}"]["IS_sharpe"] for k in ks], "o-", label="IS Sharpe")
ax.plot(ks, [g[f"{k:g}"]["OOS_sharpe"] for k in ks], "s-", label="OOS Sharpe")
ax.axhline(0, color="k", lw=0.7)
ax.axvline(res["params"]["K"], color="r", ls=":", lw=1, label="primary K=1")
ax.set_title("K-grid (band width in prior-day ATRs): 1 of 5 IS cells > 0, "
             "all 5 OOS cells < 0", fontsize=10)
ax.set_xlabel("K", fontsize=8)
ax.set_ylabel("Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
ax.legend(fontsize=7)
save(fig, "grid.svg")

# 5. costs
cs = res["symbols"]["QQQ"]["costs"]
cx = [float(k[:-2]) for k in cs]
fig, ax = plt.subplots(figsize=(5.5, 3.0))
ax.plot(cx, [cs[f"{c:g}bp"]["sharpe"] for c in cx], "o-", label="full Sharpe")
ax2 = ax.twinx()
ax2.plot(cx, [cs[f"{c:g}bp"]["total_return"] for c in cx], "s-", color="tab:orange", label="full return")
ax.axhline(0, color="k", lw=0.7)
ax2.axhline(0, color="k", lw=0.4)
ax.set_title("Cost sweep (QQQ): Sharpe < 0 already at zero cost — costs are not the cause",
            fontsize=10)
ax.set_xlabel("cost per side (bp)", fontsize=8)
ax.set_ylabel("Sharpe", fontsize=8)
ax2.set_ylabel("total return", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3)
fig.legend(fontsize=7, loc="lower right")
save(fig, "costs.svg")

# 6. placebo
null = np.array(ph["direction_placebo_repeat"]["null_sharpe_samples"])
act = res["placebo"]["direction_actual_gross_sharpe"]
fig, ax = plt.subplots(figsize=(5.5, 3.0))
ax.hist(null, bins=40, color="lightgray", edgecolor="gray", lw=0.4)
ax.axvline(act, color="r", lw=1.5, label=f"actual gross Sharpe {act:.2f}")
ax.axvline(null.mean(), color="k", ls="--", lw=1, label=f"null mean {null.mean():.2f}")
ax.set_title(f"Direction placebo: p = {ph['direction_placebo_repeat']['p']:.3f} "
             "(actual sits inside the coin-flip null, on its bad side)", fontsize=10)
ax.set_xlabel("gross Sharpe", fontsize=8)
ax.tick_params(labelsize=7)
ax.legend(fontsize=7)
save(fig, "placebo.svg")

# 7. rolling sharpe
rs = ph["rolling_sharpe"]
rd = dates[rs["window"] - 1:]
fig, ax = plt.subplots(figsize=(7, 3.0))
ax.plot(rd, rs["strategy"], lw=1.0, label="strategy")
ax.plot(rd, rs["bh"], lw=0.8, alpha=0.7, label="QQQ buy & hold")
ax.axhline(0, color="k", lw=0.7)
style(ax, f"Rolling {rs['window']}-session Sharpe (post hoc): negative for most of the sample", "Sharpe")
ax.legend(fontsize=7)
save(fig, "rolling_sharpe.svg")

# 8. move quintiles
qm = res["breakdown"]["quintile_move"]
fig, ax = plt.subplots(figsize=(5.5, 3.0))
qi = sorted(qm, key=int)
ax.bar([f"Q{int(k)+1}" for k in qi], [qm[k]["mean_net_bp"] for k in qi],
       color=["tab:green" if qm[k]["mean_net_bp"] > 0 else "tab:red" for k in qi])
ax.axhline(0, color="k", lw=0.7)
ax2 = ax.twinx()
ax2.plot([f"Q{int(k)+1}" for k in qi], [qm[k]["win_rate"] for k in qi], "ko-", ms=4, label="win rate")
ax2.set_ylabel("win rate", fontsize=8)
ax.set_title("Net bp per trade by quintile of |open→close| (Q1 small move … Q5 large): "
             "losses concentrate where the day kept going down", fontsize=9.5)
ax.set_ylabel("avg net trade (bp)", fontsize=8)
ax.tick_params(labelsize=7)
ax.grid(alpha=0.3, axis="y")
save(fig, "move_quintiles.svg")

print("wrote", sorted(p.name for p in FIG.glob("*.svg")))
