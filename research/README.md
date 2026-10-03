# Strategy research

One folder per strategy. Each folder holds its rules, code, and raw output in `research/`, and its final report in `report/`. Studies follow the [research protocol](../.claude/skills/quant-research/SKILL.md): the hypothesis and rules are locked before any return is computed, and the status comes from acceptance criteria fixed in advance.

| Study | Idea | Status | Date |
|---|---|---|---|
| [qqq-intraday-trend](qqq-intraday-trend/) | Trade QQQ in the direction of the day's move once it leaves a time-of-day noise band (Zarattini, Aziz & Barbon 2024) | Paper-trading candidate | 2026-09-26 |
| [intraday-channel-trend](intraday-channel-trend/) | 15-minute Donchian breakout with a chandelier trail on QQQ, SPY, and IGV | Rejected | 2026-09-26 |
| [micro-futures-trend](micro-futures-trend/) | 1/3/12-month trend following (Hurst, Ooi & Pedersen 2017) on 11 CME micro futures with whole contracts in a $100k account | Rejected | 2026-09-26 |
| [qqq-15m-turtle-overnight](qqq-15m-turtle-overnight/) | Turtle System 2 (55/20 channels, 2N stop) on QQQ 15-minute bars, held overnight | Rejected | 2026-09-26 |
| [qqq-strategy-portfolio](qqq-strategy-portfolio/) | Combine QQQ buy and hold with the studies above at in-sample max-Sharpe weights, capped at 1× overnight and 2× intraday | Rejected | 2026-09-26 |
| [qqq-intraday-reversion](qqq-intraday-reversion/) | Fade 5-minute shocks in QQQ that are extreme against both the time of day and the day so far | In progress: not yet in this layout | 2026-09-26 |
| [qqq-atr-scale-in](qqq-atr-scale-in/) | Scale into a QQQ move of half a prior-day ATR from the open, up to three equal units, and take profit a quarter ATR past the average | Rejected | 2026-09-26 |
| [low-liq-high-vol-mean-reversion](low-liq-high-vol-mean-reversion/) | One-week quintile reversal on a low-dollar-volume, high-volatility, small-cap screen | Rejected | 2026-09-26 |
| [qqq-atr-martingale](qqq-atr-martingale/) | Double that QQQ fade at each further half-ATR rung and hold until the round trip covers its cost | Paper-trading candidate | 2026-09-26 |
| [qqq-bollinger-adding](qqq-bollinger-adding/) | Fade a 5-minute QQQ close outside a session-local 20-bar, 2-SD Bollinger band, add up to two units at each further band SD, and exit at the middle band | Rejected | 2026-09-26 |
| [small-cap-gap-up-fade](small-cap-gap-up-fade/) | Short the opening auction when a name on the small-cap screen gaps up at least 5%, and cover at the close | Paper-trading candidate | 2026-09-26 |
| [spy-rsi2-dip-buy](spy-rsi2-dip-buy/) | Buy SPY at the close when RSI(2) < 10 and sell at the first close above the 5-day average (Connors & Alvarez 2008): a negatively skewed, high-win-rate index trade | Paper-trading candidate (timing placebo p = 0.046, seed-sensitive) | 2026-09-26 |
| [finviz-gap-up-fade](finviz-gap-up-fade/) | The same opening-gap short on a Finviz sample of other small-cap names, with a mid-cap cross book | Inconclusive | 2026-09-26 |
| [index-opening-pop-fade](index-opening-pop-fade/) | Short SPY at 10:00 after an unusually large first-half-hour rise and cover at the close (Grant, Wolf & Yu 2005), with QQQ as the cross-market test | Rejected | 2026-09-26 |
| [qqq-return-stack](qqq-return-stack/) | Hold QQQ at 1× and stack every in-sample-positive strategy above on top at 5% in-sample volatility each (return stacking); charts show each strategy's impact | Rejected (failed the 2× cost line; OOS Sharpe 1.70 vs QQQ 1.01 at base cost) | 2026-09-26 |
| [qqq-atr-band-dip-eod](qqq-atr-band-dip-eod/) | Buy QQQ at the session open minus 1× prior-day ATR(14) on a resting limit and hold to the close | Rejected (negative gross; OOS Sharpe −0.40, 4 of 6 lines fail) | 2026-09-30 |
| [spy-overnight-premium](spy-overnight-premium/) | Buy SPY at every close and sell at the next open | Rejected | 2026-10-02 |
| [treasury-etf-trend](treasury-etf-trend/) | 12-month time-series momentum on TLT and IEF, equal weight, monthly | Inconclusive | 2026-10-02 |
| [commodity-etf-momentum](commodity-etf-momentum/) | 12-month cross-sectional momentum on six commodity ETFs, long the top 2 and short the bottom 2 | Inconclusive | 2026-10-02 |
| [fx-etf-momentum](fx-etf-momentum/) | 63-session time-series momentum on six CurrencyShares ETFs, equal weight, monthly | Rejected | 2026-10-02 |
| [spdr-sector-momentum](spdr-sector-momentum/) | 12-1 month cross-sectional momentum on the SPDR sector ETFs, long the top 3 and short the bottom 3 | Rejected | 2026-10-02 |
| [spy-pre-holiday](spy-pre-holiday/) | Buy SPY at the open and sell at the close on the last session before a full-day NYSE closure | Rejected | 2026-10-02 |
| [eem-us-leadlag](eem-us-leadlag/) | Trade EEM open-to-close in the direction of the prior SPY close-to-close move | Rejected | 2026-10-02 |
| [country-bab](country-bab/) | Long the 3 lowest-beta country ETFs and short the 3 highest-beta, monthly | Inconclusive | 2026-10-02 |
| [vixy-variance-premium](vixy-variance-premium/) | Constant short of VIXY, reset monthly to a weight of −1 | Void | 2026-10-02 |
| [gold-silver-ratio](gold-silver-ratio/) | Dollar-neutral GLD/SLV pair when the ratio is 2 standard deviations from its 60-session mean | Inconclusive | 2026-10-02 |
| [qqq-holdings-ma-bounce](qqq-holdings-ma-bounce/) | Long a QQQ holding when price dips into the 150–250 session average band and closes back above the faster average | Paper-trading candidate | 2026-10-03 |
| [qqq-holdings-volume-node](qqq-holdings-volume-node/) | High-volume-node test on QQQ holdings | Not run: no intraday volume profile | 2026-10-03 |
| [qqq-holdings-obv-divergence](qqq-holdings-obv-divergence/) | Granville OBV swing divergence, long and short, on the current QQQ holdings | Rejected | 2026-10-03 |
| [qqq-holdings-relative-strength](qqq-holdings-relative-strength/) | 6–1 month excess return versus QQQ, long the top quintile | Paper-trading candidate | 2026-10-03 |
| [qqq-holdings-earnings](qqq-holdings-earnings/) | Pre- and post-earnings setups on QQQ holdings | Not run: no announcement history | 2026-10-03 |
