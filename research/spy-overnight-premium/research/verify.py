# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Naive replay of the SPY close-to-next-open book.

Does not import backtest.py. Reads the store, rebuilds every night, and
matches trades.csv and daily.csv. A mismatch exits non-zero.
"""

import csv
import datetime as dt
import hashlib
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, session_date_of  # noqa: E402

FIRST = dt.date(2011, 1, 4)
LAST = dt.date(2026, 10, 1)
SEED = 20261043


def rules_ok():
    have = hashlib.sha256(
        (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    ).hexdigest()
    want = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()[0].split()[1]
    if have != want:
        sys.exit(f"RULES.md hash {have} != lock {want}; refusing to verify")
    return have


def clock(day, hh, mm):
    return f"{day.isoformat()} {hh:02d}:{mm:02d}"


def rebuild():
    """One loop. Buy the close, sell the next open, charge 2 bp."""
    with MarketData() as md:
        bars = md.bars("SPY", "1d", start=FIRST, end=LAST)
    sessions = [(session_date_of(b.ts), b.open, b.close) for b in bars]
    trades = []
    nets = []
    for i in range(len(sessions) - 1):
        day0, _o0, c0 = sessions[i]
        day1, o1, _c1 = sessions[i + 1]
        hh, mm = (13, 0) if day0 in EARLY_CLOSES else (16, 0)
        gross = o1 / c0 - 1.0
        net = gross - 0.0002
        trades.append({
            "side": "long",
            "entry_time": clock(day0, hh, mm),
            "entry_px": c0,
            "exit_time": clock(day1, 9, 30),
            "exit_px": o1,
            "gross": gross,
            "net": net,
            "exit_reason": "next_open",
        })
        nets.append(net)
    return sessions, trades, nets


def close_enough(a, b):
    return math.isclose(float(a), float(b), rel_tol=1e-9, abs_tol=1e-6)


def append_log(text):
    path = HERE / "RUNLOG.md"
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)
        if not text.endswith("\n"):
            f.write("\n")


def main():
    digest = rules_ok()
    sessions, built, nets = rebuild()
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as f:
        stored = list(csv.DictReader(f))
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as f:
        daily = list(csv.DictReader(f))
    if len(stored) != len(built):
        sys.exit(f"trade count {len(stored)} != rebuilt {len(built)}")
    if len(daily) != len(nets):
        sys.exit(f"daily rows {len(daily)} != rebuilt {len(nets)}")
    # The last close is not an entry. The last open is an exit.
    last_day = sessions[-1][0].isoformat()
    if any(row["entry_time"].startswith(last_day) for row in stored):
        sys.exit("last session was used as an entry")
    if not stored[-1]["exit_time"].startswith(last_day):
        sys.exit("last trade does not exit on the last session")
    mismatches = []
    for i, (got, exp) in enumerate(zip(stored, built)):
        ok = (
            got["side"] == exp["side"]
            and got["entry_time"] == exp["entry_time"]
            and got["exit_time"] == exp["exit_time"]
            and got["exit_reason"] == exp["exit_reason"]
            and close_enough(got["entry_px"], exp["entry_px"])
            and close_enough(got["exit_px"], exp["exit_px"])
            and close_enough(got["gross"], exp["gross"])
            and close_enough(got["net"], exp["net"])
        )
        if not ok:
            mismatches.append(i)
        if not close_enough(daily[i]["strategy_net"], nets[i]):
            mismatches.append(f"daily {i}")
    rng = np.random.default_rng(SEED)
    sample = [int(i) for i in rng.choice(len(stored), size=40, replace=False)]
    sample_bad = [i for i in sample if i in mismatches or f"daily {i}" in mismatches]
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = "yes" if subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip() else "no"
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    if mismatches:
        append_log(
            f"\n## {now}\n"
            f"- rules_sha256 {digest}\n"
            f"- git_head {head} dirty={dirty}\n"
            f"- reason: verification replay failed\n"
            f"- mismatches {len(mismatches)}; first {mismatches[:5]}\n"
        )
        sys.exit(f"verify failed: {len(mismatches)} mismatches, first {mismatches[:5]}")
    append_log(
        f"\n## {now}\n"
        f"- rules_sha256 {digest}\n"
        f"- git_head {head} dirty={dirty}\n"
        f"- reason: verification replay\n"
        f"- matched {len(stored)}/{len(stored)} SPY trades on side, entry time, entry price, "
        f"exit time, exit price, gross, net, and reason; strategy_net matched on all "
        f"{len(daily)} evaluation sessions; explicit sample of 40 (seed {SEED}) matched "
        f"({len(sample_bad)} bad)\n"
    )
    print(f"verify matched {len(stored)} SPY trades and {len(daily)} sessions, sample 40 ok")


if __name__ == "__main__":
    main()
