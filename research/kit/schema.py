# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Validate a kit_schema 1 results document. Legacy study files are rejected."""

from __future__ import annotations

import math
import numbers

from research.kit.checks import status_from
from research.kit.metrics import bench_keys, trade_keys

KIT_SCHEMA = 1

_REQUIRED = (
    "kit_schema",
    "rules_sha256",
    "git_head",
    "git_dirty",
    "strategy_label",
    "benchmark_name",
    "return_model",
    "samples",
    "seeds",
    "primary",
    "benchmark_metrics",
    "costs",
    "delay",
    "grid",
    "placebo",
    "bootstrap",
    "by_year",
    "by_move_quintile",
    "cross_market",
    "cross_market_applicable",
    "acceptance",
    "status",
    "secondaries",
    "study",
)
_STATUSES = {
    "Paper-trading candidate",
    "Rejected",
    "Inconclusive",
    "Void",
    "Rejected (Survivorship Contaminated)",
    "Diagnostic Only",
}
_COST_MULTIPLES = (0.0, 0.5, 1.0, 2.0, 3.0)
_PLACEBO_KEYS = (
    "actual_gross_sharpe",
    "null_mean",
    "null_p95",
    "p",
    "draws",
    "seed",
    "samples_file",
)
_BOOTSTRAP_KEYS = ("block", "draws", "seed", "sharpe_lo", "sharpe_hi", "share_le_0")
_SAMPLE_KEYS = ("is_end", "oos_start", "first", "last")
_SEED_KEYS = ("direction", "timing", "bootstrap")
_DELAY_KEYS = ("full_sharpe", "oos_sharpe", "full_return", "oos_return")


def _has_multiple(costs, target: float) -> bool:
    return any(abs(float(row["multiple"]) - target) < 1e-9 for row in costs)


def _walk(value, problems: list[str], where: str) -> None:
    if isinstance(value, dict):
        for key, item in value.items():
            _walk(item, problems, f"{where}.{key}")
        return
    if isinstance(value, (list, tuple)):
        for index, item in enumerate(value):
            _walk(item, problems, f"{where}[{index}]")
        return
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return
    if isinstance(value, numbers.Real) and not math.isfinite(float(value)):
        problems.append(f"{where} is not a finite number")


def validate(doc: dict) -> None:
    """Raise ValueError listing every problem found in `doc`."""
    problems: list[str] = []
    if not isinstance(doc, dict):
        raise ValueError("results document is not an object")
    for key in _REQUIRED:
        if key not in doc:
            problems.append(f"missing {key}")
    if doc.get("kit_schema") != KIT_SCHEMA:
        problems.append(f"kit_schema must be {KIT_SCHEMA}")
    if doc.get("return_model") not in ("compound", "additive"):
        problems.append("return_model must be compound or additive")
    status = doc.get("status")
    if status not in _STATUSES:
        problems.append(f"status {status!r} is not a study verdict")
    if status == "Void" and not str(doc.get("void_reason") or "").strip():
        problems.append("Void requires void_reason")
    _walk(doc, problems, "results")

    costs = doc.get("costs")
    if isinstance(costs, list):
        for target in _COST_MULTIPLES:
            if not _has_multiple(costs, target):
                problems.append(f"costs are missing multiple {target}")
    else:
        problems.append("costs must be a list")

    grid = doc.get("grid")
    if isinstance(grid, list):
        primaries = [row for row in grid if isinstance(row, dict) and row.get("primary") is True]
        if len(primaries) != 1:
            problems.append(f"grid must have exactly one primary, found {len(primaries)}")
    else:
        problems.append("grid must be a list")

    acceptance = doc.get("acceptance")
    if isinstance(acceptance, list):
        for index, line in enumerate(acceptance):
            if not isinstance(line, dict) or any(key not in line for key in ("id", "name", "required", "actual", "passed")):
                problems.append(f"acceptance[{index}] is missing id, name, required, actual, or passed")
        if status in _STATUSES and status != "Void":
            expected = status_from(acceptance, void_reason=doc.get("void_reason"))
            if status != expected:
                problems.append(f"status is {status}, acceptance lines say {expected}")
    else:
        problems.append("acceptance must be a list")

    primary = doc.get("primary")
    if isinstance(primary, dict):
        for window in ("full", "is", "oos"):
            block = primary.get(window)
            if not isinstance(block, dict):
                problems.append(f"primary.{window} is missing")
                continue
            for key in trade_keys():
                if key not in block:
                    problems.append(f"primary.{window} is missing {key}")
    else:
        problems.append("primary must be an object")

    benchmark = doc.get("benchmark_metrics")
    if isinstance(benchmark, dict):
        for window in ("full", "is", "oos"):
            block = benchmark.get(window)
            if not isinstance(block, dict):
                problems.append(f"benchmark_metrics.{window} is missing")
                continue
            for key in bench_keys():
                if key not in block:
                    problems.append(f"benchmark_metrics.{window} is missing {key}")
    else:
        problems.append("benchmark_metrics must be an object")

    placebo = doc.get("placebo")
    if isinstance(placebo, dict):
        direction = placebo.get("direction")
        if not isinstance(direction, dict):
            problems.append("placebo.direction is missing")
        else:
            for key in _PLACEBO_KEYS:
                if key not in direction:
                    problems.append(f"placebo.direction is missing {key}")
    else:
        problems.append("placebo must be an object")

    bootstrap = doc.get("bootstrap")
    if isinstance(bootstrap, dict):
        for key in _BOOTSTRAP_KEYS:
            if key not in bootstrap:
                problems.append(f"bootstrap is missing {key}")
    else:
        problems.append("bootstrap must be an object")

    samples = doc.get("samples")
    if isinstance(samples, dict):
        for key in _SAMPLE_KEYS:
            if key not in samples:
                problems.append(f"samples is missing {key}")
    else:
        problems.append("samples must be an object")

    seeds = doc.get("seeds")
    if not isinstance(seeds, dict) or any(key not in seeds for key in _SEED_KEYS):
        problems.append("seeds must include direction, timing, and bootstrap")

    delay = doc.get("delay")
    if not isinstance(delay, dict) or any(key not in delay for key in _DELAY_KEYS):
        problems.append("delay must include full and OOS Sharpe and return")

    applicable = doc.get("cross_market_applicable")
    if not isinstance(applicable, bool):
        problems.append("cross_market_applicable must be a boolean")
    elif applicable and not isinstance(doc.get("cross_market"), list):
        problems.append("cross_market must be a list when it applies")
    elif applicable is False and doc.get("cross_market") is not None:
        problems.append("cross_market must be null when it does not apply")

    if "universe" in doc:
        universe = doc["universe"]
        if not isinstance(universe, dict):
            problems.append("universe must be an object")
        elif universe.get("type") not in {"single_asset", "fixed_basket", "point_in_time", "static_snapshot"}:
            problems.append("universe.type must be one of 'single_asset', 'fixed_basket', 'point_in_time', 'static_snapshot'")

    if problems:
        raise ValueError("results.json failed kit_schema 1:\n- " + "\n- ".join(problems))
