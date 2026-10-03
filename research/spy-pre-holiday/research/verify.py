# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of every SPY pre-holiday trade. Does not import backtest.py."""

import csv
import hashlib
import subprocess
import sys
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import EARLY_CLOSES, MarketData, is_nyse_holiday

HERE = Path(__file__).resolve().parent
FIRST = "2011-01-04"
LAST = "2026-10-01"
OOS_START = date(2024, 7, 1)
NOTIONAL = 1.0
COST_SIDE = 0.0001
SEED = 20261094
SAMPLE = 40


def is_pre_holiday(session: date) -> bool:
    nxt = session + timedelta(days=1)
    while nxt.weekday() >= 5:
        nxt += timedelta(days=1)
    return is_nyse_holiday(nxt)


def rules_digest():
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    digest = hashlib.sha256(raw).hexdigest()
    locked = {}
    text = (HERE / "RULES.lock").read_text(encoding="utf-8").replace("\r\n", "\n")
    for line in text.splitlines():
        if line.strip():
            key, value = line.split(" ", 1)
            locked[key] = value
    if locked.get("sha256") != digest:
        raise SystemExit(f"RULES.md hash mismatch: {digest} != {locked.get('sha256')}")
    return digest


def append_log(text):
    path = HERE / "RUNLOG.md"
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
        if not text.endswith("\n"):
            handle.write("\n")


def replay(bars):
    first = next(i for i, b in enumerate(bars) if is_pre_holiday(b.session))
    prev = bars[first - 1].close
    daily = []
    trades = []
    equity = 1.0
    for bar in bars[first:]:
        gross = bar.close / bar.open - 1.0
        c2c = bar.close / prev - 1.0
        prev = bar.close
        if is_pre_holiday(bar.session):
            net = NOTIONAL * (gross - 2.0 * COST_SIDE)
            strat_gross = NOTIONAL * gross
            hh, mm = (13, 0) if bar.session in EARLY_CLOSES else (16, 0)
            trades.append({
                "side": "long",
                "entry_time": f"{bar.session.isoformat()} 09:30",
                "entry_px": bar.open,
                "exit_time": f"{bar.session.isoformat()} {hh:02d}:{mm:02d}",
                "exit_px": bar.close,
                "gross": gross,
                "net": net,
                "exit_reason": "session_close",
            })
            traded = 1
        else:
            net = 0.0
            strat_gross = 0.0
            traded = 0
        equity *= 1.0 + net
        daily.append({
            "date": bar.session.isoformat(),
            "strategy_net": net,
            "strategy_gross": strat_gross,
            "open_to_close": gross,
            "close_to_close": c2c,
            "traded": traded,
            "equity": equity,
        })
    return trades, daily


def close_enough(left, right, tol):
    return abs(float(left) - float(right)) <= tol


def main():
    digest = rules_digest()
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    dirty = "yes" if subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip() else "no"
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as handle:
        stored_trades = list(csv.DictReader(handle))
    with (HERE / "daily.csv").open(encoding="utf-8", newline="") as handle:
        stored_daily = list(csv.DictReader(handle))
    with MarketData() as md:
        bars = md.bars("SPY", "1d", start=FIRST, end=LAST)
    trades, daily = replay(bars)
    bad = []
    if len(trades) != len(stored_trades):
        bad.append(f"trade count {len(trades)} != {len(stored_trades)}")
    for i, (got, row) in enumerate(zip(trades, stored_trades)):
        for key in ("side", "entry_time", "exit_time", "exit_reason"):
            if got[key] != row[key]:
                bad.append(f"trade {i} {key} {got[key]} != {row[key]}")
        for key, tol in (("entry_px", 1e-8), ("exit_px", 1e-8), ("gross", 1e-12), ("net", 1e-12)):
            if not close_enough(got[key], row[key], tol):
                bad.append(f"trade {i} {key} {got[key]} != {row[key]}")
    if len(daily) != len(stored_daily):
        bad.append(f"session count {len(daily)} != {len(stored_daily)}")
    for i, (got, row) in enumerate(zip(daily, stored_daily)):
        if got["date"] != row["date"] or got["traded"] != int(row["traded"]):
            bad.append(f"session {i} date/traded")
        for key, tol in (
            ("strategy_net", 1e-12), ("strategy_gross", 1e-12),
            ("open_to_close", 1e-12), ("close_to_close", 1e-12), ("equity", 1e-12),
        ):
            if not close_enough(got[key], row[key], tol):
                bad.append(f"session {got['date']} {key}")
    rng = np.random.default_rng(SEED)
    picked = rng.choice(len(daily), size=min(SAMPLE, len(daily)), replace=False)
    sample_bad = 0
    for i in picked:
        if not close_enough(daily[i]["strategy_net"], stored_daily[i]["strategy_net"], 1e-12):
            sample_bad += 1
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    if bad or sample_bad:
        append_log(
            f"\n## {now}\n"
            f"- rules_sha256 {digest}\n"
            f"- git_head {head} dirty={dirty}\n"
            f"- reason: verification replay failed\n"
            f"- mismatches {len(bad)}; sample bad {sample_bad}; first {bad[:5]}\n"
        )
        raise SystemExit(f"verify failed: {len(bad)} mismatches; sample bad {sample_bad}; {bad[:5]}")
    append_log(
        f"\n## {now}\n"
        f"- rules_sha256 {digest}\n"
        f"- git_head {head} dirty={dirty}\n"
        f"- reason: verification replay\n"
        f"- matched {len(trades)}/{len(trades)} SPY pre-holiday trades on side, entry time, "
        f"entry price, exit time, exit price, gross, net, and reason; strategy_net matched on "
        f"all {len(daily)} evaluation sessions; explicit sample of {len(picked)} "
        f"(seed {SEED}) matched (0 bad)\n"
    )
    print(f"matched {len(trades)} trades and {len(daily)} sessions")


if __name__ == "__main__":
    main()
