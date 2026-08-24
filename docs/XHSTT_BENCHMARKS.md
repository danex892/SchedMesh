# XHSTT benchmark integration

SchedMesh uses selected real-school instances from the High School Timetabling Project as
reproducible end-to-end tests. Download the selected files directly from the official archive
and verify their pinned checksums:

```powershell
make download-testdata
```

The files are stored under the ignored `.tools/testdata/xhstt` directory. Run every import,
published-schedule validation, independent solve, generated-schedule validation, and XLSX export
with:

```powershell
make benchmark
```

Every feasible benchmark writes a formatted `timetable.xlsx` beside its `project.json` and
`solved-schedule.json` under `.tools/benchmarks/xhstt/<benchmark>/`. The intentionally infeasible
Netherlands GEPRO case has no solved schedule and therefore no workbook. The benchmark runner
checks the XLSX container and its required workbook entries before reporting success.

> [!CAUTION]
> Upstream availability and research use do not by themselves establish a redistribution
> license. The opt-in downloader obtains the files from the University of Twente and keeps them
> outside Git. Importer and solver regressions use synthetic XML so normal builds and tests do
> not require the third-party datasets.

## Finland HighSchool baseline

`FinlandHighSchool.xml` (`FI-WP-06`) is the first supported instance. It represents West-Pori
High School in 2006 and contains 35 times, 18 teachers, 13 rooms, 10 classes, 172 events, and
published solutions.

Import the instance and the last published solution into ignored local output files:

```powershell
New-Item -ItemType Directory -Force .tools/benchmarks/xhstt/finland-high-school | Out-Null
.\.tools\build\schedmesh-next.exe import-xhstt `
  .tools/testdata/xhstt/FinlandHighSchool.xml `
  .tools/benchmarks/xhstt/finland-high-school/project.json `
  .tools/benchmarks/xhstt/finland-high-school/reference-schedule.json
```

Generate a fresh timetable without using the published event times:

```powershell
.\.tools\build\schedmesh-next.exe solve `
  .tools/benchmarks/xhstt/finland-high-school/project.json `
  .tools/benchmarks/xhstt/finland-high-school/solved-schedule.json `
  --time-limit-ms 60000 --workers 8 --seed 0
```

The control run on 24 August 2026 imported all 172 events, validated the published solution,
and independently found an optimal feasible schedule in 483 ms after 33,679 branches and 437
search conflicts.

## Finland SecondarySchool

`FinlandSecondarySchool.xml` (`FI-MP-06`) extends the baseline to 280 events over 35 times,
with 25 teachers, 25 rooms, and 14 classes. In addition to the baseline constraints, its hard
model includes explicit unavailable times for all three resource kinds.

```powershell
New-Item -ItemType Directory -Force .tools/benchmarks/xhstt/finland-secondary-school | Out-Null
.\.tools\build\schedmesh-next.exe import-xhstt `
  .tools/testdata/xhstt/FinlandSecondarySchool.xml `
  .tools/benchmarks/xhstt/finland-secondary-school/project.json `
  .tools/benchmarks/xhstt/finland-secondary-school/reference-schedule.json
.\.tools\build\schedmesh-next.exe solve `
  .tools/benchmarks/xhstt/finland-secondary-school/project.json `
  .tools/benchmarks/xhstt/finland-secondary-school/solved-schedule.json `
  --time-limit-ms 60000 --workers 8 --seed 0
```

The control run on 24 August 2026 imported all 280 events and 609 hard resource-time
exclusions, validated the published solution, and independently found an optimal feasible
schedule in 368 ms after 18,279 branches and no search conflicts.

## Netherlands GEPRO infeasibility baseline

`NetherlandsGEPRO.xml` (`GEPRO_XHSTT-v2014`) contains 2,675 events, 132 teachers, 44 classes,
and 846 individual students. It introduces student-level clashes, resource-only events,
event resource groups, course-specific daily occurrence limits, and 158 required linked-event
groups.

The [official dataset page](https://www.utwente.nl/en/eemcs/dmmp/hstt/datasets/Netherlands/GEPRO/)
documents an unavoidable infeasibility: student 817 belongs to two lessons in the same required
link group and must choose which one to attend. The published solution therefore has feasibility
value 1 rather than 0.

```powershell
New-Item -ItemType Directory -Force .tools/benchmarks/xhstt/netherlands-gepro | Out-Null
.\.tools\build\schedmesh-next.exe import-xhstt `
  .tools/testdata/xhstt/NetherlandsGEPRO.xml `
  .tools/benchmarks/xhstt/netherlands-gepro/project.json `
  .tools/benchmarks/xhstt/netherlands-gepro/reference-schedule.json
.\.tools\build\schedmesh-next.exe solve `
  .tools/benchmarks/xhstt/netherlands-gepro/project.json `
  .tools/benchmarks/xhstt/netherlands-gepro/solved-schedule.json `
  --time-limit-ms 60000 --workers 8 --seed 0
```

The import command intentionally reports the single `schedule.group_overlap` while still
writing the canonical project and reference schedule. The control solve on 24 August 2026
proved the hard model infeasible in 8,373 ms during presolve, with no search branches or
conflicts.

## Netherlands Kottenpark 2003

`NetherlandsKottenpark2003.xml` (`NL-KP-03`) contains 1,156 events over 38 times, with 75
teachers, 41 rooms, 18 classes, and 453 individual students. Its events introduce required
resource assignment: 744 ordinary-room roles select from 39 rooms, while 62 gym-room roles
select from two gyms. Teacher and room counts are independent; for example, a co-taught event
can require two teachers but only one room.

```powershell
New-Item -ItemType Directory -Force .tools/benchmarks/xhstt/netherlands-kottenpark-2003 | Out-Null
.\.tools\build\schedmesh-next.exe import-xhstt `
  .tools/testdata/xhstt/NetherlandsKottenpark2003.xml `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2003/project.json `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2003/reference-schedule.json
.\.tools\build\schedmesh-next.exe solve `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2003/project.json `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2003/solved-schedule.json `
  --time-limit-ms 60000 --workers 8 --seed 1
```

The control run on 24 August 2026 imported all 1,156 events, reconstructed each required room
candidate set, and validated the published solution. An independent solve found an optimal
feasible schedule in 39,508 ms after 8,396 branches and no search conflicts.

## Netherlands Kottenpark 2005

`NetherlandsKottenpark2005.xml` (`NL-KP-05`) expands the same school model to 1,235 events over
37 times, with 78 teachers, 42 rooms, 26 classes, and 498 individual students. It contains 44
required resource-availability constraints and 1,224 room lanes, including ordinary and gym
room assignments.

```powershell
New-Item -ItemType Directory -Force .tools/benchmarks/xhstt/netherlands-kottenpark-2005 | Out-Null
.\.tools\build\schedmesh-next.exe import-xhstt `
  .tools/testdata/xhstt/NetherlandsKottenpark2005.xml `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2005/project.json `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2005/reference-schedule.json
.\.tools\build\schedmesh-next.exe solve `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2005/project.json `
  .tools/benchmarks/xhstt/netherlands-kottenpark-2005/solved-schedule.json `
  --time-limit-ms 180000 --workers 8 --seed 1
```

The control run on 24 August 2026 imported all 1,235 events and validated the published
solution. The larger room-choice model exhausted a 60-second budget during presolve; with the
documented 180-second budget, an independent solve found an optimal feasible schedule in
83,347 ms after 7,862 branches and no search conflicts.

## Current mapping

- XHSTT days and times become calendar days, periods, and slots.
- Teacher, Class, and Room resources become canonical resources. A fixed event resource remains
  fixed; a required unassigned Teacher or Room role becomes a canonical candidate lane.
- Student resources become canonical student groups so their individual clashes are retained;
  event resource groups expand to their member resources.
- Courses become subjects and preserve their identity across events.
- SpreadEvents daily maxima become subject-specific occurrence limits, including maxima greater
  than one. The legacy group-wide repeated-subject policy remains available independently.
- Event durations remain meeting-defined. A subject consecutive-period value of zero means
  that its meetings may declare different positive durations.
- Required LinkEvents groups become meeting simultaneity keys enforced by the solver and schedule
  validator.
- Events may omit student, teacher, or room resources when XHSTT does not assign those roles.
- Required AssignResource and PreferResources constraints determine candidate resources by role,
  including preferences expressed through resource groups. Teacher and room lanes may be
  independent when an event does not pair them one-to-one.
- Required unavailable times become teacher and room exclusions or the complement of student
  group allowed slots. Direct resource references, resource groups, individual times, and time
  groups are supported.
- Required AssignTime, SplitEvents, PreferTimes, SpreadEvents, and AvoidClashes constraints in
  the supported Finnish instances are represented by complete assignment, meeting
  durations/domains, one course occurrence per class per day, and normal resource overlap
  constraints.
- The last published solution for the instance is imported as the reference schedule.

Soft XHSTT constraints such as idle-time and daily busy-time penalties are intentionally not
mapped by the supported hard-constraint slice. Therefore `optimal` currently means an optimal
zero-objective feasible solution in that model, not equality with the upstream XHSTT objective
value.
