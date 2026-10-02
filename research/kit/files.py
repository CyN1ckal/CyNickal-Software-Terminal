# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Lock file, run log, and the results document a new study writes."""

from __future__ import annotations

import csv
import hashlib
import json
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

from research.kit.checks import (
    NO_CROSS_MARKET,
    TIMING_FILE,
    Thresholds,
    evaluate,
    placebo_summary,
    save_samples,
    status_from,
)
from research.kit.metrics import as_date, as_dates, benchmark_performance, performance
from research.kit.schema import KIT_SCHEMA

_COST_MULTIPLES = (0.0, 0.5, 1.0, 2.0, 3.0)
_TRADE_COLUMNS = (
    "session",
    "side",
    "entry_time",
    "entry_price",
    "exit_time",
    "exit_price",
    "gross",
    "net",
    "exit_reason",
)
_RUNLOG_HEADER = (
    "# Run log\n\n"
    "One entry per store run, appended by backtest.py. Never edited.\n"
)


def rules_sha256(rules_path: Path) -> str:
    """SHA-256 of RULES.md after CRLF is normalized to LF."""
    payload = Path(rules_path).read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(payload).hexdigest()


_hash_rules = rules_sha256


def _git(repo: Path, *args: str) -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo), *args],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip() or "git failed"
        raise RuntimeError(detail)
    return proc.stdout.strip()


def repo_root(study_dir: Path) -> Path:
    return Path(_git(study_dir, "rev-parse", "--show-toplevel"))


def git_state(study_dir: Path) -> tuple[str, bool]:
    """Return (HEAD, dirty). Dirty is true when `git status --porcelain` is non-empty."""
    root = repo_root(study_dir)
    head = _git(root, "rev-parse", "HEAD")
    porcelain = _git(root, "status", "--porcelain")
    return head, bool(porcelain.strip())


def write_lock(study_dir: Path) -> str:
    """Write RULES.lock next to RULES.md. Returns the sha256."""
    study_dir = Path(study_dir)
    digest = rules_sha256(study_dir / "RULES.md")
    head = _git(repo_root(study_dir), "rev-parse", "HEAD")
    locked = datetime.now(timezone.utc).isoformat(timespec="seconds")
    text = f"sha256 {digest}\nlocked_utc {locked}\ngit_head {head}\n"
    (study_dir / "RULES.lock").write_text(text, encoding="utf-8", newline="\n")
    return digest


def _lock_hash(study_dir: Path) -> str:
    path = Path(study_dir) / "RULES.lock"
    if not path.is_file():
        print(f"RULES.lock is missing in {study_dir}. Refusing to run.", file=sys.stderr)
        sys.exit(1)
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("sha256 "):
            return line.split(" ", 1)[1].strip()
    print(f"RULES.lock in {study_dir} has no sha256 line. Refusing to run.", file=sys.stderr)
    sys.exit(1)


def assert_lock(study_dir: Path) -> str:
    """Return the RULES.md hash, or exit if it disagrees with RULES.lock."""
    study_dir = Path(study_dir)
    have = rules_sha256(study_dir / "RULES.md")
    want = _lock_hash(study_dir)
    if have != want:
        print(
            f"RULES.md hash {have} does not match RULES.lock {want}. Refusing to run.",
            file=sys.stderr,
        )
        sys.exit(1)
    return have


def _fmt3(value) -> str:
    if value is None:
        return "null"
    return f"{float(value):.3f}"


def append_runlog(study_dir: Path, doc: dict, *, reason: str) -> None:
    """Append one store-run entry. Existing entries are left as they are."""
    study_dir = Path(study_dir)
    path = study_dir / "RUNLOG.md"
    if not path.exists():
        path.write_text(_RUNLOG_HEADER, encoding="utf-8", newline="\n")
    primary = doc["primary"]
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    entry = (
        f"\n## {stamp}\n\n"
        f"- Reason: {reason}\n"
        f"- Rules sha256: `{doc['rules_sha256']}`\n"
        f"- Git HEAD: `{doc['git_head']}` (dirty: {doc['git_dirty']})\n"
        "- Sharpe full / IS / OOS: "
        f"{_fmt3(primary['full']['sharpe'])} / {_fmt3(primary['is']['sharpe'])} / {_fmt3(primary['oos']['sharpe'])}\n"
        "- Total return full / IS / OOS: "
        f"{_fmt3(primary['full']['total_return'])} / {_fmt3(primary['is']['total_return'])} / "
        f"{_fmt3(primary['oos']['total_return'])}\n"
        f"- Trades full / OOS: {primary['full']['trades']} / {primary['oos']['trades']}\n"
        f"- Status: {doc['status']}\n"
    )
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(entry)


def _iso(value) -> str:
    return as_date(value).isoformat()


def write_daily(path, sessions, strategy_net, benchmark, held=None) -> None:
    path = Path(path)
    columns = ["session", "strategy_net", "benchmark"]
    if held is not None:
        columns.append("held")
    if not (len(sessions) == len(strategy_net) == len(benchmark)):
        raise ValueError("daily columns have different lengths")
    if held is not None and len(held) != len(sessions):
        raise ValueError("held and sessions have different lengths")
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        for index, session in enumerate(sessions):
            row = {
                "session": _iso(session),
                "strategy_net": f"{float(strategy_net[index]):.10f}",
                "benchmark": f"{float(benchmark[index]):.10f}",
            }
            if held is not None:
                row["held"] = f"{float(held[index]):.10f}"
            writer.writerow(row)


def write_trades(path, trades: list[dict]) -> None:
    path = Path(path)
    extras: list[str] = []
    for trade in trades:
        for key in trade:
            if key not in _TRADE_COLUMNS and key not in extras:
                extras.append(key)
    extras.sort()
    columns = list(_TRADE_COLUMNS) + extras
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        for trade in trades:
            for key in _TRADE_COLUMNS:
                if key not in trade:
                    raise ValueError(f"trade is missing {key}")
            if trade["side"] not in ("long", "short"):
                raise ValueError(f"trade side must be long or short, got {trade['side']!r}")
            row = {key: trade[key] for key in columns}
            row["session"] = _iso(trade["session"])
            writer.writerow(row)


def write_results(path, doc: dict) -> None:
    from research.kit.schema import validate

    validate(doc)
    payload = json.dumps(doc, indent=2, allow_nan=False) + "\n"
    Path(path).write_text(payload, encoding="utf-8", newline="\n")


def _jsonify(value):
    if value is None or isinstance(value, str):
        return value
    if isinstance(value, bool):
        return bool(value)
    if isinstance(value, int):
        return int(value)
    if isinstance(value, float):
        return float(value)
    if isinstance(value, date):
        return value.isoformat()
    if isinstance(value, dict):
        return {str(key): _jsonify(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_jsonify(item) for item in value]
    if hasattr(value, "item"):
        return _jsonify(value.item())
    raise TypeError(f"cannot store {type(value).__name__} in results.json")


def _require_cost_multiples(costs: list[dict]) -> None:
    for target in _COST_MULTIPLES:
        if not any(abs(float(row["multiple"]) - target) < 1e-9 for row in costs):
            raise ValueError(f"costs are missing multiple {target}")


def _one_primary(grid: list[dict]) -> None:
    primaries = [row for row in grid if row.get("primary") is True]
    if len(primaries) != 1:
        raise ValueError(f"grid must have exactly one primary, found {len(primaries)}")


def _store_placebo(result: dict | None, filename: str, study_dir: Path | None) -> dict | None:
    if result is None:
        return None
    samples = result.get("samples")
    summary = placebo_summary(result)
    summary["samples_file"] = filename
    if samples is not None and study_dir is not None:
        save_samples(Path(study_dir) / filename, samples)
    return summary


def assemble(
    *,
    label: str,
    benchmark_name: str,
    model: str,
    sessions,
    strategy_net,
    benchmark,
    trades,
    is_end,
    oos_start,
    seeds: dict,
    costs: list[dict],
    delay: dict,
    grid: list[dict],
    placebo_direction: dict,
    placebo_timing: dict | None,
    bootstrap: dict,
    by_year: list,
    by_move_quintile: list,
    cross_market,
    thresholds: Thresholds | None = None,
    extra: list | None = None,
    study: dict | None = None,
    held=None,
    void_reason: str | None = None,
    study_dir: Path | None = None,
    rules_sha256: str | None = None,
    git_head: str | None = None,
    git_dirty: bool | None = None,
) -> dict:
    """Build a kit_schema 1 document. Does not write results.json."""
    if study_dir is not None:
        study_dir = Path(study_dir)
        digest = rules_sha256 if rules_sha256 is not None else _hash_rules(study_dir / "RULES.md")
        lock_path = study_dir / "RULES.lock"
        if lock_path.is_file():
            locked = ""
            for line in lock_path.read_text(encoding="utf-8").splitlines():
                if line.startswith("sha256 "):
                    locked = line.split(" ", 1)[1].strip()
            if locked and locked != digest:
                raise RuntimeError(f"RULES.md hash {digest} does not match RULES.lock {locked}")
        if git_head is None or git_dirty is None:
            head, dirty = git_state(study_dir)
            git_head = head if git_head is None else git_head
            git_dirty = dirty if git_dirty is None else git_dirty
    elif rules_sha256 is None or git_head is None or git_dirty is None:
        raise ValueError("assemble requires rules_sha256, git_head, and git_dirty when study_dir is omitted")
    else:
        digest = rules_sha256

    for key in ("direction", "timing", "bootstrap"):
        if key not in seeds:
            raise ValueError(f"seeds is missing {key}")
    for key in ("full_sharpe", "oos_sharpe", "full_return", "oos_return"):
        if key not in delay:
            raise ValueError(f"delay is missing {key}")
    _require_cost_multiples(costs)
    _one_primary(grid)
    if cross_market is not NO_CROSS_MARKET:
        if not isinstance(cross_market, list) or not cross_market:
            raise ValueError("cross_market must be a non-empty list or NO_CROSS_MARKET")
        for row in cross_market:
            for key in ("symbol", "is_sharpe", "oos_sharpe", "full_return", "full_profit_factor"):
                if key not in row:
                    raise ValueError(f"cross_market row is missing {key}")

    dates = as_dates(sessions)
    primary = performance(
        dates, strategy_net, trades,
        is_end=is_end, oos_start=oos_start, model=model, held=held,
    )
    bench = benchmark_performance(
        dates, benchmark, is_end=is_end, oos_start=oos_start, model=model,
    )
    lines = evaluate(
        oos_sharpe=primary["oos"]["sharpe"],
        oos_profit_factor=primary["oos"]["profit_factor"],
        placebo_p=placebo_direction.get("p"),
        is_sharpe=primary["is"]["sharpe"],
        grid=grid,
        costs=costs,
        oos_trades=primary["oos"]["trades"],
        cross_market=cross_market,
        thresholds=thresholds,
        extra=extra,
    )
    reason = void_reason if void_reason and void_reason.strip() else None
    status = status_from(lines, void_reason=reason)
    direction = _store_placebo(placebo_direction, "placebo_direction.npy", study_dir)
    timing = _store_placebo(placebo_timing, TIMING_FILE, study_dir)
    applicable = cross_market is not NO_CROSS_MARKET
    doc = {
        "kit_schema": KIT_SCHEMA,
        "rules_sha256": digest,
        "git_head": git_head,
        "git_dirty": bool(git_dirty),
        "strategy_label": label,
        "benchmark_name": benchmark_name,
        "return_model": model,
        "samples": {
            "is_end": as_date(is_end).isoformat(),
            "oos_start": as_date(oos_start).isoformat(),
            "first": min(dates).isoformat() if dates else None,
            "last": max(dates).isoformat() if dates else None,
        },
        "seeds": {key: int(seeds[key]) for key in ("direction", "timing", "bootstrap")},
        "primary": primary,
        "benchmark_metrics": bench,
        "costs": costs,
        "delay": delay,
        "grid": grid,
        "placebo": {"direction": direction, "timing": timing},
        "bootstrap": bootstrap,
        "by_year": by_year,
        "by_move_quintile": by_move_quintile,
        "cross_market": cross_market if applicable else None,
        "cross_market_applicable": applicable,
        "acceptance": lines,
        "status": status,
        "secondaries": [],
        "study": study or {},
    }
    if reason:
        doc["void_reason"] = reason
    return _jsonify(doc)
