# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Post hoc analyses, added after seeing the pre-registered results.

Reads ONLY the output files in this folder (daily.csv, trades.csv,
results.json) - the store is not opened and nothing here computes a new
return from it. Nothing here can change the verdict. Writes posthoc.json.
Seed: the pre-registered direction-placebo seed, applied to the stored
trades exactly as RULES.md specifies, so charts.py can draw the null."""

import json
import math
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
RULES_SHA = None  # filled from results.json


def sharpe(rs):
    r = np.asarray(rs, float)
    sd = r.std(ddof=1)
    return float(r.mean() / sd * math.sqrt(252)) if sd > 0 else 0.0


def main():
    res = json.loads((HERE / "results.json").read_text())
    daily = [line.strip().split(",") for line in (HERE / "daily.csv").read_text().splitlines()[1:]]
    sessions = [d[0] for d in daily]
    r_net = np.array([float(d[1]) for d in daily])
    bh = np.array([float(d[3]) for d in daily])
    trades = [line.strip().split(",") for line in (HERE / "trades.csv").read_text().splitlines()[1:]]
    gross = np.array([float(t[6]) for t in trades])

    out = {"note": "post hoc; does not feed the verdict", "rules_sha256": res["rules_sha256"]}

    # rolling 63-session Sharpe
    W = 63
    roll = []
    for i in range(len(r_net) - W + 1):
        roll.append(sharpe(r_net[i:i + W]))
    roll_bh = [sharpe(bh[i:i + W]) for i in range(len(bh) - W + 1)]
    out["rolling_sharpe"] = dict(window=W, start=sessions[W - 1], end=sessions[-1],
                                 strategy=roll, bh=roll_bh)

    # trade concentration (post hoc)
    order = np.argsort(gross)
    dn = np.array([float(t[8]) for t in trades])          # dollars_net
    out["concentration"] = dict(
        sum_net_worst10=float(dn[order[:10]].sum()),
        sum_net_best10=float(dn[order[-10:]].sum()),
        total_net=float(dn.sum()),
        mean_excl_best_worst5pct=float(np.mean(np.sort(gross)[int(0.05 * len(gross)):int(0.95 * len(gross))])),
        mean_all=float(gross.mean()))

    # sub-periods (post hoc)
    ix = {s: i for i, s in enumerate(sessions)}
    apr25 = [s for s in sessions if s.startswith("2025-04")]
    oos_ix = [i for s, i in ix.items() if s >= "2024-07-01"]
    no_apr = [i for i in oos_ix if sessions[i] not in apr25]
    y26 = [i for s, i in ix.items() if s.startswith("2026")]
    out["subperiods"] = dict(
        oos_ex_april2025_sharpe=sharpe(r_net[no_apr]),
        oos_sharpe=res["symbols"]["QQQ"]["OOS"]["sharpe"],
        full_ex_2026_sharpe=sharpe(np.delete(r_net, y26)),
        y2026_return=float(np.prod(1 + r_net[y26]) - 1), y2026_sharpe=sharpe(r_net[y26]),
        n_apr2025_sessions=len(apr25))

    # direction placebo draws again (same seed, same rule) -> null samples for the chart
    cols = [ix[t[0]] for t in trades]
    rng = np.random.default_rng(res["seeds"]["direction"])
    N = 2000
    signs = rng.choice([-1.0, 1.0], size=(N, len(cols)))
    daily_m = np.zeros((N, len(sessions)))
    daily_m[np.repeat(np.arange(N), len(cols)), np.tile(cols, N)] = (signs * gross[None, :]).reshape(-1)
    mu, sd = daily_m.mean(axis=1), daily_m.std(axis=1, ddof=1)
    null_sh = np.where(sd > 0, mu / sd * math.sqrt(252), 0.0)
    gd = np.zeros(len(sessions))
    gd[cols] = gross
    p = float((1 + int((null_sh >= sharpe(gd)).sum())) / (N + 1))
    out["direction_placebo_repeat"] = dict(p=p, p_matches_results=bool(abs(p - res["placebo"]["direction_p"]) < 1e-9),
                                           null_sharpe_samples=[round(float(x), 4) for x in null_sh])

    # best/worst trade list (post hoc)
    bt = [dict(session=t[0], entry=t[2], gross=float(t[6])) for t in trades]
    out["best5"] = sorted(bt, key=lambda x: -x["gross"])[:5]
    out["worst5"] = sorted(bt, key=lambda x: x["gross"])[:5]

    (HERE / "posthoc.json").write_text(json.dumps(out, indent=1))
    print(json.dumps({k: v for k, v in out.items() if k not in ("rolling_sharpe",)}, indent=1, default=str)[:1500])


if __name__ == "__main__":
    main()
