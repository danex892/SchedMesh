# SchedMesh

School timetable generator for multi-class scheduling with teacher, room, and gym constraints.

SchedMesh is being rewritten as a bounded and testable C++20 application powered by
OR-Tools CP-SAT. The original C++17 generator remains available as the `SchedMesh`
target while the new `schedmesh-next` target is developed in parallel. See the
[roadmap](docs/ROADMAP.md) and [rewrite plan](docs/CPP_CP_SAT_REWRITE_PLAN.md).
The canonical schema is documented in
[project format v1](docs/formats/project-v1.md).

## What It Does

- Reads input data from CSV files.
- Builds weekly schedules for classes and teachers.
- Supports up to two sessions/shifts and then merges them.
- Tries to reduce teacher windows (gaps) via local optimization.

## Project Layout

- `include/schedmesh/` public headers for the new implementation.
- `src/app/` and `src/solver/` new C++20 application and solver code.
- `src/*.cpp` and `src/*.h` preserved legacy implementation.
- `tests/` tests for the new implementation.
- `data/` input and runtime files (`settings.conf`, CSVs, outputs).
- `build/` local CMake build directory (generated).

## Requirements

- CMake `>= 3.24`
- A C++20 compiler; MSVC 2022 or later is recommended on Windows.
- Git and network access for the first configure when dependencies are not installed.

## Build

The preset builds both the new executable and preserved legacy target. The first
configure downloads the pinned OR-Tools 9.15 and GoogleTest 1.17 sources and can
take several minutes.

```powershell
cmake --preset next
cmake --build --preset next-release -j 4
ctest --preset next-release
```

Install the pinned local development tools and configure the compilation database
(the downloads and build tree are kept in the ignored `.tools/` directory):

```powershell
make install-dev
```

Check formatting and run strict static analysis on every next-generation C++
translation unit:

```powershell
make lint
```

Apply the repository formatting rules:

```powershell
make format
```

Download the selected public XHSTT school-timetabling datasets into the ignored
`.tools/testdata` directory. The archive and every extracted XML file are verified
against pinned SHA-256 checksums:

```powershell
make download-testdata
```

Build the CLI, import all downloaded projects, validate their published schedules,
generate fresh schedules, validate every generated result, and export each feasible timetable
to XLSX:

```powershell
make benchmark
```

Four datasets must solve optimally within their individual budgets and produce `timetable.xlsx`
under their ignored benchmark output directories. Netherlands GEPRO
must reproduce its documented reference overlap and prove the corresponding hard model
infeasible. Generated projects, schedules, and statistics remain under
`.tools/benchmarks`.

The commands prefer LLVM from `.tools/`, then `CLANG_FORMAT`/`CLANG_TIDY`, `PATH`,
and the conventional Windows LLVM installation. `make install-dev` currently
supports Windows; on other systems install LLVM and Ninja with the package manager.
Dependency-backed targets are also checked during a clang-tidy-enabled build:

```powershell
cmake --preset next -DSCHEDMESH_ENABLE_CLANG_TIDY=ON
cmake --build --preset next-release
```

## Run

```powershell
.\build\next\Release\schedmesh-next.exe --version
.\build\next\Release\schedmesh-next.exe validate tests\fixtures\tiny_project.json
.\build\next\Release\schedmesh-next.exe migrate-legacy data\settings.conf outputs\project.json
.\build\next\Release\schedmesh-next.exe solve outputs\project.json outputs\schedule.json `
  --time-limit-ms 30000 --workers 1 --seed 1
.\build\next\Release\schedmesh-next.exe export-xlsx outputs\project.json `
  outputs\schedule.json outputs\timetable.xlsx
```

`migrate-legacy` reads the timetable, classroom mapping, and optional methodical-day
file referenced by the legacy configuration. It writes canonical project JSON only
after every input has been parsed and migrated successfully. Legacy room codes whose
facility identity cannot be inferred are reported as warnings for manual review.

`solve` runs deterministic fixed-staffing CP-SAT feasibility by default, independently
validates every returned schedule, and writes schedule JSON only for a valid feasible
or optimal result. See [M3 acceptance evidence](docs/M3_ACCEPTANCE.md) for current
coverage and the solved historical acceptance fixture.

M4 completes canonical room reconstruction, per-lane capacity and feature enforcement,
simultaneous subgroup room lanes, and the preserved gym sharing rule. The public Medium
fixture is defined in `tests/fixtures/medium_project.h`; see
[M4 acceptance evidence](docs/M4_ACCEPTANCE.md).

Run `make example` to migrate and solve the anonymized built-in school dataset and export its
validated schedule to a formatted XLSX workbook. Its legacy subgroup and profile semantics are
documented in [the built-in example guide](docs/LEGACY_EXAMPLE.md).

The legacy generator remains available at:

```powershell
.\build\next\Release\SchedMesh.exe data/settings.conf
```

On non-Windows platforms, run the produced binary from `build/`.

## Configuration

Main config: `data/settings.conf`

Key options:
- `days` number of study days.
- `steps` number of generation iterations per session.
- `threads` parallel generation workers.
- `maxlessons` lessons per day in one session.
- `sessions` number of shifts (`1` or `2`).
- `improve_timetable` enables post-optimization.
- `random_seed` enables time-based random seed.
- `file` input classes/teachers CSV path.
- `classrooms_file` teacher-room mapping CSV path.
- `output_file` result CSV path.

## Input Files

- `data/input5-11.csv` main matrix with classes, teachers, and hours.
- `data/classrooms.csv` teacher-to-room mapping.
- `data/methodical_days.csv` optional teacher unavailable days (if enabled in config).

## Output

Generated timetable CSVs are written to `data/` according to `output_file`.
If the target file already exists, the app auto-increments file names.

## Notes

- The algorithm uses randomized and greedy decisions plus local swaps.
- Some constraints are hard (availability, room occupancy), some are soft/heuristic (window minimization).
- For reproducibility, set `random_seed = 0`.

## License

See `LICENSE`.
