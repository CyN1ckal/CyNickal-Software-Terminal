# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""QQQ core at 1x with the studied strategies stacked on top at equal IS risk.

Reads the net daily returns that twelve earlier studies published, sizes each
overlay to the same in-sample volatility (RULES.md), applies the scales unchanged
out of sample, and scores the acceptance table. Writes results.json and daily.csv
next to this file and appends RUNLOG.md.

The script refuses to run if RULES.md no longer matches RULES.lock.

    python research/qqq-return-stack/research/backtest.py --reason "..."
    python research/qqq-return-stack/research/backtest.py self-test
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
RESEARCH = ROOT / "research"

# ---- constants, one for one with RULES.md --------------------------------------
IS_END = "2024-06-28"
OOS_START = "2024-07-01"
N_MASTER, N_IS, N_OOS = 1235, 673, 562
SLEEVE_VOL = 0.05
Q_WEIGHT = 1.0
REBAL_COST_BPS = 1.0
FIN_RATE = 0.05
ANN = 252
SLEEVES = ("P1", "T", "C", "F", "M", "POP", "SCALE", "MART", "BOLL", "GAP", "REV", "RSI2")
S_SET = ("P1", "T", "C", "MART", "GAP", "REV", "RSI2")
K_SET = ("P1", "MART", "GAP", "RSI2")
REPORTED_IS_SHARPE = {"P1": 1.34, "T": 0.81, "C": 0.19, "F": -4.28, "M": -0.46, "POP": -0.57,
                      "SCALE": -1.53, "MART": 1.52, "BOLL": -1.98, "GAP": 1.74, "REV": 0.01, "RSI2": 0.64}
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
BUDGETS = (0.0, 0.025, 0.05, 0.075, 0.10)
FIN_RATES = (0.0, 0.025, 0.05, 0.075)
BOOT_BLOCK, BOOT_DRAWS, BOOT_SEED = 20, 2000, 20260926
TAIL_Q = 0.05
MIN_OOS_SESSIONS = 250

# Expected missing master rows (RULES.md, Data / Prior exposure)
EXPECTED_MISSING = {
    "P1": ["2021-12-31"],
    "C": ["2021-12-31"],
    "M": ["2025-06-18", "2025-07-03", "2025-08-29", "2025-11-03"],
    "POP": None,  # the first 40 master sessions, checked below
}

SOURCES = {  # key: (file, date column, return column)
    "Q": ("qqq-15m-turtle-overnight/research/daily.csv", "session", "bench_qqq_ret"),
    "P1": ("qqq-intraday-trend/research/daily.csv", "session", "p1"),
    "T": ("qqq-15m-turtle-overnight/research/daily.csv", "session", "qqq_ret"),
    "C": ("intraday-channel-trend/research/daily.csv", "session", "r_book"),
    "F": ("igv-small-account-fade/research/daily.csv", "session", "strategy"),
    "M": ("micro-futures-trend/research/daily.csv", "date", "ret"),
    "POP": ("index-opening-pop-fade/research/daily.csv", "session", "primary_net"),
    "SCALE": ("qqq-atr-scale-in/research/daily.csv", "session", "qqq_ret"),
    "MART": ("qqq-atr-martingale/research/daily.csv", "session", "pnl"),
    "BOLL": ("qqq-bollinger-adding/research/daily.csv", "session", "qqq_net"),
    "GAP": ("small-cap-gap-up-fade/research/daily.csv", "session", "ret"),
    "REV": ("low-liq-high-vol-mean-reversion/research/daily.csv", "session", "ret"),
    "RSI2": ("spy-rsi2-dip-buy/research/daily.csv", "date", "strategy_net"),
}


# ---- lock ---------------------------------------------------------------------
def rules_hash() -> str:
    return hashlib.sha256((HERE / "RULES.md").read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def check_lock() -> str:
    h = rules_hash()
    locked = dict(line.split(" ", 1) for line in (HERE / "RULES.lock").read_text().splitlines() if line.strip())
    if locked.get("sha256") != h:
        sys.exit(f"RULES.md hash {h} does not match RULES.lock {locked.get('sha256')}. Refusing to run.")
    return h


# ---- data ---------------------------------------------------------------------
def read_csv(rel: str) -> list[dict]:
    with (RESEARCH / rel).open(newline="", encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def num(v: str, what: str) -> float:
    if v == "" or not math.isfinite(float(v)):
        sys.exit(f"blank or non-finite value in {what}")
    return float(v)


def prev_equity(rows: list[dict], dcol: str) -> dict[str, float]:
    """Equity at the end of the previous row, keyed by session (1 on the first row)."""
    out, prev = {}, 1.0
    for row in rows:
        out[row[dcol]] = prev
        prev = float(row["equity"])
    return out


def held_at_close(trades: list[dict], sessions: list[str]) -> np.ndarray:
    """T's signed position at each session close: entry date <= s < exit date."""
    e = np.zeros(len(sessions))
    for t in trades:
        if t["symbol"] != "QQQ":
            continue
        side = 1.0 if t["side"] == "long" else -1.0
        a, b = t["entry_time"][:10], t["exit_time"][:10]
        for i, s in enumerate(sessions):
            if a <= s < b:
                e[i] += side
    return e


def load() -> dict:
    turtle = read_csv(SOURCES["T"][0])
    sessions = [r["session"] for r in turtle]
    idx = {s: i for i, s in enumerate(sessions)}
    n = len(sessions)
    if n != N_MASTER:
        sys.exit(f"master calendar has {n} sessions, expected {N_MASTER}")
    is_mask = np.array([s <= IS_END for s in sessions])
    if is_mask.sum() != N_IS or (~is_mask).sum() != N_OOS or sessions[int(is_mask.sum())] != OOS_START:
        sys.exit("IS/OOS session counts do not match RULES.md")

    r, live, missing, raw = {}, {}, {}, {}
    for k, (rel, dcol, col) in SOURCES.items():
        rows = raw.setdefault(rel, read_csv(rel))
        arr, lv, seen = np.zeros(n), np.zeros(n, dtype=bool), set()
        for row in rows:
            i = idx.get(row[dcol])
            if i is None:
                continue
            seen.add(row[dcol])
            if k == "M" and row["in_window"] != "1":
                continue
            arr[i] = num(row[col], f"{k}.{col} {row[dcol]}")
            lv[i] = True
        r[k], live[k] = arr, lv
        missing[k] = [s for s in sessions if s not in seen]
    for k in SOURCES:
        exp = EXPECTED_MISSING.get(k, [])
        if k == "POP":
            if missing[k] != sessions[:40] or sessions[40] != "2021-12-21":
                sys.exit(f"POP missing rows {missing[k][:3]}... do not match RULES.md")
        elif missing[k] != exp:
            sys.exit(f"{k} missing master rows {missing[k]}, expected {exp}")

    hold = {row["session"]: float(row["hold"]) for row in raw[SOURCES["P1"][0]]}
    qdiff = max(abs(hold[s] - r["Q"][idx[s]]) for s in sessions if s in hold)
    if qdiff > 1e-8:
        sys.exit(f"Q disagrees with qqq-intraday-trend hold by {qdiff}")

    # ---- 1x cost per session, x_i (RULES.md Data table; GAP/REV per RUNLOG deviation)
    x = {k: np.zeros(n) for k in SLEEVES}
    x_rev_literal = np.zeros(n)

    def add(k, s, v):
        i = idx.get(s)
        if i is not None:
            x[k][i] += v

    for t in read_csv("qqq-intraday-trend/research/trades_p1.csv"):
        add("P1", t["session"], 2e-4)
    x["T"] = np.array([float(row["qqq_ret"]) - float(row["qqq_2bp_ret"]) for row in turtle])
    for t in read_csv("intraday-channel-trend/research/trades.csv"):
        add("C", t["session"], 2e-4 / 3)
    f_rows = raw[SOURCES["F"][0]]
    f_prev = prev_equity(f_rows, "session")
    for t in read_csv("igv-small-account-fade/research/trades.csv"):
        if t["session"] in idx:
            add("F", t["session"], (float(t["gross_dollars"]) - float(t["net_dollars"])) / (25000.0 * f_prev[t["session"]]))
    for row in raw[SOURCES["M"][0]]:
        if row["date"] in idx and row["in_window"] == "1":
            add("M", row["date"], float(row["gross_ret"]) - float(row["ret"]))
    for row in raw[SOURCES["POP"][0]]:
        add("POP", row["session"], float(row["primary_gross"]) - float(row["primary_net"]))
    for t in read_csv("qqq-atr-scale-in/research/trades.csv"):
        add("SCALE", t["session"], int(t["n_units"]) / 3.0 * 2e-4)
    mart_trades = read_csv("qqq-atr-martingale/research/trades.csv")
    for t in mart_trades:
        add("MART", t["exit_session"], 2e-4 * sum(float(v) for v in t["leg_notionals"].split(";")))
    for row in raw[SOURCES["BOLL"][0]]:
        add("BOLL", row["session"], float(row["qqq_gross"]) - float(row["qqq_net"]))
    for t in read_csv("small-cap-gap-up-fade/research/trades.csv"):
        add("GAP", t["entry_session"], float(t["weight"]) * (float(t["gross_ret"]) - float(t["net_ret"])))
    rev_rows = raw[SOURCES["REV"][0]]
    rev_prev = prev_equity(rev_rows, "session")
    for t in read_csv("low-liq-high-vol-mean-reversion/research/trades.csv"):
        s = t["exit_session"]
        if s in idx:
            c = float(t["cost"]) + float(t["borrow"])
            x["REV"][idx[s]] += c / rev_prev[s]
            x_rev_literal[idx[s]] += c * float(t["equity_at_entry"]) / rev_prev[s]
    for row in raw[SOURCES["RSI2"][0]]:
        add("RSI2", row["date"], float(row["strategy_gross"]) - float(row["strategy_net"]))

    # ---- overnight exposure at each close, e_i,t
    e = {k: np.zeros(n) for k in SLEEVES}
    e["T"] = held_at_close(read_csv("qqq-15m-turtle-overnight/research/trades.csv"), sessions)
    mart_rows = raw[SOURCES["MART"][0]]
    mart_notional_max = 0.0
    for row in mart_rows:
        i = idx.get(row["session"])
        if i is not None:
            e["MART"][i] = float(row["side"]) * float(row["gross_notional"])
            mart_notional_max = max(mart_notional_max, float(row["gross_notional"]))
    for row in raw[SOURCES["RSI2"][0]]:
        i = idx.get(row["date"])
        if i is not None:
            e["RSI2"][i] = float(row["held_at_close"])

    return {"sessions": sessions, "is": is_mask, "r": r, "live": live, "x": x, "x_rev_literal": x_rev_literal,
            "e": e, "missing": missing, "q_hold_maxdiff": qdiff, "mart_notional_max": mart_notional_max}


# ---- portfolio ----------------------------------------------------------------
def scales(r: dict, live: dict, is_mask: np.ndarray, budget: float = SLEEVE_VOL) -> tuple[dict, dict]:
    sig, w = {}, {}
    for k in SLEEVES:
        m = is_mask & live[k]
        sig[k] = float(np.std(r[k][m], ddof=1) * math.sqrt(ANN))
        w[k] = budget / sig[k]
    return sig, w


def composite(rq, r, x, e, w, sleeves, k=1.0, fin_rate=FIN_RATE, x_override=None):
    """Return (r_p, L, fin, rebal) for Q at 1x plus sleeves at weights w."""
    g = Q_WEIGHT * rq.copy()
    L = np.full(len(rq), Q_WEIGHT)
    for s in sleeves:
        xs = x_override.get(s, x[s]) if x_override else x[s]
        g = g + w[s] * (r[s] - (k - 1.0) * xs)
        L = L + w[s] * e[s]
    rebal = k * REBAL_COST_BPS / 1e4 * np.abs(rq - g)
    fin = fin_rate / ANN * np.maximum(0.0, L - 1.0)
    return g - rebal - fin, L, fin, rebal


# ---- metrics ------------------------------------------------------------------
def sharpe(v: np.ndarray) -> float:
    sd = np.std(v, ddof=1)
    return float(np.mean(v) / sd * math.sqrt(ANN)) if sd > 0 else float("nan")


def max_dd(v: np.ndarray) -> float:
    eq = np.cumprod(1.0 + v)
    peak = np.maximum.accumulate(np.concatenate([[1.0], eq]))[1:]
    return float(np.min(eq / peak - 1.0))


def metrics(v: np.ndarray) -> dict:
    n = len(v)
    tot = float(np.prod(1.0 + v) - 1.0)
    sd = float(np.std(v, ddof=1))
    return {
        "sessions": n, "total_return": tot,
        "cagr": float((1.0 + tot) ** (ANN / n) - 1.0) if tot > -1 else float("nan"),
        "ann_vol": sd * math.sqrt(ANN), "sharpe": sharpe(v), "max_dd": max_dd(v),
        "t_stat": float(np.mean(v) / (sd / math.sqrt(n))) if sd > 0 else float("nan"),
    }


def windows(v: np.ndarray, is_mask: np.ndarray) -> dict:
    return {"full": metrics(v), "is": metrics(v[is_mask]), "oos": metrics(v[~is_mask])}


def boot_p(a: np.ndarray, b: np.ndarray) -> dict:
    """Paired circular block bootstrap of Sharpe(a) - Sharpe(b)."""
    rng = np.random.default_rng(BOOT_SEED)
    n = len(a)
    nb = -(-n // BOOT_BLOCK)
    d = np.empty(BOOT_DRAWS)
    for j in range(BOOT_DRAWS):
        st = rng.integers(0, n, nb)
        ix = ((st[:, None] + np.arange(BOOT_BLOCK)[None, :]) % n).ravel()[:n]
        d[j] = sharpe(a[ix]) - sharpe(b[ix])
    return {"delta": sharpe(a) - sharpe(b), "p": float((1 + np.sum(d <= 0)) / (BOOT_DRAWS + 1)),
            "lo": float(np.percentile(d, 2.5)), "hi": float(np.percentile(d, 97.5))}


def years(v: np.ndarray, sessions: list[str]) -> dict:
    out = {}
    for y in sorted({s[:4] for s in sessions}):
        m = np.array([s[:4] == y for s in sessions])
        out[y] = {"return": float(np.prod(1 + v[m]) - 1), "sharpe": sharpe(v[m])}
    return out


# ---- self-test ----------------------------------------------------------------
def self_test() -> None:
    fails = []

    def check(name, got, want, tol=1e-12):
        if not (abs(got - want) <= tol):
            fails.append(f"{name}: got {got!r}, want {want!r}")

    # 1. scale from a known series
    base = np.array([0.01, -0.01] * 50)
    rr = {k: base for k in SLEEVES}
    lv = {k: np.ones(100, dtype=bool) for k in SLEEVES}
    ism = np.ones(100, dtype=bool)
    sig, w = scales(rr, lv, ism)
    want_sig = float(np.std(base, ddof=1)) * math.sqrt(252)
    check("sigma", sig["P1"], want_sig)
    check("w", w["P1"], 0.05 / want_sig)
    # live mask restricts the window
    lv2 = dict(lv); lv2["M"] = np.array([True] * 50 + [False] * 50)
    rr2 = dict(rr); rr2["M"] = np.concatenate([base[:50], np.full(50, 0.3)])
    sig2, _ = scales(rr2, lv2, ism)
    check("sigma live", sig2["M"], float(np.std(base[:50], ddof=1)) * math.sqrt(252))

    # 2. composite: one long overnight sleeve
    rq = np.array([0.01]); r = {"A": np.array([0.005])}; x = {"A": np.array([0.0002])}
    e = {"A": np.array([1.0])}; ww = {"A": 2.0}
    rp, L, fin, reb = composite(rq, r, x, e, ww, ("A",))
    check("gross+costs", rp[0], 0.02 - 1e-4 * 0.01 - 0.05 / 252 * 2.0)
    check("L", L[0], 3.0)
    # 3. net short overnight pays no financing
    e2 = {"A": np.array([-1.0])}
    _, L2, fin2, _ = composite(rq, r, x, e2, ww, ("A",))
    check("L short", L2[0], -1.0); check("fin short", fin2[0], 0.0)
    # 4. cost multiplier: k=2 subtracts x once more, rebal doubles
    rp2, _, _, reb2 = composite(rq, r, x, {"A": np.array([0.0])}, ww, ("A",), k=2.0)
    g2 = 0.01 + 2.0 * (0.005 - 0.0002)
    check("k=2", rp2[0], g2 - 2 * 1e-4 * abs(0.01 - g2))
    # k=0 adds x back
    rp0, _, _, _ = composite(rq, r, x, {"A": np.array([0.0])}, ww, ("A",), k=0.0)
    check("k=0", rp0[0], 0.01 + 2.0 * 0.0052)
    # 5. metrics
    v = np.array([0.1, -0.5, 0.2])
    check("maxdd", max_dd(v), -0.5)
    check("maxdd first-day loss", max_dd(np.array([-0.1, 0.05])), -0.1)
    check("total", metrics(v)["total_return"], 1.1 * 0.5 * 1.2 - 1)
    # 6. identical series -> p = 1
    b = boot_p(base, base)
    check("boot p identical", b["p"], 1.0)
    # 7. T held-at-close
    tr = [{"symbol": "QQQ", "side": "long", "entry_time": "2021-10-25 12:45", "exit_time": "2021-10-28 09:30"},
          {"symbol": "QQQ", "side": "short", "entry_time": "2021-10-29 10:00", "exit_time": "2021-10-29 15:00"},
          {"symbol": "SPY", "side": "long", "entry_time": "2021-10-25 10:00", "exit_time": "2021-11-05 10:00"}]
    ss = ["2021-10-25", "2021-10-26", "2021-10-27", "2021-10-28", "2021-10-29"]
    h = held_at_close(tr, ss)
    for i, want in enumerate([1, 1, 1, 0, 0]):
        check(f"held {ss[i]}", h[i], want)
    # 8. zero-weight sleeve changes nothing
    rp3, _, _, _ = composite(rq, r, x, e, {"A": 0.0}, ("A",))
    check("zero weight", rp3[0], 0.01)

    if fails:
        sys.exit("SELF-TEST FAILED:\n  " + "\n  ".join(fails))
    print("self-test: 8 groups passed")


# ---- run ----------------------------------------------------------------------
def git(*args) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=ROOT).stdout.strip()


def run(reason: str) -> None:
    h = check_lock()
    self_test()
    d = load()
    ses, ism, r, x, e = d["sessions"], d["is"], d["r"], d["x"], d["e"]
    rq = r["Q"]
    sig, w = scales(r, d["live"], ism)

    books = {"S": S_SET, "K": K_SET, "ALL": SLEEVES}
    port = {}
    for name, sl in books.items():
        rp, L, fin, reb = composite(rq, r, x, e, w, sl)
        port[name] = {"r": rp, "L": L, "fin": fin, "rebal": reb}

    res: dict = {"rules_sha256": h, "git_head": git("rev-parse", "HEAD"), "seed": BOOT_SEED,
                 "sessions": {"full": N_MASTER, "is": N_IS, "oos": N_OOS, "first": ses[0], "last": ses[-1]},
                 "q_hold_maxdiff": d["q_hold_maxdiff"], "missing_rows": {k: len(v) for k, v in d["missing"].items()}}
    res["scales"] = {k: {"is_vol": sig[k], "w": w[k], "reported_is_sharpe": REPORTED_IS_SHARPE[k],
                         "in_S": k in S_SET, "in_K": k in K_SET} for k in SLEEVES}
    res["mart_notional_max"] = d["mart_notional_max"]
    res["mart_notional_max_scaled"] = d["mart_notional_max"] * w["MART"]

    res["metrics"] = {"Q": windows(rq, ism)}
    for name in books:
        res["metrics"][name] = windows(port[name]["r"], ism)
    res["sleeves_raw"] = {k: windows(r[k], ism) for k in SLEEVES}
    res["sleeves_scaled"] = {k: windows(w[k] * r[k], ism) for k in SLEEVES}

    # leverage and financing
    res["leverage"] = {name: {"mean_L": float(np.mean(p["L"])), "max_L": float(np.max(p["L"])),
                              "min_L": float(np.min(p["L"])), "share_L_gt_1": float(np.mean(p["L"] > 1 + 1e-12)),
                              "fin_total": float(np.sum(p["fin"])), "rebal_total": float(np.sum(p["rebal"])),
                              "fin_oos": float(np.sum(p["fin"][~ism]))}
                       for name, p in port.items()}

    # impact: add-one, leave-one-out, contribution, risk contribution
    q_m = res["metrics"]["Q"]
    add_one, add_series = {}, {}
    for k in SLEEVES:
        rp, _, _, _ = composite(rq, r, x, e, w, (k,))
        add_series[k] = rp
        m = windows(rp, ism)
        add_one[k] = {win: {"sharpe": m[win]["sharpe"], "d_sharpe": m[win]["sharpe"] - q_m[win]["sharpe"],
                            "cagr": m[win]["cagr"], "d_cagr": m[win]["cagr"] - q_m[win]["cagr"],
                            "max_dd": m[win]["max_dd"], "d_max_dd": m[win]["max_dd"] - q_m[win]["max_dd"]}
                      for win in ("full", "is", "oos")}
    res["add_one"] = add_one
    loo = {}
    for name in ("S", "ALL"):
        base_m = res["metrics"][name]
        loo[name] = {}
        for k in books[name]:
            rp, _, _, _ = composite(rq, r, x, e, w, tuple(s for s in books[name] if s != k))
            m = windows(rp, ism)
            loo[name][k] = {win: {"sharpe": m[win]["sharpe"], "d_sharpe": base_m[win]["sharpe"] - m[win]["sharpe"],
                                  "cagr": m[win]["cagr"], "d_cagr": base_m[win]["cagr"] - m[win]["cagr"]}
                            for win in ("full", "oos")}
    res["leave_one_out"] = loo  # d_* = book minus book-without-k: the sleeve's marginal effect
    res["contribution"] = {k: {"full": float(np.sum(w[k] * r[k])), "is": float(np.sum(w[k] * r[k][ism])),
                               "oos": float(np.sum(w[k] * r[k][~ism]))} for k in SLEEVES}
    res["contribution"]["Q"] = {"full": float(np.sum(rq)), "is": float(np.sum(rq[ism])), "oos": float(np.sum(rq[~ism]))}
    rc = {}
    for name in ("S", "ALL"):
        rc[name] = {}
        for win, m in (("full", np.ones(N_MASTER, dtype=bool)), ("oos", ~ism)):
            rp = port[name]["r"][m]
            var = float(np.var(rp, ddof=1))
            parts = {"Q": float(np.cov(rq[m], rp, ddof=1)[0, 1] / var)}
            for k in books[name]:
                parts[k] = float(w[k] * np.cov(r[k][m], rp, ddof=1)[0, 1] / var)
            parts["costs"] = 1.0 - sum(parts.values())
            rc[name][win] = parts
    res["risk_contribution"] = rc

    # correlations
    keys = ("Q",) + SLEEVES
    res["correlation"] = {win: {"keys": list(keys), "matrix": np.corrcoef(np.vstack([r[k][m] for k in keys])).tolist()}
                          for win, m in (("is", ism), ("oos", ~ism), ("full", np.ones(N_MASTER, dtype=bool)))}

    # tails
    ntail = int(round(TAIL_Q * N_MASTER))
    order = np.argsort(rq, kind="stable")
    worst, best = order[:ntail], order[::-1][:ntail]
    res["tails"] = {"n": ntail, "q_worst_mean": float(np.mean(rq[worst])), "q_best_mean": float(np.mean(rq[best])),
                    "worst": {k: float(np.mean(w[k] * r[k][worst])) for k in SLEEVES},
                    "best": {k: float(np.mean(w[k] * r[k][best])) for k in SLEEVES}}
    for name in books:
        ov = port[name]["r"] - rq
        res["tails"]["worst"][f"{name}_overlay"] = float(np.mean(ov[worst]))
        res["tails"]["best"][f"{name}_overlay"] = float(np.mean(ov[best]))

    # predictions
    ov_s = port["S"]["r"] - rq
    cs = np.corrcoef(np.vstack([r[k][~ism] for k in S_SET]))
    iu = np.triu_indices(len(S_SET), 1)
    res["predictions"] = {
        "1_overlay_corr_oos": float(np.corrcoef(ov_s[~ism], rq[~ism])[0, 1]),
        "1_overlay_corr_is": float(np.corrcoef(ov_s[ism], rq[ism])[0, 1]),
        "2_overlay_worst_mean": res["tails"]["worst"]["S_overlay"],
        "3_mean_pairwise_corr_oos": float(np.mean(cs[iu])),
        "3_mean_pairwise_corr_is": float(np.mean(np.corrcoef(np.vstack([r[k][ism] for k in S_SET]))[iu])),
        "4_oos_maxdd_S": res["metrics"]["S"]["oos"]["max_dd"], "4_oos_maxdd_Q": q_m["oos"]["max_dd"],
    }

    # years
    res["years"] = {name: years(v, ses) for name, v in
                    (("S", port["S"]["r"]), ("K", port["K"]["r"]), ("ALL", port["ALL"]["r"]), ("Q", rq))}

    # sweeps
    res["cost_sweep"] = {}
    for name, sl in books.items():
        res["cost_sweep"][name] = {}
        for k in COST_MULTS:
            rp, _, _, _ = composite(rq, r, x, e, w, sl, k=k)
            res["cost_sweep"][name][str(k)] = {"full_sharpe": sharpe(rp), "oos_sharpe": sharpe(rp[~ism]),
                                               "full_return": float(np.prod(1 + rp) - 1),
                                               "oos_return": float(np.prod(1 + rp[~ism]) - 1)}
    rp_lit, _, _, _ = composite(rq, r, x, e, w, S_SET, k=2.0, x_override={"REV": d["x_rev_literal"]})
    res["cost_2x_S_rev_literal_oos_sharpe"] = sharpe(rp_lit[~ism])
    res["budget_sweep"] = {}
    for name, sl in books.items():
        res["budget_sweep"][name] = {}
        for bgt in BUDGETS:
            wb = {k: bgt / sig[k] for k in SLEEVES}
            rp, _, _, _ = composite(rq, r, x, e, wb, sl)
            mm = windows(rp, ism)
            res["budget_sweep"][name][str(bgt)] = {win: {kk: mm[win][kk] for kk in ("sharpe", "cagr", "max_dd", "ann_vol")}
                                                  for win in ("full", "oos")}
    res["fin_sweep"] = {}
    for name, sl in books.items():
        res["fin_sweep"][name] = {}
        for fr in FIN_RATES:
            rp, _, _, _ = composite(rq, r, x, e, w, sl, fin_rate=fr)
            res["fin_sweep"][name][str(fr)] = {"full_sharpe": sharpe(rp), "oos_sharpe": sharpe(rp[~ism]),
                                               "oos_cagr": windows(rp, ism)["oos"]["cagr"]}

    # bootstrap
    res["bootstrap"] = {}
    for name in books:
        res["bootstrap"][name] = {"full": boot_p(port[name]["r"], rq), "oos": boot_p(port[name]["r"][~ism], rq[~ism])}

    # acceptance
    acc = {}
    for name in ("S", "K"):
        m = res["metrics"][name]
        c2 = res["cost_sweep"][name]["2.0"]["oos_sharpe"]
        lines = [
            ("1. OOS Sharpe > Q's", m["oos"]["sharpe"], q_m["oos"]["sharpe"], m["oos"]["sharpe"] > q_m["oos"]["sharpe"]),
            ("2. OOS CAGR > Q's", m["oos"]["cagr"], q_m["oos"]["cagr"], m["oos"]["cagr"] > q_m["oos"]["cagr"]),
            ("3. OOS max DD shallower than Q's", m["oos"]["max_dd"], q_m["oos"]["max_dd"], m["oos"]["max_dd"] > q_m["oos"]["max_dd"]),
            ("4. bootstrap p <= 0.05, full", res["bootstrap"][name]["full"]["p"], 0.05, res["bootstrap"][name]["full"]["p"] <= 0.05),
            ("5. OOS Sharpe at 2x cost > Q's", c2, q_m["oos"]["sharpe"], c2 > q_m["oos"]["sharpe"]),
            ("6. OOS sessions >= 250", N_OOS, MIN_OOS_SESSIONS, N_OOS >= MIN_OOS_SESSIONS),
        ]
        if not lines[5][3]:
            status = "Inconclusive"
        else:
            status = "Paper-trading candidate" if all(l[3] for l in lines) else "Rejected"
        acc[name] = {"lines": [{"line": l[0], "actual": l[1], "required": l[2], "pass": bool(l[3])} for l in lines],
                     "passed": int(sum(l[3] for l in lines)), "status": status}
    acc["S"]["line5_rev_literal"] = {"actual": res["cost_2x_S_rev_literal_oos_sharpe"],
                                     "pass": res["cost_2x_S_rev_literal_oos_sharpe"] > q_m["oos"]["sharpe"]}
    res["acceptance"] = acc

    (HERE / "results.json").write_text(json.dumps(res, indent=1))
    with (HERE / "daily.csv").open("w", newline="") as fh:
        wr = csv.writer(fh)
        wr.writerow(["session", "sample", "Q"] + list(SLEEVES) + ["S", "K", "ALL", "L_S", "L_K", "L_ALL"]
                    + [f"addone_{k}" for k in SLEEVES])
        for i, s in enumerate(ses):
            wr.writerow([s, "IS" if ism[i] else "OOS", f"{rq[i]:.10f}"] + [f"{r[k][i]:.10f}" for k in SLEEVES]
                        + [f"{port[nm]['r'][i]:.12f}" for nm in ("S", "K", "ALL")]
                        + [f"{port[nm]['L'][i]:.8f}" for nm in ("S", "K", "ALL")]
                        + [f"{add_series[k][i]:.12f}" for k in SLEEVES])

    ms = res["metrics"]
    dirty = "dirty" if git("status", "--porcelain") else "clean"
    entry = (f"\n## {dt.datetime.now(dt.timezone.utc):%Y-%m-%d %H:%M} UTC — run\n\n"
             f"- Reason: {reason}\n- Rules sha256 `{h[:12]}`, git `{res['git_head'][:7]}` ({dirty})\n"
             f"- S Sharpe full/IS/OOS {ms['S']['full']['sharpe']:.2f} / {ms['S']['is']['sharpe']:.2f} / {ms['S']['oos']['sharpe']:.2f}; "
             f"return {ms['S']['full']['total_return']:+.1%} / {ms['S']['is']['total_return']:+.1%} / {ms['S']['oos']['total_return']:+.1%}\n"
             f"- K OOS Sharpe {ms['K']['oos']['sharpe']:.2f}, ALL OOS Sharpe {ms['ALL']['oos']['sharpe']:.2f}, "
             f"Q OOS Sharpe {ms['Q']['oos']['sharpe']:.2f}\n"
             f"- Status S: {acc['S']['status']} ({acc['S']['passed']}/6); K: {acc['K']['status']} ({acc['K']['passed']}/6)\n")
    with (HERE / "RUNLOG.md").open("a", encoding="utf-8") as fh:
        fh.write(entry)
    print(entry)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", nargs="?", default="run", choices=["run", "self-test"])
    ap.add_argument("--reason", default="")
    a = ap.parse_args()
    if a.cmd == "self-test":
        self_test()
        return
    if not a.reason:
        sys.exit("--reason is required for a run")
    run(a.reason)


if __name__ == "__main__":
    main()
