# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""CLI: lock, charts, summary, and the verify.py import guard."""

from __future__ import annotations

import json
import sys
from pathlib import Path

_USAGE = """\
usage:
  python -m research.kit.lock <study_dir>
  python -m research.kit.charts <study_dir>
  python -m research.kit.summary <study_dir>
  python -m research.kit.guard <verify.py>
"""


def _percent(value) -> str:
    if value is None:
        return "—"
    text = f"{float(value) * 100:+.1f}%"
    return text.replace("-", "−")


def _sharpe(value) -> str:
    if value is None:
        return "—"
    return f"{float(value):.2f}"


def _plain(value) -> str:
    if value is None:
        return "—"
    return f"{float(value):.2f}"


def summary_table(doc: dict) -> str:
    """The report's first numbers table, rendered from a kit_schema 1 document."""
    primary = doc["primary"]
    benchmark = doc["benchmark_metrics"]
    samples = doc["samples"]
    windows = {
        "Full sample": (primary["full"], benchmark["full"], f"{samples['first']} → {samples['last']}"),
        "In-sample": (primary["is"], benchmark["is"], f"{samples['first']} → {samples['is_end']}"),
        "Out-of-sample": (primary["oos"], benchmark["oos"], f"{samples['oos_start']} → {samples['last']}"),
    }
    order = ("Full sample", "In-sample", "Out-of-sample")

    def cells(pick) -> str:
        return " | ".join(pick(windows[name]) for name in order)

    rows = [
        ("Window", lambda item: item[2]),
        ("Sessions", lambda item: str(item[0]["sessions"])),
        ("Total return", lambda item: _percent(item[0]["total_return"])),
        ("CAGR", lambda item: _percent(item[0]["cagr"])),
        ("Annual volatility", lambda item: _percent(item[0]["ann_vol"])),
        ("Sharpe", lambda item: _sharpe(item[0]["sharpe"])),
        ("Max drawdown", lambda item: _percent(item[0]["max_dd"])),
        ("Trades / profit factor", lambda item: f"{item[0]['trades']} / {_plain(item[0]['profit_factor'])}"),
        ("Avg net trade", lambda item: "—" if item[0]["avg_net_bp"] is None else f"{item[0]['avg_net_bp']:+.1f} bp"),
        (
            "Benchmark Sharpe (max DD)",
            lambda item: f"{_sharpe(item[1]['sharpe'])} ({_percent(item[1]['max_dd'])})",
        ),
    ]
    header = "| | " + " | ".join(order) + " |"
    rule = "|---|" + "|".join("---:" for _ in order) + "|"
    body = [f"| {label} | {cells(pick)} |" for label, pick in rows]
    return "\n".join([header, rule, *body])


def _study_dir(argv: list[str]) -> Path:
    if len(argv) != 1:
        print(_USAGE, file=sys.stderr)
        sys.exit(1)
    return Path(argv[0])


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if not args:
        print(_USAGE, file=sys.stderr)
        return 1
    command, rest = args[0], args[1:]
    try:
        if command == "lock":
            from research.kit.files import write_lock

            digest = write_lock(_study_dir(rest))
            print(digest)
            return 0
        if command == "charts":
            from research.kit.charts import render_standard

            for path in render_standard(_study_dir(rest)):
                print(path)
            return 0
        if command == "summary":
            from research.kit.schema import validate

            path = _study_dir(rest) / "results.json"
            doc = json.loads(path.read_text(encoding="utf-8"))
            validate(doc)
            print(summary_table(doc))
            return 0
        if command == "guard":
            from research.kit.guard import check_verify

            if len(rest) != 1:
                print(_USAGE, file=sys.stderr)
                return 1
            check_verify(Path(rest[0]))
            print("ok")
            return 0
    except (ValueError, RuntimeError, OSError, json.JSONDecodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(_USAGE, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
