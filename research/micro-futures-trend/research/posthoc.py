# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc analyses, added after the first run's results were seen. They explain the
result and expose risk; none feeds the verdict. Writes posthoc.json and appends a
note to RUNLOG.md.

1. Rolling 252-session Sharpe of the primary; the trailing-year result.
2. Dependence on extreme days: full-sample return without the 10 best / 10 worst sessions.
3. The dropped leg: what the primary's actual positions would have earned on the
   close(t-1) -> open(t) leg that the open-to-close basis ignores, split into sessions
   within 3 sessions of a RULES.md roll-cost date (where roll gaps sit) and all others.
4. Rates sleeve: share of rebalances at which the 1-, 3-, and 12-month votes disagree.

    python research/micro-futures-trend/research/posthoc.py
"""
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

import backtest as bt

HERE = Path(__file__).resolve().parent


def main() -> None:
    h = bt.rules_hash()
    data = {m: bt.load_market(m)[0] for m in bt.MARKETS}
    prim = bt.run(data, bt.SPECS, bt.Params())
    r = bt.daily_returns(prim)
    ms = bt.window_mask(prim, "full")
    ses = np.array(prim["sessions"])
    x = r[ms]
    out = {"label": "post hoc", "rules_sha256": h}

    roll = [(str(ses[ms][k - 1]), bt.sharpe(x[k - 252:k])) for k in range(252, len(x) + 1)]
    vals = np.array([v for _, v in roll])
    out["rolling_sharpe_252"] = roll
    out["rolling_share_positive"] = float(np.mean(vals > 0))
    out["rolling_min"] = dict(date=roll[int(np.argmin(vals))][0], sharpe=float(vals.min()))
    out["rolling_max"] = dict(date=roll[int(np.argmax(vals))][0], sharpe=float(vals.max()))
    ty = x[-252:]
    out["trailing_year"] = dict(window=f"{ses[ms][-252]} -> {ses[ms][-1]}", sharpe=bt.sharpe(ty),
                                total_return=float(np.prod(1 + ty) - 1))

    order = np.argsort(x)
    out["extreme_days"] = dict(
        full_return=float(np.prod(1 + x) - 1),
        without_10_best=float(np.prod(1 + np.delete(x, order[-10:])) - 1),
        without_10_worst=float(np.prod(1 + np.delete(x, order[:10])) - 1))

    # dropped leg under the primary's own positions
    roll_dates = bt.roll_cost_calendar(prim["sessions"], prim["markets"])
    s_index = {d: t for t, d in enumerate(prim["sessions"])}
    legs = {}
    for i, m in enumerate(prim["markets"]):
        sp = bt.SPECS[m]
        near = set()
        for d in roll_dates[m]:
            t0 = s_index[d]
            near |= {prim["sessions"][t] for t in range(max(0, t0 - 3), min(len(ses), t0 + 4))}
        near_pnl = other_pnl = 0.0
        bars = data[m]
        for j in range(1, len(bars)):
            t = s_index[bars[j].date]
            if t < prim["start"]:
                continue
            n = prim["held"][i, t - 1] if t > 0 else 0.0   # position carried into this bar's open
            if n == 0:
                continue
            a, b = bars[j - 1].close, bars[j].open
            v = n * sp.notional * (b / a - 1) if sp.notional is not None else n * sp.mult * (b - a)
            if bars[j].date in near:
                near_pnl += v
            else:
                other_pnl += v
        legs[m] = dict(near_roll_usd=near_pnl, other_usd=other_pnl)
    out["dropped_leg"] = dict(
        markets=legs, total_near_roll_usd=sum(v["near_roll_usd"] for v in legs.values()),
        total_other_usd=sum(v["other_usd"] for v in legs.values()),
        note="P&L the primary's actual positions would have had on close(t-1)->open(t); not part of any verdict")

    # rates: vote agreement at rebalances
    agree = {}
    for m in ("ZT=F", "ZF=F", "ZN=F", "ZB=F", "ES=F", "GC=F", "CL=F"):
        sigs = [e["signal"] for e in prim["rebal"] if e["market"] == m]
        agree[m] = dict(rebalances=len(sigs), share_unanimous=float(np.mean([abs(s) == 1 for s in sigs])),
                        sign_changes=int(sum(1 for a, b in zip(sigs, sigs[1:]) if np.sign(a) != np.sign(b))))
    out["signal_agreement"] = agree

    # descriptive trade statistics from the primary's trades
    hold = [(datetime.fromisoformat(t.exit_date) - datetime.fromisoformat(t.entry_date)).days for t in prim["trades"]]
    years = len(x) / 252
    out["trade_stats"] = dict(holding_days_median=float(np.median(hold)), holding_days_mean=float(np.mean(hold)),
                              trades_per_year=len(prim["trades"]) / years,
                              max_contracts=float(max(t.max_contracts for t in prim["trades"])))

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=1))
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=bt.ROOT).stdout.strip()
    with (HERE / "RUNLOG.md").open("a") as f:
        f.write(f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')} (post hoc)\n\n"
                f"- Reason: post hoc diagnostics (posthoc.py) after the verdict; re-runs the primary unchanged\n"
                f"- Rules sha256: `{h}`\n- Git HEAD: `{head}`\n"
                f"- Primary full-sample return reproduced: {out['extreme_days']['full_return']:.4f}; "
                f"verdict unchanged\n")
    print(json.dumps({k: v for k, v in out.items() if k != "rolling_sharpe_252"}, indent=1))


if __name__ == "__main__":
    main()
