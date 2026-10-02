# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pre-registered QQQ ATR martingale. Mark-to-market decides the verdict.

    python research/qqq-atr-martingale/research/backtest.py
    python research/qqq-atr-martingale/research/backtest.py --self-test
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
import subprocess
import sys
from collections import defaultdict
from datetime import date, datetime, timezone
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "agent-data"))

from engine import BAR_S, Bar, Params, drive  # noqa: E402
from mdq import (  # noqa: E402
    EARLY_CLOSES,
    MarketData,
    ny_datetime,
    nyse_sessions,
    resample,
)
import selftest  # noqa: E402

# engine.BAR_S is the bar size. ANNUAL is the session year.
ANNUAL = 252
ATR_N = 14
IS_END = date(2024, 6, 28)
OOS_START = date(2024, 7, 1)
LAST_DAY = date(2026, 9, 25)
QQQ_FIRST = date(2021, 10, 7)
SEED_DIRECTION = 20260926
SEED_TIMING = 20260927
SEED_BOOT = 20260928
GRID_SPACING = (0.35, 0.50, 0.65, 0.80, 1.00)
GRID_MULT = (1.5, 2.0, 3.0)
COST_SWEEP = (0.0, 0.5, 1.0, 2.0, 3.0)


def minute_of(ts: int) -> int:
    t = ny_datetime(ts)
    return t.hour * 60 + t.minute


def clock(ts: int) -> str:
    return ny_datetime(ts).strftime("%Y-%m-%d %H:%M")


def end_minute(day: date) -> int:
    return 13 * 60 if day in EARLY_CLOSES else 16 * 60


def atr_at_close(daily):
    if len(daily) < ATR_N + 1:
        return []
    trs, days = [], []
    for i in range(1, len(daily)):
        prev = daily[i - 1].close
        hi, lo = daily[i].high, daily[i].low
        trs.append(max(hi - lo, abs(hi - prev), abs(lo - prev)))
        days.append(daily[i].session)
    atr = sum(trs[:ATR_N]) / ATR_N
    out = [(days[ATR_N - 1], atr)]
    for j in range(ATR_N, len(trs)):
        atr = ((ATR_N - 1) * atr + trs[j]) / ATR_N
        if atr > 0:
            out.append((days[j], atr))
    return out


def atr_for_day(closes, day):
    j = -1
    for i, (d, _) in enumerate(closes):
        if d < day:
            j = i
        else:
            break
    if j < 0:
        return None
    return closes[j]


def rules_hash() -> str:
    return hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def check_lock() -> str:
    h = rules_hash()
    locked = {}
    for line in (HERE / "RULES.lock").read_text(encoding="utf-8").splitlines():
        if " " in line:
            k, v = line.split(" ", 1)
            locked[k] = v.strip()
    if locked.get("sha256") != h:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {locked.get('sha256')}. Refusing to run.")
    return h


def prepare(md: MarketData, symbol: str):
    daily = md.bars(symbol, "1d")
    grouped = defaultdict(list)
    for b in resample(md.bars(symbol, "1m"), BAR_S):
        grouped[b.session].append(b)
    closes = atr_at_close(daily)
    first = next(d for d in sorted(grouped) if atr_for_day(closes, d) is not None)
    calendar = list(nyse_sessions(first, LAST_DAY))
    bars = []
    for day in calendar:
        seq = grouped.get(day, [])
        if not seq:
            continue
        info = atr_for_day(closes, day)
        atr, atr_day = (info[1], info[0]) if info else (None, None)
        sess_open = seq[0].open if minute_of(seq[0].ts) == 9 * 60 + 30 else None
        need = 12 * 60 + 55 if day in EARLY_CLOSES else 15 * 60 + 55
        qualified = bool(sess_open is not None and atr and atr > 0 and atr_day < day and minute_of(seq[-1].ts) >= need)
        em = end_minute(day)
        for i, b in enumerate(seq):
            bars.append(Bar(
                b.ts, b.open, b.close, minute_of(b.ts), day, i == len(seq) - 1,
                sess_open, atr, em, qualified,
            ))
    daily_close = {b.session: b.close for b in daily}
    return bars, calendar, daily_close


def sharpe(r: np.ndarray) -> float:
    r = np.asarray(r, dtype=float)
    if len(r) < 2:
        return float("nan")
    sd = float(r.std(ddof=1))
    if sd <= 0:
        return float("nan")
    return float(r.mean() / sd * math.sqrt(ANNUAL))


def profit_factor(r: np.ndarray) -> float:
    r = np.asarray(r, dtype=float)
    pos, neg = float(r[r > 0].sum()), float(r[r < 0].sum())
    if neg < 0:
        return pos / abs(neg)
    if pos > 0:
        return float("inf")
    return float("nan")


def restarted_dd(r: np.ndarray) -> float:
    r = np.asarray(r, dtype=float)
    path = np.concatenate([[1.0], 1.0 + np.cumsum(r)])
    peak = np.maximum.accumulate(path)
    return float((path / peak - 1.0).min()) if len(path) else 0.0


def metrics(r: np.ndarray, eq_start: float, eq_end: float) -> dict:
    r = np.asarray(r, dtype=float)
    n = len(r)
    total = float(r.sum()) if n else float("nan")
    cagr = float("nan")
    if n and eq_start > 0 and eq_end > 0:
        cagr = float((eq_end / eq_start) ** (ANNUAL / n) - 1.0)
    sd = float(r.std(ddof=1)) if n > 1 else float("nan")
    return {
        "sessions": n,
        "sharpe": sharpe(r),
        "total_return": total,
        "cagr": cagr,
        "ann_vol": float(sd * math.sqrt(ANNUAL)) if sd == sd else float("nan"),
        "max_dd": restarted_dd(r),
        "t_stat": float(r.mean() / (sd / math.sqrt(n))) if sd == sd and sd > 0 else float("nan"),
        "profit_factor": profit_factor(r),
        "day_win_rate": float((r > 0).mean()) if n else float("nan"),
        "eq_start": eq_start,
        "eq_end": eq_end,
    }


def path_stats(equity: np.ndarray) -> dict:
    path = np.concatenate([[1.0], np.asarray(equity, dtype=float)])
    peak = np.maximum.accumulate(path)
    dd = path / peak - 1.0
    return {
        "max_dd": float(dd.min()),
        "peak_to_trough": float((peak - path).max()),
        "min_equity": float(path.min()),
        "max_equity": float(path.max()),
        "dd_path": dd[1:],
    }


def gross_from(n: int, pieces: list, signs: np.ndarray | None = None) -> np.ndarray:
    g = np.zeros(n)
    for cid, si, delta in pieces:
        g[si] += delta if signs is None else signs[cid] * delta
    return g


def pack(out: dict, calendar: list[date]) -> dict:
    equity = np.asarray(out["equity"], dtype=float)
    r = np.asarray(out["r"], dtype=float)
    ps = path_stats(equity)
    closed = [c for c in out["campaigns"] if c["reason"] == "breakeven"]
    opened = [c for c in out["campaigns"] if c["reason"] == "open"]
    realized_sum = float(sum(c["net"] for c in closed))
    losses = sum(1 for c in closed if c["net"] < 0)
    mask_is = np.array([d <= IS_END for d in calendar])
    mask_oos = np.array([d >= OOS_START for d in calendar])
    eq_is_end = float(equity[mask_is][-1]) if mask_is.any() else 1.0
    full = metrics(r, 1.0, float(equity[-1]) if len(equity) else 1.0)
    ins = metrics(r[mask_is], 1.0, eq_is_end)
    oos = metrics(r[mask_oos], eq_is_end, float(equity[-1]) if len(equity) else eq_is_end)
    full["max_dd_on_full_path"] = ps["max_dd"]
    ins["max_dd_on_full_path"] = float(ps["dd_path"][mask_is].min()) if mask_is.any() else float("nan")
    oos["max_dd_on_full_path"] = float(ps["dd_path"][mask_oos].min()) if mask_oos.any() else float("nan")
    holds = [c["exit_i"] - c["entry_i"] + 1 for c in closed if c["entry_i"] >= 0]
    return {
        "equity": equity, "r": r, "calendar": calendar, "full": full, "is": ins, "oos": oos,
        "path": ps, "closed": closed, "open": opened, "realized_sum": realized_sum,
        "n_losses": losses, "active": np.asarray(out["active"]), "notional": np.asarray(out["notional"], dtype=float),
        "side": np.asarray(out["side"]), "units": np.asarray(out["units"]),
        "pieces": out["pieces"], "gap_sum": out["gap_sum"], "refused_add": out["refused_add"],
        "campaigns": out["campaigns"], "mask_is": mask_is, "mask_oos": mask_oos,
        "median_bars_held": float(np.median(holds)) if holds else float("nan"),
        "oos_active": int(np.asarray(out["active"])[mask_oos].sum()) if mask_oos.any() else 0,
    }


def milestones(calendar, notional, equity) -> list:
    rows = []
    seen = set()
    for thr in (2, 5, 10, 20):
        for i, n in enumerate(notional):
            if n > thr and thr not in seen:
                rows.append({"multiple": thr, "session": calendar[i], "notional": float(n), "equity": float(equity[i])})
                seen.add(thr)
                break
    return rows


def bench_marks(calendar, bars, daily_close):
    last_close = {}
    sess_open = {}
    for b in bars:
        last_close[b.session] = b.close
        if b.minute == 9 * 60 + 30 and b.session not in sess_open:
            sess_open[b.session] = b.open
    prior = [d for d in daily_close if calendar and d < calendar[0]]
    prev = daily_close[max(prior)] if prior else None
    cc = np.zeros(len(calendar))
    oc = np.zeros(len(calendar))
    n_5m = 0
    for i, day in enumerate(calendar):
        if day in daily_close:
            mark = daily_close[day]
        elif day in last_close:
            mark = last_close[day]
            n_5m += 1
        else:
            mark = None
        if mark is not None and prev is not None and prev > 0:
            cc[i] = mark / prev - 1.0
        if mark is not None:
            prev = mark
        if day in sess_open and day in last_close and sess_open[day]:
            oc[i] = last_close[day] / sess_open[day] - 1.0
    return cc, oc, n_5m


def run_symbol(bars, calendar, p: Params, forced=None, rng=None):
    out = drive(bars, calendar, p, forced, rng)
    return pack(out, calendar)


def units_table(campaigns) -> list:
    rows = []
    for n in range(1, 17):
        sub = [c for c in campaigns if c["n_units"] == n and c["reason"] == "breakeven"]
        if not sub and n > 4:
            continue
        rows.append({
            "units": n,
            "closed": len(sub),
            "realized": float(sum(c["net"] for c in sub)),
        })
    return rows


def year_table(calendar, r, dd_path, bench) -> list:
    rows = []
    for year in sorted({d.year for d in calendar}):
        mask = np.array([d.year == year for d in calendar])
        rr = r[mask]
        rows.append({
            "year": year,
            "sessions": int(mask.sum()),
            "pnl": float(rr.sum()),
            "sharpe": sharpe(rr),
            "max_dd_restarted": restarted_dd(rr),
            "worst_full_path_dd": float(dd_path[mask].min()) if mask.any() else float("nan"),
            "benchmark_return": float(np.prod(1.0 + bench[mask]) - 1.0),
        })
    return rows


def _ge(a, b) -> bool:
    return isinstance(a, (int, float)) and a == a and isinstance(b, (int, float)) and a >= b


def _gt(a, b) -> bool:
    return isinstance(a, (int, float)) and a == a and a > b


def acceptance(oos, placebo_p, is_sharpe, grid_pos, ret_2x, spy_oos, oos_active):
    lines = [
        {"id": 1, "name": "OOS Sharpe and daily profit factor",
         "required": "Sharpe >= 0.5 and profit factor >= 1.10",
         "actual": {"sharpe": oos["sharpe"], "profit_factor": oos["profit_factor"]},
         "passed": _ge(oos["sharpe"], 0.5) and _ge(oos["profit_factor"], 1.10)},
        {"id": 2, "name": "Direction placebo", "required": "p <= 0.05", "actual": placebo_p,
         "passed": _ge(0.05, placebo_p)},
        {"id": 3, "name": "In-sample Sharpe and plateau",
         "required": "IS Sharpe > 0 and at least 9 of 15 grid cells > 0",
         "actual": {"is_sharpe": is_sharpe, "grid_positive": grid_pos},
         "passed": _gt(is_sharpe, 0.0) and grid_pos >= 9},
        {"id": 4, "name": "Full-sample mark-to-market return at 2 bp",
         "required": "E_T - 1 > 0", "actual": ret_2x, "passed": _gt(ret_2x, 0.0)},
        {"id": 5, "name": "SPY out-of-sample Sharpe", "required": "> 0", "actual": spy_oos,
         "passed": _gt(spy_oos, 0.0)},
        {"id": 6, "name": "Out-of-sample sessions with a position or a close",
         "required": ">= 100", "actual": oos_active, "passed": oos_active >= 100},
    ]
    if not lines[5]["passed"]:
        status = "Inconclusive"
    elif all(line["passed"] for line in lines):
        status = "Paper-trading candidate"
    else:
        status = "Rejected"
    return lines, status


def clean(obj):
    if isinstance(obj, dict):
        return {str(k): clean(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [clean(v) for v in obj]
    if isinstance(obj, np.ndarray):
        return clean(obj.tolist())
    if isinstance(obj, np.floating):
        obj = float(obj)
    if isinstance(obj, np.integer):
        return int(obj)
    if isinstance(obj, float):
        if math.isnan(obj):
            return None
        if math.isinf(obj):
            return "inf" if obj > 0 else "-inf"
        return obj
    if isinstance(obj, date):
        return obj.isoformat()
    return obj


def git_head() -> str:
    return subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()


def git_dirty() -> bool:
    return bool(subprocess.run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True).stdout.strip())


def write_log(rules_sha, reason, full, ins, oos) -> None:
    path = HERE / "RUNLOG.md"
    if not path.exists():
        path.write_text("# Run log\n\nAppend-only. One entry per store run.\n\n", encoding="utf-8", newline="\n")
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    text = (
        f"## {stamp}\n- reason: {reason}\n- rules_sha256: {rules_sha}\n"
        f"- git_head: {git_head()}\n- git_dirty: {str(git_dirty()).lower()}\n"
        f"- full: sharpe {full['sharpe']:.4f}, return {full['total_return']:.4%}\n"
        f"- IS: sharpe {ins['sharpe']:.4f}, return {ins['total_return']:.4%}\n"
        f"- OOS: sharpe {oos['sharpe']:.4f}, return {oos['total_return']:.4%}\n\n"
    )
    with path.open("a", encoding="utf-8", newline="\n") as f:
        f.write(text)


def write_trades(path: Path, rows: list[dict]) -> None:
    fields = ["symbol", "entry_session", "exit_session", "side", "n_units", "entry_ts", "exit_ts",
              "entry_time", "exit_time", "avg_entry", "exit_price", "exit_reason", "leg_prices",
              "leg_notionals", "realized_net", "mtm_net", "bars_held", "capped"]
    with path.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for t in rows:
            w.writerow({
                "symbol": t["symbol"],
                "entry_session": t["entry_session"].isoformat(),
                "exit_session": t["exit_session"].isoformat() if t["exit_session"] else "",
                "side": t["side"],
                "n_units": t["n_units"],
                "entry_ts": t["entry_ts"],
                "exit_ts": t["exit_ts"],
                "entry_time": clock(t["entry_ts"]),
                "exit_time": clock(t["exit_ts"]),
                "avg_entry": f"{t['avg_entry']:.10f}",
                "exit_price": f"{t['exit_price']:.10f}",
                "exit_reason": t["reason"],
                "leg_prices": ";".join(f"{px:.10f}" for _, px, _ in t["legs"]),
                "leg_notionals": ";".join(f"{n:.10f}" for _, _, n in t["legs"]),
                "realized_net": "" if t["net"] is None else f"{t['net']:.12f}",
                "mtm_net": f"{t['mtm_net']:.12f}",
                "bars_held": t["exit_i"] - t["entry_i"] + 1,
                "capped": int(t["capped"]),
            })


def write_daily(path, qqq, bench_cc, bench_oc, spy, igv) -> None:
    fields = ["session", "in_sample", "pnl", "equity", "bench_cc", "bench_oc", "gross_notional",
              "side", "n_units", "active", "spy_pnl", "igv_pnl"]
    spy_map = {d: spy["r"][i] for i, d in enumerate(spy["calendar"])}
    igv_map = {d: igv["r"][i] for i, d in enumerate(igv["calendar"])}
    with path.open("w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for i, day in enumerate(qqq["calendar"]):
            w.writerow({
                "session": day.isoformat(),
                "in_sample": int(day <= IS_END),
                "pnl": f"{qqq['r'][i]:.12f}",
                "equity": f"{qqq['equity'][i]:.12f}",
                "bench_cc": f"{bench_cc[i]:.12f}",
                "bench_oc": f"{bench_oc[i]:.12f}",
                "gross_notional": f"{qqq['notional'][i]:.8f}",
                "side": int(qqq["side"][i]),
                "n_units": int(qqq["units"][i]),
                "active": int(qqq["active"][i]),
                "spy_pnl": "" if day not in spy_map else f"{spy_map[day]:.12f}",
                "igv_pnl": "" if day not in igv_map else f"{igv_map[day]:.12f}",
            })


def slim(block: dict) -> dict:
    return {k: block[k] for k in block if k in (
        "sessions", "sharpe", "total_return", "cagr", "ann_vol", "max_dd", "max_dd_on_full_path",
        "t_stat", "profit_factor", "day_win_rate", "eq_start", "eq_end",
    )}


def side_pnl(r, side) -> list:
    rows = []
    for s, name in ((1, "long"), (-1, "short"), (0, "flat")):
        mask = side == s
        rr = r[mask]
        rows.append({"side": name, "sessions": int(mask.sum()), "pnl": float(rr.sum()) if mask.any() else 0.0,
                     "sharpe": sharpe(rr) if mask.any() else float("nan")})
    return rows


def main() -> None:
    selftest.main()
    if "--self-test" in sys.argv:
        return
    rules_sha = check_lock()
    reason = "initial"
    if "--reason" in sys.argv:
        reason = sys.argv[sys.argv.index("--reason") + 1]
    p = Params()
    prepared = {}
    dailies = {}
    with MarketData() as md:
        for symbol in ("QQQ", "SPY", "IGV"):
            bars, calendar, daily_close = prepare(md, symbol)
            prepared[symbol] = (bars, calendar)
            dailies[symbol] = daily_close
    q_bars, q_cal = prepared["QQQ"]
    if q_cal[0] != QQQ_FIRST:
        sys.exit(f"QQQ first session {q_cal[0]} is not {QQQ_FIRST}")
    if date(2021, 12, 31) not in q_cal or any(b.session == date(2021, 12, 31) for b in q_bars):
        sys.exit("2021-12-31 should be on the calendar and have no five-minute bars")

    q = run_symbol(q_bars, q_cal, p)
    bench_cc, bench_oc, n_5m = bench_marks(q_cal, q_bars, dailies["QQQ"])
    bench = {
        "close_to_close": {
            "full": metrics(bench_cc, 1.0, float(np.prod(1.0 + bench_cc))),
            "is": metrics(bench_cc[q["mask_is"]], 1.0, float(np.prod(1.0 + bench_cc[q["mask_is"]]))),
            "oos": metrics(bench_cc[q["mask_oos"]], 1.0, float(np.prod(1.0 + bench_cc[q["mask_oos"]]))),
            "marks_from_5m_close": n_5m,
        },
        "open_to_close": {
            "full": metrics(bench_oc, 1.0, float(np.prod(1.0 + bench_oc))),
            "is": metrics(bench_oc[q["mask_is"]], 1.0, float(np.prod(1.0 + bench_oc[q["mask_is"]]))),
            "oos": metrics(bench_oc[q["mask_oos"]], 1.0, float(np.prod(1.0 + bench_oc[q["mask_oos"]]))),
        },
    }
    # The benchmark metrics above use sum(r) as total_return via metrics(), but a
    # buy-and-hold total is the compounded wealth minus one. Store that explicitly.
    for key, series in (("close_to_close", bench_cc), ("open_to_close", bench_oc)):
        for name, mask in (("full", slice(None)), ("is", q["mask_is"]), ("oos", q["mask_oos"])):
            rr = series[mask]
            bench[key][name]["compound_return"] = float(np.prod(1.0 + rr) - 1.0)
            bench[key][name]["sharpe"] = sharpe(rr)
            bench[key][name]["max_dd"] = restarted_dd(rr)

    costs = {}
    for bps in COST_SWEEP:
        rr = run_symbol(q_bars, q_cal, Params(cost_bps=bps))
        costs[str(bps)] = {
            "full_sharpe": rr["full"]["sharpe"], "oos_sharpe": rr["oos"]["sharpe"],
            "is_sharpe": rr["is"]["sharpe"], "full_return": rr["full"]["total_return"],
            "oos_return": rr["oos"]["total_return"], "min_equity": rr["path"]["min_equity"],
        }

    delays = {}
    for delay in (1, 2, 0):
        rr = q if delay == 1 else run_symbol(q_bars, q_cal, Params(delay=delay))
        delays[str(delay)] = {
            "full_sharpe": rr["full"]["sharpe"], "oos_sharpe": rr["oos"]["sharpe"],
            "full_return": rr["full"]["total_return"], "oos_return": rr["oos"]["total_return"],
            "min_equity": rr["path"]["min_equity"],
        }

    n_camp = len(q["campaigns"])
    actual_g = gross_from(len(q_cal), q["pieces"])
    actual_g_sharpe = sharpe(actual_g)
    rng = np.random.default_rng(SEED_DIRECTION)
    null = np.empty(2000)
    if n_camp:
        signs = rng.choice(np.array([-1.0, 1.0]), size=(2000, n_camp))
        for i in range(2000):
            null[i] = sharpe(gross_from(len(q_cal), q["pieces"], signs[i]))
    else:
        null[:] = float("nan")
    n_ge = int(np.sum(null >= actual_g_sharpe - 1e-15)) if actual_g_sharpe == actual_g_sharpe else 0
    direction = {
        "actual_gross_sharpe": actual_g_sharpe, "null_mean": float(np.nanmean(null)),
        "null_p95": float(np.nanquantile(null, 0.95)), "p": (1 + n_ge) / 2001,
        "draws": 2000, "seed": SEED_DIRECTION, "n_campaigns": n_camp, "null": null.tolist(),
    }

    starts = defaultdict(list)
    for c in q["campaigns"]:
        starts[c["entry_session"]].append(c["side"])
    rng_t = np.random.default_rng(SEED_TIMING)
    timing_null = np.empty(500)
    for i in range(500):
        alt = run_symbol(q_bars, q_cal, p, forced=starts, rng=rng_t)
        timing_null[i] = sharpe(gross_from(len(q_cal), alt["pieces"]))
    n_ge_t = int(np.sum(timing_null >= actual_g_sharpe - 1e-15)) if actual_g_sharpe == actual_g_sharpe else 0
    timing = {
        "actual_gross_sharpe": actual_g_sharpe, "null_mean": float(np.nanmean(timing_null)),
        "null_p95": float(np.nanquantile(timing_null, 0.95)), "p": (1 + n_ge_t) / 501,
        "draws": 500, "seed": SEED_TIMING, "null": timing_null.tolist(),
    }

    boot_rng = np.random.default_rng(SEED_BOOT)
    r = q["r"]
    n = len(r)
    n_blocks = math.ceil(n / 20)
    starts_b = boot_rng.integers(0, n, size=(2000, n_blocks))
    idx = np.arange(20)
    boot_s = np.empty(2000)
    for d in range(2000):
        sample = np.concatenate([r[(int(s) + idx) % n] for s in starts_b[d]])[:n]
        boot_s[d] = sharpe(sample)
    boot = {"p2_5": float(np.quantile(boot_s, 0.025)), "p97_5": float(np.quantile(boot_s, 0.975)),
            "share_le_0": float(np.mean(boot_s <= 0)), "draws": 2000, "block": 20, "seed": SEED_BOOT}

    grid = []
    for spacing in GRID_SPACING:
        for mult in GRID_MULT:
            rr = run_symbol(q_bars, q_cal, Params(spacing=spacing, multiplier=mult))
            grid.append({
                "spacing": spacing, "multiplier": mult,
                "is_sharpe": rr["is"]["sharpe"], "oos_sharpe": rr["oos"]["sharpe"],
                "is_return": rr["is"]["total_return"], "oos_return": rr["oos"]["total_return"],
                "min_equity": rr["path"]["min_equity"],
            })
    n_pos = sum(1 for c in grid if isinstance(c["is_sharpe"], float) and c["is_sharpe"] == c["is_sharpe"] and c["is_sharpe"] > 0)
    primary_is = next(c["is_sharpe"] for c in grid if c["spacing"] == 0.5 and c["multiplier"] == 2.0)
    better = sum(1 for c in grid if isinstance(c["is_sharpe"], float) and c["is_sharpe"] == c["is_sharpe"]
                 and isinstance(primary_is, float) and c["is_sharpe"] > primary_is)
    is_best = max(grid, key=lambda c: (
        c["is_sharpe"] if isinstance(c["is_sharpe"], float) and c["is_sharpe"] == c["is_sharpe"] else -1e99,
        -GRID_SPACING.index(c["spacing"]), -GRID_MULT.index(c["multiplier"]),
    ))

    cross = {}
    packed = {"QQQ": q}
    all_trades = []
    for c in q["campaigns"]:
        c["symbol"] = "QQQ"
        all_trades.append(c)
    for symbol in ("SPY", "IGV"):
        bars, cal = prepared[symbol]
        rr = run_symbol(bars, cal, p)
        packed[symbol] = rr
        for c in rr["campaigns"]:
            c["symbol"] = symbol
            all_trades.append(c)
        cross[symbol] = {"full": slim(rr["full"]), "is": slim(rr["is"]), "oos": slim(rr["oos"]),
                         "min_equity": rr["path"]["min_equity"], "max_notional": float(rr["notional"].max()),
                         "realized_sum": rr["realized_sum"], "n_closed": len(rr["closed"]),
                         "n_open": len(rr["open"]), "first": cal[0], "last": cal[-1]}

    def corr(a, b):
        common = [d for d in a["calendar"] if d in set(b["calendar"])]
        if len(common) < 3:
            return float("nan")
        ia = {d: i for i, d in enumerate(a["calendar"])}
        ib = {d: i for i, d in enumerate(b["calendar"])}
        return float(np.corrcoef([a["r"][ia[d]] for d in common], [b["r"][ib[d]] for d in common])[0, 1])

    pred1 = "not testable" if not q["closed"] else ("consistent" if q["n_losses"] == 0 else "not consistent")
    pred2 = "consistent" if q["path"]["max_dd"] <= -0.50 else "not consistent"
    pred3 = "consistent" if q["path"]["peak_to_trough"] > q["realized_sum"] else "not consistent"
    lines, status = acceptance(
        q["oos"], direction["p"], q["is"]["sharpe"], n_pos, costs["2.0"]["full_return"],
        cross["SPY"]["oos"]["sharpe"], q["oos_active"],
    )
    open_mtm = float(q["open"][0]["mtm_net"]) if q["open"] else 0.0
    results = {
        "rules_sha256": rules_sha, "reason": reason, "status": status,
        "n_passed": sum(1 for line in lines if line["passed"]),
        "n_failed": sum(1 for line in lines if not line["passed"]),
        "acceptance": lines,
        "seeds": {"direction": SEED_DIRECTION, "timing": SEED_TIMING, "bootstrap": SEED_BOOT},
        "primary": {"full": slim(q["full"]), "is": slim(q["is"]), "oos": slim(q["oos"])},
        "realized": {
            "n_closed": len(q["closed"]), "n_losses": q["n_losses"], "sum": q["realized_sum"],
            "n_open": len(q["open"]), "open_mtm": open_mtm,
            "max_notional": float(q["notional"].max()), "cap_refused": bool(q["refused_add"]),
            "median_bars_held": q["median_bars_held"],
            "exposure_sessions": float(np.mean(q["units"] > 0)),
        },
        "path": {k: q["path"][k] for k in ("max_dd", "peak_to_trough", "min_equity", "max_equity")},
        "milestones": milestones(q_cal, q["notional"], q["equity"]),
        "gap_gross_sum": q["gap_sum"],
        "pnl_sum": float(q["r"].sum()),
        "benchmark": bench,
        "costs": costs, "delay": delays,
        "placebo": {"direction": {k: direction[k] for k in direction if k != "null"},
                    "timing": {k: timing[k] for k in timing if k != "null"}},
        "placebo_null": {"direction": direction["null"], "timing": timing["null"]},
        "bootstrap": boot, "grid": grid,
        "grid_summary": {
            "n_cells": 15, "n_is_positive": n_pos, "primary_is_rank": better + 1,
            "is_best": {k: is_best[k] for k in ("spacing", "multiplier", "is_sharpe", "oos_sharpe")},
        },
        "cross": {"QQQ": {"full": slim(q["full"]), "is": slim(q["is"]), "oos": slim(q["oos"]),
                          "min_equity": q["path"]["min_equity"], "max_notional": float(q["notional"].max()),
                          "realized_sum": q["realized_sum"], "n_closed": len(q["closed"]), "n_open": len(q["open"])},
                  **cross},
        "correlation": {"qqq_spy": corr(q, packed["SPY"]), "qqq_igv": corr(q, packed["IGV"]),
                        "spy_igv": corr(packed["SPY"], packed["IGV"])},
        "predictions": {
            "1": {"n_closed": len(q["closed"]), "n_losses": q["n_losses"], "score": pred1},
            "2": {"max_dd": q["path"]["max_dd"], "score": pred2},
            "3": {"peak_to_trough": q["path"]["peak_to_trough"], "realized_sum": q["realized_sum"], "score": pred3},
        },
        "breakdowns": {
            "year": year_table(q_cal, q["r"], q["path"]["dd_path"], bench_cc),
            "side": side_pnl(q["r"], q["side"]),
            "units": units_table(q["campaigns"]),
            "by_reason": {
                "breakeven": {"n": len(q["closed"]), "realized": q["realized_sum"]},
                "open": {"n": len(q["open"]), "mtm": open_mtm},
            },
        },
        "coverage": {"first": q_cal[0], "last": q_cal[-1], "sessions": len(q_cal),
                     "oos_active": q["oos_active"]},
    }
    (HERE / "results.json").write_text(json.dumps(clean(results), indent=2) + "\n", encoding="utf-8", newline="\n")
    write_daily(HERE / "daily.csv", q, bench_cc, bench_oc, packed["SPY"], packed["IGV"])
    write_trades(HERE / "trades.csv", all_trades)
    write_log(rules_sha, reason, q["full"], q["is"], q["oos"])
    print(f"status {status}  OOS sharpe {q['oos']['sharpe']:.4f}  pnl {q['oos']['total_return']:.4%}  "
          f"min equity {q['path']['min_equity']:.4f}  realized {q['realized_sum']:.4f}  "
          f"closed {len(q['closed'])}  open {len(q['open'])}")


if __name__ == "__main__":
    main()
