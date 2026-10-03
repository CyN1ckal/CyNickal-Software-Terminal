# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Coverage, bar counts, and the PPLT split-continuity check.

No ratio, no return, no z-score, no threshold count. Those are not computed
here. The only price ratio is PPLT's ex-date close divided by the previous
close, raw and split-adjusted, which is a jump check.
"""

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "agent-data"))
from mdq import MarketData, nyse_sessions  # noqa: E402

HERE = Path(__file__).resolve().parent
SKIP = ("2012-10-29", "2012-10-30", "2018-12-05")
END = "2026-10-01"


def summarize(md, symbol):
    cov = md.coverage_summary(symbol, "1d")
    actions = md.corporate_actions(symbol)
    adjusted = md.bars(symbol, "1d", end=END)
    raw = md.bars(symbol, "1d", adjust=False, end=END)
    all_adj = md.bars(symbol, "1d")
    sessions = [b.session.isoformat() for b in adjusted]
    dupes = [d for d, n in Counter(sessions).items() if n > 1]
    bad = []
    zero_vol = 0
    for b in adjusted:
        if b.open <= 0 or b.high <= 0 or b.low <= 0 or b.close <= 0:
            bad.append(b.session.isoformat())
        if b.volume == 0:
            zero_vol += 1
    return {
        "symbol": symbol,
        "coverage": cov,
        "actions": actions,
        "bars_through_end": len(adjusted),
        "bars_all": len(all_adj),
        "first": sessions[0] if sessions else None,
        "last_through_end": sessions[-1] if sessions else None,
        "last_all": all_adj[-1].session.isoformat() if all_adj else None,
        "duplicate_sessions": dupes,
        "nonpositive_ohlc": bad,
        "zero_volume": zero_vol,
        "raw_count_through_end": len(raw),
        "sessions": sessions,
    }


def jump(md, symbol, ex_date):
    raw = md.bars(symbol, "1d", adjust=False)
    adj = md.bars(symbol, "1d")
    raw_s = [b for b in raw if b.session.isoformat() <= ex_date]
    adj_s = [b for b in adj if b.session.isoformat() <= ex_date]
    out = {"symbol": symbol, "ex_date": ex_date}
    for label, rows in (("raw", raw_s), ("adjusted", adj_s)):
        if len(rows) < 2 or rows[-1].session.isoformat() != ex_date:
            out[label] = None
            continue
        prev, cur = rows[-2], rows[-1]
        out[label] = {
            "prev_session": prev.session.isoformat(),
            "close_ratio": cur.close / prev.close,
        }
        if len(rows) >= 3:
            out[label]["prior_close_ratio"] = prev.close / rows[-3].close
    return out


def main():
    with MarketData() as md:
        names = {s: summarize(md, s) for s in ("GLD", "SLV", "PPLT")}
        gld, slv, pplt = names["GLD"]["sessions"], names["SLV"]["sessions"], names["PPLT"]["sessions"]
        cal = [d.isoformat() for d in nyse_sessions("2011-01-04", END)]
        skipped = [d for d in SKIP if d in cal]
        book = [d for d in cal if d not in SKIP]
        pplt_jump = jump(md, "PPLT", "2026-05-18")
    overlap = sorted(set(gld) & set(slv))
    doc = {
        "end": END,
        "nyse_sessions_2011-01-04_through_end": len(cal),
        "skipped_closures_in_calendar": skipped,
        "book_sessions": len(book),
        "gld_slv_both": len(overlap),
        "gld_not_slv": sorted(set(gld) - set(slv)),
        "slv_not_gld": sorted(set(slv) - set(gld)),
        "pplt_not_gld_through_end": sorted(set(pplt) - set(gld)),
        "gld_not_pplt_through_end": sorted(set(gld) - set(pplt)),
        "book_not_gld": sorted(set(book) - set(gld)),
        "gld_not_book": sorted(set(gld) - set(book)),
        "has_2024-06-28": "2024-06-28" in gld,
        "has_2024-07-01": "2024-07-01" in gld,
        "pplt_jump": pplt_jump,
        "names": {
            s: {k: v for k, v in docn.items() if k != "sessions"}
            for s, docn in names.items()
        },
    }
    (HERE / "counts.json").write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(doc, indent=2))


if __name__ == "__main__":
    main()
