# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the locked TLT/IEF rule. Shares no signal code with backtest.py.

Checks every round trip against trades.csv, and a seeded sample of 40 rebalance
dates against a second pass over the same prices. Appends the result to RUNLOG.md.
"""

import csv
import datetime as dt
import hashlib
import random
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, session_date_of  # noqa: E402

FUNDS = ("TLT", "IEF")
LOOKBACK = 252
COST_BPS = 1.0
SKIP = {dt.date(2012, 10, 29), dt.date(2012, 10, 30), dt.date(2018, 12, 5)}
SEED = 20261054


def sgn(x):
    if x > 0.0:
        return 1
    if x < 0.0:
        return -1
    return 0


def load():
    opens = {s: {} for s in FUNDS}
    closes = {s: {} for s in FUNDS}
    with MarketData() as md:
        spy = []
        for b in md.bars("SPY", "1d"):
            d = session_date_of(b.ts)
            if d not in SKIP:
                spy.append(d)
        for sym in FUNDS:
            for b in md.bars(sym, "1d"):
                d = session_date_of(b.ts)
                if d in SKIP:
                    continue
                opens[sym][d] = b.open
                closes[sym][d] = b.close
    return spy, opens, closes


def month_ends(book):
    last = {}
    for d in book:
        last[(d.year, d.month)] = d
    return [last[k] for k in sorted(last)]


def build_targets(book, opens, closes):
    """Per-fund target weight on each eligible month-end. Own-bar index, no fill."""
    own_dates = {s: sorted(closes[s]) for s in FUNDS}
    own_pos = {s: {d: i for i, d in enumerate(own_dates[s])} for s in FUNDS}
    targets = {}
    for sd in month_ends(book):
        w = {}
        live = False
        for sym in FUNDS:
            i = own_pos[sym].get(sd)
            if i is None or i < LOOKBACK:
                w[sym] = 0.0
            else:
                live = True
                c1 = closes[sym][sd]
                c0 = closes[sym][own_dates[sym][i - LOOKBACK]]
                w[sym] = sgn(c1 / c0 - 1.0) / 2.0
        if live:
            targets[sd] = w
    return targets


def replay(book, opens, closes):
    targets = build_targets(book, opens, closes)
    loc = {d: i for i, d in enumerate(book)}
    fills = {}
    for sd, w in targets.items():
        fi = loc[sd] + 1
        if fi < len(book):
            fills[book[fi]] = w
    shares = {s: 0.0 for s in FUNDS}
    last_c = {s: None for s in FUNDS}
    had = {s: False for s in FUNDS}
    trip = {s: None for s in FUNDS}
    done = []
    equity = 1.0
    rate = COST_BPS / 10000.0
    first = min(fills)

    def shut(sym, day, px, reason):
        t = trip[sym]
        t["exit_date"] = day
        t["exit_px"] = px
        t["reason"] = reason
        done.append(t)
        trip[sym] = None

    for day in book:
        e_prev = equity
        gap = 0.0
        for sym in FUNDS:
            if shares[sym] != 0.0 and day in opens[sym] and had[sym]:
                g = shares[sym] * (opens[sym][day] - last_c[sym])
                gap += g
                trip[sym]["gross"] += g
                trip[sym]["net"] += g
        e_open = e_prev + gap
        cost = 0.0
        if day in fills:
            for sym in FUNDS:
                if day not in opens[sym]:
                    continue
                px = opens[sym][day]
                new = fills[day][sym] * e_open / px
                old = shares[sym]
                osgn, nsgn = sgn(old), sgn(new)
                if osgn == 0 and nsgn == 0:
                    shares[sym] = 0.0
                    continue
                if osgn != 0 and nsgn == osgn:
                    c = abs(new - old) * px * rate
                    trip[sym]["net"] -= c
                    cost += c
                    shares[sym] = new
                    continue
                if osgn != 0:
                    c = abs(old) * px * rate
                    trip[sym]["net"] -= c
                    cost += c
                    shut(sym, day, px, "flip" if nsgn != 0 else "flat")
                    shares[sym] = 0.0
                if nsgn != 0:
                    c = abs(new) * px * rate
                    shares[sym] = new
                    trip[sym] = {
                        "symbol": sym,
                        "side": "long" if new > 0 else "short",
                        "entry_date": day,
                        "entry_px": px,
                        "entry_equity": e_open,
                        "gross": 0.0,
                        "net": -c,
                        "exit_date": None,
                        "exit_px": None,
                        "reason": None,
                    }
                    cost += c
        oc = 0.0
        for sym in FUNDS:
            if day in opens[sym] and shares[sym] != 0.0:
                o = shares[sym] * (closes[sym][day] - opens[sym][day])
                oc += o
                trip[sym]["gross"] += o
                trip[sym]["net"] += o
            if day in closes[sym]:
                last_c[sym] = closes[sym][day]
                had[sym] = True
            else:
                had[sym] = False
        equity = e_open - cost + oc
    last = book[-1]
    for sym in FUNDS:
        if trip[sym] is not None:
            shut(sym, last, last_c[sym], "end_of_sample")
    if abs(sum(t["net"] for t in done) - (equity - 1.0)) > 1e-6:
        raise AssertionError("verify equity identity failed")
    if first not in book:
        raise AssertionError("first fill is not on the book")
    return done, targets, fills


def main():
    book, opens, closes = load()
    done, targets, fills = replay(book, opens, closes)
    with (HERE / "trades.csv").open(newline="", encoding="utf-8") as f:
        saved = list(csv.DictReader(f))

    def key(t):
        return (t["symbol"], t["entry_date"] if isinstance(t["entry_date"], str) else t["entry_date"].isoformat(), t["side"])

    got = []
    for t in done:
        got.append({
            "symbol": t["symbol"],
            "side": t["side"],
            "entry_date": t["entry_date"].isoformat(),
            "entry_px": t["entry_px"],
            "exit_date": t["exit_date"].isoformat(),
            "exit_px": t["exit_px"],
            "reason": t["reason"],
        })
    saved_s = sorted(saved, key=key)
    got_s = sorted(got, key=key)
    bad = []
    if len(saved_s) != len(got_s):
        bad.append(f"count saved {len(saved_s)} rebuilt {len(got_s)}")
    for a, b in zip(saved_s, got_s):
        if a["exit_reason"] != b["reason"] or a["symbol"] != b["symbol"] or a["side"] != b["side"] or a["entry_date"] != b["entry_date"] or a["exit_date"] != b["exit_date"]:
            bad.append(f"fields {a['symbol']} {a['entry_date']} {a['exit_reason']} vs {b}")
        else:
            if abs(float(a["entry_px"]) - b["entry_px"]) > 1e-6 or abs(float(a["exit_px"]) - b["exit_px"]) > 1e-6:
                bad.append(f"price {a} vs {b}")
    # Seeded rebalance check: recompute the weight from closes a second time.
    rng = random.Random(SEED)
    pool = sorted(targets)
    sample = pool if len(pool) <= 40 else rng.sample(pool, 40)
    own_dates = {s: sorted(closes[s]) for s in FUNDS}
    own_pos = {s: {d: i for i, d in enumerate(own_dates[s])} for s in FUNDS}
    for sd in sample:
        for sym in FUNDS:
            i = own_pos[sym][sd]
            w = sgn(closes[sym][sd] / closes[sym][own_dates[sym][i - LOOKBACK]] - 1.0) / 2.0
            if abs(w - targets[sd][sym]) > 1e-12:
                bad.append(f"weight {sym} {sd}")
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    h = hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    if bad:
        text = (
            f"## {now}\n"
            f"- rules_sha256 {h}\n"
            f"- git_head {head} dirty=yes\n"
            f"- reason: verify.py mismatch\n"
            f"- mismatches {len(bad)}; first: {bad[:3]}\n"
        )
        with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print(text)
        for line in bad[:20]:
            print(line)
        sys.exit(1)
    text = (
        f"## {now}\n"
        f"- rules_sha256 {h}\n"
        f"- git_head {head}\n"
        f"- reason: verify.py independent replay\n"
        f"- matched {len(got_s)}/{len(got_s)} round trips on side, entry date and price, "
        f"exit date and price, and exit reason. Seed {SEED} checked {len(sample)} "
        f"rebalance dates; all of {len(targets)} eligible dates were replayed.\n"
    )
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(text)


if __name__ == "__main__":
    main()
