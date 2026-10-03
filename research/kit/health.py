# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Phase 0 data health and integrity audit for instruments and sample spans."""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT / "agent-data") not in sys.path:
    sys.path.insert(0, str(ROOT / "agent-data"))

try:
    from mdq import MarketData, session_date_of
except ImportError:
    MarketData = None
    session_date_of = None


@dataclass
class Anomaly:
    symbol: str
    session: str
    category: str  # 'unadjusted_jump', 'extended_hours_contamination', 'zero_dividend_drag', 'calendar_hole'
    severity: str  # 'CRITICAL', 'WARNING'
    detail: str


@dataclass
class HealthReport:
    passed: bool
    evaluated_symbols: list[str]
    anomalies: list[Anomaly] = field(default_factory=list)

    def critical_count(self) -> int:
        return sum(1 for a in self.anomalies if a.severity == "CRITICAL")


class DataHealthAuditor:
    """Read-only data integrity auditor for Phase 0 checks."""

    def __init__(self, md=None):
        if md is None and MarketData is not None:
            self._md = MarketData()
            self._owns_md = True
        else:
            self._md = md
            self._owns_md = False

    def close(self):
        if self._owns_md and self._md is not None:
            self._md.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def audit_instrument(
        self,
        symbol: str,
        start=None,
        end=None,
        *,
        is_yield_instrument: bool = False,
        jump_threshold: float = 0.50,
    ) -> list[Anomaly]:
        anomalies: list[Anomaly] = []
        if self._md is None:
            return anomalies

        inst = self._md.resolve(symbol)
        if not inst:
            anomalies.append(
                Anomaly(
                    symbol=symbol,
                    session=str(start or "UNKNOWN"),
                    category="unresolved_symbol",
                    severity="CRITICAL",
                    detail=f"Symbol {symbol} could not be resolved in the market-data store.",
                )
            )
            return anomalies

        # 1. Price Jump & Missing Split Check
        adj_bars = self._md.bars(symbol, timeframe="1d", start=start, end=end, adjust=True)
        raw_splits = self._md.corporate_actions(symbol)
        split_dates = {a.get("ex_date") for a in raw_splits if a.get("type") == "split"}

        for i in range(1, len(adj_bars)):
            prev, curr = adj_bars[i - 1], adj_bars[i]
            if prev.close <= 0.0 or curr.close <= 0.0:
                continue
            ratio = curr.close / prev.close
            pct_change = abs(ratio - 1.0)
            sess_str = curr.session.isoformat() if hasattr(curr.session, "isoformat") else str(curr.session)

            if pct_change >= jump_threshold:
                if sess_str not in split_dates:
                    anomalies.append(
                        Anomaly(
                            symbol=symbol,
                            session=sess_str,
                            category="unadjusted_jump",
                            severity="CRITICAL",
                            detail=(
                                f"Single-day price dislocation of {pct_change:.1%} (ratio {ratio:.3f}) "
                                f"from {prev.close:.2f} to {curr.close:.2f} with no split recorded in corporate_action."
                            ),
                        )
                    )

        # 2. Extended-Hours Contamination Check (for instruments with 1m bars)
        cov_1m = self._md.coverage_summary(symbol, "1m")
        if cov_1m and cov_1m.get("complete", 0) > 0:
            m_bars = self._md.bars(symbol, timeframe="1m", start=start, end=end)
            m_by_day = {}
            for b in m_bars:
                m_by_day.setdefault(b.session, []).append(b)

            d_bars = {b.session: b.close for b in adj_bars}
            for d, day_m_bars in m_by_day.items():
                if d in d_bars and day_m_bars:
                    rth_close = day_m_bars[-1].close
                    d_close = d_bars[d]
                    if rth_close > 0:
                        discrepancy_bp = abs(d_close / rth_close - 1.0) * 10000.0
                        if discrepancy_bp > 20.0:
                            d_str = d.isoformat() if hasattr(d, "isoformat") else str(d)
                            anomalies.append(
                                Anomaly(
                                    symbol=symbol,
                                    session=d_str,
                                    category="extended_hours_contamination",
                                    severity="CRITICAL" if discrepancy_bp > 100.0 else "WARNING",
                                    detail=(
                                        f"Stored daily close {d_close:.2f} differs from 16:00 RTH close "
                                        f"{rth_close:.2f} by {discrepancy_bp:.1f} bp."
                                    ),
                                )
                            )

        # 3. Dividend Coverage Check for Fixed-Income / Yield Assets
        if is_yield_instrument:
            dividends = [a for a in raw_splits if a.get("type") == "dividend"]
            if not dividends:
                anomalies.append(
                    Anomaly(
                        symbol=symbol,
                        session=str(start or "START"),
                        category="zero_dividend_drag",
                        severity="CRITICAL",
                        detail=(
                            f"Instrument {symbol} is flagged as yield-dependent, but 0 dividend "
                            "records exist in corporate_action. Total return cannot be evaluated."
                        ),
                    )
                )

        return anomalies

    def audit_universe(
        self,
        symbols: list[str],
        start=None,
        end=None,
        *,
        yield_symbols: set[str] | None = None,
    ) -> HealthReport:
        yield_symbols = yield_symbols or set()
        all_anomalies: list[Anomaly] = []
        for sym in symbols:
            anoms = self.audit_instrument(
                sym,
                start=start,
                end=end,
                is_yield_instrument=(sym in yield_symbols),
            )
            all_anomalies.extend(anoms)

        passed = not any(a.severity == "CRITICAL" for a in all_anomalies)
        return HealthReport(
            passed=passed,
            evaluated_symbols=list(symbols),
            anomalies=all_anomalies,
        )
