# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Standard SVG figures for a kit_schema 1 study. Reads saved files only."""

from __future__ import annotations

import csv
import json
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("svg", force=True)
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

from research.kit.schema import validate  # noqa: E402

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
BLUE = "#2a78d6"
ORANGE = "#eb6834"
AQUA = "#1baf7a"

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
    "font.family": "DejaVu Sans",
    "font.size": 10,
    "text.color": TEXT,
    "axes.edgecolor": GRID,
    "axes.labelcolor": TEXT_2,
    "axes.titlesize": 12,
    "axes.titleweight": "bold",
    "axes.titlelocation": "left",
    "axes.titlecolor": TEXT,
    "xtick.color": TEXT_2,
    "ytick.color": TEXT_2,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.spines.left": False,
    "legend.frameon": False,
    "lines.linewidth": 2,
    "svg.fonttype": "path",
    "axes.titlepad": 14,
    "axes.axisbelow": True,
    "axes.ymargin": 0.12,
})

_STANDARD = (
    "equity.svg",
    "drawdown.svg",
    "by_year.svg",
    "placebo.svg",
    "grid.svg",
    "costs.svg",
    "move_quintiles.svg",
)


def _save(fig, directory: Path, name: str) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    path = directory / name
    fig.savefig(path)
    plt.close(fig)
    return path


def _mark_oos(ax, oos: date) -> None:
    ax.axvline(oos, color=TEXT_2, linewidth=1, linestyle=(0, (3, 3)))


def _dates(rows: list[dict]) -> list[date]:
    return [date.fromisoformat(row["session"]) for row in rows]


def _column(rows: list[dict], name: str) -> np.ndarray:
    return np.array([float(row[name]) for row in rows], dtype=float)


def _wealth(series: np.ndarray, model: str) -> np.ndarray:
    if model == "additive":
        return 1.0 + np.cumsum(series)
    return np.cumprod(1.0 + series)


def _drawdown(series: np.ndarray, model: str) -> np.ndarray:
    equity = _wealth(series, model)
    if len(equity) == 0:
        return equity
    peak = np.maximum.accumulate(np.concatenate([np.array([1.0]), equity]))[1:]
    return equity / peak - 1.0


def _finite(value):
    if value is None:
        return np.nan
    return float(value)


def _varying(grid: list[dict]) -> list[str]:
    keys: list[str] = []
    for row in grid:
        for key in row.get("params", {}):
            if key not in keys:
                keys.append(key)
    varying = []
    for key in keys:
        values = []
        for row in grid:
            value = row.get("params", {}).get(key)
            values.append(round(value, 10) if isinstance(value, float) else value)
        if len(set(values)) > 1:
            varying.append(key)
    return varying


def _equity(rows, doc, out: Path, model: str) -> Path:
    days = _dates(rows)
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, _wealth(_column(rows, "benchmark"), model), color=ORANGE, label=doc["benchmark_name"])
    if rows and "benchmark_2" in rows[0]:
        ax.plot(days, _wealth(_column(rows, "benchmark_2"), model), color=AQUA, label="benchmark 2")
    ax.plot(days, _wealth(_column(rows, "strategy_net"), model), color=BLUE, label=doc["strategy_label"])
    ax.axhline(1, color=TEXT_2, linewidth=1)
    _mark_oos(ax, date.fromisoformat(doc["samples"]["oos_start"]))
    ax.legend(loc="upper left")
    ax.set_title("Growth of $1")
    ax.set_ylabel("Equity")
    return _save(fig, out, "equity.svg")


def _dd(rows, doc, out: Path, model: str) -> Path:
    days = _dates(rows)
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, _drawdown(_column(rows, "benchmark"), model), color=ORANGE, label=doc["benchmark_name"])
    ax.plot(days, _drawdown(_column(rows, "strategy_net"), model), color=BLUE, label=doc["strategy_label"])
    ax.axhline(0, color=TEXT_2, linewidth=1)
    _mark_oos(ax, date.fromisoformat(doc["samples"]["oos_start"]))
    ax.legend(loc="lower left")
    ax.set_title("Drawdown")
    ax.set_ylabel("Drawdown")
    return _save(fig, out, "drawdown.svg")


def _by_year(doc, out: Path) -> Path:
    rows = doc["by_year"]
    years = [str(row["year"]) for row in rows]
    index = np.arange(len(rows))
    width = 0.38
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.bar(index - width / 2, [row["strategy_return"] for row in rows], width, color=BLUE, label=doc["strategy_label"])
    ax.bar(index + width / 2, [row["benchmark_return"] for row in rows], width, color=ORANGE, label=doc["benchmark_name"])
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_xticks(index, years)
    ax.legend(loc="upper left")
    ax.set_title("Calendar-year return")
    ax.set_ylabel("Return")
    return _save(fig, out, "by_year.svg")


def _bars_or_hist(ax, samples_path: Path, summary: dict, title: str) -> None:
    actual = summary.get("actual_gross_sharpe")
    if samples_path.is_file():
        samples = np.load(samples_path)
        ax.hist(samples[np.isfinite(samples)], bins=40, color=BLUE)
        if actual is not None:
            ax.axvline(actual, color=ORANGE, linewidth=2, label="actual")
            ax.legend(loc="upper left")
    else:
        labels = ["null mean", "actual", "95th"]
        values = [summary.get("null_mean"), actual, summary.get("null_p95")]
        colors = [TEXT_2, ORANGE, BLUE]
        ax.bar(labels, [0.0 if value is None else value for value in values], color=colors)
    ax.set_title(title)
    ax.set_ylabel("Sharpe")


def _placebo(study_dir: Path, doc, out: Path) -> Path:
    direction = doc["placebo"]["direction"]
    timing = doc["placebo"].get("timing")
    timing_path = study_dir / "placebo_timing.npy"
    panels = 2 if timing_path.is_file() or isinstance(timing, dict) else 1
    fig, axes = plt.subplots(1, panels, figsize=(8, 3.8), squeeze=False)
    _bars_or_hist(axes[0, 0], study_dir / "placebo_direction.npy", direction, "Direction placebo")
    if panels == 2:
        _bars_or_hist(axes[0, 1], timing_path, timing or {}, "Timing placebo")
    return _save(fig, out, "placebo.svg")


def _grid_panel(ax, grid: list[dict], field: str, title: str) -> None:
    varying = _varying(grid)
    if len(varying) == 1:
        key = varying[0]
        ordered = sorted(grid, key=lambda row: row["params"][key])
        xs = [row["params"][key] for row in ordered]
        ys = [_finite(row.get(field)) for row in ordered]
        ax.plot(xs, ys, color=BLUE, marker="o")
        for row, x_value, y_value in zip(ordered, xs, ys):
            if row.get("primary"):
                ax.scatter([x_value], [y_value], s=80, color=ORANGE, zorder=3, label="primary")
                ax.legend(loc="best")
        ax.set_xlabel(key)
    elif len(varying) == 2:
        x_key, y_key = varying
        xs = sorted({row["params"][x_key] for row in grid})
        ys = sorted({row["params"][y_key] for row in grid})
        heat = np.full((len(ys), len(xs)), np.nan)
        primary_cell = None
        for row in grid:
            x_index = xs.index(row["params"][x_key])
            y_index = ys.index(row["params"][y_key])
            heat[y_index, x_index] = _finite(row.get(field))
            if row.get("primary"):
                primary_cell = (x_index, y_index)
        image = ax.imshow(heat, origin="lower", aspect="auto", cmap="coolwarm")
        ax.set_xticks(range(len(xs)), [str(value) for value in xs])
        ax.set_yticks(range(len(ys)), [str(value) for value in ys])
        ax.set_xlabel(x_key)
        ax.set_ylabel(y_key)
        if primary_cell is not None:
            ax.scatter([primary_cell[0]], [primary_cell[1]], s=80, facecolors="none", edgecolors=TEXT, linewidths=2)
        plt.colorbar(image, ax=ax, fraction=0.046, pad=0.04)
    else:
        labels = [row.get("name", str(index)) for index, row in enumerate(grid)]
        values = [_finite(row.get(field)) for row in grid]
        colors = [ORANGE if row.get("primary") else BLUE for row in grid]
        ax.barh(labels, values, color=colors)
        ax.set_xlabel("Sharpe")
    ax.set_title(title)
    if len(varying) == 1:
        ax.set_ylabel("Sharpe")


def _grid(doc, out: Path) -> Path:
    fig, axes = plt.subplots(1, 2, figsize=(8, 3.8))
    _grid_panel(axes[0], doc["grid"], "is_sharpe", "In-sample Sharpe")
    _grid_panel(axes[1], doc["grid"], "oos_sharpe", "OOS, not used for selection")
    return _save(fig, out, "grid.svg")


def _costs(doc, out: Path) -> Path:
    rows = sorted(doc["costs"], key=lambda row: float(row["multiple"]))
    xs = [float(row["multiple"]) for row in rows]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(xs, [_finite(row.get("full_sharpe")) for row in rows], color=BLUE, marker="o", label="Full sample")
    ax.plot(xs, [_finite(row.get("oos_sharpe")) for row in rows], color=ORANGE, marker="o", label="Out of sample")
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.legend(loc="best")
    ax.set_title("Cost sensitivity")
    ax.set_xlabel("Cost multiple")
    ax.set_ylabel("Sharpe")
    return _save(fig, out, "costs.svg")


def _quintiles(doc, out: Path) -> Path:
    rows = doc["by_move_quintile"]
    labels = [str(row["quintile"]) for row in rows]
    values = [0.0 if row["mean_strategy_net"] is None else row["mean_strategy_net"] for row in rows]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.bar(labels, values, color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_title("Mean result by move quintile")
    ax.set_xlabel("Quintile of the day's move (1 = lowest)")
    ax.set_ylabel("Mean strategy return")
    return _save(fig, out, "move_quintiles.svg")


def _rolling(posthoc: dict, out: Path) -> Path | None:
    series = posthoc.get("rolling_sharpe") if isinstance(posthoc, dict) else None
    if not isinstance(series, list):
        return None
    days = [date.fromisoformat(row["session"]) for row in series]
    values = [np.nan if row.get("sharpe") is None else float(row["sharpe"]) for row in series]
    fig, ax = plt.subplots(figsize=(8, 3.8))
    ax.plot(days, values, color=BLUE)
    ax.axhline(0, color=TEXT_2, linewidth=1)
    ax.set_title("Trailing Sharpe (post hoc)")
    ax.set_ylabel("Sharpe")
    return _save(fig, out, "rolling_sharpe.svg")


def render_standard(study_dir: Path) -> list[Path]:
    """Write the standard SVGs under the study's report/figures directory."""
    study_dir = Path(study_dir)
    doc = json.loads((study_dir / "results.json").read_text(encoding="utf-8"))
    validate(doc)
    with (study_dir / "daily.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    posthoc_path = study_dir / "posthoc.json"
    posthoc = json.loads(posthoc_path.read_text(encoding="utf-8")) if posthoc_path.is_file() else {}
    out = study_dir.parent / "report" / "figures"
    model = doc["return_model"]
    written = [
        _equity(rows, doc, out, model),
        _dd(rows, doc, out, model),
        _by_year(doc, out),
        _placebo(study_dir, doc, out),
        _grid(doc, out),
        _costs(doc, out),
        _quintiles(doc, out),
    ]
    rolling = _rolling(posthoc, out)
    if rolling is not None:
        written.append(rolling)
    return written
