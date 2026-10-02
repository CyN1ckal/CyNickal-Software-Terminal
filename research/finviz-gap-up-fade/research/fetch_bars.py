# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Download daily bars for the sampled Finviz names. Paid ingest. No returns.

    python research/finviz-gap-up-fade/research/fetch_bars.py
"""

from __future__ import annotations

import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
INGEST = ROOT / "cmake-build-release" / "ingest.exe"
LOG = HERE / "ingest.log"


def main() -> None:
    universe = json.loads((HERE / "universe.json").read_text(encoding="utf-8"))
    symbols = list(universe["primary"]) + list(universe["cross"])
    failed = []
    with LOG.open("w", encoding="utf-8") as log:
        for i, sym in enumerate(symbols, 1):
            print(f"START {i}/{len(symbols)} {sym}", flush=True)
            proc = subprocess.run(
                [str(INGEST), "--timeframe", "1d", "--from", "20160104", "--to", "20260925", sym],
                cwd=ROOT,
                stdout=log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            if proc.returncode != 0:
                failed.append(sym)
                print(f"FAIL {sym} exit={proc.returncode}", flush=True)
            else:
                print(f"OK {sym}", flush=True)
    print("FAILED " + " ".join(failed))


if __name__ == "__main__":
    main()
