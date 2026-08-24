#!/usr/bin/env python3
"""Migrate and solve the repository's anonymized legacy school example."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = REPOSITORY_ROOT / ".tools/examples/legacy-school"


def executable_path() -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    return REPOSITORY_ROOT / f".tools/build/schedmesh-next{suffix}"


def run(arguments: list[str]) -> None:
    completed = subprocess.run(arguments, cwd=REPOSITORY_ROOT, check=False)
    if completed.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {completed.returncode}: {' '.join(arguments)}")


def main() -> int:
    executable = executable_path()
    if not executable.is_file():
        print(f"Missing {executable.relative_to(REPOSITORY_ROOT)}; run make install-dev first.", file=sys.stderr)
        return 2

    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    project = OUTPUT_ROOT / "project.json"
    schedule = OUTPUT_ROOT / "schedule.json"
    workbook = OUTPUT_ROOT / "timetable.xlsx"
    run([str(executable), "migrate-legacy", "data/settings.conf", str(project)])
    run([str(executable), "validate", str(project)])
    run(
        [
            str(executable),
            "solve",
            str(project),
            str(schedule),
            "--time-limit-ms",
            "60000",
            "--workers",
            "1",
            "--seed",
            "1",
        ]
    )
    run([str(executable), "export-xlsx", str(project), str(schedule), str(workbook)])
    print(f"Project:  {project.relative_to(REPOSITORY_ROOT)}")
    print(f"Schedule: {schedule.relative_to(REPOSITORY_ROOT)}")
    print(f"Workbook: {workbook.relative_to(REPOSITORY_ROOT)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1) from error
