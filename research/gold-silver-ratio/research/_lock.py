# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Write RULES.lock from RULES.md. CRLF is normalized to LF before the hash."""

import datetime
import hashlib
import pathlib
import subprocess
import sys

d = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent
b = (d / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
h = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
(d / "RULES.lock").write_text(
    "sha256 " + hashlib.sha256(b).hexdigest() + "\n"
    "locked_utc " + datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds") + "\n"
    "git_head " + h + "\n",
    encoding="utf-8",
    newline="\n",
)
print((d / "RULES.lock").read_text(encoding="utf-8"))
