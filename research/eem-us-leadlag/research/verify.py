# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the EEM lead-lag trades. Does not import backtest.py."""

import csv
import hashlib
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData  # noqa: E402

HERE = Path(__file__).resolve().parent
LAST = date(2026, 10, 1)
SEED = 20261104


def digest_rules() -> str:
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(raw).hexdigest()


def locked_hash() -> str:
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        if line.startswith("sha256 "):
            return line.split()[1]
    raise SystemExit("RULES.lock has no sha256")


def append_log(text: str) -> None:
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)
        if not text.endswith("\n"):
            f.write("\n")


def replay():
    with MarketData() as md:
        spy = md.bars("SPY", "1d", start="2011-01-04", end="2026-10-01")
        eem = md.bars("EEM", "1d", start="2011-01-04", end="2026-10-01")
    by = {b.session: b for b in eem}
    expected = []
    flat_dates = []
    eval_dates = []
    for i in range(2, len(spy)):
        d = spy[i].session
        if d > LAST:
            continue
        eval_dates.append(d)
        prev = spy[i - 1].close / spy[i - 2].close - 1.0
        if prev > 0.0:
            side = "long"
        elif prev < 0.0:
            side = "short"
        else:
            side = None
        bar = by.get(d)
        if side is None or bar is None:
            flat_dates.append(d)
            continue
        gross = (1.0 if side == "long" else -1.0) * (bar.close / bar.open - 1.0)
        stamp = "13:00" if d in EARLY_CLOSES else "16:00"
        expected.append({
            "date": d,
            "side": side,
            "entry_time": f"{d.isoformat()} 09:30",
            "entry_px": round(bar.open, 6),
            "exit_time": f"{d.isoformat()} {stamp}",
            "exit_px": round(bar.close, 6),
            "gross": gross,
        })
    return eval_dates, flat_dates, expected


def main() -> None:
    got = digest_rules()
    locked = locked_hash()
    if got != locked:
        raise SystemExit(f"RULES.md hash {got} does not match RULES.lock")
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as f:
        saved = list(csv.DictReader(f))
    eval_dates, flat_dates, expected = replay()
    mismatches = []
    if len(saved) != len(expected):
        mismatches.append(f"trade count {len(saved)} != {len(expected)}")
    for i, (row, exp) in enumerate(zip(saved, expected)):
        if row["side"] != exp["side"]:
            mismatches.append(f"{exp['date']} side {row['side']} != {exp['side']}")
        if row["entry_time"] != exp["entry_time"] or row["exit_time"] != exp["exit_time"]:
            mismatches.append(f"{exp['date']} time {row['entry_time']} {row['exit_time']}")
        if abs(float(row["entry_px"]) - exp["entry_px"]) > 1e-9:
            mismatches.append(f"{exp['date']} entry {row['entry_px']} != {exp['entry_px']}")
        if abs(float(row["exit_px"]) - exp["exit_px"]) > 1e-9:
            mismatches.append(f"{exp['date']} exit {row['exit_px']} != {exp['exit_px']}")
        if abs(float(row["gross"]) - exp["gross"]) > 1e-9:
            mismatches.append(f"{exp['date']} gross {row['gross']} != {exp['gross']}")
        if len(mismatches) > 8:
            break
    saved_dates = {row["entry_time"][:10] for row in saved}
    rng = np.random.default_rng(SEED)
    pick = rng.choice(len(eval_dates), size=40, replace=False)
    sample_bad = 0
    for j in pick:
        d = eval_dates[int(j)]
        in_file = d.isoformat() in saved_dates
        in_expected = any(t["date"] == d for t in expected)
        if in_file != in_expected:
            sample_bad += 1
            mismatches.append(f"sample {d} file={in_file} replay={in_expected}")
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = "yes" if subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip() else "no"
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    ok = not mismatches
    append_log(
        f"## {now}\n"
        f"- reason: verify replay\n"
        f"- rules_sha256: {locked}\n"
        f"- git_head: {head}\n"
        f"- dirty: {dirty}\n"
        f"- trades_checked: {len(expected)}\n"
        f"- trades_matched: {len(expected) if ok else 0}\n"
        f"- flat_sessions: {len(flat_dates)}\n"
        f"- sample_sessions: 40\n"
        f"- sample_seed: {SEED}\n"
        f"- sample_mismatches: {sample_bad}\n"
        f"- mismatches: {len(mismatches)}\n"
    )
    if not ok:
        print("\n".join(mismatches))
        raise SystemExit(f"verify failed with {len(mismatches)} mismatches")
    print(f"matched {len(expected)} trades; sampled 40 sessions; flat sessions {len(flat_dates)}")


if __name__ == "__main__":
    main()
