# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Automated pipeline linter and quantitative governance audit across all studies."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

from research.kit.files import rules_sha256
from research.kit.guard import check_verify
from research.kit.schema import validate as validate_schema


@dataclass
class StudyAuditResult:
    slug: str
    status: str = "Unknown"
    registry_status: str | None = None
    report_status: str | None = None
    passed: bool = True
    registry_ok: bool = True
    lock_ok: bool = True
    verify_ok: bool = True
    schema_ok: bool = True
    verdict_consistent: bool = True
    sanity_ok: bool = True
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)


def parse_registry(readme_path: Path) -> dict[str, dict[str, str]]:
    """Parse research/README.md markdown table into {slug: {status, idea, date}}."""
    if not readme_path.is_file():
        return {}
    lines = readme_path.read_text(encoding="utf-8").splitlines()
    registry = {}
    table_started = False
    for line in lines:
        if line.strip().startswith("| Study |"):
            table_started = True
            continue
        if table_started and line.startswith("| ["):
            match = re.match(
                r"\|\s*\[([^\]]+)\]\([^\)]+\)\s*\|\s*([^\|]+)\|\s*([^\|]+)\|\s*([^\|]+)\|",
                line,
            )
            if match:
                slug, idea, status, date_str = match.groups()
                registry[slug.strip()] = {
                    "idea": idea.strip(),
                    "status": status.strip(),
                    "date": date_str.strip(),
                }
    return registry


def parse_report_status(report_path: Path) -> str | None:
    if not report_path.is_file():
        return None
    text = report_path.read_text(encoding="utf-8", errors="ignore")
    m = re.search(r"\|\s*Status\s*\|\s*([^|\n]+)\|", text, re.IGNORECASE)
    if m:
        return m.group(1).strip()
    for line in text.splitlines()[:30]:
        if "status" in line.lower() or "verdict" in line.lower():
            return line.strip()
    return None


def audit_study(study_path: Path, registry: dict[str, dict[str, str]]) -> StudyAuditResult:
    slug = study_path.name
    res = StudyAuditResult(slug=slug)
    research_dir = study_path / "research"
    report_dir = study_path / "report"
    report_file = report_dir / "REPORT.md"

    # 1. Registry Parity
    if slug not in registry:
        res.registry_ok = False
        res.errors.append(f"Orphaned study: '{slug}' exists on disk but is missing from research/README.md")
    else:
        res.registry_status = registry[slug]["status"]

    # 2. Stub Detection
    rules_file = research_dir / "RULES.md"
    if not research_dir.is_dir() or not rules_file.is_file():
        if res.registry_status and "Not run" in res.registry_status:
            res.warnings.append(f"Unexecuted stub directory: '{slug}' (Phase 0 stopped)")
            res.status = "Not run"
            res.passed = True
            return res
        res.errors.append(f"Incomplete directory structure: '{slug}' is missing research/RULES.md")
        res.passed = False
        return res

    # Read Report Status
    res.report_status = parse_report_status(report_file)

    # 3. Lock Hash Integrity
    lock_file = research_dir / "RULES.lock"
    results_file = research_dir / "results.json"

    if not lock_file.is_file():
        res.lock_ok = False
        res.warnings.append(f"Missing RULES.lock in {slug}/research")
    else:
        actual_hash = rules_sha256(rules_file)
        lock_text = lock_file.read_text(encoding="utf-8", errors="ignore")
        lock_hash = None
        for line in lock_text.splitlines():
            line = line.strip()
            if line.startswith("sha256 "):
                lock_hash = line.split(" ", 1)[1].strip()
                break
            elif len(line) == 64 and " " not in line:
                lock_hash = line
                break
        if actual_hash != lock_hash:
            res.lock_ok = False
            res.errors.append(f"RULES.lock hash mismatch: disk={actual_hash[:12]}, lock={str(lock_hash)[:12]}")

    # 4. Verify Script AST Isolation
    verify_file = research_dir / "verify.py"
    if verify_file.is_file():
        try:
            check_verify(verify_file)
        except ValueError as ex:
            res.verify_ok = False
            res.errors.append(f"verify.py isolation breach: {ex}")
    else:
        res.warnings.append(f"verify.py missing in {slug}/research")

    # 5. Schema Validation & Results Integrity
    if results_file.is_file():
        try:
            doc = json.loads(results_file.read_text(encoding="utf-8", errors="ignore"))
            res.status = doc.get("status") or doc.get("acceptance", {}).get("status") or "Unknown"
            if doc.get("kit_schema") == 1:
                try:
                    validate_schema(doc)
                except ValueError as ex:
                    res.schema_ok = False
                    res.errors.append(f"Schema v1 validation error: {ex}")
            else:
                res.warnings.append(f"Legacy schema in results.json (kit_schema={doc.get('kit_schema')})")

            # 6. Sanity & Capital Protection Checks
            primary = doc.get("primary", {})
            full_perf = primary.get("full", {}) if isinstance(primary, dict) else {}
            max_dd = full_perf.get("max_drawdown") if isinstance(full_perf, dict) else None
            if max_dd is None and isinstance(full_perf, dict):
                max_dd = full_perf.get("max_dd")

            ending_eq = doc.get("terminal_equity")
            if ending_eq is None and isinstance(full_perf, dict):
                ending_eq = full_perf.get("ending_equity") or full_perf.get("final_equity")

            is_ruined = bool(doc.get("ruined") or (isinstance(full_perf, dict) and full_perf.get("ruined")))

            if ((max_dd is not None and max_dd <= -1.0) or (ending_eq is not None and ending_eq <= 0.0) or is_ruined) and res.status == "Inconclusive":
                reg_status = res.registry_status or ""
                if "reject" in reg_status.lower() or "void" in reg_status.lower():
                    res.warnings.append(
                        f"Legacy results.json recorded Inconclusive despite account ruin; superseded by registry reclassification '{reg_status}'"
                    )
                else:
                    res.sanity_ok = False
                    res.errors.append(f"Account ruin masked as Inconclusive (max_dd={max_dd}, final_equity={ending_eq}, ruined={is_ruined})")

        except json.JSONDecodeError:
            res.schema_ok = False
            res.errors.append(f"results.json in {slug} is corrupted JSON")
    else:
        res.warnings.append(f"results.json missing in {slug}/research")

    # 7. Verdict Consistency Check
    if res.registry_status and res.report_status:
        # Check if code status conflicts with written report
        if "void" in res.report_status.lower() and "inconclusive" in str(res.status).lower():
            res.verdict_consistent = False
            res.warnings.append(f"Verdict discrepancy: results.json says '{res.status}', but REPORT.md says '{res.report_status[:30]}'")

    res.passed = len(res.errors) == 0
    return res


def run_audit(research_dir: Path | None = None) -> list[StudyAuditResult]:
    if research_dir is None:
        research_dir = Path(__file__).resolve().parents[1]
    readme_path = research_dir / "README.md"
    registry = parse_registry(readme_path)

    studies = sorted([
        d for d in research_dir.iterdir()
        if d.is_dir() and d.name not in ("kit", "__pycache__")
    ])

    results = []
    for study in studies:
        res = audit_study(study, registry)
        results.append(res)
    return results


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit quantitative research studies.")
    parser.add_argument("slug", nargs="?", help="Specific study slug to audit")
    parser.add_argument("--json", action="store_true", help="Print audit results as JSON")
    args = parser.parse_args()

    research_dir = Path(__file__).resolve().parents[1]
    results = run_audit(research_dir)

    if args.slug:
        results = [r for r in results if r.slug == args.slug]
        if not results:
            print(f"Study '{args.slug}' not found.")
            sys.exit(2)

    if args.json:
        data = [r.__dict__ for r in results]
        print(json.dumps(data, indent=2))
        sys.exit(0 if all(r.passed for r in results) else 1)

    print("\n" + "=" * 95)
    print(f"{'STUDY':<32} | {'STATUS':<20} | {'VERIFY':<8} | {'LOCK':<8} | {'PARITY':<8} | {'HEALTH'}")
    print("=" * 95)

    n_passed = 0
    n_errors = 0
    n_warnings = 0

    for r in results:
        status_str = str(r.status)[:20]
        v_str = "OK" if r.verify_ok else "FAIL"
        l_str = "OK" if r.lock_ok else ("WARN" if r.warnings else "FAIL")
        p_str = "OK" if r.registry_ok else "ORPHAN"
        h_str = "PASS" if r.passed else "FAIL"

        print(f"{r.slug:<32} | {status_str:<20} | {v_str:<8} | {l_str:<8} | {p_str:<8} | {h_str}")
        if r.errors:
            for err in r.errors:
                print(f"   [ERROR] {err}")
            n_errors += len(r.errors)
        if r.warnings:
            for warn in r.warnings:
                print(f"   [WARN]  {warn}")
            n_warnings += len(r.warnings)

        if r.passed:
            n_passed += 1

    print("=" * 95)
    print(f"Total Studies: {len(results)} | Clean: {n_passed} | Errors: {n_errors} | Warnings: {n_warnings}\n")
    sys.exit(0 if n_errors == 0 else 1)


if __name__ == "__main__":
    main()
