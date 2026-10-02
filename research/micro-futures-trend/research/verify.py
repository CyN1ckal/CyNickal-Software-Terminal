# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent, deliberately naive re-implementation of the primary rule in RULES.md.
Shares no code with backtest.py. Recomputes every signal from raw prices by direct
products, the EWMA from scratch at each rebalance, and walks the book day by day.
Checks every trade in trades.csv (market, side, entry date/price, exit date/price,
reason) and every session's P&L in daily.csv to within $0.01.

    python research/micro-futures-trend/research/verify.py
"""
import calendar
import csv
import json
import math
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent

MARKETS = {
    # symbol: (class, $ per point or None, notional or None, cost per side)
    "ES=F": ("equity", 5.0, None, 2.25), "NQ=F": ("equity", 2.0, None, 1.50),
    "RTY=F": ("equity", 5.0, None, 1.50), "YM=F": ("equity", 0.5, None, 1.50),
    "ZT=F": ("rates", None, 100000 / 1.9, 2.0), "ZF=F": ("rates", None, 100000 / 4.2, 2.0),
    "ZN=F": ("rates", None, 100000 / 6.3, 2.0), "ZB=F": ("rates", None, 100000 / 11.5, 2.0),
    "GC=F": ("commodities", 10.0, None, 2.0), "HG=F": ("commodities", 2500.0, None, 2.25),
    "CL=F": ("commodities", 100.0, None, 2.0),
}
W = {"equity": 1 / 12, "rates": 1 / 12, "commodities": 1 / 9}


def bars_of(sym):
    doc = json.loads((HERE / "data" / "raw" / (sym.replace("=", "_") + ".json")).read_text())
    out = []
    for k in sorted(doc["body"], key=lambda s: int(s)):
        r = doc["body"][k]
        vals = [r.get("open"), r.get("high"), r.get("low"), r.get("close"), r.get("volume")]
        if all(v is not None and v > 0 for v in vals):
            out.append((r["date"], float(r["open"]), float(r["close"])))
    return out


def main():
    bars = {m: bars_of(m) for m in MARKETS}
    days = sorted(set(d for m in bars for d, _, _ in bars[m]))
    # month ends: a session whose next session is in another month
    month_end = [days[i] for i in range(len(days) - 1) if days[i][:7] != days[i + 1][:7]]
    first = next(d for d in month_end if all(len([b for b in bars[m] if b[0] <= d]) >= 253 for m in MARKETS))

    # roll-cost sessions
    def on_or_after(d):
        for s in days:
            if s >= d:
                return s
        return None
    last_of_month = {d[:7]: d for d in month_end}
    rolls = {m: set() for m in MARKETS}
    for ym in sorted(set(d[:7] for d in days)):
        y, mo = int(ym[:4]), int(ym[5:])
        cal = calendar.monthcalendar(y, mo)
        fridays = [wk[calendar.FRIDAY] for wk in cal if wk[calendar.FRIDAY] != 0]
        third_fri = f"{ym}-{fridays[2]:02d}"
        for m, (cls, _, _, _) in MARKETS.items():
            s = None
            if cls == "equity" and mo in (3, 6, 9, 12):
                s = on_or_after(third_fri)
            elif m == "CL=F":
                s = on_or_after(f"{ym}-20")
            elif (m == "GC=F" and mo in (1, 3, 5, 7, 11)) or (m == "HG=F" and mo in (2, 4, 6, 8, 11)) \
                    or cls == "rates":
                s = last_of_month.get(ym)
            if s:
                rolls[m].add(s)

    by_date = {m: {b[0]: i for i, b in enumerate(bars[m])} for m in MARKETS}
    pos = {m: 0 for m in MARKETS}
    queued = {}
    trades, openT = [], {}
    E = 100000.0
    pnl_by_day = {}
    lastbar = {m: None for m in MARKETS}

    for d in days:
        today = 0.0
        for m, (cls, mult, notional, c) in MARKETS.items():
            i = by_date[m].get(d)
            if i is not None:
                dt, o, cl = bars[m][i]
                before = pos[m]
                if m in queued and queued[m][0] == i:
                    after = queued.pop(m)[1]
                    fee = abs(after - before) * c
                    today -= fee
                    if before != 0 and (after == 0 or (after > 0) != (before > 0)):
                        t = openT.pop(m)
                        t.update(exit_date=d, exit_price=o, reason="flat" if after == 0 else "flip")
                        trades.append(t)
                    if after != 0 and (before == 0 or (after > 0) != (before > 0)):
                        openT[m] = dict(market=m, side="long" if after > 0 else "short", entry_date=d, entry_price=o)
                    pos[m] = after
                n = pos[m]
                if mult is not None:
                    today += n * mult * (cl - o)
                else:
                    today += n * notional * (cl / o - 1)
                lastbar[m] = i
            if d in rolls[m] and pos[m] != 0:
                today -= 2 * abs(pos[m]) * c
        E += today
        pnl_by_day[d] = today

        if d >= first and (d == first or d in month_end):
            for m, (cls, mult, notional, c) in MARKETS.items():
                i = lastbar[m]
                series = bars[m][: i + 1]
                rets = [cl / o - 1 for _, o, cl in series]
                votes = 0
                for k in (21, 63, 252):
                    growth = 1.0
                    for r in rets[len(rets) - k:]:
                        growth *= 1 + r
                    votes += 1 if growth > 1 else (-1 if growth < 1 else 0)
                sig = votes / 3
                lam = 60 / 61
                var = sum(r * r for r in rets[:60]) / 60
                for r in rets[60:]:
                    var = lam * var + (1 - lam) * r * r
                vol = math.sqrt(var * 252)
                value = notional if notional is not None else mult * series[-1][2]
                raw = sig * E * 0.20 * W[cls] * 2.0 / (value * vol)
                n = math.floor(abs(raw) + 0.5) * (1 if raw >= 0 else -1)
                if i + 1 < len(bars[m]) and (n != pos[m] or m in queued):
                    queued[m] = (i + 1, n)

    for m, t in openT.items():
        dt, o, cl = bars[m][lastbar[m]]
        t.update(exit_date=dt, exit_price=cl, reason="end")
        trades.append(t)

    # compare
    with (HERE / "trades.csv").open() as f:
        ref = list(csv.DictReader(f))
    key = lambda t: (t["entry_date"], t["market"])
    mine = sorted(trades, key=key)
    ref = sorted(ref, key=key)
    bad = 0
    if len(mine) != len(ref):
        print(f"trade count differs: verify {len(mine)} vs backtest {len(ref)}")
        bad += 1
    for a, b in zip(mine, ref):
        same = (a["market"] == b["market"] and a["side"] == b["side"] and a["entry_date"] == b["entry_date"]
                and abs(a["entry_price"] - float(b["entry_price"])) < 1e-9 and a["exit_date"] == b["exit_date"]
                and abs(a["exit_price"] - float(b["exit_price"])) < 1e-9 and a["reason"] == b["exit_reason"])
        if not same:
            bad += 1
            if bad <= 10:
                print("trade mismatch:", a, {k: b[k] for k in ("market", "side", "entry_date", "entry_price",
                                                               "exit_date", "exit_price", "exit_reason")})
    with (HERE / "daily.csv").open() as f:
        rows = list(csv.DictReader(f))
    prev = 100000.0
    worst = 0.0
    for r in rows:
        eq = float(r["equity"])
        worst = max(worst, abs((eq - prev) - pnl_by_day[r["date"]]))
        prev = eq
    print(f"trades checked: {len(ref)}; mismatches: {bad}; sessions checked: {len(rows)}; "
          f"worst daily P&L difference: ${worst:.6f}; final equity verify {E:.2f} vs backtest {prev:.2f}")
    if bad or worst > 0.01:
        raise SystemExit("VERIFY FAILED")
    print("verify: PASS")


if __name__ == "__main__":
    main()
