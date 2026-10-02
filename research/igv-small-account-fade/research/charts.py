# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Figures for research/igv-small-account-fade/report/. Run after backtest.py and posthoc.py.

    python research/igv-small-account-fade/research/charts.py
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

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "report" / "figures"
OOS = date(2024, 7, 1)
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
BLUE, ORANGE, AQUA = "#2a78d6", "#eb6834", "#1baf7a"

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


def save(fig, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(OUT / name)
    plt.close(fig)


def mark_oos(ax) -> None:
    ax.axvline(OOS, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))
    ax.text(OOS, 1.0, "  out of sample →", transform=ax.get_xaxis_transform(),
            va="top", fontsize=9, color=TEXT_2)


def load():
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    posthoc = json.loads((HERE / "posthoc.json").read_text(encoding="utf-8"))
    rows = list(csv.DictReader((HERE / "daily.csv").open(encoding="utf-8")))
    return results, posthoc, rows


def equity(rows) -> None:
    days = [date.fromisoformat(row["session"]) for row in rows]
    strategy = np.cumprod(1 + np.array([float(row["strategy"]) for row in rows]))
    cc = np.cumprod(1 + np.array([float(row["bench_cc"]) for row in rows]))
    oc = np.cumprod(1 + np.array([float(row["bench_oc"]) for row in rows]))
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, cc, color=ORANGE, label="IGV close to close")
    ax.plot(days, oc, color=AQUA, label="IGV open to close")
    ax.plot(days, strategy, color=BLUE, label="Fade, 2 bp per side")
    ax.axhline(1, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="upper left")
    ax.set_title("Growth of $1")
    ax.set_ylabel("Equity")
    save(fig, "equity.svg")


def drawdown(rows) -> None:
    days = [date.fromisoformat(row["session"]) for row in rows]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    for key, color, label in (
        ("bench_cc", ORANGE, "IGV close to close"),
        ("strategy", BLUE, "Fade, 2 bp per side"),
    ):
        r = np.array([float(row[key]) for row in rows])
        eq = np.cumprod(1 + r)
        peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
        ax.plot(days, eq / peak - 1, color=color, label=label)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Drawdown")
    ax.set_ylabel("Drawdown")
    save(fig, "drawdown.svg")


def by_year(results) -> None:
    years = results["breakdowns"]["by_year"]
    labels = list(years)
    strategy = [years[y]["total_return"] for y in labels]
    bench = [years[y]["bench_cc_return"] for y in labels]
    x = np.arange(len(labels))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar(x - 0.18, strategy, width=0.36, color=BLUE, label="Fade")
    ax.bar(x + 0.18, bench, width=0.36, color=ORANGE, label="IGV close to close")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, labels)
    ax.set_title("Calendar-year return")
    ax.set_ylabel("Return")
    ax.legend()
    save(fig, "by_year.svg")


def costs(results) -> None:
    order = ["0.0", "1.0", "2.0", "4.0", "6.0"]
    labels = ["0", "1", "2", "4", "6"]
    full = [results["costs"][k]["full_sharpe"] for k in order]
    oos = [results["costs"][k]["oos_sharpe"] for k in order]
    x = np.arange(len(labels))
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(x, full, color=BLUE, marker="o", label="Full sample")
    ax.plot(x, oos, color=ORANGE, marker="o", label="Out of sample")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(x, labels)
    ax.set_xlabel("Cost per side (bp)")
    ax.set_ylabel("Sharpe")
    ax.set_title("Cost sensitivity")
    ax.legend()
    save(fig, "costs.svg")


def grid(results) -> None:
    cells = results["grid"]["cells"]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.scatter(
        [c["is_sharpe"] for c in cells if not c["primary"]],
        [c["oos_sharpe"] for c in cells if not c["primary"]],
        color=BLUE, s=28, label="Grid",
    )
    primary = next(c for c in cells if c["primary"])
    ax.scatter([primary["is_sharpe"]], [primary["oos_sharpe"]], color=ORANGE, s=64, zorder=3, label="Primary")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.axvline(0, color=TEXT_2, linewidth=1)
    ax.set_xlabel("In-sample Sharpe")
    ax.set_ylabel("Out-of-sample Sharpe")
    ax.set_title("Parameter grid")
    ax.legend()
    save(fig, "grid.svg")


def placebo(results) -> None:
    d = results["direction_placebo"]
    labels = ["Null mean", "Actual gross", "Null 95th"]
    values = [d["null_mean"], d["actual_gross_sharpe"], d["null_p95"]]
    colors = [TEXT_2, BLUE, ORANGE]
    fig, ax = plt.subplots(figsize=(8, 3.4))
    ax.bar(labels, values, color=colors)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_ylabel("Gross Sharpe")
    ax.set_title("Direction placebo")
    save(fig, "placebo.svg")


def quintiles(results) -> None:
    rows = results["breakdowns"]["by_open_close_quintile"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar([str(row["quintile"]) for row in rows], [row["avg_net_bps"] for row in rows], color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xlabel("Quintile of IGV open-to-close return (1 = down day)")
    ax.set_ylabel("Average net trade (bp)")
    ax.set_title("Result by the day's move")
    save(fig, "move_quintiles.svg")


def by_entry(results) -> None:
    rows = results["breakdowns"]["by_entry"]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.bar(list(rows), [rows[k]["avg_net_bps"] for k in rows], color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_ylabel("Average net trade (bp)")
    ax.set_title("Result by entry time")
    fig.autofmt_xdate(rotation=0)
    save(fig, "by_entry.svg")


def rolling(posthoc) -> None:
    points = posthoc["rolling_252"]
    days = [date.fromisoformat(point["session"]) for point in points]
    values = [point["sharpe"] for point in points]
    fig, ax = plt.subplots(figsize=(8, 3.6))
    ax.plot(days, values, color=BLUE, label="252-session Sharpe")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    mark_oos(ax)
    ax.legend(loc="lower left")
    ax.set_title("Rolling Sharpe")
    ax.set_ylabel("Sharpe")
    save(fig, "rolling_sharpe.svg")


def main() -> None:
    results, posthoc, rows = load()
    equity(rows)
    drawdown(rows)
    by_year(results)
    costs(results)
    grid(results)
    placebo(results)
    quintiles(results)
    by_entry(results)
    rolling(posthoc)
    print(f"wrote figures to {OUT}")


if __name__ == "__main__":
    main()
