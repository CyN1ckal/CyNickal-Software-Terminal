# QQQ holdings volume node

This study was not run. The status is not a paper-trading verdict. No rules were locked, and no return was computed.

## What was asked

A high-volume node: a price level where volume exceeds some magnitude, traded when price comes back within an error band. The universe is the individual stock holdings of Invesco QQQ, not the ETF. The magnitude and the band were not specified, and they were not taken from prices.

No study already under `research/` trades a volume-at-price node.

## What the store has

The holdings file for CUSIP 46090E103, effective 2026-10-03 (business date 2026-10-02), has 106 lines. This check covers the 101 common stocks and depositary receipts. Cash, currency collateral, a synthetic cash line, pending dividends, and the NQZ6 future were left out. All 101 names are in the store. Each has daily bars, and every daily session that has a bar has exactly one bar.

Two of the 101 have any 1-minute bars. ADBE and GOOGL each have 19 sessions, on the same dates, from 2026-08-31 through 2026-09-25. GOOGL's 19 sessions are complete, 390 bars each. ADBE matches that except 2026-09-25, which is partial, 86 of 390 bars. The other 99 names, including GOOG, have no 1-minute coverage rows.

QQQ, SPY, and IGV are the only other names with 1-minute bars: 1,254 sessions each, from 2021-09-27 through 2026-09-25. They are not these holdings.

The looks were the instrument list, coverage, and bar counts by timeframe, read through `agent-data/mdq.py`. No price, return, forward return, or hit rate was read.

## Why that blocks the mechanism

A node is volume located at a price. A daily bar is one volume print for the whole session. It does not say where inside the high-low range that volume traded. Nineteen sessions of 1-minute bars, on two names, are not a profile either. There is no history in which a level can form and then be traded when price comes back.

Distributing a day's volume across the high-low range, or calling a high-volume day a node, would be a different claim. This study does not make that substitution.

## What a future study would need

Historical 1-minute bars for these holdings, or a real volume profile, long enough to form a level and then see price return to it. That study needs its own rules, locked before any return.

### Ideas for a new study

Unwired. No results.

- The same rule, once these holdings have historical 1-minute bars or a stored volume profile.
- A different rule that uses the daily bar alone: spread that day's volume across the high-low range, or treat a high-volume day as the node. That is not a volume-at-price node, and it is not tested here.
