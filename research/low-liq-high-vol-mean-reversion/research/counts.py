# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Coverage and signal counts before any forward return is computed.

Prints session counts, corporate-action counts, and how many weeks the
eligibility rule selects a book. It does not print a price, a return, a P&L,
or a summary of the formation returns.

    python research/low-liq-high-vol-mean-reversion/research/counts.py
"""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData  # noqa: E402

from backtest import (  # noqa: E402
    MIN_BARS,
    OOS_START,
    Params,
    build_eligible,
    build_plan,
    build_selections,
    load_panel,
)

HERE = Path(__file__).resolve().parent


def main() -> None:
    universe = json.loads((HERE / "universe.json").read_text(encoding="utf-8"))
    print(f"screen_rows {universe['screen_rows']}")
    print(f"primary_assigned {len(universe['primary'])}")
    print(f"cross_assigned {len(universe['cross'])}")
    kept = {"primary": [], "cross": []}
    with MarketData() as md:
        for book in ("primary", "cross"):
            for sym in universe[book]:
                try:
                    md.resolve(sym)
                except LookupError:
                    print(f"drop {book} {sym} not_in_store")
                    continue
                bars = md.bars(sym, "1d", start="2016-01-04", end="2026-09-25")
                acts = md.corporate_actions(sym)
                kinds = Counter(a["type"] for a in acts)
                n = len(bars)
                if n < MIN_BARS:
                    first = bars[0].session.isoformat() if bars else "-"
                    last = bars[-1].session.isoformat() if bars else "-"
                    print(f"drop {book} {sym} bars={n} first={first} last={last}")
                    continue
                kept[book].append(sym)
                print(
                    f"keep {book} {sym} bars={n} "
                    f"first={bars[0].session.isoformat()} last={bars[-1].session.isoformat()} "
                    f"splits={kinds.get('split', 0)} dividends={kinds.get('dividend', 0)} "
                    f"other={sorted(k for k in kinds if k not in ('split', 'dividend'))}"
                )
    print("KEPT_PRIMARY " + " ".join(kept["primary"]))
    print("KEPT_CROSS " + " ".join(kept["cross"]))
    with MarketData() as md:
        bad = 0
        for sym in kept["primary"] + kept["cross"]:
            for bar in md.bars(sym, "1d", start="2016-01-04", end="2026-09-25"):
                if bar.open <= 0 or bar.close <= 0 or bar.high <= 0 or bar.low <= 0:
                    bad += 1
                    print(f"nonpositive {sym} {bar.session.isoformat()}")
        print(f"nonpositive_bars {bad}")

    params = Params()
    for book, names in kept.items():
        panel = load_panel(names)
        eligible = build_eligible(panel, params)
        selections = build_selections(eligible, params, rng=None)
        plan = build_plan(panel, params)
        weeks = 0
        slots = 0
        oos_slots = 0
        oos_weeks = 0
        for day, (signal_week, _field) in plan.items():
            if signal_week is None:
                continue
            chosen = selections.get(signal_week) or {}
            if not chosen:
                continue
            weeks += 1
            slots += len(chosen)
            if day >= OOS_START:
                oos_weeks += 1
                oos_slots += len(chosen)
        print(
            f"signals {book} names={len(names)} planned_rebalances={len(plan)} "
            f"weeks_with_book={weeks} slots={slots} "
            f"oos_weeks_with_book={oos_weeks} oos_slots={oos_slots}"
        )


if __name__ == "__main__":
    main()
