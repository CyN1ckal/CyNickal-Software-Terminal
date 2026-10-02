# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
import hashlib, subprocess, datetime, sys, pathlib
d = pathlib.Path(sys.argv[1])
b = (d / "RULES.md").read_bytes().replace(b"\r\n", b"\n")
h = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
(d / "RULES.lock").write_text(
    "sha256 " + hashlib.sha256(b).hexdigest() + "\n"
    "locked_utc " + datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds") + "\n"
    "git_head " + h + "\n",
    newline="\n",
)
print((d / "RULES.lock").read_text())
