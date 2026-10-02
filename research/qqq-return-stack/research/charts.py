# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/qqq-return-stack/report/. Run after backtest.py and posthoc.py.

    python research/qqq-return-stack/research/charts.py

Every figure is drawn from results.json, posthoc.json, or daily.csv. Cumulative
paths are running sums or products of daily.csv columns; nothing else is recomputed.
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
from matplotlib.colors import LinearSegmentedColormap, TwoSlopeNorm  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"
OOS = date(2024, 7, 1)
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
NEUTRAL = "#f0efec"
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"
RED = "#e34948"
GRAY = "#8a8984"
PORTS = [("S", "S: QQQ + 7 IS-positive sleeves (primary)", BLUE),
         ("K", "K: QQQ + 4 paper-trading candidates", ORANGE),
         ("ALL", "ALL: QQQ + all 12 sleeves", AQUA),
         ("Q", "QQQ buy and hold", GRAY)]
SLEEVES = ("P1", "T", "C", "F", "M", "POP", "SCALE", "MART", "BOLL", "GAP", "REV", "RSI2")
NAMES = {"P1": "P1 intraday trend", "T": "T 15m Turtle", "C": "C channel breakout", "F": "F IGV minute fade",
         "M": "M micro futures trend", "POP": "POP opening-pop fade", "SCALE": "SCALE ATR scale-in",
         "MART": "MART ATR martingale", "BOLL": "BOLL Bollinger adding", "GAP": "GAP small-cap gap fade",
         "REV": "REV weekly reversal", "RSI2": "RSI2 SPY dip-buy"}
DIVERGE = LinearSegmentedColormap.from_list("div", [RED, NEUTRAL, BLUE])

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "font.family": "DejaVu Sans", "font.size": 10, "text.color": TEXT,
    "axes.edgecolor": GRID, "axes.labelcolor": TEXT_2, "axes.titlesize": 12,
    "axes.titleweight": "bold", "axes.titlelocation": "left", "axes.titlecolor": TEXT,
    "xtick.color": TEXT_2, "ytick.color": TEXT_2, "axes.grid": True, "grid.color": GRID,
    "grid.linewidth": 0.8, "axes.spines.top": False, "axes.spines.right": False,
    "axes.spines.left": False, "legend.frameon": False, "lines.linewidth": 2,
    "svg.fonttype": "path", "ytick.major.size": 0, "axes.titlepad": 12, "axes.axisbelow": True, "axes.ymargin": 0.08,
})


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT / name, bbox_inches="tight")
    plt.close(fig)


def mark_oos(ax, label=True) -> None:
    ax.axvline(OOS, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    if label:
        ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(), va="top", fontsize=9, color=TEXT_2)


def value_labels(ax, y, v, fmt, dots=None) -> None:
    """Label each bar just past whichever is further out: the bar end or its dot."""
    for i, (yi, x) in enumerate(zip(y, v)):
        end = x if dots is None else (max(x, dots[i]) if x >= 0 else min(x, dots[i]))
        ax.annotate(fmt.format(x), (end, yi), xytext=(7 if x >= 0 else -7, 0), textcoords="offset points",
                    ha="left" if x >= 0 else "right", va="center", fontsize=8, color=TEXT_2)


def label(k: str, res: dict) -> str:
    tag = " ●" if res["scales"][k]["in_S"] else ""
    return NAMES[k] + tag


def load():
    res = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    post = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    rows = list(csv.DictReader((HERE / "daily.csv").open(newline="", encoding="utf-8")))
    d = [date.fromisoformat(r["session"]) for r in rows]
    cols = {k: np.array([float(r[k]) for r in rows]) for k in rows[0] if k not in ("session", "sample")}
    return res, post, d, cols


def fig_equity(res, d, c):
    fig, ax = plt.subplots(figsize=(10, 5.2))
    for k, name, color in PORTS:
        eq = np.cumprod(1 + c[k])
        ax.plot(d, eq, color=color, linewidth=2 if k != "Q" else 1.8, label=name)
        ax.annotate(f"{eq[-1]:.2f}", (d[-1], eq[-1]), xytext=(6, 0), textcoords="offset points",
                    va="center", fontsize=9, color=TEXT)
    ax.set_yscale("log")
    ax.set_yticks([0.5, 1, 2, 4, 8])
    ax.set_yticklabels(["$0.50", "$1", "$2", "$4", "$8"])
    ax.minorticks_off()
    mark_oos(ax)
    ax.set_title("Growth of $1: QQQ core with strategies stacked on top (log scale)")
    ax.legend(loc="upper left", fontsize=9)
    save(fig, "equity.svg")


def fig_drawdown(res, d, c):
    fig, ax = plt.subplots(figsize=(10, 3.8))
    for k, name, color in PORTS:
        if k == "ALL":
            continue
        eq = np.cumprod(1 + c[k])
        dd = eq / np.maximum.accumulate(np.maximum(eq, 1.0)) - 1
        ax.plot(d, dd * 100, color=color, linewidth=1.6, label=name)
    mark_oos(ax)
    ax.set_ylabel("drawdown, %")
    ax.set_title("Drawdown from the running peak")
    ax.legend(loc="lower right", fontsize=9)
    save(fig, "drawdown.svg")


def fig_addone(res):
    a = res["add_one"]
    order = sorted(SLEEVES, key=lambda k: a[k]["oos"]["d_sharpe"])
    y = np.arange(len(order))
    fig, axes = plt.subplots(1, 3, figsize=(12, 5.4), sharey=True)
    specs = [("d_sharpe", "Δ Sharpe", 1.0, "{:+.2f}"), ("d_cagr", "Δ CAGR, pp", 100.0, "{:+.1f}"),
             ("d_max_dd", "Δ max drawdown, pp (+ = shallower)", 100.0, "{:+.1f}")]
    for ax, (key, title, mult, fmt) in zip(axes, specs):
        v = np.array([a[k]["oos"][key] for k in order]) * mult
        ax.barh(y, v, height=0.62, color=[BLUE if x >= 0 else RED for x in v])
        full = None
        if key == "d_sharpe":
            full = np.array([a[k]["full"][key] for k in order])
            ax.scatter(full, y, s=34, facecolor=SURFACE, edgecolor=TEXT, linewidth=1.3, zorder=3,
                       label="full sample")
            ax.legend(loc="lower right", fontsize=8.5, handletextpad=0.3)
        value_labels(ax, y, v, fmt, full)
        ax.axvline(0, color=TEXT_2, linewidth=1)
        ax.set_title(title, fontsize=10.5)
        ax.grid(axis="y", visible=False)
        lo, hi = ax.get_xlim()
        pad = (hi - lo) * 0.18
        ax.set_xlim(lo - pad, hi + pad)
    axes[0].set_yticks(y)
    axes[0].set_yticklabels([label(k, res) for k in order])
    fig.suptitle("Impact of each strategy: QQQ 1× plus that one sleeve at 5% risk, minus QQQ alone (out of sample)",
                 x=0.01, ha="left", fontsize=12, fontweight="bold")
    fig.text(0.01, -0.02, "Bars: 2024-07-01 → 2026-09-25. Hollow dots: full sample. ● = in the primary S. "
             f"QQQ alone OOS: Sharpe {res['metrics']['Q']['oos']['sharpe']:.2f}, "
             f"CAGR {res['metrics']['Q']['oos']['cagr']:.1%}, max DD {res['metrics']['Q']['oos']['max_dd']:.1%}.",
             fontsize=8.5, color=TEXT_2)
    fig.tight_layout()
    save(fig, "impact_addone.svg")


def fig_loo(res):
    fig, axes = plt.subplots(1, 2, figsize=(12, 5.2), gridspec_kw={"width_ratios": [7, 12]})
    for ax, name, title in ((axes[0], "S", "Inside S (7 sleeves)"), (axes[1], "ALL", "Inside ALL (12 sleeves)")):
        loo = res["leave_one_out"][name]
        order = sorted(loo, key=lambda k: loo[k]["oos"]["d_sharpe"])
        y = np.arange(len(order))
        v = np.array([loo[k]["oos"]["d_sharpe"] for k in order])
        f = np.array([loo[k]["full"]["d_sharpe"] for k in order])
        ax.barh(y, v, height=0.62, color=[BLUE if x >= 0 else RED for x in v])
        ax.barh([], [], color=GRAY, label="out of sample (bar)")
        ax.scatter(f, y, s=34, facecolor=SURFACE, edgecolor=TEXT, linewidth=1.3, zorder=3, label="full sample")
        value_labels(ax, y, v, "{:+.2f}", f)
        ax.axvline(0, color=TEXT_2, linewidth=1)
        ax.set_yticks(y)
        ax.set_yticklabels([NAMES[k] for k in order])
        ax.grid(axis="y", visible=False)
        ax.set_title(title, fontsize=10.5)
        lo, hi = ax.get_xlim()
        ax.set_xlim(lo - (hi - lo) * 0.15, hi + (hi - lo) * 0.15)
        ax.legend(loc="lower right", fontsize=8.5)
    fig.suptitle("Marginal effect of each sleeve: Sharpe(book) − Sharpe(book without that sleeve)", x=0.01, ha="left",
                 fontsize=12, fontweight="bold")
    fig.tight_layout()
    save(fig, "impact_leave_one_out.svg")


def fig_contribution(res, d, c):
    fig, axes = plt.subplots(3, 4, figsize=(12, 7.2), sharex=True, sharey=True)
    order = sorted(SLEEVES, key=lambda k: -res["contribution"][k]["full"])
    for ax, k in zip(axes.ravel(), order):
        path = np.cumsum(res["scales"][k]["w"] * c[k]) * 100
        color = BLUE if path[-1] >= 0 else RED
        ax.plot(d, path, color=color, linewidth=1.6)
        ax.axhline(0, color=TEXT_2, linewidth=0.8)
        mark_oos(ax, label=False)
        ax.set_title(label(k, res), fontsize=9.5, pad=6)
        ax.annotate(f"{path[-1]:+.0f}", (d[-1], path[-1]), xytext=(3, 0), textcoords="offset points",
                    va="center", fontsize=8.5, color=TEXT)
        ax.tick_params(axis="x", labelsize=8, rotation=0)
    for ax in axes[:, 0]:
        ax.set_ylabel("pp of equity", fontsize=9)
    for ax in axes[-1]:
        ax.xaxis.set_major_locator(matplotlib.dates.YearLocator(2))
        ax.xaxis.set_major_formatter(matplotlib.dates.DateFormatter("%Y"))
    q = res["contribution"]["Q"]["full"] * 100
    fig.suptitle("Cumulative contribution of each sleeve at 5% risk (sum of scaled daily returns)", x=0.01, ha="left",
                 fontsize=12, fontweight="bold")
    fig.text(0.01, -0.01, f"Shared y-axis. Dashed line: start of out of sample. ● = in S. The QQQ core's own sum over "
             f"the same sessions is {q:+.0f} pp, off this scale.", fontsize=8.5, color=TEXT_2)
    fig.tight_layout()
    save(fig, "contribution.svg")


def heat(ax, m, rows_, cols_, fmt, vmax):
    im = ax.imshow(m, cmap=DIVERGE, norm=TwoSlopeNorm(0, -vmax, vmax), aspect="auto")
    ax.set_xticks(range(len(cols_)))
    ax.set_xticklabels(cols_)
    ax.set_yticks(range(len(rows_)))
    ax.set_yticklabels(rows_)
    ax.grid(False)
    for i in range(m.shape[0]):
        for j in range(m.shape[1]):
            ax.text(j, i, fmt(m[i, j]), ha="center", va="center", fontsize=8,
                    color="#ffffff" if abs(m[i, j]) > 0.6 * vmax else TEXT)
    for s in ax.spines.values():
        s.set_visible(False)
    return im


def fig_years(res, post):
    cy = post["contribution_by_year"]
    order = sorted(SLEEVES, key=lambda k: -res["contribution"][k]["full"])
    m = np.array([cy[k] for k in order]) * 100
    fig, ax = plt.subplots(figsize=(8.5, 6))
    heat(ax, m, [label(k, res) for k in order], [y if y != "2021" else "2021*" for y in cy["years"]],
         lambda v: f"{v:+.1f}", max(abs(m.min()), abs(m.max())))
    ax.set_title("Contribution by calendar year, pp of equity (post hoc)")
    fig.text(0.01, -0.02, "* 2021 starts 2021-10-25; 2026 ends 2026-09-25. "
             "Blue adds to the book, red subtracts. ● = in S. QQQ core: "
             + ", ".join(f"{y} {v * 100:+.0f}" for y, v in zip(cy["years"], cy["Q"])) + ".",
             fontsize=8.2, color=TEXT_2, wrap=True)
    fig.tight_layout()
    save(fig, "contribution_by_year.svg")


def fig_corr(res):
    fig, axes = plt.subplots(1, 2, figsize=(13, 6))
    for ax, win in zip(axes, ("is", "oos")):
        c = res["correlation"][win]
        m = np.array(c["matrix"])
        heat(ax, m, c["keys"], c["keys"], lambda v: f"{v:.2f}".replace("0.", ".").replace("-.", "−."), 1.0)
        ax.set_title(f"{'In sample' if win == 'is' else 'Out of sample'}", fontsize=11)
        ax.tick_params(axis="x", rotation=90)
    fig.suptitle("Daily-return correlations: QQQ and the twelve sleeves (unscaled)", x=0.01, ha="left",
                 fontsize=12, fontweight="bold")
    fig.tight_layout()
    save(fig, "correlation.svg")


def fig_tails(res):
    t = res["tails"]
    order = sorted(SLEEVES, key=lambda k: t["worst"][k])
    y = np.arange(len(order))
    fig, axes = plt.subplots(1, 2, figsize=(12, 5.2), sharey=True)
    for ax, key, title in ((axes[0], "worst", f"QQQ's worst 5% of days (n = {t['n']}, QQQ avg {t['q_worst_mean']:+.2%})"),
                           (axes[1], "best", f"QQQ's best 5% of days (QQQ avg {t['q_best_mean']:+.2%})")):
        v = np.array([t[key][k] for k in order]) * 1e4
        ax.barh(y, v, height=0.62, color=[BLUE if x >= 0 else RED for x in v])
        for yi, x in zip(y, v):
            ax.annotate(f"{x:+.0f}", (x, yi), xytext=(4 if x >= 0 else -4, 0), textcoords="offset points",
                        ha="left" if x >= 0 else "right", va="center", fontsize=8, color=TEXT_2)
        ax.axvline(0, color=TEXT_2, linewidth=1)
        ax.set_title(title, fontsize=10.5)
        ax.set_xlabel("average scaled sleeve return, bp of equity")
        ax.grid(axis="y", visible=False)
        lo, hi = ax.get_xlim()
        ax.set_xlim(lo - (hi - lo) * 0.12, hi + (hi - lo) * 0.12)
    axes[0].set_yticks(y)
    axes[0].set_yticklabels([label(k, res) for k in order])
    fig.suptitle(f"What each sleeve does on QQQ's extreme days (full sample). S overlay: "
                 f"{t['worst']['S_overlay'] * 1e4:+.0f} bp on crash days, {t['best']['S_overlay'] * 1e4:+.0f} bp on boom days",
                 x=0.01, ha="left", fontsize=12, fontweight="bold")
    fig.tight_layout()
    save(fig, "tails.svg")


def fig_risk_return(res, c):
    oos = np.array([s == "OOS" for s in c["_sample"]])
    comps = ["Q"] + list(res["leave_one_out"]["S"].keys())
    rc = res["risk_contribution"]["S"]["oos"]
    total = float(np.sum(c["S"][oos]))
    ret = {k: res["contribution"][k]["oos"] / total for k in comps}
    y = np.arange(len(comps))
    fig, ax = plt.subplots(figsize=(9, 4.8))
    ax.barh(y - 0.18, [ret[k] * 100 for k in comps], height=0.34, color=BLUE, label="share of S's OOS return")
    ax.barh(y + 0.18, [rc[k] * 100 for k in comps], height=0.34, color=ORANGE, label="share of S's OOS variance")
    for yi, k in zip(y, comps):
        for off, v in ((-0.18, ret[k] * 100), (0.18, rc[k] * 100)):
            ax.annotate(f"{v:+.0f}%", (v, yi + off), xytext=(4 if v >= 0 else -4, 0), textcoords="offset points",
                        ha="left" if v >= 0 else "right", va="center", fontsize=8, color=TEXT_2)
    ax.axvline(0, color=TEXT_2, linewidth=1)
    ax.set_yticks(y)
    ax.set_yticklabels(["QQQ core"] + [NAMES[k] for k in comps[1:]])
    ax.invert_yaxis()
    ax.grid(axis="y", visible=False)
    ax.set_title("Where S's out-of-sample return and risk came from")
    ax.legend(loc="lower right", fontsize=9)
    fig.text(0.01, -0.03, "Return share: sum of the component's scaled daily returns ÷ sum of S's daily returns. "
             "Variance share: w·cov(r, S) ÷ var(S). Costs, rebalance, and financing make up the remainder.",
             fontsize=8.2, color=TEXT_2)
    save(fig, "risk_return.svg")


def fig_sweeps(res):
    q = res["metrics"]["Q"]["oos"]["sharpe"]
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.4), sharey=True)
    mults = [float(k) for k in res["cost_sweep"]["S"]]
    buds = [float(k) for k in res["budget_sweep"]["S"]]
    for k, name, color in PORTS[:3]:
        axes[0].plot(mults, [res["cost_sweep"][k][str(m)]["oos_sharpe"] for m in mults], color=color, marker="o",
                     markersize=6, markeredgecolor=SURFACE, label=name)
        axes[1].plot([b * 100 for b in buds], [res["budget_sweep"][k][str(b)]["oos"]["sharpe"] for b in buds],
                     color=color, marker="o", markersize=6, markeredgecolor=SURFACE, label=name)
    for ax in axes:
        ax.axhline(q, color=GRAY, linewidth=1.5)
        ax.text(ax.get_xlim()[1], q, f"QQQ {q:.2f} ", va="bottom", ha="right", fontsize=8.5, color=TEXT_2)
    axes[0].axvline(2, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    axes[0].text(2, 0.02, " acceptance line 5", transform=axes[0].get_xaxis_transform(), fontsize=8.5, color=TEXT_2)
    axes[1].axvline(5, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    axes[1].text(5, 0.02, " locked budget", transform=axes[1].get_xaxis_transform(), fontsize=8.5, color=TEXT_2)
    axes[0].set_xlabel("strategy cost, multiple of each study's base cost")
    axes[1].set_xlabel("risk budget per sleeve, % annual IS volatility (diagnostic)")
    axes[0].set_ylabel("OOS Sharpe")
    axes[0].set_title("Cost sensitivity", fontsize=10.5)
    axes[1].set_title("Stack size", fontsize=10.5)
    axes[1].legend(loc="lower left", fontsize=8.5)
    fig.suptitle("Out-of-sample Sharpe as costs rise and as the stack grows", x=0.01, ha="left", fontsize=12,
                 fontweight="bold")
    fig.tight_layout()
    save(fig, "sweeps.svg")


def fig_leverage(res, d, c):
    fig, ax = plt.subplots(figsize=(10, 3.6))
    ax.plot(d, c["L_S"], color=BLUE, linewidth=1.2, label="S, net long notional at the close")
    ax.axhline(1, color=GRAY, linewidth=1.2, label="1×: the QQQ core alone")
    ax.axhline(2, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)), label="2×: Reg-T overnight limit")
    ax.legend(loc="lower left", fontsize=8.5, ncol=3)
    mark_oos(ax)
    lv = res["leverage"]["S"]
    ax.set_ylabel("net long notional ÷ equity")
    ax.set_title(f"S's overnight net exposure at each close: mean {lv['mean_L']:.2f}×, range {lv['min_L']:.1f}× to "
                 f"{lv['max_L']:.1f}× (spikes are MART)")
    save(fig, "leverage.svg")


def fig_rolling(post, d):
    r = post["rolling_252"]
    x = d[251:]
    fig, ax = plt.subplots(figsize=(10, 3.8))
    for k, name, color in PORTS:
        ax.plot(x, r[k]["series"], color=color, linewidth=1.5, label=name)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.set_title("Trailing 252-session Sharpe (post hoc)")
    ax.legend(loc="lower right", fontsize=8.5, ncol=2)
    save(fig, "rolling_sharpe.svg")


def main() -> None:
    res, post, d, c = load()
    rows = list(csv.DictReader((HERE / "daily.csv").open(newline="", encoding="utf-8")))
    c["_sample"] = [r["sample"] for r in rows]
    fig_equity(res, d, c)
    fig_drawdown(res, d, c)
    fig_addone(res)
    fig_loo(res)
    fig_contribution(res, d, c)
    fig_years(res, post)
    fig_corr(res)
    fig_tails(res)
    fig_risk_return(res, c)
    fig_sweeps(res)
    fig_leverage(res, d, c)
    fig_rolling(post, d)
    print("wrote", sorted(p.name for p in OUT.glob("*.svg")))


if __name__ == "__main__":
    main()
