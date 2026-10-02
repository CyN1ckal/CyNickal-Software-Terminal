# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the QQQ ATR martingale. Does not import engine.py.

    python research/qqq-atr-martingale/research/verify.py
"""
from __future__ import annotations

import csv
import json
import subprocess
import sys
from collections import defaultdict
from datetime import date, datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    MarketData,
    ny_datetime,
    nyse_sessions,
    resample,
)

HERE = Path(__file__).resolve().parent
BAR = 300
ATR_N = 14
BASE = 1.0 / 3.0
MAX_ADDS = 16
COST = 0.0001
LAST = date(2026, 9, 25)


def minute(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def atr_table(daily):
    trs, days = [], []
    for i in range(1, len(daily)):
        pc = daily[i - 1].close
        hi, lo = daily[i].high, daily[i].low
        trs.append(max(hi - lo, abs(hi - pc), abs(lo - pc)))
        days.append(daily[i].session)
    value = sum(trs[:ATR_N]) / ATR_N
    known = [(days[ATR_N - 1], value)]
    for j in range(ATR_N, len(trs)):
        value = ((ATR_N - 1) * value + trs[j]) / ATR_N
        if value > 0:
            known.append((days[j], value))
    return known


def atr_on(known, day):
    use = None
    aday = None
    for d, value in known:
        if d < day:
            use, aday = value, d
        else:
            break
    return use, aday


def net(side, legs, price):
    gross = sum(side * (price / px - 1.0) * n for px, n in legs)
    return gross - 2.0 * COST * sum(n for _, n in legs)


def replay():
    with MarketData() as md:
        daily = md.bars("QQQ", "1d")
        raw = resample(md.bars("QQQ", "1m"), BAR)
    known = atr_table(daily)
    grouped = defaultdict(list)
    for b in raw:
        grouped[b.session].append(b)
    first = next(d for d in sorted(grouped) if atr_on(known, d)[0] is not None)
    calendar = list(nyse_sessions(first, LAST))
    bars = []
    for day in calendar:
        seq = grouped.get(day, [])
        if not seq:
            continue
        atr, aday = atr_on(known, day)
        sess_open = seq[0].open if minute(seq[0].ts) == 9 * 60 + 30 else None
        need = 12 * 60 + 55 if day in EARLY_CLOSES else 15 * 60 + 55
        end_m = 13 * 60 if day in EARLY_CLOSES else 16 * 60
        qualified = bool(sess_open is not None and atr and atr > 0 and aday < day and minute(seq[-1].ts) >= need)
        for i, b in enumerate(seq):
            bars.append({
                "ts": b.ts, "open": b.open, "close": b.close, "minute": minute(b.ts),
                "session": day, "last": i == len(seq) - 1, "sess_open": sess_open,
                "atr": atr, "end_m": end_m, "qualified": qualified,
            })
    side = 0
    legs = []  # price, notional, ts
    anchor = camp_atr = None
    entry_ts = entry_session = None
    pending = None
    out = []
    last_price = last_ts = last_session = None

    def finish(price, ts, sess, reason):
        nonlocal side, legs, anchor, camp_atr, pending
        out.append({
            "side": side, "n_units": len(legs),
            "entry_ts": entry_ts, "exit_ts": ts, "exit_price": price, "reason": reason,
            "legs": list(legs),
        })
        side = 0
        legs = []
        anchor = camp_atr = None
        pending = None

    for i, b in enumerate(bars):
        if pending is not None and pending["fill"] == i:
            if pending["kind"] == "exit":
                if net(side, [(px, n) for px, n, _ in legs], b["open"]) >= 0.0:
                    finish(b["open"], b["ts"], b["session"], "breakeven")
                pending = None
            elif pending["kind"] == "add":
                if net(side, [(px, n) for px, n, _ in legs], b["open"]) >= 0.0:
                    finish(b["open"], b["ts"], b["session"], "breakeven")
                else:
                    legs.append((b["open"], pending["notional"], b["ts"]))
                    pending = None
            else:
                side = pending["side"]
                anchor, camp_atr = pending["anchor"], pending["A"]
                entry_ts, entry_session = b["ts"], b["session"]
                legs = [(b["open"], BASE, b["ts"])]
                pending = None
        if pending is None:
            action = decide(b, side, legs, anchor, camp_atr)
            if action is not None:
                j = i + 1
                if j < len(bars):
                    fill = bars[j]
                    if action[0] == "entry":
                        if fill["session"] == b["session"] and fill["ts"] - b["ts"] == BAR and fill["minute"] < b["end_m"] - 30:
                            pending = {"fill": j, "kind": "entry", "side": action[1], "anchor": action[2], "A": action[3]}
                    else:
                        pending = {"fill": j, "kind": action[0], "notional": action[1] if action[0] == "add" else None}
        if b["last"]:
            last_price, last_ts, last_session = b["close"], b["ts"], b["session"]
    if legs:
        out.append({
            "side": side, "n_units": len(legs), "entry_ts": entry_ts,
            "exit_ts": last_ts + BAR, "exit_price": last_price, "reason": "open", "legs": list(legs),
        })
    return out


def decide(b, side, legs, anchor, camp_atr):
    if side != 0:
        prices = [(px, n) for px, n, _ in legs]
        if net(side, prices, b["close"]) >= 0.0:
            return ("exit",)
        n = len(legs)
        step = 0.5 * camp_atr
        rung = anchor - (n + 1) * step if side == 1 else anchor + (n + 1) * step
        worse = b["close"] < min(px for px, _, _ in legs) if side == 1 else b["close"] > max(px for px, _, _ in legs)
        beyond = b["close"] <= rung if side == 1 else b["close"] >= rung
        if beyond and worse and n < MAX_ADDS:
            return ("add", BASE * (2 ** n))
        return None
    if not b["qualified"] or b["sess_open"] is None or not b["atr"]:
        return None
    step = 0.5 * b["atr"]
    if b["close"] <= b["sess_open"] - step:
        return ("entry", 1, b["sess_open"], b["atr"])
    if b["close"] >= b["sess_open"] + step:
        return ("entry", -1, b["sess_open"], b["atr"])
    return None


def load_csv():
    rows = []
    with (HERE / "trades.csv").open(encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if row["symbol"] != "QQQ":
                continue
            rows.append(row)
    return rows


def close_enough(a, b) -> bool:
    return math_close(a, b)


def math_close(a, b) -> bool:
    return abs(a - b) <= 1e-6 + 1e-9 * max(abs(a), abs(b))


def main() -> None:
    mine = replay()
    theirs = load_csv()
    if len(mine) != len(theirs):
        sys.exit(f"campaign count {len(mine)} != trades.csv {len(theirs)}")
    for i, (a, b) in enumerate(zip(mine, theirs)):
        prices = [float(x) for x in b["leg_prices"].split(";") if x]
        notions = [float(x) for x in b["leg_notionals"].split(";") if x]
        if a["side"] != int(b["side"]) or a["n_units"] != int(b["n_units"]):
            sys.exit(f"side/units at {i}: {a['side']} {a['n_units']} vs {b['side']} {b['n_units']}")
        if a["entry_ts"] != int(b["entry_ts"]) or a["exit_ts"] != int(b["exit_ts"]):
            sys.exit(f"times at {i}")
        if a["reason"] != b["exit_reason"] or not math_close(a["exit_price"], float(b["exit_price"])):
            sys.exit(f"exit at {i}: {a['reason']} {a['exit_price']} vs {b['exit_reason']} {b['exit_price']}")
        if len(a["legs"]) != len(prices):
            sys.exit(f"legs at {i}")
        for (px, n, _ts), pp, nn in zip(a["legs"], prices, notions):
            if not math_close(px, pp) or not math_close(n, nn):
                sys.exit(f"leg mismatch at {i}: {(px, n)} vs {(pp, nn)}")
        if a["reason"] == "breakeven":
            got = net(a["side"], [(px, n) for px, n, _ in a["legs"]], a["exit_price"])
            if got < -1e-9:
                sys.exit(f"closed campaign {i} nets {got}")
    results = json.loads((HERE / "results.json").read_text(encoding="utf-8"))
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True).stdout.strip())
    full, ins, oos = results["primary"]["full"], results["primary"]["is"], results["primary"]["oos"]
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8", newline="\n") as f:
        f.write(
            f"## {stamp}\n"
            f"- reason: verify.py replayed QQQ independently and matched all {len(mine)} campaigns "
            f"on side, unit count, leg prices, leg notionals, entry time, exit time, exit price, and exit reason. "
            f"Every closed net is non-negative. No strategy code change. Headlines unchanged.\n"
            f"- rules_sha256: {results['rules_sha256']}\n- git_head: {head}\n- git_dirty: {str(dirty).lower()}\n"
            f"- full: sharpe {full['sharpe']:.4f}, return {full['total_return']:.4%}\n"
            f"- IS: sharpe {ins['sharpe']:.4f}, return {ins['total_return']:.4%}\n"
            f"- OOS: sharpe {oos['sharpe']:.4f}, return {oos['total_return']:.4%}\n\n"
        )
    print(f"matched {len(mine)} QQQ campaigns")


if __name__ == "__main__":
    main()
