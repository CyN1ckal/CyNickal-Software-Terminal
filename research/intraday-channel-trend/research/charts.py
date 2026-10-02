# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/intraday-channel-trend/report/. Run after backtest.py."""

from __future__ import annotations

import csv
import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("svg")
import matplotlib.dates as mdates  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap, TwoSlopeNorm  # noqa: E402

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"
RED = "#e34948"
NEUTRAL = "#f0efec"
OOS_START = date(2024, 7, 1)

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "font.family": "DejaVu Sans", "font.size": 10, "text.color": TEXT,
    "axes.edgecolor": GRID, "axes.labelcolor": TEXT_2, "axes.titlesize": 12,
    "axes.titleweight": "bold", "axes.titlelocation": "left", "axes.titlecolor": TEXT,
    "xtick.color": TEXT_2, "ytick.color": TEXT_2, "axes.grid": True, "grid.color": GRID,
    "grid.linewidth": 0.8, "axes.spines.top": False, "axes.spines.right": False,
    "axes.spines.left": False, "legend.frameon": False, "lines.linewidth": 2,
    "svg.fonttype": "path", "axes.titlepad": 14, "axes.axisbelow": True, "axes.ymargin": 0.12,
})


def fnum(text: str) -> float:
    return 0.0 if text == "" else float(text)


def load():
    summary = json.loads((HERE / "summary.json").read_text(encoding="utf-8"))
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    days = [date.fromisoformat(r["session"]) for r in rows]
    series = {
        "book": np.array([float(r["r_book"]) for r in rows]),
        "cc": np.array([fnum(r["cc_book"]) for r in rows]),
        "otc": np.array([fnum(r["otc_book"]) for r in rows]),
    }
    trades = list(csv.DictReader((HERE / "trades.csv").open(encoding="utf-8")))
    return summary, days, series, trades


def mark_oos(ax, label=True) -> None:
    ax.axvline(OOS_START, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    if label:
        ax.text(OOS_START, 1.0, "  out-of-sample →", transform=ax.get_xaxis_transform(),
                va="top", ha="left", fontsize=9, color=TEXT_2)


def end_label(ax, x, y, text, color, dy=0) -> None:
    ax.annotate(text, (x, y), xytext=(6, dy), textcoords="offset points", va="center",
                fontsize=9, color=TEXT, fontweight="bold")
    ax.plot([x], [y], "o", color=color, markersize=6, markeredgecolor=SURFACE, markeredgewidth=2)


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT / f"{name}.svg", bbox_inches="tight")
    plt.close(fig)


def growth(values: np.ndarray) -> np.ndarray:
    return np.cumprod(1.0 + values)


def equity(days, series) -> None:
    fig, ax = plt.subplots(figsize=(9, 4.6))
    curves = [
        ("Close to close", series["cc"], ORANGE),
        ("Open to close", series["otc"], AQUA),
        ("Channel book, 1×", series["book"], BLUE),
    ]
    ends = {name: growth(r)[-1] for name, r, _ in curves}
    for name, r, color in curves:
        eq = growth(r)
        ax.plot(days, eq, color=color, label=name)
        others = [v for k, v in ends.items() if k != name and abs(np.log(v / eq[-1])) < 0.04]
        dy = 0 if not others else (8 if eq[-1] >= np.mean(others) else -8)
        end_label(ax, days[-1], eq[-1], f"{eq[-1]:.2f}×", color, dy)
    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:.1f}×"))
    ax.yaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    ax.set_yticks([0.7, 1.0, 1.5, 2.0])
    mark_oos(ax)
    ax.set_title("Growth of $1, after 1 bp per side (log scale)")
    ax.legend(loc="upper left", fontsize=9)
    ax.xaxis.set_major_locator(mdates.YearLocator())
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
    ax.set_xlim(days[0], days[-1] + (days[-1] - days[0]) * 0.08)
    save(fig, "equity")


def drawdown(days, series) -> None:
    fig, ax = plt.subplots(figsize=(9, 2.8))
    for name, r, color in (("Close to close", series["cc"], ORANGE), ("Channel book", series["book"], BLUE)):
        eq = growth(r)
        dd = eq / np.maximum.accumulate(eq) - 1
        ax.plot(days, dd * 100, color=color, label=name, linewidth=1.6)
        i = int(np.argmin(dd))
        ax.annotate(f"{dd[i]:.1%}", (days[i], dd[i] * 100), xytext=(6, -2), textcoords="offset points",
                    fontsize=9, color=TEXT, va="top")
    mark_oos(ax, label=False)
    ax.set_title("Drawdown from peak (%)")
    ax.legend(loc="lower left", ncols=2, fontsize=9)
    ax.xaxis.set_major_locator(mdates.YearLocator())
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
    save(fig, "drawdown")


def rolling(days, series) -> None:
    r = series["book"]
    window = 252
    out = np.full(len(r), np.nan)
    for i in range(window - 1, len(r)):
        sl = r[i - window + 1 : i + 1]
        sd = float(np.std(sl, ddof=1))
        out[i] = 0.0 if sd == 0 else float(np.mean(sl) / sd * np.sqrt(252))
    valid = np.isfinite(out)
    d = [days[i] for i in range(len(days)) if valid[i]]
    v = out[valid]
    fig, ax = plt.subplots(figsize=(9, 2.8))
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.plot(d, v, color=BLUE)
    i = int(np.argmin(v))
    ax.annotate(f"{v[i]:+.2f} (12 months to {d[i]:%b %Y})", (d[i], v[i]), xytext=(8, 0),
                textcoords="offset points", fontsize=9, va="center")
    ax.plot([d[i]], [v[i]], "o", color=BLUE, markersize=6, markeredgecolor=SURFACE, markeredgewidth=2)
    mark_oos(ax)
    ax.set_title("Book trailing 252-session Sharpe")
    ax.xaxis.set_major_locator(mdates.YearLocator())
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%Y"))
    save(fig, "rolling_sharpe")
    print(f"rolling min {v[i]:+.3f} at {d[i]}  max {v.max():+.3f}  positive {np.mean(v > 0):.1%}")


def quintiles(summary, series) -> None:
    rows = summary["quintiles"]
    otc = series["otc"]
    finite = otc[np.isfinite(otc)]
    edges = np.quantile(finite, [0.0, 0.2, 0.4, 0.6, 0.8, 1.0])
    labels = []
    for i in range(5):
        lo, hi = edges[i] * 100, edges[i + 1] * 100
        labels.append(f"Q{i + 1}\n{lo:+.1f} to {hi:+.1f}%")
    v = [r["mean_strategy"] * 1e4 for r in rows]
    fig, ax = plt.subplots(figsize=(7.5, 3.4))
    ax.bar(labels, v, color=[BLUE if x > 0 else RED for x in v], width=0.6, edgecolor=SURFACE, linewidth=2)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for i, x in enumerate(v):
        ax.annotate(f"{x:+.1f}", (i, x), xytext=(0, 4 if x > 0 else -4), textcoords="offset points",
                    ha="center", va="bottom" if x > 0 else "top", fontsize=9)
    ax.set_title("Average book day (bp) by the day's open-to-close move")
    ax.set_xlabel("Quintile of the equal-weight open-to-close return")
    ax.grid(axis="x", visible=False)
    save(fig, "move_quintiles")


def costs(summary) -> None:
    order = ["0bp", "0.5bp", "1bp", "2bp", "3bp"]
    x = [0.0, 0.5, 1.0, 2.0, 3.0]
    fig, ax = plt.subplots(figsize=(7.5, 3.2))
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for key, name, color in (("full", "Full sample", BLUE), ("oos", "Out-of-sample", ORANGE)):
        y = [summary["costs"][label][key]["book"]["sharpe"] for label in order]
        ax.plot(x, y, color=color, marker="o", markersize=7, markeredgecolor=SURFACE, markeredgewidth=2, label=name)
        ax.annotate(name, (x[-1], y[-1]), xytext=(8, 0), textcoords="offset points", va="center",
                    fontsize=9, fontweight="bold")
    ax.axvline(1.0, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(1.0, 1.0, " assumed", transform=ax.get_xaxis_transform(), va="top", fontsize=9, color=TEXT_2)
    ax.set_xticks(x)
    ax.set_xlabel("Cost per side (bp of notional)")
    ax.set_title("Book Sharpe as trading costs rise")
    ax.set_xlim(-0.2, 3.9)
    ax.legend(loc="upper right", fontsize=9)
    save(fig, "costs")


def grid(summary) -> None:
    cells = summary["grid"]
    ns = sorted({c["n"] for c in cells})
    ks = sorted({c["k"] for c in cells})
    cmap = LinearSegmentedColormap.from_list("div", [RED, NEUTRAL, BLUE])
    fig, axes = plt.subplots(1, 2, figsize=(9.5, 3.6), sharey=True)
    lim = max(abs(c[key]) for c in cells for key in ("is_sharpe", "oos_sharpe"))
    norm = TwoSlopeNorm(0, -lim, lim)
    for ax, key, title in ((axes[0], "is_sharpe", "In-sample Sharpe"), (axes[1], "oos_sharpe", "Out-of-sample Sharpe")):
        m = np.array([[next(c[key] for c in cells if c["n"] == n and c["k"] == k) for k in ks] for n in ns])
        ax.imshow(m, cmap=cmap, norm=norm, aspect="auto")
        for i, n in enumerate(ns):
            for j, k in enumerate(ks):
                primary = n == 8 and k == 2.5
                ax.text(j, i, f"{m[i, j]:.2f}", ha="center", va="center", fontsize=9,
                        fontweight="bold" if primary else "normal", color=TEXT)
                if primary:
                    ax.add_patch(plt.Rectangle((j - 0.5, i - 0.5), 1, 1, fill=False, edgecolor=TEXT, linewidth=2))
        ax.set_xticks(range(len(ks)), [f"{k:g}" for k in ks])
        ax.set_yticks(range(len(ns)), [str(n) for n in ns])
        ax.set_xlabel("Chandelier multiple K")
        ax.set_title(title)
        ax.grid(False)
    axes[0].set_ylabel("Channel length N (15-minute bars)")
    fig.text(0.01, -0.08, "Outlined cell = pre-registered primary (N 8, K 2.5). Out-of-sample was not used to choose it.",
             ha="left", fontsize=9, color=TEXT_2)
    save(fig, "grid")


def placebo(summary) -> None:
    p = summary["placebo"]
    edges = np.array(p["hist_edges"])
    fig, ax = plt.subplots(figsize=(7.5, 3.0))
    ax.bar(edges[:-1], p["hist"], width=np.diff(edges), align="edge", color="#b9b8b2", edgecolor=SURFACE, linewidth=1)
    ax.axvline(p["actual_sharpe"], color=BLUE, linewidth=2)
    ax.annotate(f"actual {p['actual_sharpe']:.2f}", (p["actual_sharpe"], 1.0), xycoords=("data", "axes fraction"),
                xytext=(-6, -4), textcoords="offset points", ha="right", va="top", fontsize=9, fontweight="bold")
    ax.set_title(f"Same trades, random direction ({p['draws']:,} draws)")
    ax.set_xlabel("Full-sample Sharpe after 1 bp per side")
    ax.set_ylabel("Draws")
    ax.grid(axis="x", visible=False)
    save(fig, "placebo")


def by_entry(summary) -> None:
    rows = summary["hours"]
    labels = [f"{r['key']}:00" for r in rows]
    v = [r["sum_net"] * 100 for r in rows]
    fig, ax = plt.subplots(figsize=(8.5, 3.0))
    ax.bar(labels, v, color=[BLUE if x > 0 else RED for x in v], width=0.6, edgecolor=SURFACE, linewidth=2)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for i, (x, r) in enumerate(zip(v, rows)):
        ax.annotate(f"{r['trades']}", (i, x), xytext=(0, 3 if x > 0 else -3), textcoords="offset points",
                    ha="center", va="bottom" if x > 0 else "top", fontsize=8, color=TEXT_2)
    ax.set_title("Summed net return by entry hour (% of that day's equity; label = trades)")
    ax.grid(axis="x", visible=False)
    save(fig, "by_entry")


def by_year(summary) -> None:
    rows = summary["years"]
    years = [str(r["year"]) + ("*" if r["year"] in (2021, 2026) else "") for r in rows]
    a = [r["strategy"] * 100 for r in rows]
    b = [r["close_to_close"] * 100 for r in rows]
    x = np.arange(len(years))
    fig, ax = plt.subplots(figsize=(8, 3.2))
    ax.bar(x - 0.19, a, 0.36, color=BLUE, label="Channel book", edgecolor=SURFACE, linewidth=2)
    ax.bar(x + 0.19, b, 0.36, color=ORANGE, label="Close to close", edgecolor=SURFACE, linewidth=2)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    for i, v in enumerate(a):
        ax.annotate(f"{v:+.1f}", (i - 0.19, v), xytext=(0, 3 if v > 0 else -3), textcoords="offset points",
                    ha="center", va="bottom" if v > 0 else "top", fontsize=8)
    ax.set_xticks(x, years)
    ax.set_title("Calendar-year return (%)  * partial year")
    ax.legend(loc="upper right", ncols=2, fontsize=9)
    ax.grid(axis="x", visible=False)
    save(fig, "by_year")


def describe(summary, trades) -> None:
    holds = np.array([float(t["hold_minutes"]) for t in trades])
    sessions = {t["session"] for t in trades}
    eod = sum(1 for t in trades if t["reason"] == "eod")
    print(f"trades {len(trades)}  sessions_with_trade {len(sessions)}  "
          f"median_hold {np.median(holds):.0f} min  mean_hold {np.mean(holds):.0f} min  "
          f"eod_share {eod / len(trades):.1%}")
    print(f"placebo p {summary['placebo']['p']:.4f}")


def main() -> None:
    summary, days, series, trades = load()
    equity(days, series)
    drawdown(days, series)
    rolling(days, series)
    quintiles(summary, series)
    costs(summary)
    grid(summary)
    placebo(summary)
    by_entry(summary)
    by_year(summary)
    describe(summary, trades)
    print("wrote", sorted(p.name for p in OUT.glob("*.svg")))


if __name__ == "__main__":
    main()
