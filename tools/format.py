#!/usr/bin/env python3
"""Format next-generation C++ sources in place."""

from __future__ import annotations

import subprocess
import sys

from check_format import find_llvm_tool, find_sources


def main() -> int:
    clang_format = find_llvm_tool("clang-format", "CLANG_FORMAT")
    if clang_format is None:
        print("clang-format is required. Set CLANG_FORMAT if it is not on PATH.", file=sys.stderr)
        return 2

    sources = find_sources()
    if not sources:
        print("No next-generation C++ sources found.", file=sys.stderr)
        return 1

    result = subprocess.run([clang_format, "-i", *map(str, sources)], check=False)
    if result.returncode == 0:
        print(f"Formatted {len(sources)} files.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
