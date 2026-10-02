# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Hand-built paths for the martingale engine. No store."""
from __future__ import annotations

from datetime import date

from engine import BAR_S, Bar, Params, drive, net_at

D1 = date(2024, 1, 2)
D2 = date(2024, 1, 3)


def bars(day, rows, atr=10.0, end_m=16 * 60, qualified=True, origin=570):
    built = []
    for off, o, c in rows:
        minute = origin + off
        ts = day.toordinal() * 1_000_000 + minute * 60
        built.append((ts, o, c, minute))
    sess_open = built[0][1] if built and built[0][3] == 9 * 60 + 30 else None
    out = []
    for i, (ts, o, c, minute) in enumerate(built):
        out.append(Bar(ts, o, c, minute, day, i == len(built) - 1, sess_open, atr, end_m, qualified))
    return out


def run(seq, p=None, calendar=None):
    p = p or Params()
    flat = [b for day in seq for b in day]
    cal = calendar or [day[0].session for day in seq]
    return drive(flat, cal, p)


def test_scratch_clears_costs():
    # Fill at 100. A close back at 100 does not cover 2 bp. 100.03 does.
    day = bars(D1, [(0, 100, 94), (5, 100, 100), (10, 100, 100.03), (15, 100.03, 100.03)])
    out = run([day])
    closed = [c for c in out["campaigns"] if c["reason"] == "breakeven"]
    assert len(closed) == 1, out["campaigns"]
    c = closed[0]
    assert c["side"] == 1 and c["n_units"] == 1
    assert abs(c["legs"][0][1] - 100) < 1e-12
    assert abs(c["exit_price"] - 100.03) < 1e-12
    assert c["net"] >= 0
    hand = net_at(1, [(100, 1 / 3, 100)], 100.03, 0.0001)
    assert abs(c["net"] - hand) < 1e-12


def test_back_to_entry_does_not_exit():
    day = bars(D1, [(0, 100, 94), (5, 100, 100), (10, 100, 100)])
    out = run([day])
    assert [c["reason"] for c in out["campaigns"]] == ["open"]
    assert out["realized"] == 0.0
    assert out["equity"][-1] < 1.0


def test_double_then_scratch():
    day = bars(D1, [
        (0, 100, 94), (5, 96, 89), (10, 89, 89), (15, 120, 120), (20, 120, 120),
    ])
    out = run([day])
    closed = [c for c in out["campaigns"] if c["reason"] == "breakeven"]
    assert len(closed) == 1
    c = closed[0]
    assert c["n_units"] == 2
    assert abs(c["legs"][1][2] - 2 / 3) < 1e-12
    assert c["net"] >= 0


def test_exit_cancelled_when_open_gaps():
    # A = 100 so the next rung is far and a gap to 90 does not add.
    day = bars(D1, [(0, 100, 40), (5, 100, 110), (10, 90, 90), (15, 90, 90)], atr=100.0)
    out = run([day])
    assert [c["reason"] for c in out["campaigns"]] == ["open"]
    assert out["campaigns"][0]["n_units"] == 1
    assert out["realized"] == 0.0


def test_add_becomes_exit_when_open_already_scratches():
    day = bars(D1, [(0, 100, 40), (5, 100, -1), (10, 110, 110), (15, 110, 110)], atr=100.0)
    out = run([day])
    closed = [c for c in out["campaigns"] if c["reason"] == "breakeven"]
    assert len(closed) == 1 and closed[0]["n_units"] == 1
    assert closed[0]["net"] >= 0
    assert abs(closed[0]["exit_price"] - 110) < 1e-12


def test_never_recovers():
    day = bars(D1, [(0, 100, 94), (5, 94, 94), (10, 94, 94)])
    out = run([day])
    assert out["campaigns"][0]["reason"] == "open"
    assert out["realized"] == 0.0
    assert out["equity"][-1] < 1.0


def test_cap_sixteen_and_a_small_cap():
    rows = []
    price = 100.0
    for i in range(17):
        nxt = price - 1.0
        rows.append((5 * i, price, nxt))
        price = nxt
    day = bars(D1, rows, atr=1.0)
    out = run([day], Params(max_adds=16))
    assert out["campaigns"][0]["reason"] == "open"
    assert out["campaigns"][0]["n_units"] == 16
    assert out["refused_add"] is True
    assert out["realized"] == 0.0
    notions = [n for _, _, n in out["campaigns"][0]["legs"]]
    assert abs(notions[0] - 1 / 3) < 1e-9
    assert abs(notions[15] - (1 / 3) * (2 ** 15)) < 1e-6
    small = run([day], Params(max_adds=3))
    assert small["campaigns"][0]["n_units"] == 3 and small["realized"] == 0.0


def test_cutoff_and_hole():
    blocked = bars(D1, [(0, 100, 94), (5, 94, 94)], end_m=600)
    assert run([blocked])["campaigns"] == []
    hole = bars(D1, [(0, 100, 94), (10, 90, 90)])
    assert run([hole])["campaigns"] == []


def test_add_fills_next_session():
    a = bars(D1, [(0, 100, 94), (5, 96, 89)])
    b = bars(D2, [(0, 88, 88), (5, 88, 88)])
    out = run([a, b])
    c = out["campaigns"][0]
    assert c["n_units"] == 2
    assert c["legs"][1][0] == b[0].ts
    assert abs(c["legs"][1][1] - 88) < 1e-12
    assert c["reason"] == "open"


def test_short_scratch():
    day = bars(D1, [(0, 100, 106), (5, 100, 99.9), (10, 99.9, 99.9)])
    out = run([day])
    closed = [c for c in out["campaigns"] if c["reason"] == "breakeven"]
    assert len(closed) == 1 and closed[0]["side"] == -1
    assert closed[0]["net"] >= 0


def test_delay_zero():
    day = bars(D1, [(0, 100, 94), (5, 94.05, 94.05)])
    out = run([day], Params(delay=0))
    c = [x for x in out["campaigns"] if x["reason"] == "breakeven"][0]
    assert abs(c["legs"][0][1] - 94) < 1e-12
    assert c["entry_ts"] == day[0].ts + BAR_S
    assert c["net"] >= 0


def main():
    test_scratch_clears_costs()
    test_back_to_entry_does_not_exit()
    test_double_then_scratch()
    test_exit_cancelled_when_open_gaps()
    test_add_becomes_exit_when_open_already_scratches()
    test_never_recovers()
    test_cap_sixteen_and_a_small_cap()
    test_cutoff_and_hole()
    test_add_fills_next_session()
    test_short_scratch()
    test_delay_zero()
    print("self-test passed")


if __name__ == "__main__":
    main()
