# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Draw the standard figures from the saved result files. Does not recompute metrics.

Run from the repo root:
    python research/qqq-holdings-obv-divergence/research/charts.py
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from research.kit.charts import render_standard  # noqa: E402


def main() -> None:
    for path in render_standard(HERE):
        print(path)


if __name__ == "__main__":
    main()
