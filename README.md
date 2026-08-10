# SchedMesh

School timetable generator for multi-class scheduling with teacher, room, and gym constraints.

## What It Does

- Reads input data from CSV files.
- Builds weekly schedules for classes and teachers.
- Supports up to two sessions/shifts and then merges them.
- Tries to reduce teacher windows (gaps) via local optimization.

## Project Layout

- `src/` C++ sources and headers.
- `data/` input and runtime files (`settings.conf`, CSVs, outputs).
- `build/` local CMake build directory (generated).

## Requirements

- CMake `>= 3.16`
- C++17 compiler
  - MSVC 2022+
  - GCC/Clang with C++17 support

## Build

```powershell
cd SchedMesh
cmake -S . -B build
cmake --build build -j
```

## Run

```powershell
cd SchedMesh
.\build\Debug\SchedMesh.exe
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
