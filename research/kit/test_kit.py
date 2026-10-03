# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Synthetic tests for the research kit. No market-data store and no study imports."""

from __future__ import annotations

import json
import math
import shutil
import subprocess
import sys
import tempfile
import unittest
from datetime import date, timedelta
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from research.kit import (  # noqa: E402
    NO_CROSS_MARKET,
    Criterion,
    Thresholds,
    assemble,
    assert_lock,
    asset_adjusted_direction_placebo,
    block_bootstrap,
    check_verify,
    concentration_stress_test,
    deflated_sharpe_ratio,
    direction_placebo,
    evaluate,
    performance,
    rules_sha256,
    standardized_timing_placebo,
    status_from,
    write_daily,
    write_lock,
    write_results,
)
from research.kit.metrics import cagr, max_drawdown, profit_factor, sharpe, total_return  # noqa: E402


def _costs(full_return_2x: float = 0.10) -> list[dict]:
    rows = []
    for multiple in (0, 0.5, 1, 2, 3):
        rows.append({
            "multiple": multiple,
            "cost_bp": multiple,
            "full_sharpe": 0.40,
            "oos_sharpe": 0.30,
            "full_return": full_return_2x if multiple == 2 else 0.05,
            "oos_return": 0.02,
        })
    return rows


def _grid() -> list[dict]:
    rows = []
    primary = True
    for length in (1, 2):
        for width in (10, 20):
            rows.append({
                "name": f"k={length},m={width}",
                "params": {"k": length, "m": width},
                "is_sharpe": 0.20,
                "oos_sharpe": 0.10,
                "primary": primary,
            })
            primary = False
    return rows


def _sample_paths(count: int = 10):
    sessions = [date(2024, 6, 24) + timedelta(days=i) for i in range(count)]
    net = [0.001 * ((index % 3) - 1) for index in range(count)]
    benchmark = [0.0004 * ((index % 2) * 2 - 1) for index in range(count)]
    trades = []
    for index, day in enumerate(sessions):
        trades.append({
            "session": day.isoformat(),
            "side": "long" if index % 2 == 0 else "short",
            "entry_time": f"{day.isoformat()}T14:30:00",
            "entry_price": 100.0,
            "exit_time": f"{day.isoformat()}T20:00:00",
            "exit_price": 100.1,
            "gross": 0.001,
            "net": 0.0008 if index % 2 == 0 else -0.0004,
            "exit_reason": "close",
            "note": "kept",
        })
    return sessions, net, benchmark, trades


def _document(**overrides):
    sessions, net, benchmark, trades = _sample_paths()
    pieces = [[(day, value)] for day, value in zip(sessions, net)]
    placebo = direction_placebo(sessions, pieces, seed=11, draws=50)
    payload = dict(
        label="Fixture",
        benchmark_name="Benchmark",
        model="compound",
        sessions=sessions,
        strategy_net=net,
        benchmark=benchmark,
        trades=trades,
        is_end=sessions[5],
        oos_start=sessions[6],
        seeds={"direction": 11, "timing": 12, "bootstrap": 13},
        costs=_costs(),
        delay={"full_sharpe": 0.2, "oos_sharpe": 0.1, "full_return": 0.01, "oos_return": 0.01},
        grid=_grid(),
        placebo_direction=placebo,
        placebo_timing=None,
        bootstrap=block_bootstrap(net, seed=13, draws=50),
        by_year=__import__("research.kit.metrics", fromlist=["by_year"]).by_year(sessions, net, benchmark),
        by_move_quintile=__import__("research.kit.metrics", fromlist=["move_quintiles"]).move_quintiles(
            sessions, net, benchmark
        ),
        cross_market=[{
            "symbol": "QQQ",
            "is_sharpe": 0.2,
            "oos_sharpe": 0.1,
            "full_return": 0.05,
            "full_profit_factor": 1.2,
        }],
        rules_sha256="abc123",
        git_head="deadbeef",
        git_dirty=False,
    )
    payload.update(overrides)
    return assemble(**payload)


class KitTests(unittest.TestCase):
    def test_import_does_not_load_charts(self) -> None:
        code = (
            "import research.kit, sys; "
            "assert 'research.kit.charts' not in sys.modules, sorted(sys.modules)"
        )
        proc = subprocess.run(
            [sys.executable, "-c", code],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_first_day_loss_is_inside_the_drawdown(self) -> None:
        # One session, return -0.10. Wealth = 0.90. Peak is seeded at 1,
        # so drawdown = 0.90/1 - 1 = -0.10. Without the seed the peak would
        # be 0.90 and the drawdown would be 0.
        self.assertAlmostEqual(max_drawdown([-0.10], model="compound"), -0.10)
        self.assertAlmostEqual(max_drawdown([-0.10], model="additive"), -0.10)
        # Two sessions: -10% then +20%. End wealth = 0.90 * 1.20 = 1.08.
        # Drawdown stays -0.10 because the second session makes a new high.
        series = [-0.10, 0.20]
        self.assertAlmostEqual(max_drawdown(series), -0.10)
        self.assertAlmostEqual(cagr(series), 1.08 ** (252 / 2) - 1)
        values = [0.01, -0.02, 0.015]
        mean = sum(values) / len(values)
        deviation = (sum((value - mean) ** 2 for value in values) / (len(values) - 1)) ** 0.5
        self.assertAlmostEqual(sharpe(values), mean / deviation * math.sqrt(252))

    def test_profit_factor_ignores_scratches_and_refuses_no_loser(self) -> None:
        self.assertAlmostEqual(profit_factor([0.02, -0.01, 0.0]), 2.0)
        self.assertIsNone(profit_factor([0.02, 0.0]))
        self.assertIsNone(profit_factor([]))

    def test_additive_return_is_the_sum_and_has_no_cagr(self) -> None:
        series = [0.10, -0.04, 0.02]
        self.assertAlmostEqual(total_return(series, "additive"), 0.08)
        self.assertIsNone(cagr(series, "additive"))

    def test_placebo_p_matches_the_draw_count(self) -> None:
        sessions = [date(2024, 1, 2) + timedelta(days=i) for i in range(6)]
        pieces = [
            [(sessions[0], 0.01), (sessions[1], 0.02)],
            [(sessions[2], -0.01)],
            [(sessions[3], 0.005), (sessions[4], -0.004)],
        ]
        result = direction_placebo(sessions, pieces, seed=7, draws=2000)
        samples = result["samples"]
        actual = result["actual_gross_sharpe"]
        hand = (1 + int(np.sum(samples >= actual))) / (len(samples) + 1)
        self.assertAlmostEqual(result["p"], hand)
        self.assertEqual(result["draws"], 2000)

    def test_acceptance_precedence(self) -> None:
        grid = _grid()
        costs = _costs(0.10)
        common = dict(
            oos_profit_factor=1.50,
            placebo_p=0.01,
            is_sharpe=0.50,
            grid=grid,
            costs=costs,
            cross_market=[{
                "symbol": "QQQ",
                "is_sharpe": 0.2,
                "oos_sharpe": 0.2,
                "full_return": 0.1,
                "full_profit_factor": 1.2,
            }],
        )
        short = evaluate(oos_sharpe=-1.0, oos_trades=10, **common)
        self.assertEqual(status_from(short), "Rejected")
        inconclusive_sample = evaluate(oos_sharpe=1.0, oos_trades=10, **common)
        self.assertEqual(status_from(inconclusive_sample), "Inconclusive")
        rejected = evaluate(oos_sharpe=-1.0, oos_trades=100, **common)
        self.assertEqual(status_from(rejected), "Rejected")
        passed = evaluate(oos_sharpe=1.0, oos_trades=100, **common)
        self.assertEqual(status_from(passed), "Paper-trading candidate")
        self.assertTrue(all(line["passed"] for line in passed))
        omitted = evaluate(oos_sharpe=1.0, oos_trades=100, cross_market=NO_CROSS_MARKET,
                           oos_profit_factor=1.50, placebo_p=0.01, is_sharpe=0.50,
                           grid=grid, costs=costs)
        self.assertNotIn("cross_market", [line["name"] for line in omitted])
        self.assertEqual(status_from(omitted), "Paper-trading candidate")
        self.assertEqual(status_from(passed, void_reason="tape broke"), "Void")
        extra = evaluate(
            oos_sharpe=1.0, oos_trades=100, **common,
            extra=[Criterion("oos_win_rate", "win rate >= 0.60", 0.40, ">=", 0.60)],
        )
        self.assertEqual(extra[-1]["passed"], False)
        self.assertEqual(status_from(extra), "Rejected")

    def test_write_results_rejects_bad_documents(self) -> None:
        doc = _document()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "results.json"
            nan_doc = json.loads(json.dumps(doc))
            nan_doc["primary"]["full"]["sharpe"] = float("nan")
            with self.assertRaises(ValueError):
                write_results(path, nan_doc)
            inf_doc = json.loads(json.dumps(doc))
            inf_doc["primary"]["full"]["sharpe"] = float("inf")
            with self.assertRaises(ValueError):
                write_results(path, inf_doc)
            two = json.loads(json.dumps(doc))
            two["grid"][1]["primary"] = True
            with self.assertRaises(ValueError):
                write_results(path, two)
            missing = json.loads(json.dumps(doc))
            missing["costs"] = [row for row in missing["costs"] if row["multiple"] != 2]
            with self.assertRaises(ValueError):
                write_results(path, missing)
            write_results(path, doc)
            loaded = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["kit_schema"], 1)
            self.assertEqual(loaded["status"], "Rejected")

    def test_lock_normalizes_newlines_and_refuses_a_changed_file(self) -> None:
        scratch = ROOT / "research" / "_kit_test_scratch"
        study = scratch / "research"
        try:
            study.mkdir(parents=True)
            (study / "lf.md").write_bytes(b"alpha\nbeta\n")
            (study / "cr.md").write_bytes(b"alpha\r\nbeta\r\n")
            self.assertEqual(rules_sha256(study / "lf.md"), rules_sha256(study / "cr.md"))
            (study / "RULES.md").write_bytes(b"alpha\nbeta\n")
            write_lock(study)
            (study / "RULES.md").write_bytes(b"alpha\nbeta!\n")
            with self.assertRaises(SystemExit) as caught:
                assert_lock(study)
            self.assertEqual(caught.exception.code, 1)
        finally:
            shutil.rmtree(scratch, ignore_errors=True)
        self.assertFalse(scratch.exists())

    def test_guard_blocks_sibling_imports(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "backtest.py").write_text("x = 1\n", encoding="utf-8")
            (root / "engine.py").write_text("def run():\n    return 1\n", encoding="utf-8")
            (root / "bad_import.py").write_text("import backtest\n", encoding="utf-8")
            (root / "bad_from.py").write_text("from engine import run\n", encoding="utf-8")
            (root / "ok.py").write_text(
                "import mdq\nfrom research.kit import assemble\n",
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                check_verify(root / "bad_import.py")
            with self.assertRaises(ValueError):
                check_verify(root / "bad_from.py")
            check_verify(root / "ok.py")

    def test_bootstrap_interval_is_ordered(self) -> None:
        series = [0.01, -0.005, 0.002, 0.003, -0.001] * 10
        result = block_bootstrap(series, seed=3, draws=200)
        self.assertLessEqual(result["sharpe_lo"], result["sharpe_hi"])

    def test_render_standard_writes_seven_then_eight_figures(self) -> None:
        from research.kit.charts import render_standard

        doc = _document()
        sessions, net, benchmark, _trades = _sample_paths()
        with tempfile.TemporaryDirectory() as tmp:
            study = Path(tmp) / "fake" / "research"
            study.mkdir(parents=True)
            write_results(study / "results.json", doc)
            write_daily(study / "daily.csv", sessions, net, benchmark)
            written = render_standard(study)
            names = {path.name for path in written}
            self.assertEqual(names, {
                "equity.svg", "drawdown.svg", "by_year.svg", "placebo.svg",
                "grid.svg", "costs.svg", "move_quintiles.svg",
            })
            for path in written:
                self.assertGreater(path.stat().st_size, 0)
                self.assertEqual(path.parent, study.parent / "report" / "figures")
            posthoc = {
                "rolling_sharpe": [
                    {"session": day.isoformat(), "sharpe": 0.1 * index}
                    for index, day in enumerate(sessions)
                ]
            }
            (study / "posthoc.json").write_text(json.dumps(posthoc), encoding="utf-8")
            again = render_standard(study)
            self.assertEqual(len(again), 8)
            self.assertIn("rolling_sharpe.svg", {path.name for path in again})

    def test_cli_usage_exits_1(self) -> None:
        proc = subprocess.run(
            [sys.executable, "-m", "research.kit"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 1)
        self.assertIn("usage", proc.stderr)

    def test_performance_splits_on_the_locked_dates(self) -> None:
        sessions, net, _benchmark, trades = _sample_paths()
        result = performance(sessions, net, trades, is_end=sessions[5], oos_start=sessions[6])
        self.assertEqual(result["full"]["sessions"], 10)
        self.assertEqual(result["is"]["sessions"], 6)
        self.assertEqual(result["oos"]["sessions"], 4)
        self.assertEqual(result["oos"]["trades"], 4)
        with self.assertRaises(ValueError):
            performance(sessions, net, trades, is_end=sessions[6], oos_start=sessions[6])

    def test_threshold_override_is_what_the_line_records(self) -> None:
        lines = evaluate(
            oos_sharpe=0.4,
            oos_profit_factor=1.2,
            placebo_p=0.01,
            is_sharpe=0.2,
            grid=_grid(),
            costs=_costs(),
            oos_trades=40,
            cross_market=NO_CROSS_MARKET,
            thresholds=Thresholds(oos_sharpe_min=0.3, min_oos_trades=40),
        )
        sample = next(line for line in lines if line["name"] == "oos_sample")
        self.assertIn("40", sample["required"])
        self.assertTrue(sample["passed"])
        self.assertEqual(status_from(lines), "Paper-trading candidate")

    def test_deflated_sharpe_ratio(self) -> None:
        rng = np.random.default_rng(42)
        # Insufficient sample
        self.assertIsNone(deflated_sharpe_ratio(rng.normal(0.001, 0.01, size=20)))

        # 252 sessions with positive mean
        rets = rng.normal(0.001, 0.01, size=252)
        dsr_single = deflated_sharpe_ratio(rets, trials_count=1)
        self.assertIsNotNone(dsr_single)
        self.assertTrue(0.0 <= dsr_single <= 1.0)

        # Deflated Sharpe with 100 multiple trials should be lower than single trial
        dsr_multiple = deflated_sharpe_ratio(rets, trials_count=100, trials_variance=0.25)
        self.assertIsNotNone(dsr_multiple)
        self.assertLess(dsr_multiple, dsr_single)

    def test_asset_adjusted_direction_placebo(self) -> None:
        sessions, net, benchmark, trades = _sample_paths(30)
        pieces = [[(day, val)] for day, val in zip(sessions, net)]
        result = asset_adjusted_direction_placebo(sessions, pieces, benchmark, seed=42, draws=100)
        self.assertIn("actual_gross_sharpe", result)
        self.assertIn("null_mean", result)
        self.assertIn("p", result)
        self.assertEqual(result["draws"], 100)
        self.assertTrue(0.0 <= result["p"] <= 1.0)

    def test_standardized_timing_placebo(self) -> None:
        sessions, _net, benchmark, trades = _sample_paths(30)
        durations = [1] * len(trades)
        result = standardized_timing_placebo(sessions, durations, benchmark, actual_sharpe=1.2, seed=42, draws=100)
        self.assertIn("actual_sharpe", result)
        self.assertIn("p", result)
        self.assertEqual(result["draws"], 100)
        self.assertTrue(0.0 <= result["p"] <= 1.0)

    def test_concentration_stress_test(self) -> None:
        sessions = [date(2024, 1, 1) + timedelta(days=i) for i in range(100)]
        rng = np.random.default_rng(42)
        rets = rng.normal(0.0001, 0.005, size=100)
        rets[50] = 0.15  # huge outlier spike
        res = concentration_stress_test(sessions, rets, top_fraction=0.01, max_influence_limit=0.30)
        self.assertEqual(res["k_dropped"], 1)
        self.assertGreater(res["max_session_influence"], 0.30)
        self.assertGreater(res["outlier_concentration_ratio"], 0.40)
        self.assertFalse(res["passed"])

    def test_thresholds_risk_limits(self) -> None:
        grid = _grid()
        costs = _costs(0.10)
        common = dict(
            oos_sharpe=1.50,
            oos_profit_factor=1.50,
            placebo_p=0.01,
            is_sharpe=0.50,
            grid=grid,
            costs=costs,
            oos_trades=100,
            cross_market=NO_CROSS_MARKET,
        )
        # Passing all criteria
        clean = evaluate(**common)
        self.assertEqual(status_from(clean), "Paper-trading candidate")

        # Violate gross leverage limit in status_from (default <= 4.0)
        self.assertEqual(status_from(clean, max_leverage=5.5, leverage_limit=4.0), "Rejected")

        # Violate solvency floor in status_from (min equity <= 0)
        self.assertEqual(status_from(clean, min_equity=-0.1), "Rejected")

        # Ruined account in status_from
        self.assertEqual(status_from(clean, ruined=True), "Rejected")

        # Static snapshot universe in status_from
        self.assertEqual(status_from(clean, universe_type="static_snapshot"), "Rejected (Survivorship Contaminated)")

        # Adding explicit risk limit criteria to evaluate() via extra
        extra_checks = [
            Criterion("max_gross_leverage", "gross leverage <= 4.0", 5.5, "<=", 4.0),
            Criterion("min_equity", "equity > 0", -0.1, ">", 0.0),
        ]
        with_extra = evaluate(**common, extra=extra_checks)
        self.assertEqual(status_from(with_extra), "Rejected")
        lev_line = next(line for line in with_extra if line["name"] == "max_gross_leverage")
        self.assertFalse(lev_line["passed"])

    def test_audit_linter(self) -> None:
        from research.kit.audit import parse_registry, run_audit
        research_dir = ROOT / "research"
        registry = parse_registry(research_dir / "README.md")
        self.assertIn("igv-small-account-fade", registry)
        self.assertIn("qqq-atr-martingale", registry)

        results = run_audit(research_dir)
        self.assertGreater(len(results), 25)
        # Ensure zero errors across the repository
        errors = [f"{r.slug}: {err}" for r in results for err in r.errors]
        self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
