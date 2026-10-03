# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Standard figures. Reads the saved files and does not recompute metrics."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))

from research.kit.charts import render_standard  # noqa: E402


def main() -> None:
    for path in render_standard(Path(__file__).resolve().parent):
        print(path)


if __name__ == "__main__":
    main()
