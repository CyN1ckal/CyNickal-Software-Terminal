# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""QQQ plus studied strategies: the pre-registered max-Sharpe portfolio.

Reads the net daily returns that five earlier studies published, fits weights on
the in-sample window only (RULES.md), applies them unchanged out of sample, and
scores the acceptance table. Writes results.json and daily.csv next to this file
and appends RUNLOG.md.

The script refuses to run if RULES.md no longer matches RULES.lock.

    python research/qqq-strategy-portfolio/research/backtest.py --reason "..."
    python research/qqq-strategy-portfolio/research/backtest.py self-test
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
GRID_STEP = 0.01
GRID_UNITS = 100                      # 1 / GRID_STEP
OVERNIGHT_CAP = 1.0
INTRADAY_CAP = 2.0
REBAL_COST_BPS = 1.0
OVERNIGHT_SLEEVES = ("Q", "T")
ELIGIBLE = ("Q", "P1", "T", "C")      # order is the tie-break order
B_SET = ("Q", "P1")
ALL_SLEEVES = ("Q", "P1", "T", "C", "F", "M")
REPORTED_IS_SHARPE = {"P1": 1.34, "T": 0.81, "C": 0.19, "F": -4.28, "M": -0.46}
COST_MULTS = (0.0, 0.5, 1.0, 2.0, 3.0)
BOOT_BLOCK, BOOT_DRAWS, BOOT_SEED = 20, 2000, 20260926
WSTAB_DRAWS, WSTAB_SEED = 500, 20260927
CRASH_Q = 0.05
MIN_OOS_SESSIONS = 250
ANN = 252

SOURCES = {
    "Q": ("qqq-15m-turtle-overnight/research/daily.csv", "session", "bench_qqq_ret"),
    "P1": ("qqq-intraday-trend/research/daily.csv", "session", "p1"),
    "T": ("qqq-15m-turtle-overnight/research/daily.csv", "session", "qqq_ret"),
    "C": ("intraday-channel-trend/research/daily.csv", "session", "r_book"),
    "F": ("igv-small-account-fade/research/daily.csv", "session", "strategy"),
    "M": ("micro-futures-trend/research/daily.csv", "date", "ret"),
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


def load() -> dict:
    """Sleeve returns and 1x per-session costs on the master calendar."""
    turtle = read_csv(SOURCES["T"][0])
    sessions = [r["session"] for r in turtle]
    idx = {s: i for i, s in enumerate(sessions)}
    n = len(sessions)
    if n != N_MASTER:
        sys.exit(f"master calendar has {n} sessions, expected {N_MASTER}")
    is_mask = np.array([s <= IS_END for s in sessions])
    if is_mask.sum() != N_IS or (~is_mask).sum() != N_OOS or sessions[int(is_mask.sum())] != OOS_START:
        sys.exit("IS/OOS session counts do not match RULES.md")

    r: dict[str, np.ndarray] = {}
    missing: dict[str, list[str]] = {}
    for k, (rel, dcol, col) in SOURCES.items():
        rows = read_csv(rel)
        arr = np.zeros(n)
        seen = set()
        for row in rows:
            i = idx.get(row[dcol])
            if i is None:
                continue
            v = row[col]
            if v == "" or not math.isfinite(float(v)):
                sys.exit(f"{k}: blank or non-finite {col} on {row[dcol]}")
            arr[i] = float(v)
            seen.add(row[dcol])
        r[k] = arr
        missing[k] = [s for s in sessions if s not in seen]
    # 2021-12-31 has no bars in the store (known calendar bug); P1 and C have no row for it.
    # RULES.md's calendar clause counts a missing row as 0. The data check named only P1;
    # C's identical gap is a recorded deviation (RUNLOG.md), not a rule change.
    for k in ("P1", "C"):
        if missing[k] != ["2021-12-31"]:
            sys.exit(f"{k} missing sessions {missing[k]}, expected only 2021-12-31")
    for k in ("Q", "T", "F"):
        if missing[k]:
            sys.exit(f"{k} missing master sessions {missing[k]}")

    hold = {row["session"]: float(row["hold"]) for row in read_csv(SOURCES["P1"][0])}
    diff = max(abs(hold[s] - r["Q"][idx[s]]) for s in sessions if s in hold)
    if diff > 1e-8:
        sys.exit(f"Q disagrees with qqq-intraday-trend hold by {diff}")

    # 1x cost per session, x_i (RULES.md primary rule step 7)
    x = {k: np.zeros(n) for k in ALL_SLEEVES}
    for row in read_csv("qqq-intraday-trend/research/trades_p1.csv"):
        i = idx.get(row["session"])
        if i is not None:
            x["P1"][i] += 2e-4
    x["T"] = np.array([float(row["qqq_ret"]) - float(row["qqq_2bp_ret"]) for row in turtle])
    for row in read_csv("intraday-channel-trend/research/trades.csv"):
        i = idx.get(row["session"])
        if i is not None:
            x["C"][i] += 2e-4 / 3
    return {"sessions": sessions, "is": is_mask, "r": r, "x": x, "missing": missing, "q_hold_maxdiff": diff}


# ---- statistics ---------------------------------------------------------------
def sharpe(v: np.ndarray) -> float:
    sd = v.std(ddof=1)
    return float(v.mean() / sd * math.sqrt(ANN)) if sd > 0 else float("nan")


def max_dd(v: np.ndarray) -> float:
    eq = np.cumprod(1 + v)
    return float((eq / np.maximum.accumulate(np.maximum(eq, 1.0)) - 1).min())


def metrics(v: np.ndarray) -> dict:
    tr = float(np.prod(1 + v) - 1)
    sd = v.std(ddof=1)
    return {"sessions": int(len(v)), "total_return": tr,
            "cagr": float((1 + tr) ** (ANN / len(v)) - 1),
            "ann_vol": float(sd * math.sqrt(ANN)), "sharpe": sharpe(v), "max_dd": max_dd(v),
            "t_stat": float(v.mean() / (sd / math.sqrt(len(v)))) if sd > 0 else float("nan")}


def windows(v: np.ndarray, is_mask: np.ndarray) -> dict:
    return {"full": metrics(v), "IS": metrics(v[is_mask]), "OOS": metrics(v[~is_mask])}


# ---- construction -------------------------------------------------------------
def simplex_grid(k: int, units: int = GRID_UNITS) -> np.ndarray:
    """All d in {0, 1/units, ...}^k with sum 1, in lexicographic order."""
    if k == 1:
        return np.array([[1.0]])
    rows = []

    def rec(prefix: list[int], left: int, slots: int):
        if slots == 1:
            rows.append(prefix + [left])
            return
        for a in range(left + 1):
            rec(prefix + [a], left - a, slots - 1)

    rec([], units, k)
    return np.array(rows, dtype=float) / units


def grid_sharpes(R: np.ndarray, D: np.ndarray) -> np.ndarray:
    mu = R.mean(axis=0)
    cov = np.cov(R, rowvar=False, ddof=1).reshape(R.shape[1], R.shape[1])
    var = np.einsum("ij,jk,ik->i", D, cov, D)
    with np.errstate(divide="ignore", invalid="ignore"):
        s = (D @ mu) / np.sqrt(var) * math.sqrt(ANN)
    return np.where(var > 0, s, -np.inf)


def best_direction(R: np.ndarray, D: np.ndarray) -> np.ndarray:
    # Ties (within float rounding) go to the first maximizer in lexicographic order.
    s = grid_sharpes(R, D)
    return D[int(np.flatnonzero(s >= s.max() - 1e-12)[0])]


def scale(d: np.ndarray, names: tuple[str, ...], R_is: np.ndarray, vol_cap: float) -> tuple[float, str]:
    on = sum(d[i] for i, k in enumerate(names) if k in OVERNIGHT_SLEEVES)
    caps = {"overnight": OVERNIGHT_CAP / on if on > 0 else math.inf,
            "intraday": INTRADAY_CAP / d.sum()}
    vol = float((R_is @ d).std(ddof=1) * math.sqrt(ANN))
    caps["vol"] = vol_cap / vol if vol > 0 else math.inf
    binding = min(caps, key=caps.get)
    return caps[binding], binding


def sleeves_at(data: dict, names: tuple[str, ...], k: float) -> np.ndarray:
    return np.column_stack([data["r"][n] - (k - 1) * data["x"][n] for n in names])


def portfolio(R: np.ndarray, w: np.ndarray, names: tuple[str, ...], k: float = 1.0) -> np.ndarray:
    gross = R @ w
    rebal = np.zeros(len(gross))
    for i, n in enumerate(names):
        if n in OVERNIGHT_SLEEVES and w[i] > 0:
            rebal += w[i] * np.abs(R[:, i] - gross)
    return gross - k * REBAL_COST_BPS / 1e4 * rebal


# ---- bootstrap ----------------------------------------------------------------
def block_indices(rng: np.random.Generator, n: int, block: int = BOOT_BLOCK) -> np.ndarray:
    starts = rng.integers(0, n, size=math.ceil(n / block))
    return ((starts[:, None] + np.arange(block)[None, :]) % n).ravel()[:n]


def paired_boot(a: np.ndarray, b: np.ndarray, seed: int) -> dict:
    rng = np.random.default_rng(seed)
    deltas = np.empty(BOOT_DRAWS)
    sa = np.empty(BOOT_DRAWS)
    for j in range(BOOT_DRAWS):
        ix = block_indices(rng, len(a))
        sa[j] = sharpe(a[ix])
        deltas[j] = sa[j] - sharpe(b[ix])
    return {"actual_delta": sharpe(a) - sharpe(b),
            "p_delta_le_0": float((1 + (deltas <= 0).sum()) / (BOOT_DRAWS + 1)),
            "delta_ci95": [float(np.percentile(deltas, 2.5)), float(np.percentile(deltas, 97.5))],
            "sharpe_ci95": [float(np.percentile(sa, 2.5)), float(np.percentile(sa, 97.5))]}


def weight_stability(R_is: np.ndarray, D: np.ndarray, names: tuple[str, ...]) -> dict:
    rng = np.random.default_rng(WSTAB_SEED)
    draws = np.array([best_direction(R_is[block_indices(rng, len(R_is))], D) for _ in range(WSTAB_DRAWS)])
    return {n: {"mean": float(draws[:, i].mean()), "p5": float(np.percentile(draws[:, i], 5)),
                "p95": float(np.percentile(draws[:, i], 95)), "share_positive": float((draws[:, i] > 0).mean())}
            for i, n in enumerate(names)}


# ---- self-test ----------------------------------------------------------------
def self_test() -> None:
    fails = []

    def check(name: str, cond: bool) -> None:
        if not cond:
            fails.append(name)

    D2 = simplex_grid(2)
    check("grid2 size", D2.shape == (101, 2) and np.allclose(D2.sum(1), 1))
    D4 = simplex_grid(4)
    check("grid4 size", D4.shape == (176851, 4) and np.allclose(D4.sum(1), 1))
    check("grid4 lexicographic", (D4[0] == [0, 0, 0, 1]).all() and (D4[-1] == [1, 0, 0, 0]).all())

    # Two exactly uncorrelated series with means m and sds s: the tangency direction is
    # proportional to m / s^2. Build them from +/- patterns so the sample moments are exact.
    base = np.array([1, -1, 1, -1] * 50, float)
    other = np.array([1, 1, -1, -1] * 50, float)
    R = np.column_stack([0.001 + 0.02 * base, 0.0005 + 0.005 * other])
    tang = np.array([0.001 / 0.02**2, 0.0005 / 0.005**2])
    tang /= tang.sum()
    d = best_direction(R, D2)
    check("tangency within one step", np.abs(d - tang).max() <= GRID_STEP + 1e-12)

    # tie-break: two identical series, every mix has the same Sharpe; first in lex order wins
    Rt = np.column_stack([0.001 + 0.01 * base, 0.001 + 0.01 * base])
    check("tie-break first", (best_direction(Rt, D2) == [0, 1]).all())

    # scale caps
    names = ("Q", "P1")
    Rs = np.column_stack([0.02 * base, 0.001 * other])
    s, b = scale(np.array([1.0, 0.0]), names, Rs, vol_cap=1.0)
    check("overnight cap", abs(s - 1.0) < 1e-12 and b == "overnight")
    s, b = scale(np.array([0.2, 0.8]), names, Rs, vol_cap=10.0)
    check("intraday cap", abs(s - 2.0) < 1e-12 and b == "intraday")
    volq = float(Rs[:, 0].std(ddof=1) * math.sqrt(ANN))
    d = np.array([0.5, 0.5])
    s, b = scale(d, names, Rs, vol_cap=volq * 0.1)
    check("vol cap", b == "vol" and abs((Rs @ (s * d)).std(ddof=1) * math.sqrt(ANN) - volq * 0.1) < 1e-12)

    # rebalance cost: pure Q pays nothing; Q + P1 pays on |rQ - rp|
    rq = np.array([0.01, -0.02]); rp1 = np.array([0.003, 0.0])
    R2 = np.column_stack([rq, rp1])
    check("pure Q no rebal", np.allclose(portfolio(R2, np.array([1.0, 0.0]), names), rq))
    w = np.array([1.0, 1.0])
    gross = rq + rp1
    exp = gross - 1e-4 * np.abs(rq - gross)
    check("rebal formula", np.allclose(portfolio(R2, w, names), exp))
    check("rebal scales with k", np.allclose(portfolio(R2, w, names, k=2), gross - 2e-4 * np.abs(rq - gross)))

    # cost multiplier on sleeves
    fake = {"r": {"P1": np.array([0.01, 0.0])}, "x": {"P1": np.array([2e-4, 0.0])}}
    check("k=0 adds back cost", np.allclose(sleeves_at(fake, ("P1",), 0.0)[:, 0], [0.0102, 0.0]))
    check("k=3 removes 2x more", np.allclose(sleeves_at(fake, ("P1",), 3.0)[:, 0], [0.0096, 0.0]))

    # metrics
    v = np.array([0.1, -0.5, 0.2])
    check("max_dd", abs(max_dd(v) - (-0.5)) < 1e-12)
    check("total return", abs(metrics(v)["total_return"] - (1.1 * 0.5 * 1.2 - 1)) < 1e-12)

    if fails:
        sys.exit("SELF-TEST FAILED: " + ", ".join(fails))
    print("self-test passed (15 checks)")


# ---- run ----------------------------------------------------------------------
def git_state() -> tuple[str, bool]:
    head = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True, cwd=ROOT).stdout.strip())
    return head, dirty


def build(data: dict, names: tuple[str, ...], D: np.ndarray, vol_cap: float) -> dict:
    is_m = data["is"]
    R1 = sleeves_at(data, names, 1.0)
    d = best_direction(R1[is_m], D)
    s, binding = scale(d, names, R1[is_m], vol_cap)
    w = s * d
    return {"names": list(names), "d": d.tolist(), "scale": s, "binding_cap": binding, "w": w.tolist(),
            "is_sharpe_of_direction": sharpe(R1[is_m] @ d)}


def main(reason: str) -> None:
    h = check_lock()
    self_test()
    data = load()
    is_m = data["is"]
    sessions = data["sessions"]

    R_all = sleeves_at(data, ALL_SLEEVES, 1.0)
    vol_cap = float(data["r"]["Q"][is_m].std(ddof=1) * math.sqrt(ANN))

    A = build(data, ELIGIBLE, simplex_grid(len(ELIGIBLE)), vol_cap)
    B = build(data, B_SET, simplex_grid(len(B_SET)), vol_cap)
    ew_d = np.full(len(ELIGIBLE), 1 / len(ELIGIBLE))
    ew_s, ew_bind = scale(ew_d, ELIGIBLE, sleeves_at(data, ELIGIBLE, 1.0)[is_m], vol_cap)
    EW = {"names": list(ELIGIBLE), "d": ew_d.tolist(), "scale": ew_s, "binding_cap": ew_bind, "w": (ew_s * ew_d).tolist()}
    N = {"names": list(B_SET), "w": [1.0, 1.0]}
    Qp = {"names": ["Q"], "w": [1.0]}
    ports = {"A": A, "B": B, "N": N, "EW": EW, "Q": Qp}

    def series(p: dict, k: float = 1.0) -> np.ndarray:
        names = tuple(p["names"])
        return portfolio(sleeves_at(data, names, k), np.array(p["w"]), names, k)

    ret = {k: series(p) for k, p in ports.items()}

    res: dict = {"rules_sha256": h, "seeds": {"bootstrap": BOOT_SEED, "weight_stability": WSTAB_SEED},
                 "samples": {"IS": [sessions[0], IS_END], "OOS": [OOS_START, sessions[-1]],
                             "sessions": {"full": N_MASTER, "IS": N_IS, "OOS": N_OOS}},
                 "data_checks": {"missing_rows_zeroed": {k: v for k, v in data["missing"].items() if v},
                                 "q_vs_hold_maxdiff": data["q_hold_maxdiff"]},
                 "eligibility": {k: {"reported_is_sharpe": v, "eligible": v > 0} for k, v in REPORTED_IS_SHARPE.items()},
                 "vol_cap": vol_cap, "portfolios": ports}
    res["metrics"] = {k: windows(v, is_m) for k, v in ret.items()}
    res["sleeve_metrics"] = {n: windows(R_all[:, i], is_m) for i, n in enumerate(ALL_SLEEVES)}
    res["sleeve_is_sharpe_this_window"] = {n: sharpe(R_all[is_m, i]) for i, n in enumerate(ALL_SLEEVES)}
    res["corr"] = {w: np.corrcoef(R_all[m], rowvar=False).round(4).tolist()
                   for w, m in (("IS", is_m), ("OOS", ~is_m), ("full", np.ones(len(is_m), bool)))}
    res["corr_names"] = list(ALL_SLEEVES)

    years = sorted({s[:4] for s in sessions})
    res["by_year"] = {y: {k: {"return": float(np.prod(1 + ret[k][m]) - 1), "sharpe": sharpe(ret[k][m])}
                          for k in ("A", "B", "N", "Q")}
                      for y in years for m in [np.array([s[:4] == y for s in sessions])]}

    res["cost_sweep"] = {k: {str(c): {"full_sharpe": sharpe(series(ports[k], c)),
                                      "oos_sharpe": sharpe(series(ports[k], c)[~is_m]),
                                      "full_return": float(np.prod(1 + series(ports[k], c)) - 1)}
                             for c in COST_MULTS} for k in ("A", "B", "N")}
    res["cost_sweep"]["Q"] = {"any": {"full_sharpe": sharpe(ret["Q"]), "oos_sharpe": sharpe(ret["Q"][~is_m])}}

    res["bootstrap"] = {f"{k}_minus_Q": {"full": paired_boot(ret[k], ret["Q"], BOOT_SEED),
                                          "OOS": paired_boot(ret[k][~is_m], ret["Q"][~is_m], BOOT_SEED)}
                        for k in ("A", "B")}

    res["weight_stability"] = {
        "A": weight_stability(sleeves_at(data, ELIGIBLE, 1.0)[is_m], simplex_grid(len(ELIGIBLE)), ELIGIBLE),
        "B": weight_stability(sleeves_at(data, B_SET, 1.0)[is_m], simplex_grid(len(B_SET)), B_SET)}

    q = data["r"]["Q"]
    crash = q <= np.quantile(q, CRASH_Q)
    res["crash_days"] = {"n": int(crash.sum()), "q_avg": float(q[crash].mean()),
                         "sleeve_avg": {n: float(R_all[crash, i].mean()) for i, n in enumerate(ALL_SLEEVES)}}
    for k in ("A", "B"):
        names = tuple(ports[k]["names"])
        w = np.array(ports[k]["w"])
        nonq = sleeves_at(data, names, 1.0)[:, 1:] @ w[1:]
        res["crash_days"][f"{k}_nonQ_avg"] = float(nonq[crash].mean())
        res["crash_days"][f"{k}_avg"] = float(ret[k][crash].mean())

    ci = ALL_SLEEVES.index
    res["predictions"] = {
        "1_p1_q_corr_oos": res["corr"]["OOS"][ci("Q")][ci("P1")],
        "2_A_nonQ_on_crash_days": res["crash_days"]["A_nonQ_avg"],
        "3_A_oos_dd_vs_Q": [res["metrics"]["A"]["OOS"]["max_dd"], res["metrics"]["Q"]["OOS"]["max_dd"]],
        "4_A_oos_vs_is_sharpe": [res["metrics"]["A"]["OOS"]["sharpe"], res["metrics"]["A"]["IS"]["sharpe"]]}

    def accept(k: str) -> dict:
        m, mq = res["metrics"][k]["OOS"], res["metrics"]["Q"]["OOS"]
        lines = {
            "1_oos_sharpe_gt_Q": {"required": f"> {mq['sharpe']:.4f}", "actual": m["sharpe"], "pass": m["sharpe"] > mq["sharpe"]},
            "2_oos_dd_shallower": {"required": f"> {mq['max_dd']:.4f}", "actual": m["max_dd"], "pass": m["max_dd"] > mq["max_dd"]},
            "3_boot_p_full": {"required": "<= 0.05", "actual": res["bootstrap"][f"{k}_minus_Q"]["full"]["p_delta_le_0"],
                              "pass": res["bootstrap"][f"{k}_minus_Q"]["full"]["p_delta_le_0"] <= 0.05},
            "4_oos_sharpe_gt_Q_at_2x": {"required": f"> {mq['sharpe']:.4f}", "actual": res["cost_sweep"][k]["2.0"]["oos_sharpe"],
                                        "pass": res["cost_sweep"][k]["2.0"]["oos_sharpe"] > mq["sharpe"]},
            "5_min_oos_sessions": {"required": f">= {MIN_OOS_SESSIONS}", "actual": N_OOS, "pass": N_OOS >= MIN_OOS_SESSIONS}}
        if not lines["5_min_oos_sessions"]["pass"]:
            status = "Inconclusive"
        else:
            status = "Paper-trading candidate" if all(v["pass"] for v in lines.values()) else "Rejected"
        return {"lines": lines, "status": status}

    res["acceptance"] = {"A": accept("A"), "B": accept("B")}

    (HERE / "results.json").write_text(json.dumps(res, indent=2), encoding="utf-8")
    with (HERE / "daily.csv").open("w", newline="", encoding="utf-8") as fh:
        wr = csv.writer(fh)
        wr.writerow(["session", "sample", *ALL_SLEEVES, "A", "B", "N", "EW"])
        for i, s in enumerate(sessions):
            wr.writerow([s, "IS" if is_m[i] else "OOS", *(f"{R_all[i, j]:.10f}" for j in range(len(ALL_SLEEVES))),
                         *(f"{ret[k][i]:.10f}" for k in ("A", "B", "N", "EW"))])

    head, dirty = git_state()
    mA = res["metrics"]["A"]
    entry = (f"\n## {dt.datetime.now(dt.timezone.utc).isoformat(timespec='seconds')}\n"
             f"- reason: {reason}\n- rules_sha256: {h}\n- git_head: {head}\n- git_dirty: {str(dirty).lower()}\n"
             f"- A weights: " + ", ".join(f"{n} {w:.4f}" for n, w in zip(A["names"], A["w"])) + f" (binding: {A['binding_cap']})\n"
             f"- A full: sharpe {mA['full']['sharpe']:.4f}, return {mA['full']['total_return']*100:.4f}%\n"
             f"- A IS: sharpe {mA['IS']['sharpe']:.4f}, return {mA['IS']['total_return']*100:.4f}%\n"
             f"- A OOS: sharpe {mA['OOS']['sharpe']:.4f}, return {mA['OOS']['total_return']*100:.4f}%\n"
             f"- status from acceptance table: A {res['acceptance']['A']['status']}; B {res['acceptance']['B']['status']}\n")
    log = HERE / "RUNLOG.md"
    if not log.exists():
        log.write_text("# Run log\n\nAppend-only. One entry per run that computes portfolio returns.\n", encoding="utf-8")
    with log.open("a", encoding="utf-8") as fh:
        fh.write(entry)
    print(entry)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", nargs="?", default="run", choices=["run", "self-test"])
    ap.add_argument("--reason", default="initial pre-registered run")
    a = ap.parse_args()
    if a.mode == "self-test":
        check_lock()
        self_test()
    else:
        main(a.reason)
