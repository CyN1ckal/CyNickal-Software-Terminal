# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-lock look: 15-minute bar counts and 55-bar breakout onsets.

Counts only. No price after a signal is read, and no return, P&L, or hit
rate is computed. Run from the repo root:

    python research/qqq-15m-turtle-overnight/research/counts.py
"""

from __future__ import annotations

import sys
from collections import Counter
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData  # noqa: E402

IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
N_IN = 55


def main() -> None:
    with MarketData() as md:
        for sym in ("QQQ", "SPY", "IGV"):
            bars = md.bars(sym, "15m")
            per_session = Counter(b.session for b in bars)
            full = sum(1 for d, n in per_session.items() if d not in EARLY_CLOSES and n == 26)
            early = sum(1 for d, n in per_session.items() if d in EARLY_CLOSES and n == 14)
            odd = sorted((d, n) for d, n in per_session.items()
                         if not ((d in EARLY_CLOSES and n == 14) or (d not in EARLY_CLOSES and n == 26)))
            onsets = Counter()
            prev_state = 0
            for i in range(N_IN, len(bars)):
                hi = max(b.high for b in bars[i - N_IN:i])
                lo = min(b.low for b in bars[i - N_IN:i])
                c = bars[i].close
                state = 1 if c > hi else (-1 if c < lo else 0)
                if state != 0 and state != prev_state:
                    half = "IS" if bars[i].session <= IS_END else "OOS"
                    onsets[(half, state)] += 1
                prev_state = state
            print(f"{sym}: 15m bars {len(bars)}, sessions {len(per_session)}, "
                  f"full(26) {full}, early(14) {early}, other {len(odd)} {odd[:8]}")
            print(f"  55-bar breakout onsets: IS long {onsets[('IS', 1)]} short {onsets[('IS', -1)]}; "
                  f"OOS long {onsets[('OOS', 1)]} short {onsets[('OOS', -1)]}")


if __name__ == "__main__":
    main()
