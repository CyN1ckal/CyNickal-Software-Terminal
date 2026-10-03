# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Numbers, files, and figures for a new pre-registered study.

Importing this package does not import matplotlib. Figures live in
`research.kit.charts`.
"""

from research.kit.checks import (
    NO_CROSS_MARKET,
    Criterion,
    Thresholds,
    asset_adjusted_direction_placebo,
    block_bootstrap,
    concentration_stress_test,
    direction_placebo,
    evaluate,
    pvalue,
    standardized_timing_placebo,
    status_from,
)
from research.kit.files import (
    append_runlog,
    assemble,
    assert_lock,
    git_state,
    rules_sha256,
    write_daily,
    write_lock,
    write_results,
    write_trades,
)
from research.kit.guard import check_verify
from research.kit.metrics import (
    benchmark_performance,
    by_year,
    deflated_sharpe_ratio,
    move_quintiles,
    performance,
)
from research.kit.schema import KIT_SCHEMA, validate

__all__ = [
    "KIT_SCHEMA",
    "NO_CROSS_MARKET",
    "Criterion",
    "Thresholds",
    "append_runlog",
    "assemble",
    "assert_lock",
    "asset_adjusted_direction_placebo",
    "benchmark_performance",
    "block_bootstrap",
    "by_year",
    "check_verify",
    "concentration_stress_test",
    "deflated_sharpe_ratio",
    "direction_placebo",
    "evaluate",
    "git_state",
    "move_quintiles",
    "performance",
    "pvalue",
    "render_standard",
    "rules_sha256",
    "standardized_timing_placebo",
    "status_from",
    "validate",
    "write_daily",
    "write_lock",
    "write_results",
    "write_trades",
]


def __getattr__(name: str):
    if name == "render_standard":
        from research.kit.charts import render_standard

        return render_standard
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
