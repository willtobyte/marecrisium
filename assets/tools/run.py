#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14"
# ///

import subprocess
from pathlib import Path

here = Path(__file__).resolve()
root = here.parent
for path in sorted(root.glob("*.py")):
    if path != here:
        subprocess.run(["uv", "run", path], check=True)
