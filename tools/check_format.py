#!/usr/bin/env python3
"""Check formatting for next-generation C++ sources without rewriting files."""

from __future__ import annotations

from pathlib import Path
import os
import shutil
import subprocess
import sys


SOURCE_ROOTS = (
    Path("include/schedmesh"),
    Path("src/app"),
    Path("src/domain"),
    Path("src/import"),
    Path("src/io"),
    Path("src/solver"),
    Path("src/validation"),
    Path("tests"),
)
SOURCE_SUFFIXES = {".cc", ".cpp", ".h", ".hpp"}


def find_llvm_tool(name: str, environment_variable: str) -> str | None:
    configured = os.environ.get(environment_variable)
    if configured:
        return configured
    if os.name == "nt":
        local = Path(".tools/llvm/bin") / f"{name}.exe"
        if local.is_file():
            return str(local)
    discovered = shutil.which(name)
    if discovered:
        return discovered
    if os.name == "nt":
        conventional = (
            Path(os.environ.get("ProgramFiles", "C:/Program Files"))
            / "LLVM/bin"
            / f"{name}.exe"
        )
        if conventional.is_file():
            return str(conventional)
    return None


def find_sources() -> list[Path]:
    return sorted(
        path
        for root in SOURCE_ROOTS
        if root.exists()
        for path in root.rglob("*")
        if path.suffix in SOURCE_SUFFIXES
    )


def main() -> int:
    clang_format = find_llvm_tool("clang-format", "CLANG_FORMAT")
    if clang_format is None:
        print("clang-format is required for the formatting check.", file=sys.stderr)
        return 2

    sources = find_sources()
    if not sources:
        print("No next-generation C++ sources found.", file=sys.stderr)
        return 1

    result = subprocess.run(
        [clang_format, "--dry-run", "--Werror", *map(str, sources)],
        check=False,
    )
    if result.returncode == 0:
        print(f"Formatting check passed for {len(sources)} files.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
