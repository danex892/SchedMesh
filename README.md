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
.\build\next\Release\schedmesh-next.exe solve-smoke
.\build\next\Release\schedmesh-next.exe validate tests\fixtures\tiny_project.json
```

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
