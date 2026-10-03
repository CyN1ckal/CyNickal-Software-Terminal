# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the locked GLD/SLV rule.

Does not import backtest.py. Two passes: z-scores from closes, then fills.
A store run appends one RUNLOG entry.
"""

from __future__ import annotations

import csv
import hashlib
import math
import subprocess
import sys
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "agent-data"))

WINDOW = 60
ENTRY = 2.0
RATE = 1.0 / 10000.0
OOS = date(2024, 7, 1)
IS_END = date(2024, 6, 28)
END = date(2026, 10, 1)
SKIP = {date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)}
WEIGHT = {"flat": (0.0, 0.0), "short_ratio": (-0.5, 0.5), "long_ratio": (0.5, -0.5)}


def rules_sha256() -> str:
    payload = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(payload).hexdigest()


def assert_lock() -> str:
    have = rules_sha256()
    text = (HERE / "RULES.lock").read_text(encoding="utf-8")
    want = next(line.split(" ", 1)[1].strip() for line in text.splitlines() if line.startswith("sha256 "))
    if have != want:
        print(f"RULES.md hash {have} does not match RULES.lock {want}. Refusing to run.", file=sys.stderr)
        sys.exit(1)
    return have


def population_z(points: list[float]) -> float:
    count = len(points)
    mean = sum(points) / count
    square = 0.0
    for value in points:
        square += (value - mean) ** 2
    scale = math.sqrt(square / count)
    if scale == 0.0:
        return 0.0
    return (points[-1] - mean) / scale


def next_side(held: str, score: float, entry: float) -> str:
    if held == "flat":
        if score > entry:
            return "short_ratio"
        if score < -entry:
            return "long_ratio"
        return "flat"
    if held == "short_ratio":
        if score < -entry:
            return "long_ratio"
        if score <= 0.0:
            return "flat"
        return "short_ratio"
    if score > entry:
        return "short_ratio"
    if score >= 0.0:
        return "flat"
    return "long_ratio"


def replay(sessions, opens_a, closes_a, opens_b, closes_b):
    """opens_* maps are None when that leg has no bar."""
    cash = 1.0
    shares = [0.0, 0.0]
    last = [None, None]
    held = "flat"
    lot = None
    order = None
    ratios: list[float] = []
    pair_index = -1
    closed = []
    daily = []
    equity = 1.0
    ident = 1

    def mark(prices):
        return cash + shares[0] * prices[0] + shares[1] * prices[1]

    def trade_to(target, prices, equity_now, why, when):
        nonlocal cash, held, lot, ident
        if target != "flat" and not equity_now > 0.0:
            target = "flat"
            why = "flat"
        fresh = [0.0, 0.0]
        if target != "flat":
            w = WEIGHT[target]
            fresh = [w[0] * equity_now / prices[0], w[1] * equity_now / prices[1]]
        close_cost = 0.0
        open_cost = 0.0
        for i in range(2):
            delta = fresh[i] - shares[i]
            cost = abs(delta) * prices[i] * RATE
            shut = abs(shares[i]) * prices[i]
            opened = abs(fresh[i]) * prices[i]
            base = shut + opened
            if base > 0.0:
                close_cost += cost * (shut / base)
                open_cost += cost * (opened / base)
            cash -= delta * prices[i] + cost
        previous = held
        if lot is not None and target != previous:
            lot["cost"] += close_cost
            lot["exit_date"] = when
            lot["exit_prices"] = (prices[0], prices[1])
            lot["exit_reason"] = why
            lot["net"] = lot["gross"] - lot["cost"]
            closed.append(lot)
            lot = None
        shares[0], shares[1] = fresh
        held = target
        if target != "flat":
            lot = {
                "id": ident,
                "side": target,
                "entry_date": when,
                "entry_prices": (prices[0], prices[1]),
                "shares": (fresh[0], fresh[1]),
                "gross": 0.0,
                "a_gross": 0.0,
                "b_gross": 0.0,
                "cost": open_cost,
                "exit_date": None,
                "exit_prices": None,
                "exit_reason": None,
                "net": None,
            }
            ident += 1

    def book(lot_now, a_pnl, b_pnl):
        if lot_now is None:
            return
        lot_now["gross"] += a_pnl + b_pnl
        lot_now["a_gross"] += a_pnl
        lot_now["b_gross"] += b_pnl

    last_day = sessions[-1]
    for day in sessions:
        oa, ca = opens_a.get(day), closes_a.get(day)
        ob, cb = opens_b.get(day), closes_b.get(day)
        if oa is None or ob is None:
            daily.append((day, 0.0, 0.0))
            continue
        previous = equity
        gap_a = shares[0] * (oa - last[0]) if last[0] is not None else 0.0
        gap_b = shares[1] * (ob - last[1]) if last[1] is not None else 0.0
        book(lot, gap_a, gap_b)
        equity_open = mark([oa, ob])
        if order is not None and pair_index + 1 - order["at"] == 1:
            target = order["side"]
            why = "flat" if held == "flat" or target == "flat" else "flip"
            trade_to(target, [oa, ob], equity_open, why, day)
            order = None
        oc_a = shares[0] * (ca - oa)
        oc_b = shares[1] * (cb - ob)
        book(lot, oc_a, oc_b)
        equity_close = mark([ca, cb])
        pair_index += 1
        ratios.append(ca / cb)
        score = population_z(ratios[-WINDOW:]) if len(ratios) >= WINDOW else None
        if day == last_day and held != "flat":
            trade_to("flat", [ca, cb], equity_close, "end", day)
        if day == last_day or score is None:
            order = None
        else:
            want = next_side(held, score, ENTRY)
            if want == held:
                order = None
            elif order is None or order["side"] != want:
                order = {"side": want, "at": pair_index}
        last = [ca, cb]
        equity = cash if held == "flat" else mark([ca, cb])
        gross = gap_a + gap_b + oc_a + oc_b
        daily.append((day, (equity - previous) / previous, gross / previous))
    return closed, daily


def load_pair(md, symbol):
    rows = md.bars(symbol, "1d", start="2011-01-04", end="2026-10-01")
    opens = {b.session: b.open for b in rows}
    closes = {b.session: b.close for b in rows}
    return opens, closes


def read_trades(path):
    out = []
    with path.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            out.append(
                {
                    "side": row["side"],
                    "entry_date": date.fromisoformat(row["entry_date"]),
                    "exit_date": date.fromisoformat(row["exit_date"]),
                    "entry_prices": (float(row["a_entry"]), float(row["b_entry"])),
                    "exit_prices": (float(row["a_exit"]), float(row["b_exit"])),
                    "gross": float(row["gross_pnl"]),
                    "net": float(row["net_pnl"]),
                    "exit_reason": row["exit_reason"],
                }
            )
    return out


def read_daily(path):
    out = []
    with path.open(encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            out.append((date.fromisoformat(row["date"]), float(row["strategy_net"]), float(row["strategy_gross"])))
    return out


def compare(got, expected, label):
    errors = []
    if len(got) != len(expected):
        errors.append(f"{label} count {len(got)} != {len(expected)}")
    for i, (g, e) in enumerate(zip(got, expected)):
        for field in ("side", "entry_date", "exit_date", "exit_reason"):
            if g[field] != e[field]:
                errors.append(f"{label} {i} {field} {g[field]} != {e[field]}")
        for field in ("gross", "net"):
            if abs(g[field] - e[field]) > 1e-6:
                errors.append(f"{label} {i} {field} {g[field]} != {e[field]}")
        for which in ("entry_prices", "exit_prices"):
            for leg in (0, 1):
                if abs(g[which][leg] - e[which][leg]) > 1e-6:
                    errors.append(f"{label} {i} {which}[{leg}] {g[which][leg]} != {e[which][leg]}")
        if errors:
            break
    return errors


def sharpe(values):
    arr = np.asarray(values, dtype=float)
    if arr.size < 2:
        return None
    sd = float(arr.std(ddof=1))
    if sd == 0.0:
        return None
    return float(arr.mean() / sd * math.sqrt(252))


def total_return(values):
    arr = np.asarray(values, dtype=float)
    if arr.size == 0:
        return 0.0
    return float(np.prod(1.0 + arr) - 1.0)


def append_log(digest, full, is_m, oos, n_trades, n_oos, matched):
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT).stdout.strip())
    path = HERE / "RUNLOG.md"
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")

    def f3(value):
        return "null" if value is None else f"{value:.3f}"

    text = (
        f"\n## {stamp}\n\n"
        f"- Reason: verification, independent rebuild of every trade ({'match' if matched else 'MISMATCH'})\n"
        f"- Rules sha256: `{digest}`\n"
        f"- Git HEAD: `{head}` (dirty: {dirty})\n"
        f"- Sharpe full / IS / OOS: {f3(full[0])} / {f3(is_m[0])} / {f3(oos[0])}\n"
        f"- Total return full / IS / OOS: {f3(full[1])} / {f3(is_m[1])} / {f3(oos[1])}\n"
        f"- Trades full / OOS: {n_trades} / {n_oos}\n"
        f"- Line 6 OOS flat-or-flip: {n_oos}\n"
        f"- Status: {'verified' if matched else 'mismatch'}\n"
    )
    with path.open("a", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def main():
    digest = assert_lock()
    from mdq import MarketData, nyse_sessions

    with MarketData() as md:
        book = [d for d in nyse_sessions("2011-01-04", "2026-10-01") if d not in SKIP]
        gld_o, gld_c = load_pair(md, "GLD")
        slv_o, slv_c = load_pair(md, "SLV")
        pplt_o, pplt_c = load_pair(md, "PPLT")
    primary, daily = replay(book, gld_o, gld_c, slv_o, slv_c)
    cross, _ = replay(book, gld_o, gld_c, pplt_o, pplt_c)
    expected = read_trades(HERE / "trades.csv")
    expected_cross = read_trades(HERE / "cross_trades.csv")
    errors = compare(primary, expected, "primary")
    errors += compare(cross, expected_cross, "cross")
    stored = read_daily(HERE / "daily.csv")
    if primary:
        first = primary[0]["entry_date"]
        got_daily = [(d, n, g) for d, n, g in daily if first <= d <= END]
    else:
        got_daily = []
    if len(got_daily) != len(stored):
        errors.append(f"daily rows {len(got_daily)} != {len(stored)}")
    else:
        for i, (g, e) in enumerate(zip(got_daily, stored)):
            if g[0] != e[0] or abs(g[1] - e[1]) > 1e-8 or abs(g[2] - e[2]) > 1e-8:
                errors.append(f"daily {i} {g} != {e}")
                break
    rng = np.random.default_rng(20261084)
    if len(primary) > 40:
        sample = sorted(int(i) for i in rng.choice(len(primary), size=40, replace=False))
    else:
        sample = list(range(len(primary)))
    eval_net = [row[1] for row in got_daily]
    is_net = [n for (d, n, _) in got_daily if d <= IS_END]
    oos_net = [n for (d, n, _) in got_daily if d >= OOS]
    full = (sharpe(eval_net), total_return(eval_net))
    is_m = (sharpe(is_net), total_return(is_net))
    oos = (sharpe(oos_net), total_return(oos_net))
    n_oos = sum(1 for t in primary if t["entry_date"] >= OOS)
    append_log(digest, full, is_m, oos, len(primary), n_oos, not errors)
    if errors:
        print("\n".join(errors[:12]))
        sys.exit(1)
    print(f"match primary {len(primary)} cross {len(cross)} daily {len(got_daily)} sample {len(sample)}")


if __name__ == "__main__":
    main()
