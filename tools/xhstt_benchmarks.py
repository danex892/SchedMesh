#!/usr/bin/env python3
"""Download and run the selected public XHSTT school-timetabling benchmarks."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
TESTDATA_ROOT = REPOSITORY_ROOT / ".tools/testdata/xhstt"
OUTPUT_ROOT = REPOSITORY_ROOT / ".tools/benchmarks/xhstt"
ARCHIVE_URL = "https://www.utwente.nl/en/eemcs/dmmp/hstt/archives/ALL_INSTANCES.zip"
ARCHIVE_SHA256 = "1d55957f1bea7f95f5d1aed89b9450b72e21dd2af64e1f718fafc0109b2b6e8e"


@dataclass(frozen=True)
class Benchmark:
    key: str
    filename: str
    sha256: str
    project_id: str
    meetings: int
    time_limit_ms: int
    expected_status: str = "optimal"
    expected_import_exit: int = 0
    expected_import_diagnostic: str = ""


BENCHMARKS = (
    Benchmark(
        key="finland-high-school",
        filename="FinlandHighSchool.xml",
        sha256="d0d882186f8aec2bae6ca6ba23469ad6e49f618695245c189dc5d24e1c1c97c6",
        project_id="FI-WP-06",
        meetings=172,
        time_limit_ms=60_000,
    ),
    Benchmark(
        key="finland-secondary-school",
        filename="FinlandSecondarySchool.xml",
        sha256="a694f3dd1847823ca16b03f44d8dc7bf8e58bc80f1e74b61754f7c2b5bf10a02",
        project_id="FI-MP-06",
        meetings=280,
        time_limit_ms=60_000,
    ),
    Benchmark(
        key="netherlands-kottenpark-2003",
        filename="NetherlandsKottenpark2003.xml",
        sha256="2c80192fe41ca910a25e1bbdb45eda3b4b2bb87713c39d95c0666285b0a13592",
        project_id="NL-KP-03",
        meetings=1_156,
        time_limit_ms=60_000,
    ),
    Benchmark(
        key="netherlands-kottenpark-2005",
        filename="NetherlandsKottenpark2005.xml",
        sha256="bf271666fc9026d104b717975ab3b102ef7112a98f0070a33978d7430ac0359c",
        project_id="NL-KP-05",
        meetings=1_235,
        time_limit_ms=180_000,
    ),
    Benchmark(
        key="netherlands-gepro",
        filename="NetherlandsGEPRO.xml",
        sha256="5976f0fa42e493210ecfc392045bd6571c11f0a2a2b494f18a02b8aad4dc08c6",
        project_id="GEPRO_XHSTT-v2014",
        meetings=2_675,
        time_limit_ms=60_000,
        expected_status="infeasible",
        expected_import_exit=1,
        expected_import_diagnostic="schedule.group_overlap",
    ),
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def valid_file(path: Path, expected_hash: str) -> bool:
    return path.is_file() and sha256_file(path) == expected_hash


def download_archive(destination: Path) -> None:
    print(f"Downloading {ARCHIVE_URL}", flush=True)
    request = urllib.request.Request(
        ARCHIVE_URL, headers={"User-Agent": "SchedMesh-XHSTT-benchmarks"}
    )
    digest = hashlib.sha256()
    with urllib.request.urlopen(request) as response, destination.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)
            digest.update(chunk)
    if digest.hexdigest() != ARCHIVE_SHA256:
        destination.unlink(missing_ok=True)
        raise RuntimeError("SHA-256 mismatch for ALL_INSTANCES.zip")


def download_testdata() -> None:
    TESTDATA_ROOT.mkdir(parents=True, exist_ok=True)
    missing = [
        benchmark
        for benchmark in BENCHMARKS
        if not valid_file(TESTDATA_ROOT / benchmark.filename, benchmark.sha256)
    ]
    if not missing:
        print(f"XHSTT test data is ready in {TESTDATA_ROOT.relative_to(REPOSITORY_ROOT)}")
        return

    with tempfile.TemporaryDirectory(
        prefix="schedmesh-xhstt-", dir=TESTDATA_ROOT
    ) as temporary:
        archive_path = Path(temporary) / "ALL_INSTANCES.zip"
        download_archive(archive_path)
        with zipfile.ZipFile(archive_path) as archive:
            members: dict[str, list[str]] = {}
            for name in archive.namelist():
                members.setdefault(Path(name).name, []).append(name)
            for benchmark in missing:
                matches = members.get(benchmark.filename, [])
                if len(matches) != 1:
                    raise RuntimeError(
                        f"Archive must contain exactly one {benchmark.filename}; "
                        f"found {len(matches)}"
                    )
                contents = archive.read(matches[0])
                digest = hashlib.sha256(contents).hexdigest()
                if digest != benchmark.sha256:
                    raise RuntimeError(f"SHA-256 mismatch for {benchmark.filename}")
                temporary_file = Path(temporary) / benchmark.filename
                temporary_file.write_bytes(contents)
                os.replace(temporary_file, TESTDATA_ROOT / benchmark.filename)
                print(f"Verified {benchmark.filename}")
    print(f"XHSTT test data is ready in {TESTDATA_ROOT.relative_to(REPOSITORY_ROOT)}")


def default_executable() -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    return REPOSITORY_ROOT / f".tools/build/schedmesh-next{suffix}"


def print_output(completed: subprocess.CompletedProcess[str]) -> None:
    for stream in (completed.stdout, completed.stderr):
        for line in stream.splitlines():
            print(f"    {line}")


def run_process(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        arguments,
        cwd=REPOSITORY_ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def verify_import(
    benchmark: Benchmark,
    completed: subprocess.CompletedProcess[str],
    project_path: Path,
    reference_path: Path,
) -> list[str]:
    failures: list[str] = []
    if completed.returncode != benchmark.expected_import_exit:
        failures.append(
            f"import exited {completed.returncode}, expected {benchmark.expected_import_exit}"
        )
    if benchmark.expected_import_diagnostic:
        if benchmark.expected_import_diagnostic not in completed.stderr:
            failures.append(
                f"import did not report {benchmark.expected_import_diagnostic}"
            )
    elif "valid reference schedule" not in completed.stdout:
        failures.append("import did not validate the published reference schedule")
    if not reference_path.is_file():
        failures.append("import did not produce reference-schedule.json")
    if not project_path.is_file():
        failures.append("import did not produce project.json")
        return failures
    try:
        project = json.loads(project_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        failures.append(f"cannot read imported project: {error}")
        return failures
    if project.get("metadata", {}).get("id") != benchmark.project_id:
        failures.append("imported project id does not match the manifest")
    if len(project.get("meetings", [])) != benchmark.meetings:
        failures.append("imported meeting count does not match the manifest")
    return failures


def verify_solve(
    benchmark: Benchmark,
    completed: subprocess.CompletedProcess[str],
    schedule_path: Path,
) -> list[str]:
    failures: list[str] = []
    status_match = re.search(r"(?:^|\s)status=([a-z_]+)(?:\s|$)", completed.stdout)
    status = status_match.group(1) if status_match else "missing"
    if status != benchmark.expected_status:
        failures.append(f"solve status is {status}, expected {benchmark.expected_status}")
    expected_exit = 1 if benchmark.expected_status == "infeasible" else 0
    if completed.returncode != expected_exit:
        failures.append(f"solve exited {completed.returncode}, expected {expected_exit}")
    if benchmark.expected_status == "optimal" and not schedule_path.is_file():
        failures.append("solver did not write its independently validated schedule")
    if benchmark.expected_status == "optimal" and schedule_path.is_file():
        try:
            schedule = json.loads(schedule_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            failures.append(f"cannot read solved schedule: {error}")
        else:
            if len(schedule.get("meetings", [])) != benchmark.meetings:
                failures.append("solved schedule does not contain every meeting")
    if benchmark.expected_status == "infeasible" and schedule_path.exists():
        failures.append("infeasible benchmark unexpectedly produced a schedule")
    return failures


def verify_export(
    completed: subprocess.CompletedProcess[str], workbook_path: Path
) -> list[str]:
    failures: list[str] = []
    if completed.returncode != 0:
        failures.append(f"XLSX export exited {completed.returncode}, expected 0")
    if not workbook_path.is_file():
        failures.append("XLSX export did not produce timetable.xlsx")
        return failures
    if not zipfile.is_zipfile(workbook_path):
        failures.append("timetable.xlsx is not a valid ZIP-based XLSX workbook")
        return failures
    with zipfile.ZipFile(workbook_path) as workbook:
        entries = set(workbook.namelist())
    required_entries = {
        "[Content_Types].xml",
        "xl/workbook.xml",
        "xl/styles.xml",
        "xl/worksheets/sheet1.xml",
    }
    if missing_entries := sorted(required_entries - entries):
        failures.append(
            "timetable.xlsx is missing required entries: " + ", ".join(missing_entries)
        )
    return failures


def run_benchmarks(executable: Path, workers: int, seed: int, selected: set[str]) -> int:
    if not executable.is_file():
        print(
            f"Benchmark executable is missing: {executable}\n"
            "Run `make install-dev` and build the project first.",
            file=sys.stderr,
        )
        return 2
    missing = [
        item.filename
        for item in BENCHMARKS
        if not valid_file(TESTDATA_ROOT / item.filename, item.sha256)
    ]
    if missing:
        print("Missing or invalid XHSTT files; run `make download-testdata`.", file=sys.stderr)
        return 2

    benchmarks = [item for item in BENCHMARKS if not selected or item.key in selected]
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    passed = 0
    failures_by_benchmark: dict[str, list[str]] = {}
    for index, benchmark in enumerate(benchmarks, start=1):
        print(f"[{index}/{len(benchmarks)}] {benchmark.key}", flush=True)
        output_directory = OUTPUT_ROOT / benchmark.key
        output_directory.mkdir(parents=True, exist_ok=True)
        project_path = output_directory / "project.json"
        reference_path = output_directory / "reference-schedule.json"
        schedule_path = output_directory / "solved-schedule.json"
        workbook_path = output_directory / "timetable.xlsx"
        for output in (project_path, reference_path, schedule_path, workbook_path):
            output.unlink(missing_ok=True)

        imported = run_process(
            [
                str(executable),
                "import-xhstt",
                str(TESTDATA_ROOT / benchmark.filename),
                str(project_path),
                str(reference_path),
            ]
        )
        print_output(imported)
        failures = verify_import(benchmark, imported, project_path, reference_path)
        if failures:
            failures_by_benchmark[benchmark.key] = failures
            print("    result=FAILED (import)")
            continue

        solved = run_process(
            [
                str(executable),
                "solve",
                str(project_path),
                str(schedule_path),
                "--time-limit-ms",
                str(benchmark.time_limit_ms),
                "--workers",
                str(workers),
                "--seed",
                str(seed),
            ]
        )
        print_output(solved)
        failures = verify_solve(benchmark, solved, schedule_path)
        if failures:
            failures_by_benchmark[benchmark.key] = failures
            print("    result=FAILED (solve)")
            continue
        if schedule_path.is_file():
            exported = run_process(
                [
                    str(executable),
                    "export-xlsx",
                    str(project_path),
                    str(schedule_path),
                    str(workbook_path),
                ]
            )
            print_output(exported)
            failures = verify_export(exported, workbook_path)
            if failures:
                failures_by_benchmark[benchmark.key] = failures
                print("    result=FAILED (export)")
                continue
        else:
            print("    timetable=not-applicable (infeasible benchmark)")
        passed += 1
        print("    result=PASSED")

    print(f"XHSTT benchmark summary: {passed}/{len(benchmarks)} passed")
    for key, failures in failures_by_benchmark.items():
        for failure in failures:
            print(f"  {key}: {failure}", file=sys.stderr)
    return 0 if passed == len(benchmarks) else 1


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("download", help="download and verify the selected public datasets")
    run_parser = subparsers.add_parser("run", help="import, solve, and validate the datasets")
    run_parser.add_argument("--executable", type=Path, default=default_executable())
    run_parser.add_argument("--workers", type=int, default=8)
    run_parser.add_argument("--seed", type=int, default=1)
    run_parser.add_argument(
        "--only", action="append", choices=[item.key for item in BENCHMARKS], default=[]
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    if arguments.command == "download":
        download_testdata()
        return 0
    if arguments.workers <= 0:
        print("--workers must be positive", file=sys.stderr)
        return 2
    return run_benchmarks(
        arguments.executable.resolve(), arguments.workers, arguments.seed, set(arguments.only)
    )


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, urllib.error.URLError, zipfile.BadZipFile) as error:
        print(f"XHSTT benchmark command failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
