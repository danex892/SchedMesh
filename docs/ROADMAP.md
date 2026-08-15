# SchedMesh Roadmap

## Purpose

SchedMesh is a local-first desktop application for creating, validating, optimizing, manually adjusting, and exporting school schedules with hard and soft constraints.

Its first release must prove that the historical scheduling problem has finally been solved. A generic scheduling platform, web API, or broad optimization laboratory is secondary.

## Background

The repository contains an earlier C++ timetable generator, configuration for two sessions, school-specific rules, classroom mappings, and a historical `input5-11.csv` dataset. This code and data are the acceptance origin of the new C++20 and OR-Tools CP-SAT implementation.

The detailed engineering blueprint is documented in
[`CPP_CP_SAT_REWRITE_PLAN.md`](CPP_CP_SAT_REWRITE_PLAN.md). It supersedes the earlier
Python/PySide6 implementation choice while preserving the product goals in this
roadmap. The first application surface will be a C++ CLI; a Qt 6 desktop application
is planned only after solver acceptance.

The legacy implementation should remain available as provenance and as a source of domain rules. It should not be silently deleted or rewritten out of the project's history.

## Core Architecture

```text
Historical importers ──> Canonical domain model
                               │
                    ┌──────────┴──────────┐
                    │                     │
             CP-SAT solver       Independent validator
                    │                     ▲
                    └──────> Schedule ────┘
                                  ▲
                                  │
                         Manual desktop edits
```

The validator must not reuse solver variables or trust that a solver-produced schedule is valid. It accepts a completed schedule, evaluates every hard constraint, calculates the soft-objective breakdown, and returns actionable diagnostics. The same validator runs after import, solving, and manual drag-and-drop edits.

## V1.0 Closure Contract

SchedMesh may be tagged `v1.0.0` when all of the following are true:

1. The historical CSV and configuration files migrate deterministically into a documented canonical project format.
2. Every historical hard rule, including sessions, availability, room conflicts, subject conflicts, double lessons, first/last-period restrictions, and required weekly lesson counts, has an explicit representation and validator test.
3. The OR-Tools CP-SAT solver produces a schedule for the historical acceptance dataset, and the independent validator reports zero hard violations.
4. The result includes solver status, solve time, objective value, best bound where available, and a human-readable breakdown of every soft-objective component.
5. The Qt 6 desktop application loads the historical project, starts and cancels solving without freezing, displays the schedule, supports one validated manual move with undo/redo, and exports the result.
6. Invalid manual edits are rejected or clearly highlighted with precise diagnostics from the same validator.
7. CI runs formatting, type checks, domain tests, validator tests, deterministic solver fixtures, serialization tests, and package builds.
8. A tagged release contains an installable desktop artifact, the historical acceptance fixture, validation report, screenshots, architecture documentation, and the killer demo.

## Must Have

- Preservation and documentation of the imported C++ baseline.
- A canonical, versioned domain model and project format.
- A deterministic importer for the historical data.
- An independent `ScheduleValidator`.
- Explicit tests for every hard constraint.
- OR-Tools CP-SAT solving with a time limit and cancellation.
- Objective-component reporting rather than only one total score.
- A desktop schedule view by class/group, teacher, and room.
- Validated manual movement of lessons with undo/redo.
- CSV export and one portable project format.
- A historical acceptance test that checks properties, not one exact schedule layout.

## Nice to Have

- Editors for teachers, classes/groups, rooms, courses, and constraints.
- Synthetic Tiny, Small, Medium, and Large solver benchmarks.
- XLSX and printable PDF export.
- Multiple objective presets.
- Conflict highlighting before a manual edit is committed.
- A compact infeasibility diagnostic based on assumptions or unsat cores.

## Explicitly Deferred

- FastAPI and a web frontend.
- Asynchronous web job infrastructure.
- Realtime collaboration and multi-user accounts.
- A custom solver written from scratch.
- AI-generated schedules.
- Advanced lexicographic or multi-strategy optimization.
- Broad import-format support beyond formats required by real fixtures.
- Kubernetes or SaaS deployment work.

## Milestones

### M0 — Turn the Legacy Project into an Executable Specification

- Document the historical problem and known behavior.
- Inventory every configuration option and special rule.
- Preserve the legacy source and identify the imported baseline.
- Record the historical dataset and expected acceptance properties.
- Capture representative old outputs if they can be generated reliably.

**Exit evidence:** a domain-rule inventory linked to source data and examples.

### M1 — Build the Canonical Model and Importer

- Define teachers, classes/groups, rooms, subjects/courses, lessons, slots, sessions, and constraints.
- Define stable identifiers and validation rules.
- Implement deterministic legacy CSV/config migration.
- Define a versioned portable project format.
- Report migration errors without partial or silent data loss.

**Exit evidence:** the historical dataset round-trips through the canonical model.

### M2 — Implement the Independent Validator

**Status:** complete. See [M2 acceptance evidence](M2_ACCEPTANCE.md).

- Validate teacher, class/group, and room exclusivity.
- Validate availability, room features, lesson counts, sessions, and special historical rules.
- Defer room-capacity enforcement until canonical projects can provide student-group sizes;
  legacy inputs do not contain that data and migration must not invent it.
- Calculate soft penalties independently from the solver objective.
- Return structured diagnostics with entity and time-slot references.
- Test valid, invalid, and boundary fixtures for every rule.

**Exit evidence:** hand-written invalid schedules fail for the expected reasons.

### M3 — Implement the CP-SAT Solver

- Map the canonical domain model into CP-SAT variables and constraints.
- Keep constraint construction separate and testable.
- Add weighted soft objectives and an objective breakdown.
- Add time limits, deterministic tiny fixtures, and cancellation.
- Validate every returned schedule independently before exposing it to the UI.

**Exit evidence:** the historical acceptance dataset produces a validator-approved schedule.

### M4 — Deliver the Desktop Workflow

- Build Qt 6 model/view editors and schedule views.
- Run solving outside the GUI thread or in a worker process.
- Show status, elapsed time, cancellation, and result statistics.
- Support a validated drag-and-drop move with conflict feedback.
- Add undo/redo and export.

**Exit evidence:** the killer workflow completes without developer tools.

### M5 — Package and Release V1.0

- Run the complete historical acceptance test.
- Package the reference desktop build.
- Publish validation and objective reports.
- Add screenshots, diagrams, background, and limitations.
- Tag `v1.0.0` only after the closure contract passes.

### M6 — Post-1.0 Expansion

After V1.0:

- improve general-purpose data editors;
- add solver benchmark datasets and objective presets;
- add richer export formats;
- investigate infeasibility explanations;
- expose the same core and validator through FastAPI;
- build a guided web workflow if it adds portfolio value.

## Killer Demo

Load the historical dataset, enable a deliberately conflicting rule, show an infeasible or validator-failing result with diagnostics, correct the rule, solve again, display a zero-hard-violation schedule with objective breakdown, manually move a lesson, show immediate validation, undo the move, and export the final schedule.

## Release Evidence

The V1.0 release should contain:

- the preserved legacy baseline and background;
- the canonical project schema and migration report;
- the historical acceptance dataset;
- solver statistics and independent validation report;
- an installable desktop artifact;
- CI results, screenshots, architecture diagrams, and a demo recording.
