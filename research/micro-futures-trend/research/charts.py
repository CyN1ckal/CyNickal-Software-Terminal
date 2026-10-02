# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Report figures, built only from daily.csv, results.json, posthoc.json, and
placebo_null.npy. Writes SVGs to ../report/figures/.

    python research/micro-futures-trend/research/charts.py
"""
import csv
import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.dates as mdates  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

HERE = Path(__file__).resolve().parent
FIG = HERE.parent / "report" / "figures"

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
GRID = "#e4e3df"
S = {"primary": "#2a78d6", "s1": "#eb6834", "b1": "#1baf7a", "b2": "#eda100"}
NEG = "#e34948"
BOUNDARY = date(2024, 10, 1)

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": GRID, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8, "axes.spines.top": False,
    "axes.spines.right": False, "font.size": 10, "axes.titlesize": 11, "axes.titlecolor": INK,
    "axes.titleweight": "bold", "axes.titlelocation": "left", "svg.fonttype": "none",
    "lines.linewidth": 2, "lines.solid_capstyle": "round",
})


def load_daily():
    with (HERE / "daily.csv").open() as f:
        rows = [r for r in csv.DictReader(f) if r["in_window"] == "1"]
    d = [date.fromisoformat(r["date"]) for r in rows]
    col = lambda k: np.array([float(r[k]) for r in rows])
    return d, {k: col(k) for k in ("ret", "s1_ret", "b1_ret", "b2_ret", "equity_sleeve_ret",
                                     "rates_sleeve_ret", "commodities_sleeve_ret")}


def mark_split(ax, label=True):
    ax.axvline(BOUNDARY, color=INK2, lw=1, ls=(0, (3, 3)))
    if label:
        ax.text(BOUNDARY, 1.0, "  out-of-sample →", transform=ax.get_xaxis_transform(), va="top",
                ha="left", color=INK2, fontsize=9)
        ax.text(BOUNDARY, 1.0, "← in-sample  ", transform=ax.get_xaxis_transform(), va="top",
                ha="right", color=INK2, fontsize=9)


def fmt_dates(ax):
    ax.xaxis.set_major_locator(mdates.MonthLocator(bymonth=(1, 7)))
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%b %Y"))


def end_label(ax, x, y, text, color, dy=0.0):
    ax.annotate(text, (x, y), xytext=(6, dy), textcoords="offset points", va="center", color=INK,
                fontsize=9)
    ax.plot([x], [y], "o", ms=5, color=color, mec=SURFACE, mew=1.5)


def save(fig, name):
    FIG.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIG / name, bbox_inches="tight")
    plt.close(fig)


def main():
    d, r = load_daily()
    res = json.loads((HERE / "results.json").read_text())
    ph = json.loads((HERE / "posthoc.json").read_text())
    names = {"primary": "Primary (whole micros, net)", "s1": "S1 fractional (net)",
             "b1": "B1 long ES", "b2": "B2 long-only risk parity"}
    keys = {"primary": "ret", "s1": "s1_ret", "b1": "b1_ret", "b2": "b2_ret"}

    # equity
    fig, ax = plt.subplots(figsize=(9, 4.6))
    ends = []
    for k in ("b1", "b2", "s1", "primary"):
        curve = np.cumprod(1 + r[keys[k]])
        ax.plot(d, curve, color=S[k], lw=2, ls="-" if k in ("primary", "s1") else (0, (5, 2)), label=names[k])
        ends.append((curve[-1], k))
    ends.sort()
    last = None
    for v, k in ends:
        dy = 0 if last is None or abs(v - last) > 0.06 else 10
        end_label(ax, d[-1], v, f"{names[k]}  {v - 1:+.0%}", S[k], dy)
        last = v
    ax.axhline(1, color=INK2, lw=0.8)
    mark_split(ax)
    fmt_dates(ax)
    ax.set_ylabel("Growth of $1")
    ax.set_title("Growth of $1, open-to-close basis, 2022-10-03 → 2026-09-25")
    ax.legend(loc="upper left", frameon=False, fontsize=9)
    ax.set_xlim(d[0], date(2027, 7, 1))
    save(fig, "equity.svg")

    # drawdown
    fig, ax = plt.subplots(figsize=(9, 3.2))
    for k in ("b1", "primary"):
        c = np.cumprod(1 + r[keys[k]])
        dd = c / np.maximum.accumulate(c) - 1
        ax.plot(d, dd * 100, color=S[k], lw=2, ls="-" if k == "primary" else (0, (5, 2)), label=names[k])
    mark_split(ax, label=False)
    fmt_dates(ax)
    ax.set_ylabel("Drawdown, %")
    ax.set_title("Drawdown from peak")
    ax.legend(loc="lower left", frameon=False, fontsize=9)
    save(fig, "drawdown.svg")

    # by year
    yrs = list(res["by_year"])
    fig, ax = plt.subplots(figsize=(8, 3.6))
    w = 0.26
    x = np.arange(len(yrs))
    for i, (k, lab) in enumerate((("primary", "Primary"), ("b1", "B1 long ES"), ("b2", "B2 long-only RP"))):
        vals = [res["by_year"][y][k] * 100 for y in yrs]
        bars = ax.bar(x + (i - 1) * w, vals, w - 0.03, color=S[k], label=lab)
        if k == "primary":
            for b, v in zip(bars, vals):
                ax.annotate(f"{v:+.1f}%", (b.get_x() + b.get_width() / 2, v), xytext=(0, 3 if v >= 0 else -11),
                            textcoords="offset points", ha="center", fontsize=8, color=INK)
    ax.axhline(0, color=INK2, lw=0.8)
    ax.set_xticks(x, [y + (" (Q4)" if y == "2022" else " (to 9/25)" if y == "2026" else "") for y in yrs])
    ax.set_ylabel("Return, %")
    ax.set_title("Calendar-year return")
    ax.legend(frameon=False, fontsize=9, ncols=3, loc="upper left")
    save(fig, "by_year.svg")

    # placebo
    null = np.load(HERE / "placebo_null.npy")
    pl = res["placebo_primary"]
    fig, ax = plt.subplots(figsize=(8, 3.4))
    ax.hist(null, bins=50, color="#c3c2b7", edgecolor=SURFACE, linewidth=0.8)
    ax.axvline(pl["actual_gross_sharpe"], color=S["primary"], lw=2)
    ax.annotate(f"actual gross Sharpe {pl['actual_gross_sharpe']:.2f}\np = {pl['p']:.2f}",
                (pl["actual_gross_sharpe"], ax.get_ylim()[1] * 0.9), xytext=(8, 0), textcoords="offset points",
                color=INK, fontsize=9, va="top")
    ax.set_xlabel("Gross Sharpe with each trade's direction flipped at random (2,000 draws)")
    ax.set_ylabel("Draws")
    ax.set_title("Direction placebo, full sample")
    save(fig, "placebo.svg")

    # grid
    g = res["grid"]
    fig, axs = plt.subplots(1, 2, figsize=(9, 3.4), sharey=True)
    for ax, key, title in ((axs[0], "is_sharpe", "In-sample Sharpe (plateau check)"),
                           (axs[1], "oos_sharpe", "Out-of-sample Sharpe (shown, not used)")):
        for rb, mk in (("monthly", "o"), ("weekly", "s")):
            cells = [c for c in g if c["rebalance"] == rb]
            ax.plot([c["scale"] for c in cells], [c[key] for c in cells], marker=mk, ms=8, color=S["primary"],
                    ls="-" if rb == "monthly" else (0, (4, 2)), mec=SURFACE, mew=1.5, label=f"{rb} rebalance")
        prim = next(c for c in g if c["primary"])
        ax.plot([prim["scale"]], [prim[key]], "o", ms=13, mfc="none", mec=INK, mew=1.2)
        ax.axhline(0, color=INK2, lw=0.8)
        ax.set_xticks([0.5, 0.75, 1.0], ["0.5×\n10/32/126", "0.75×\n16/47/189", "1×\n21/63/252"])
        ax.set_title(title)
    axs[0].set_ylabel("Sharpe")
    axs[0].legend(frameon=False, fontsize=9, loc="lower left")
    axs[1].annotate("primary", (1.0, prim["oos_sharpe"]), xytext=(-20, -22), textcoords="offset points",
                    fontsize=9, color=INK)
    save(fig, "grid.svg")

    # costs
    c = res["costs"]
    fig, ax = plt.subplots(figsize=(7, 3.4))
    for key, lab, col, ls in (("full_sharpe", "Full sample", S["primary"], "-"),
                              ("oos_sharpe", "Out-of-sample", S["s1"], (0, (4, 2)))):
        xs = [e["mult"] for e in c]
        ys = [e[key] for e in c]
        ax.plot(xs, ys, marker="o", ms=8, color=col, ls=ls, mec=SURFACE, mew=1.5, label=lab)
        end_label(ax, xs[-1], ys[-1], f"{lab} {ys[-1]:.2f}", col)
    ax.axhline(0, color=INK2, lw=0.8)
    ax.axvline(1.0, color=INK2, lw=1, ls=(0, (3, 3)))
    ax.set_xlabel("Cost multiple (1× = $1 commission + 1 tick per side, plus rolls)")
    ax.set_ylabel("Sharpe")
    ax.set_xlim(-0.1, 3.9)
    ax.set_title("Cost sensitivity: negative even at zero cost")
    save(fig, "costs.svg")

    # move quintiles
    q = res["move_quintiles"]
    fig, ax = plt.subplots(figsize=(7, 3.4))
    vals = [e["primary_mean"] * 100 for e in q]
    bars = ax.bar(range(1, 6), vals, 0.6, color=[S["primary"] if v >= 0 else NEG for v in vals])
    for b, e, v in zip(bars, q, vals):
        ax.annotate(f"{v:+.2f}%", (b.get_x() + b.get_width() / 2, v), xytext=(0, 3 if v >= 0 else -11),
                    textcoords="offset points", ha="center", fontsize=8, color=INK)
    ax.axhline(0, color=INK2, lw=0.8)
    ax.set_xticks(range(1, 6), [f"Q{e['quintile']}\nES {e['b1_mean']*100:+.1f}%" for e in q])
    ax.set_ylabel("Mean primary monthly return, %")
    ax.set_title("Primary monthly return by quintile of the ES month")
    save(fig, "move_quintiles.svg")

    # rolling sharpe (post hoc)
    rs = ph["rolling_sharpe_252"]
    fig, ax = plt.subplots(figsize=(9, 3.2))
    ax.plot([date.fromisoformat(a) for a, _ in rs], [b for _, b in rs], color=S["primary"])
    ax.axhline(0, color=INK2, lw=0.8)
    mark_split(ax)
    fmt_dates(ax)
    ax.set_ylabel("Sharpe, trailing 252 sessions")
    ax.set_title("Rolling one-year Sharpe (post hoc)")
    save(fig, "rolling_sharpe.svg")

    # sleeves
    fig, ax = plt.subplots(figsize=(9, 3.8))
    cols = {"equity_sleeve_ret": ("Equity sleeve", S["primary"]), "rates_sleeve_ret": ("Rates sleeve", S["s1"]),
            "commodities_sleeve_ret": ("Commodities sleeve", S["b1"])}
    ends = []
    for k, (lab, col) in cols.items():
        cum = np.cumsum(r[k]) * 100
        ax.plot(d, cum, color=col, label=lab)
        ends.append((cum[-1], lab, col))
    for v, lab, col in sorted(ends):
        end_label(ax, d[-1], v, f"{lab} {v:+.1f} pts", col)
    ax.axhline(0, color=INK2, lw=0.8)
    mark_split(ax)
    fmt_dates(ax)
    ax.set_xlim(d[0], date(2027, 5, 1))
    ax.set_ylabel("Cumulative contribution, % of equity")
    ax.set_title("Contribution by asset class (net, summed daily returns)")
    ax.legend(frameon=False, fontsize=9, loc="lower left")
    save(fig, "sleeves.svg")
    print("figures:", sorted(p.name for p in FIG.glob("*.svg")))


if __name__ == "__main__":
    main()
