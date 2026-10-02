# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Fail verify.py when it imports signal code from the same study directory."""

from __future__ import annotations

import ast
from pathlib import Path


def check_verify(path: Path) -> None:
    """Raise ValueError when `path` imports a sibling module or uses a relative import.

    `mdq` and `research.kit` are allowed when those names are not files beside verify.py.
    """
    path = Path(path)
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    siblings = {item.stem for item in path.parent.glob("*.py")}
    problems: list[str] = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                root = alias.name.split(".", 1)[0]
                if root in siblings:
                    problems.append(f"line {node.lineno}: import {alias.name}")
        elif isinstance(node, ast.ImportFrom):
            if node.level and node.level > 0:
                name = node.module or ""
                problems.append(f"line {node.lineno}: relative import {name}")
                continue
            if node.module:
                root = node.module.split(".", 1)[0]
                if root in siblings:
                    problems.append(f"line {node.lineno}: from {node.module} import")
    if problems:
        raise ValueError(
            f"{path.name} imports study code from its own directory:\n- " + "\n- ".join(problems)
        )
