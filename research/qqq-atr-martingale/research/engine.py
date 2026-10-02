# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Martingale-to-breakeven engine. Closed campaigns have a non-negative net.

Shared by backtest.py. verify.py does not import it.
"""
from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from datetime import date

BAR_S = 300
BASE = 1.0 / 3.0
MAX_ADDS = 16
CUTOFF_MIN = 30
DUST = 1e-9


class Breach(RuntimeError):
    pass


@dataclass(frozen=True)
class Params:
    spacing: float = 0.5
    multiplier: float = 2.0
    base: float = BASE
    max_adds: int = MAX_ADDS
    cost_bps: float = 1.0
    delay: int = 1

    @property
    def cost(self) -> float:
        return self.cost_bps / 10_000.0


class Bar:
    __slots__ = ("ts", "open", "close", "minute", "session", "is_last",
                 "sess_open", "atr", "end_m", "qualified")

    def __init__(self, ts, open_, close, minute, session, is_last, sess_open, atr, end_m, qualified):
        self.ts = int(ts)
        self.open = float(open_)
        self.close = float(close)
        self.minute = int(minute)
        self.session = session
        self.is_last = bool(is_last)
        self.sess_open = sess_open
        self.atr = atr
        self.end_m = int(end_m)
        self.qualified = bool(qualified)


def gross_at(side: int, legs: list, price: float) -> float:
    return sum(side * (price / px - 1.0) * n for px, n, _ in legs)


def net_at(side: int, legs: list, price: float, cost: float) -> float:
    notion = sum(n for _, n, _ in legs)
    return gross_at(side, legs, price) - 2.0 * cost * notion


def gross_delta(side: int, legs: list, price: float) -> float:
    return sum(side * n * (price - mark) / px for px, n, mark in legs)


def set_marks(legs: list, price: float) -> None:
    for i, (px, n, _) in enumerate(legs):
        legs[i] = (px, n, price)


def open_equity(realized: float, side: int, legs: list, price: float, cost: float) -> float:
    if not legs:
        return 1.0 + realized
    return 1.0 + realized + gross_at(side, legs, price) - cost * sum(n for _, n, _ in legs)


def _beyond_next(side: int, legs: list, anchor: float, atr: float, spacing: float, close: float) -> bool:
    n = len(legs)
    step = spacing * atr
    rung = anchor - (n + 1) * step if side == 1 else anchor + (n + 1) * step
    worse = close < min(px for px, _, _ in legs) if side == 1 else close > max(px for px, _, _ in legs)
    beyond = close <= rung if side == 1 else close >= rung
    return beyond and worse


def _decide(b: Bar, side: int, legs, anchor, atr, p: Params, signals: bool):
    if side != 0:
        if net_at(side, legs, b.close, p.cost) >= 0.0:
            return ("exit",)
        if anchor is None:
            return None
        if _beyond_next(side, legs, anchor, atr, p.spacing, b.close):
            if len(legs) >= p.max_adds:
                return ("cap",)
            return ("add", p.base * (p.multiplier ** len(legs)))
        return None
    if not signals or not b.qualified or b.sess_open is None or b.atr is None or b.atr <= 0:
        return None
    step = p.spacing * b.atr
    if b.close <= b.sess_open - step:
        return ("entry", 1, b.sess_open, b.atr)
    if b.close >= b.sess_open + step:
        return ("entry", -1, b.sess_open, b.atr)
    return None


def _pending(action, i: int, b: Bar, bars: list, p: Params):
    j = i + p.delay
    if j >= len(bars):
        return None
    fill = bars[j]
    kind = action[0]
    if kind == "entry":
        if fill.session != b.session or fill.ts - b.ts != p.delay * BAR_S:
            return None
        if fill.minute >= b.end_m - CUTOFF_MIN:
            return None
        return {"fill": j, "kind": "entry", "side": action[1], "anchor": action[2], "A": action[3]}
    if kind == "cap":
        return None
    return {"fill": j, "kind": kind, "notional": action[1] if kind == "add" else None}


def _row(side, meta, entry_ts, entry_session, entry_i, exit_ts, exit_session, exit_price,
         reason, net, exit_i, max_adds) -> dict:
    prices = [px for _, px, _ in meta]
    notions = [n for _, _, n in meta]
    avg = sum(px * n for px, n in zip(prices, notions)) / sum(notions)
    return {
        "side": side, "n_units": len(meta), "entry_ts": entry_ts, "entry_session": entry_session,
        "exit_ts": exit_ts, "exit_session": exit_session, "exit_price": exit_price, "avg_entry": avg,
        "reason": reason, "net": None if reason == "open" else net, "mtm_net": net,
        "legs": list(meta), "entry_i": entry_i, "exit_i": exit_i, "capped": len(meta) >= max_adds,
    }


def drive(bars: list, calendar: list, p: Params, forced: dict | None = None, rng=None) -> dict:
    by_session: dict = defaultdict(list)
    for i, b in enumerate(bars):
        by_session[b.session].append(i)

    side = 0
    legs: list = []
    meta: list = []
    anchor = atr = None
    entry_ts = entry_session = None
    entry_i = -1
    pending = None
    realized = 0.0
    cid = -1
    campaigns: list = []
    refused = False
    pieces: list = []
    gap_sum = 0.0
    last_price = None
    last_ts = None
    last_session = None
    chunks: list = []

    def book(price: float) -> None:
        if not legs:
            return
        delta = gross_delta(side, legs, price)
        if delta != 0.0:
            chunks.append((cid, delta))
        set_marks(legs, price)

    def finish(price: float, ts: int, sess, reason: str, bar_i: int) -> None:
        nonlocal side, legs, meta, anchor, atr, realized, pending
        net = net_at(side, legs, price, p.cost)
        if net < -DUST:
            raise Breach(f"realized net {net} on {sess} ({reason})")
        if net < 0.0:
            net = 0.0
        book(price)
        realized += net
        campaigns.append(_row(
            side, meta, entry_ts, entry_session, entry_i, ts, sess, price, reason, net, bar_i, p.max_adds,
        ))
        side = 0
        legs, meta = [], []
        anchor = atr = None
        pending = None

    def start(price: float, ts: int, sess, bar_i: int, s: int, S: float, A: float) -> None:
        nonlocal side, anchor, atr, entry_ts, entry_session, entry_i, cid
        cid += 1
        side = s
        anchor, atr = S, A
        entry_ts, entry_session, entry_i = ts, sess, bar_i
        legs.append((price, p.base, price))
        meta.append((ts, price, p.base))

    equity, rets, active, notion, sides, units = [], [], [], [], [], []
    prev = 1.0
    for sess_i, day in enumerate(calendar):
        idxs = by_session.get(day, [])
        closed_today = False
        chunks = []
        scheduled = None
        if forced is not None and not legs and pending is None and forced.get(day) and idxs and rng is not None:
            legal = [i for i in idxs if bars[i].minute < bars[i].end_m - CUTOFF_MIN
                     and bars[i].sess_open is not None and bars[i].atr]
            if legal:
                scheduled = legal[int(rng.integers(0, len(legal)))]
        for i in idxs:
            b = bars[i]
            if legs and i == idxs[0]:
                gap_sum += gross_delta(side, legs, b.open)
            if scheduled is not None and i == scheduled and not legs and pending is None:
                start(b.open, b.ts, day, i, forced[day][0], b.sess_open, b.atr)
                scheduled = None
            if pending is not None and pending["fill"] == i:
                kind = pending["kind"]
                if kind == "exit":
                    if net_at(side, legs, b.open, p.cost) >= 0.0:
                        finish(b.open, b.ts, day, "breakeven", i)
                        closed_today = True
                    pending = None
                elif kind == "add":
                    if net_at(side, legs, b.open, p.cost) >= 0.0:
                        finish(b.open, b.ts, day, "breakeven", i)
                        closed_today = True
                    else:
                        legs.append((b.open, pending["notional"], b.open))
                        meta.append((b.ts, b.open, pending["notional"]))
                        pending = None
                else:
                    start(b.open, b.ts, day, i, pending["side"], pending["anchor"], pending["A"])
                    pending = None
            if pending is None:
                action = _decide(b, side, legs, anchor, atr, p, forced is None)
                if action is None:
                    pass
                elif action[0] == "cap":
                    refused = True
                elif p.delay == 0:
                    if action[0] == "exit":
                        finish(b.close, b.ts + BAR_S, day, "breakeven", i)
                        closed_today = True
                    elif action[0] == "add":
                        legs.append((b.close, action[1], b.close))
                        meta.append((b.ts + BAR_S, b.close, action[1]))
                    elif b.minute < b.end_m - CUTOFF_MIN:
                        start(b.close, b.ts + BAR_S, day, i, action[1], action[2], action[3])
                else:
                    made = _pending(action, i, b, bars, p)
                    if made is not None:
                        pending = made
            if b.is_last and legs:
                book(b.close)
                last_price, last_ts, last_session = b.close, b.ts, day
            elif b.is_last:
                last_price, last_ts, last_session = b.close, b.ts, day
        eq = open_equity(realized, side, legs, last_price, p.cost) if legs else 1.0 + realized
        rets.append(eq - prev)
        prev = eq
        equity.append(eq)
        for ident, delta in chunks:
            pieces.append((ident, sess_i, delta))
        held = bool(legs)
        active.append(held or closed_today)
        notion.append(sum(n for _, n, _ in legs) if held else 0.0)
        sides.append(side if held else 0)
        units.append(len(legs) if held else 0)

    if legs:
        if last_price is None:
            raise Breach("open campaign with no mark")
        mtm = net_at(side, legs, last_price, p.cost)
        campaigns.append(_row(
            side, meta, entry_ts, entry_session, entry_i, last_ts + BAR_S, last_session,
            last_price, "open", mtm, len(bars) - 1, p.max_adds,
        ))
    return {
        "equity": equity, "r": rets, "active": active, "notional": notion, "side": sides,
        "units": units, "campaigns": campaigns, "pieces": pieces, "gap_sum": float(gap_sum),
        "refused_add": refused, "realized": float(realized), "calendar": list(calendar),
    }
