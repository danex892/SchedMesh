#!/usr/bin/env python3
"""Reject files that belong only to the local private-data workflow."""

from __future__ import annotations

import fnmatch
import subprocess
import sys


FORBIDDEN_PATTERNS = (
    "data/*reconstructed*",
    "data/reference_schedule*",
    "data/teacher_roster*",
    "docs/RECONSTRUCTED_INPUTS.md",
    "docs/REFERENCE_SCHEDULE_*.md",
    "docs/TEACHER_ROSTER_ANALYSIS.md",
    "generate_reconstructed_inputs.py",
)


def tracked_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"],
        check=True,
        stdout=subprocess.PIPE,
    )
    return [path.decode("utf-8") for path in result.stdout.split(b"\0") if path]


def main() -> int:
    forbidden = sorted(
        path
        for path in tracked_files()
        if any(fnmatch.fnmatchcase(path, pattern) for pattern in FORBIDDEN_PATTERNS)
    )
    if forbidden:
        print("Private-data files are tracked in this public-safe tree:", file=sys.stderr)
        for path in forbidden:
            print(f"  {path}", file=sys.stderr)
        return 1

    print("Public-tree privacy check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
