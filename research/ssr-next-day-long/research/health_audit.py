# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Phase 0 health audit for symbols with a tradeable SSR episode. No strategy returns."""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "agent-data"))

from mdq import MarketData  # noqa: E402
from research.kit.health import DataHealthAuditor  # noqa: E402

HERE = Path(__file__).resolve().parent
# Dividend-paying funds in this snapshot. The check only asks whether any
# dividend row exists. It is not a return.
YIELD = {
    "EEM", "EFA", "EWA", "EWC", "EWG", "EWH", "EWJ", "EWL", "EWS", "EWT", "EWU",
    "EWW", "EWY", "EWZ", "GLD", "IWM", "QQQ", "SLV", "SPY", "UNG", "USO",
    "XLB", "XLC", "XLE", "XLF", "XLI", "XLK", "XLP", "XLRE", "XLU", "XLV", "XLY",
}


def main() -> None:
    counts = json.loads((HERE / "counts.json").read_text(encoding="utf-8"))
    symbols = counts["tradeable_symbols_list"]
    with MarketData() as md:
        dividend_rows = md.sql("SELECT count(*) AS n FROM corporate_action WHERE type = 'dividend'")[0]["n"]
        split_rows = md.sql("SELECT count(*) AS n FROM corporate_action WHERE type = 'split'")[0]["n"]
        minute = md.sql(
            "SELECT c.symbol AS symbol, count(*) AS sessions FROM coverage_day d "
            "JOIN instrument_current c ON c.id = d.instrument_id "
            "WHERE d.timeframe_s = 60 AND d.bar_count > 0 GROUP BY c.symbol"
        )
        auditor = DataHealthAuditor(md)
        report = auditor.audit_universe(symbols, yield_symbols=YIELD & set(symbols))
    by_cat = Counter((item.severity, item.category) for item in report.anomalies)
    summary = {
        "symbols": len(symbols),
        "passed": report.passed,
        "critical": report.critical_count(),
        "warnings": sum(1 for item in report.anomalies if item.severity == "WARNING"),
        "by_severity_category": {f"{sev}:{cat}": n for (sev, cat), n in sorted(by_cat.items())},
        "dividend_rows_in_store": dividend_rows,
        "split_rows_in_store": split_rows,
        "symbols_with_1m": [row["symbol"] for row in minute],
        "anomalies": [
            {
                "symbol": item.symbol,
                "session": item.session,
                "category": item.category,
                "severity": item.severity,
                "detail": item.detail,
            }
            for item in report.anomalies
        ],
    }
    (HERE / "health.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"symbols: {summary['symbols']}")
    print(f"passed: {summary['passed']}")
    print(f"critical: {summary['critical']}")
    print(f"warnings: {summary['warnings']}")
    print(f"by_severity_category: {summary['by_severity_category']}")
    print(f"dividend_rows_in_store: {dividend_rows}")
    print(f"split_rows_in_store: {split_rows}")
    print(f"symbols_with_1m: {summary['symbols_with_1m']}")


if __name__ == "__main__":
    main()
