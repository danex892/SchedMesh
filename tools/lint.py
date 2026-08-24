#!/usr/bin/env python3
"""Run formatting and clang-tidy checks for all next-generation C++ sources."""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys

from check_format import find_llvm_tool
from install_dev import msvc_environment


COMPILE_DATABASE = Path(".tools/build/compile_commands.json")
SOURCE_ROOTS = (Path("src/app"), Path("src/domain"), Path("src/export"), Path("src/import"),
                Path("src/io"), Path("src/solver"), Path("src/validation"), Path("tests"))


def translation_units() -> list[Path]:
    return sorted(path.resolve() for root in SOURCE_ROOTS for path in root.rglob("*.cpp"))


def database_files() -> set[Path]:
    entries = json.loads(COMPILE_DATABASE.read_text(encoding="utf-8"))
    return {Path(entry["file"]).resolve() for entry in entries}


def main() -> int:
    format_result = subprocess.run([sys.executable, "tools/check_format.py"], check=False)
    if format_result.returncode != 0:
        return format_result.returncode

    clang_tidy = find_llvm_tool("clang-tidy", "CLANG_TIDY")
    if clang_tidy is None:
        print("clang-tidy is missing; run `make install-dev`.", file=sys.stderr)
        return 2
    if not COMPILE_DATABASE.is_file():
        print("Compilation database is missing; run `make install-dev`.", file=sys.stderr)
        return 2

    sources = translation_units()
    missing = [source for source in sources if source not in database_files()]
    if missing:
        print("Compilation database does not cover these files:", file=sys.stderr)
        for source in missing:
            print(f"  {source.relative_to(Path.cwd())}", file=sys.stderr)
        print("Run `make install-dev` to refresh it.", file=sys.stderr)
        return 2

    result = subprocess.run(
        [clang_tidy, *map(str, sources), f"-p={COMPILE_DATABASE.parent}",
         "--config-file=.clang-tidy", "--quiet"],
        check=False,
        env=msvc_environment() if os.name == "nt" else None,
    )
    if result.returncode == 0:
        print(f"clang-tidy passed for all {len(sources)} C++ translation units.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
