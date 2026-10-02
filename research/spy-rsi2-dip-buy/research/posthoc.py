# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc analyses for the SPY RSI(2) dip-buy. Added after the first run.

Nothing here feeds the verdict. Reads daily.csv, trades.csv, and results.json
(and the store only for the timing-placebo seed check), writes posthoc.json.

    python research/spy-rsi2-dip-buy/research/posthoc.py
"""
from __future__ import annotations

import csv
import json
import math
import sys
from datetime import date
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import backtest as bt  # noqa: E402


def sharpe(r):
    r = np.asarray(r, float)
    return float(r.mean() / r.std(ddof=1) * math.sqrt(252)) if r.std(ddof=1) > 0 else None


def main() -> None:
    rows = list(csv.DictReader(open(HERE / "daily.csv")))
    d = [date.fromisoformat(r["date"]) for r in rows]
    s = np.array([float(r["strategy_net"]) for r in rows])
    b = np.array([float(r["spy_bh"]) for r in rows])
    trades = [t for t in csv.DictReader(open(HERE / "trades.csv")) if t["symbol"] == "SPY"]
    out: dict = {}

    # Dependence on the best and worst sessions.
    order = np.argsort(s)
    for k in (5, 10, 20):
        z = s.copy()
        z[order[-k:]] = 0.0
        out[f"zero_best_{k}"] = {"sharpe": sharpe(z), "total_return": float(np.prod(1 + z) - 1)}
        z = s.copy()
        z[order[:k]] = 0.0
        out[f"zero_worst_{k}"] = {"sharpe": sharpe(z), "total_return": float(np.prod(1 + z) - 1)}

    # Trailing 252-session Sharpe.
    roll = []
    for i in range(251, len(s)):
        roll.append({"date": str(d[i]), "strategy": sharpe(s[i - 251:i + 1]), "spy": sharpe(b[i - 251:i + 1])})
    (HERE / "rolling_sharpe.csv").write_text("date,strategy,spy\n" + "".join(
        f"{r['date']},{r['strategy']},{r['spy']}\n" for r in roll))
    vals = [r["strategy"] for r in roll if r["strategy"] is not None]
    out["rolling_252"] = {"min": min(vals), "max": max(vals), "last": vals[-1],
                          "share_positive": float(np.mean([v > 0 for v in vals]))}

    # Correlation with SPY, and SPY's worst days.
    out["corr_with_spy"] = float(np.corrcoef(s, b)[0, 1])
    worst = np.argsort(b)[:20]
    out["spy_worst_20_days"] = {"strategy_avg_bp": float(s[worst].mean() * 1e4),
                                "spy_avg_bp": float(b[worst].mean() * 1e4),
                                "strategy_in_market_share": float(np.mean([rows[i]["held_at_close"] == "1" or s[i] != 0 for i in worst]))}

    # Return per unit of exposure versus buy and hold.
    held = np.array([int(r["held_at_close"]) for r in rows])
    out["exposure"] = float(held.mean())

    # The tail: every losing trade.
    losers = sorted([t for t in trades if float(t["net"]) <= 0], key=lambda t: float(t["net"]))
    out["losing_trades"] = [{k: t[k] for k in ("entry_day", "exit_day", "hold_sessions", "net", "entry_rsi")} for t in losers]
    nets = np.array([float(t["net"]) for t in trades])
    out["worst_trade"] = float(nets.min())
    out["best_trade"] = float(nets.max())
    out["sum_net_winners"] = float(nets[nets > 0].sum())
    out["sum_net_losers"] = float(nets[nets <= 0].sum())

    # Timing placebo: seed sensitivity (the locked seed is 20260927).
    calendar = bt.nyse_sessions(bt.DATA_START, bt.END)
    with bt.MarketData() as md:
        spy = bt.load(md, "SPY")
    prim = bt.simulate(spy, calendar, cost_bps=1.0)
    bh = bt.bh_returns(spy, calendar)
    ps = {}
    for seed in (1, 2, 3, 4, 5, 20260927):
        ps[str(seed)] = bt.timing_placebo(prim, bh, seed)["p"]
    out["timing_placebo_p_by_seed"] = ps

    # Since May 2025 and the trailing year.
    for lbl, lo in (("since_2025_05_01", date(2025, 5, 1)), ("trailing_252", d[-252])):
        m = np.array([x >= lo for x in d])
        out[lbl] = {"sessions": int(m.sum()), "sharpe": sharpe(s[m]), "return": float(np.prod(1 + s[m]) - 1),
                    "spy_sharpe": sharpe(b[m]), "spy_return": float(np.prod(1 + b[m]) - 1)}

    # The best sessions by date, and the OOS result without the single best OOS session.
    out["best_sessions"] = [{"date": str(d[i]), "strategy": float(s[i]), "spy": float(b[i])} for i in order[::-1][:5]]
    out["worst_sessions"] = [{"date": str(d[i]), "strategy": float(s[i]), "spy": float(b[i])} for i in order[:5]]
    oos = np.array([x >= date(2024, 7, 1) for x in d])
    so = s[oos].copy()
    so[np.argmax(so)] = 0.0
    out["oos_without_best_oos_session"] = {"sharpe": sharpe(so), "total_return": float(np.prod(1 + so) - 1)}

    # Descriptive figures quoted in the report.
    res = json.loads((HERE / "results.json").read_text())
    years = (d[-1] - d[0]).days / 365.25
    out["trades_per_year"] = len(trades) / years
    q = res["by_move_quintile"]
    out["capture_q1"] = q[0]["strategy_avg_bp"] / q[0]["spy_avg_bp"]
    out["capture_q5"] = q[4]["strategy_avg_bp"] / q[4]["spy_avg_bp"]
    hs = np.array([int(t["hold_sessions"]) for t in trades])
    out["hold_le3"] = {"trades": int((hs <= 3).sum()), "win_rate": float((nets[hs <= 3] > 0).mean())}
    out["hold_ge6"] = {"trades": int((hs >= 6).sum()), "win_rate": float((nets[hs >= 6] > 0).mean())}
    # Worst mark-to-market depth of each trade, from its close-to-close path.
    pos_of = {x: k for k, x in enumerate(d)}
    depth = []
    for t in trades:
        i0, i1 = pos_of[date.fromisoformat(t["entry_day"])], pos_of[date.fromisoformat(t["exit_day"])]
        path = np.cumprod(1 + b[i0 + 1:i1 + 1])
        depth.append({"entry_day": t["entry_day"], "exit_day": t["exit_day"],
                      "worst_mark": float(path.min() - 1) if len(path) else 0.0, "net": float(t["net"])})
    out["trade_worst_marks"] = sorted(depth, key=lambda x: x["worst_mark"])[:5]
    s1f = res["s1"]["full"]
    out["s1_vs_primary"] = {"return_ratio": s1f["total_return"] / res["primary"]["full"]["total_return"],
                            "vol_ratio": s1f["ann_vol"] / res["primary"]["full"]["ann_vol"]}

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=2))
    from datetime import datetime, timezone
    with open(HERE / "RUNLOG.md", "a") as fh:
        fh.write(f"\n## {datetime.now(timezone.utc).isoformat(timespec='seconds')} (posthoc.py)\n\n"
                 "- Reason: post hoc analyses (not part of the verdict); re-simulates the locked primary to "
                 "check the timing placebo under other seeds\n"
                 f"- Timing placebo p by seed: {ps}\n")
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
