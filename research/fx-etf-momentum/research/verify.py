# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Independent replay of the locked FX ETF rule.

Does not import backtest.py. Recomputes signals, fills, and round trips from
the store and compares them to trades.csv. Also checks daily strategy_net
against daily.csv. Appends one RUNLOG entry.
"""

import ast
import csv
import datetime as dt
import hashlib
import subprocess
import sys
from datetime import date, timedelta
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions, session_date_of  # noqa: E402

NAMES = ("FXE", "FXB", "FXA", "FXC", "FXF", "FXY")
LOOKBACK = 63
DENOM = 6
COST = 0.0005
CLOSED = frozenset({date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5)})
START = date(2011, 1, 4)
END = date(2026, 10, 1)
OOS = date(2024, 7, 1)
SEED = 20261034


def rules_ok():
    raw = (HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
    have = hashlib.sha256(raw).hexdigest()
    want = (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines()[0].split()[1]
    if have != want:
        sys.exit(f"RULES.md hash {have} != lock {want}; refusing to verify")
    return have


def sign_of(r):
    if r > 0.0:
        return 1
    if r < 0.0:
        return -1
    return 0


def month_ends(book):
    have = set(book)
    y, m = book[0].year, book[0].month
    stop = (book[-1].year, book[-1].month)
    out = []
    while (y, m) <= stop:
        nxt = date(y + 1, 1, 1) if m == 12 else date(y, m + 1, 1)
        sess = [d for d in nyse_sessions(date(y, m, 1), nxt - timedelta(days=1)) if d not in CLOSED]
        if sess and sess[-1] in have:
            out.append(sess[-1])
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return out


def replay():
    with MarketData() as md:
        spy = md.bars("SPY", "1d", start=START, end=END)
        book = [session_date_of(b.ts) for b in spy]
        px = {}
        for sym in NAMES:
            bars = md.bars(sym, "1d", start=START, end=END)
            px[sym] = {session_date_of(b.ts): (b.open, b.close) for b in bars}
    own = {}
    for sym in NAMES:
        closes = []
        idx = {}
        for d in book:
            bar = px[sym].get(d)
            if bar is not None:
                idx[d] = len(closes)
                closes.append(bar[1])
        own[sym] = (closes, idx)
    loc = {d: i for i, d in enumerate(book)}
    ends = [d for d in month_ends(book) if loc[d] >= LOOKBACK]
    schedule = {}
    for d in ends:
        w = {}
        for sym in NAMES:
            i = own[sym][1].get(d)
            if i is None or i < LOOKBACK:
                s = 0
            else:
                s = sign_of(own[sym][0][i] / own[sym][0][i - LOOKBACK] - 1.0)
            w[sym] = s / DENOM
        nxt = loc[d] + 1
        if nxt < len(book):
            schedule[book[nxt]] = w
    equity = 1.0
    shares = {s: 0.0 for s in NAMES}
    last = {s: None for s in NAMES}
    live = {s: None for s in NAMES}
    done = []
    daily = []
    first_fill = book[loc[ends[0]] + 1]

    def open_trip(sym, side, day, price, sh, gross, cost):
        return {"symbol": sym, "side": side, "entry_date": day, "entry_price": price,
                "entry_shares": sh, "gross": gross, "cost": cost, "exit_date": None,
                "exit_price": None, "reason": None}

    for d in book:
        w = schedule.get(d)
        prev = equity
        gap = {s: 0.0 for s in NAMES}
        for s in NAMES:
            bar = px[s].get(d)
            if bar is not None and shares[s] != 0.0 and last[s] is not None:
                gap[s] = shares[s] * (bar[0] - last[s])
        equity_open = prev + sum(gap.values())
        new = dict(shares)
        cost = 0.0
        if w is not None:
            for s in NAMES:
                bar = px[s].get(d)
                if bar is None:
                    continue
                tgt = w[s] * equity_open / bar[0]
                cost += COST * abs(tgt - shares[s]) * bar[0]
                new[s] = tgt
        oc = {s: 0.0 for s in NAMES}
        for s in NAMES:
            bar = px[s].get(d)
            if bar is None:
                continue
            oc[s] = new[s] * (bar[1] - bar[0])
            last[s] = bar[1]
        for s in NAMES:
            bar = px[s].get(d)
            if bar is None:
                continue
            old, nxt_sh = shares[s], new[s]
            os_ = 1 if old > 0 else (-1 if old < 0 else 0)
            ns_ = 1 if nxt_sh > 0 else (-1 if nxt_sh < 0 else 0)
            if live[s] is not None:
                live[s]["gross"] += gap[s]
            if os_ != 0 and ns_ != os_:
                live[s]["cost"] += COST * abs(old) * bar[0]
                live[s]["exit_date"] = d
                live[s]["exit_price"] = bar[0]
                live[s]["reason"] = "flip" if ns_ else "flat"
                done.append(live[s])
                live[s] = None
            if ns_ != 0 and ns_ != os_:
                side = "long" if ns_ > 0 else "short"
                live[s] = open_trip(s, side, d, bar[0], nxt_sh, oc[s], COST * abs(nxt_sh) * bar[0])
            elif ns_ != 0 and ns_ == os_:
                if w is not None:
                    live[s]["cost"] += COST * abs(nxt_sh - old) * bar[0]
                live[s]["gross"] += oc[s]
        equity = equity_open - cost + sum(oc.values())
        shares = new
        if d >= first_fill:
            daily.append((d, equity / prev - 1.0))
    for s in NAMES:
        if live[s] is None:
            continue
        live[s]["exit_date"] = book[-1]
        live[s]["exit_price"] = last[s]
        live[s]["reason"] = "end"
        done.append(live[s])
    return done, daily, ends, first_fill


def num(text):
    return ast.literal_eval(text)


def main():
    sha = rules_ok()
    trips, daily, ends, first_fill = replay()
    with open(HERE / "trades.csv", newline="", encoding="utf-8") as f:
        file_trips = list(csv.DictReader(f))
    with open(HERE / "daily.csv", newline="", encoding="utf-8") as f:
        file_daily = list(csv.DictReader(f))
    key = lambda t: (t["symbol"], t["entry_date"].isoformat(), t["exit_date"].isoformat(), t["side"], t["reason"])
    got = sorted(trips, key=key)
    mismatches = []
    if len(got) != len(file_trips):
        mismatches.append(f"trade count {len(got)} != {len(file_trips)}")
    file_sorted = sorted(file_trips, key=lambda t: (t["symbol"], t["entry_date"], t["exit_date"], t["side"], t["exit_reason"]))
    for i, (a, b) in enumerate(zip(got, file_sorted)):
        if a["symbol"] != b["symbol"] or a["side"] != b["side"]:
            mismatches.append(f"id {i} {a['symbol']} {a['side']} vs {b['symbol']} {b['side']}")
            break
        if a["entry_date"].isoformat() != b["entry_date"] or a["exit_date"].isoformat() != b["exit_date"]:
            mismatches.append(f"dates {i} {a['entry_date']} {a['exit_date']} vs {b['entry_date']} {b['exit_date']}")
            break
        if a["entry_price"] != num(b["entry_price"]) or a["exit_price"] != num(b["exit_price"]):
            mismatches.append(f"px {i} {a['entry_price']} {a['exit_price']} vs {b['entry_price']} {b['exit_price']}")
            break
        if a["reason"] != b["exit_reason"]:
            mismatches.append(f"reason {i} {a['reason']} vs {b['exit_reason']}")
            break
    if len(daily) != len(file_daily):
        mismatches.append(f"days {len(daily)} != {len(file_daily)}")
    else:
        for (d, r), row in zip(daily, file_daily):
            if d.isoformat() != row["date"] or abs(r - num(row["strategy_net"])) > 1e-10:
                mismatches.append(f"daily {d} {r} vs {row['date']} {row['strategy_net']}")
                break
    rng = np.random.default_rng(SEED)
    pick = rng.choice(len(ends), size=40, replace=False)
    # Entry fill of a sampled signal date is the next book session. Recover it from
    # the schedule implicit in trips: a trip can open on that fill. Check every file
    # trip whose entry date equals the fill of a sampled signal, against the replay.
    book_fills = {}
    # Reconstruct fill dates from the replay schedule by re-reading ends order.
    # The fill is the session after the signal. Use the daily calendar implied by
    # the union of trip dates plus the first fill; cheaper to recompute from ends
    # via the same book the replay used. The first daily row is the first fill,
    # and file dates are the book from then on. Map signal -> next file/book date
    # using nyse-free book stored on the daily file plus the day before first fill.
    session_list = [first_fill] + [d for d, _ in daily]
    # daily already starts at first_fill, so the book tail is daily dates.
    # Signals can be before first_fill only for the warm-up, which ends excludes.
    # Build fill date as the first daily date strictly after the signal, which is
    # correct once the signal is on or after the session before first_fill.
    all_dates = [date.fromisoformat(row["date"]) for row in file_daily]
    # Insert nothing before first_fill; every eligible signal except those whose
    # fill is first_fill has both the signal and the fill inside or just before.
    # The signal itself may be the session before a daily row. Use trip entry
    # dates that we already matched globally; the sample is a subset.
    sampled_fills = set()
    date_set = set(all_dates)
    # Signal's fill is the next stored session. Sessions in the file start at the
    # first fill, so a signal whose fill is inside the file is the prior session,
    # which we do not have for the first one. Match entries whose entry_date is
    # the calendar fill. Recompute fills from ends using file dates plus first_fill.
    ordered = all_dates
    pos = {d: i for i, d in enumerate(ordered)}
    for j in pick:
        sig = ends[j]
        # next session: if sig is before the window, fill is first_fill when sig
        # is the first eligible; otherwise the first ordered date > sig.
        later = [d for d in ordered if d > sig]
        if not later:
            continue
        sampled_fills.add(later[0])
    sample_file = [t for t in file_sorted if date.fromisoformat(t["entry_date"]) in sampled_fills]
    sample_got = [t for t in got if t["entry_date"] in sampled_fills]
    if len(sample_file) != len(sample_got):
        mismatches.append(f"sample count {len(sample_got)} != {len(sample_file)}")
    else:
        for a, b in zip(sorted(sample_got, key=key), sorted(sample_file, key=lambda t: (t["symbol"], t["entry_date"], t["exit_date"], t["side"], t["exit_reason"]))):
            if a["entry_date"].isoformat() != b["entry_date"] or a["entry_price"] != num(b["entry_price"]) or a["side"] != b["side"] or a["exit_price"] != num(b["exit_price"]):
                mismatches.append("sample mismatch")
                break
    # 40 signal dates were drawn. Count how many of those fills actually exist.
    n_sample_dates = len(pick)
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    dirty = "yes" if subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True).stdout.strip() else "no"
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    # Headline from this replay, for the log. Same definition as the study.
    r = np.array([x[1] for x in daily])
    sh = float(r.mean() / r.std(ddof=1) * np.sqrt(252))
    oos = np.array([x[1] for x in daily if x[0] >= OOS])
    oos_sh = float(oos.mean() / oos.std(ddof=1) * np.sqrt(252))
    full_ret = float(np.prod(1.0 + r) - 1.0)
    oos_ret = float(np.prod(1.0 + oos) - 1.0)
    status = "matched" if not mismatches else "MISMATCH"
    with open(HERE / "RUNLOG.md", "a", encoding="utf-8", newline="\n") as f:
        f.write(
            f"\n## {now}\n"
            f"- rules_sha256 {sha}\n"
            f"- git_head {head} dirty={dirty}\n"
            f"- reason: independent verify.py replay\n"
            f"- {status}; trips {len(got)} vs file {len(file_trips)}; "
            f"sampled signal dates {n_sample_dates}; sample trips {len(sample_got)}; "
            f"full Sharpe {sh} return {full_ret} | OOS Sharpe {oos_sh} return {oos_ret}\n"
        )
        if mismatches:
            f.write("- mismatches: " + "; ".join(mismatches[:8]) + "\n")
    if mismatches:
        sys.exit("verify failed: " + "; ".join(mismatches[:8]))
    print(f"matched {len(got)} trips and {len(daily)} sessions; sample dates {n_sample_dates}; "
          f"full Sharpe {sh:.6f} OOS Sharpe {oos_sh:.6f}")


if __name__ == "__main__":
    main()
