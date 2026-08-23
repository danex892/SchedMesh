# SchedMesh C++20 and CP-SAT Rewrite Plan

## 1. Document Status

This document is the implementation blueprint for replacing the legacy randomized
timetable generator with a modern, bounded, testable C++20 application powered by
Google OR-Tools CP-SAT.

It is intentionally more detailed than `ROADMAP.md`. The roadmap defines product
outcomes; this document defines the engineering design, migration order, solver
model, quality gates, and acceptance evidence needed to reach those outcomes.

The following decisions are authoritative unless superseded by a later Architecture
Decision Record (ADR):

1. The production core will be C++20.
2. Google OR-Tools CP-SAT will be the primary constraint solver.
3. The first supported application surface will be a command-line interface.
4. A future desktop application should use Qt 6 and call the same C++ application
   services; it must not contain a second scheduling implementation.
5. The legacy generator will be preserved as historical provenance but will not be
   incrementally refactored into the new solver.
6. The canonical domain model and independent validator will not depend on OR-Tools.
7. Every solve will have explicit time and cancellation limits. An unbounded search
   is considered a correctness defect.
8. Personally identifiable staff data will remain outside public branches and public
   fixtures.

## 2. Why a Rewrite Is Required

The legacy program is not merely slow. Its architecture makes reliable improvement
unsafe and expensive:

- mutable global solver state couples parsing, generation, optimization, and output;
- fixed-size arrays encode undocumented limits for days, slots, subjects, rooms,
  teachers, and classes;
- sentinel indices such as teacher `99` replace explicit optional values;
- owning raw pointers and shared `Lesson*` objects make copying and rollback fragile;
- first and second shifts are solved separately and merged after the fact;
- failed randomized attempts are not bounded by the configured `steps` value;
- the search performs shallow local swaps instead of systematic backtracking;
- constraint checks are spread across mutation functions and cannot be tested in
  isolation;
- the output is trusted without an independent validation pass;
- infeasibility is indistinguishable from bad luck or an infinite search;
- the CSV parser does not implement normal CSV quoting rules;
- domain facts, guessed data, runtime state, and presentation state share the same
  structures.

The reconstructed acceptance dataset demonstrated these limitations directly. The
legacy generator executed thousands of attempts, repeatedly reached the final day,
and failed to complete the first shift even after diagnostic constraints were
removed. Continuing to add local heuristics would preserve the same failure modes.

## 3. Product Goals

The rewrite must provide the following user-visible capabilities:

1. Import the historical CSV/configuration format without silent data loss.
2. Load a versioned canonical project file.
3. Validate source data before solving.
4. Find a feasible timetable when one is available within the configured search
   budget.
5. Distinguish `OPTIMAL`, `FEASIBLE`, `INFEASIBLE`, `MODEL_INVALID`, `CANCELLED`, and
   `TIME_LIMIT`/`UNKNOWN` outcomes.
6. Explain hard violations in imported or manually edited schedules.
7. Provide useful diagnostics when a model is infeasible.
8. Improve feasible schedules according to explicit, measurable soft objectives.
9. Preserve the best known solution when optimization is cancelled or times out.
10. Export schedules by class/group, teacher, and room.
11. Produce a machine-readable solve report with timing, status, bounds, model size,
    and objective components.
12. Run locally without a server or mandatory network connection.

## 4. Explicit Non-Goals for the First Solver Release

The first solver release will not include:

- a generic university timetabling platform;
- SaaS, multi-user collaboration, or a web API;
- a custom SAT, CP, MIP, genetic, or local-search engine;
- automatic extraction from arbitrary PDFs;
- automatic truth claims about reconstructed teacher assignments;
- every conceivable soft preference;
- a GUI before the core can solve and validate the acceptance dataset;
- distributed solving;
- real-time collaborative editing.

## 5. Algorithm Selection

### 5.1 Primary Solver: OR-Tools CP-SAT

CP-SAT is selected because the problem is dominated by discrete choices and logical
resource conflicts:

- each meeting must occupy exactly one allowed slot;
- teachers, groups, and rooms are mutually exclusive resources;
- some meetings require simultaneous subgroup lanes;
- double lessons require adjacency;
- teacher assignment may be selected from a candidate set;
- many quality rules naturally become Boolean penalty literals.

The C++ API provides Boolean and integer variables, `ExactlyOne`, `AtMostOne`, linear
constraints, optional intervals, `NoOverlap`, hints, assumptions, solution observers,
time limits, and parallel portfolio search.

Primary references:

- <https://developers.google.com/optimization/cp/cp_solver>
- <https://developers.google.com/optimization/scheduling/job_shop>
- <https://developers.google.com/optimization/cp/cp_tasks>
- <https://github.com/google/or-tools/blob/stable/ortools/sat/cp_model.h>
- <https://github.com/google/or-tools/blob/stable/ortools/sat/sat_parameters.proto>

### 5.2 Rejected as the Primary Solver

#### Mixed Integer Programming

MIP is viable, but the timetable contains many logical implications, alternative
resources, and discrete penalty indicators. A MIP formulation would be useful as an
independent research comparison, but it is not the first implementation target.

#### Classical Constraint Programming

A classical CP engine such as Gecode is expressive and C++ native. It would require
more manual search design and tuning, while CP-SAT already supplies a modern hybrid
portfolio. It remains a fallback only if a specific required constraint is shown to
perform poorly in CP-SAT.

#### Tabu Search, Simulated Annealing, and Genetic Algorithms

These methods can improve an existing timetable but do not reliably prove
infeasibility and can reproduce the legacy behavior of searching indefinitely without
an explanation. They may be evaluated later as optional post-optimization methods,
never as the source of truth for hard feasibility.

#### Graph Coloring and Exact Cover

Graph coloring is useful for initial conflict analysis, and exact cover is elegant for
simple assignment matrices. Neither directly represents the full combination of
weekly distribution, rooms, teacher availability, subgroups, consecutive blocks, and
soft objectives. Both are useful conceptual tools, not the production solver.

## 6. System Architecture

```text
Legacy CSV/config ─┐
Canonical JSON ────┼──> Import/Application Service ──> Canonical Project
Future UI edits ───┘                                      │
                                                          ├──> ProjectValidator
                                                          │
                                                          ├──> CpSatSolver
                                                          │       │
                                                          │       ├──> SolveReport
                                                          │       └──> Schedule
                                                          │
                                                          └──> ScheduleValidator
                                                                  │
                                                                  └──> Validated output
```

The boundaries are mandatory:

- importers create domain objects but never solver variables;
- domain objects contain facts and preferences but no mutable solve state;
- the CP-SAT adapter builds a fresh model for each solve stage;
- the independent validator evaluates a completed schedule without consulting the
  CP-SAT model;
- exporters operate only on canonical projects, schedules, and reports;
- CLI and future GUI code call application services and never construct constraints.

## 7. Proposed Repository Layout

```text
CMakeLists.txt
cmake/
  Dependencies.cmake
  CompilerWarnings.cmake
  Sanitizers.cmake

include/schedmesh/
  domain/
    ids.h
    entities.h
    calendar.h
    constraints.h
    project.h
    schedule.h
  application/
    project_service.h
    solve_service.h
  solver/
    solver.h
    solve_options.h
    solve_result.h
  validation/
    diagnostics.h
    project_validator.h
    schedule_validator.h

src/
  app/
    main.cpp
    cli.cpp
  application/
    project_service.cpp
    solve_service.cpp
  domain/
    project.cpp
    schedule.cpp
  io/
    legacy_csv_importer.cpp
    legacy_config_importer.cpp
    project_json.cpp
    schedule_csv_exporter.cpp
    solve_report_json.cpp
  solver/cp_sat/
    candidate_domains.cpp
    variable_index.cpp
    hard_constraints.cpp
    staffing_constraints.cpp
    soft_constraints.cpp
    objectives.cpp
    infeasibility.cpp
    cp_sat_solver.cpp
  validation/
    project_validator.cpp
    schedule_validator.cpp
    objective_evaluator.cpp
  legacy/
    README.md
    ...preserved legacy implementation...

tests/
  unit/
  integration/
  acceptance/
  fixtures/public/

benchmarks/
  solver_benchmark.cpp

docs/
  architecture/
  constraints/
  formats/
  adr/
```

The exact directory move of the old source can be deferred until the new executable
exists. Git history must remain sufficient to recover the original layout.

## 8. C++ Engineering Standard

### 8.1 Language and Build

- C++20 with no compiler extensions.
- CMake targets instead of directory-wide flags.
- Supported compilers: current MSVC, Clang, and GCC versions available in CI.
- OR-Tools pinned to one tested release.
- Dependency acquisition documented and reproducible.
- Warnings enabled at a high level and treated as errors in CI for project code.
- Optional AddressSanitizer and UndefinedBehaviorSanitizer builds where supported.
- `clang-format` and `clang-tidy` configurations committed to the repository.

### 8.2 Code Rules

- no `using namespace` directives in headers;
- no owning raw pointers;
- no mutable global variables;
- no sentinel entity IDs;
- no fixed array sizes for domain cardinalities;
- strong identifier types rather than interchangeable integers;
- immutable input passed by `const&`;
- explicit ownership through values and smart pointers only where polymorphism is
  necessary;
- `std::chrono` for all durations and deadlines;
- structured errors rather than printing from library code;
- deterministic iteration order whenever it affects serialization or tests;
- UTF-8 at all file boundaries;
- public headers contain domain-facing types, not OR-Tools implementation types.

### 8.3 Error Model

Library operations return a value-or-error type. The project can use `absl::StatusOr`
because OR-Tools already depends on Abseil, or an equivalent project-local result type.
Errors must include:

- a stable error code;
- a human-readable message;
- source file and line/column when importing;
- relevant entity identifiers;
- a suggested corrective action where possible.

Exceptions must not be used as routine control flow.

## 9. Canonical Domain Model

### 9.1 Strong IDs

```cpp
template <typename Tag>
struct Id {
  std::uint32_t value{};
  auto operator<=>(const Id&) const = default;
};

using TeacherId = Id<struct TeacherTag>;
using StudentGroupId = Id<struct StudentGroupTag>;
using RoomId = Id<struct RoomTag>;
using SubjectId = Id<struct SubjectTag>;
using MeetingId = Id<struct MeetingTag>;
using SlotId = Id<struct SlotTag>;
```

Serialized projects use stable string keys such as `teacher-0042`; runtime dense
indices are created separately by a model index. A vector index must never be exposed
as a persistent ID.

### 9.2 Calendar

```cpp
struct Day {
  std::string id;
  std::string display_name;
  int ordinal;
};

struct Period {
  std::string id;
  int ordinal;
  std::optional<std::chrono::minutes> start_time;
  std::optional<std::chrono::minutes> end_time;
};

struct Slot {
  SlotId id;
  int day_index;
  int period_index;
};
```

The calendar is data. It is not compiled into `[6][14]` arrays. A unified day/period
axis represents both shifts. Groups receive allowed-slot domains instead of being
solved in isolated sessions.

### 9.3 Resources

```cpp
struct Teacher {
  TeacherId id;
  std::string display_name;
  std::vector<SubjectId> qualified_subjects;
  std::vector<SlotId> unavailable_slots;
  int maximum_weekly_load;
  std::optional<int> maximum_daily_load;
};

struct StudentGroup {
  StudentGroupId id;
  std::string display_name;
  int grade;
  std::vector<SlotId> allowed_slots;
};

struct Room {
  RoomId id;
  std::string display_name;
  int capacity;
  std::set<std::string> features;
  std::vector<SlotId> unavailable_slots;
};
```

Gyms, laboratories, and ordinary rooms are all `Room` resources with features. There
will be no special `Gym` global structure.

### 9.4 Meetings and Simultaneous Lanes

A `Meeting` is one simultaneous timetable event for one or more student groups.

```cpp
struct TeacherRequirement {
  std::optional<TeacherId> fixed_teacher;
  std::vector<TeacherId> candidates;
  int lane;
};

struct RoomRequirement {
  std::optional<RoomId> fixed_room;
  std::vector<RoomId> candidates;
  std::set<std::string> required_features;
  int minimum_capacity;  // 0 means unspecified
  int lane;
};

struct Meeting {
  MeetingId id;
  SubjectId subject;
  std::vector<StudentGroupId> groups;
  std::vector<TeacherRequirement> teacher_requirements;
  std::vector<RoomRequirement> room_requirements;
  std::vector<SlotId> allowed_start_slots;
  int duration_in_periods{1};
  std::string distribution_key;
};
```

Two language subgroups taught simultaneously are one meeting with two teacher lanes
and two room lanes. This prevents the model from accidentally placing subgroup lanes
at different times.

Weekly hours are expanded into meeting occurrences before model construction. The
expansion is deterministic and assigns stable occurrence IDs.

### 9.5 Hard Constraints and Preferences

Hard constraints and soft preferences are distinct types. A preference must never be
silently promoted to a hard constraint by giving it an enormous weight.

Every constraint has:

- a stable ID;
- a type;
- an enabled flag;
- a scope of affected entities;
- source provenance;
- severity (`hard` or `soft`);
- an optional weight for soft constraints;
- an explanation template used by diagnostics.

## 10. Canonical Project Format

The canonical project format will be versioned JSON initially. It must contain:

- `schema_version`;
- project metadata;
- calendar and slots;
- teachers, groups, subjects, and rooms;
- meeting requirements;
- hard constraints;
- soft preferences and weights;
- source provenance;
- optional reference schedule/hints;
- optional reconstruction-confidence metadata.

Requirements:

1. IDs are stable strings, not array positions.
2. Unknown fields are handled according to a documented compatibility policy.
3. Required fields fail validation with precise paths.
4. Serialization is deterministic to produce reviewable diffs.
5. A schema upgrade pipeline handles future versions.
6. Personal names are data values and never appear in tests, source code, variable
   names written to public logs, or public fixtures.
7. Solver reports reference IDs; a presentation layer may resolve display names.

The legacy CSV files remain import formats, not the internal database.

## 11. Import Pipeline

### 11.1 Stages

```text
bytes
  -> UTF-8/BOM handling
  -> RFC-compatible CSV rows
  -> legacy syntax records
  -> normalized entities
  -> meeting expansion
  -> canonical project
  -> ProjectValidator
  -> migration report
```

### 11.2 Import Requirements

- quoted fields and embedded commas are supported;
- duplicate teacher names are diagnosed rather than silently merged;
- subgroup inference is explicit in the migration report;
- every source column maps to a stable class ID;
- every non-empty hour value is consumed or reported as an error;
- unknown configuration keys are warnings or errors according to strict mode;
- room remapping is removed from the canonical model;
- arbitrary numbers of teachers, rooms, and classes are supported;
- import is deterministic;
- partial projects are not returned on fatal errors.

### 11.3 Migration Report

The importer produces a report containing:

- source file hashes;
- imported entity counts;
- normalized and renamed values;
- inferred subgroups;
- ignored legacy-only fields;
- warnings and errors;
- assumptions introduced by reconstruction;
- confidence counts for inferred staffing and rooms.

## 12. CP-SAT Model

### 12.1 Candidate-Domain Preprocessing

Before creating any solver variable, compute candidate domains:

- allowed slots per meeting after group shifts and availability;
- eligible teachers per lane;
- eligible rooms per lane;
- impossible meetings with an empty candidate domain;
- coarse resource-capacity checks;
- subject distribution feasibility checks;
- maximum-load checks per teacher and shift.

Preprocessing failures return diagnostics without invoking CP-SAT.

### 12.2 Time Assignment Variables

For each meeting `m` and allowed start slot `s`:

```text
x[m,s] = 1 iff meeting m starts at slot s
```

Only allowed combinations exist. Each meeting has:

```text
sum(x[m,s] for s in allowed(m)) == 1
```

For duration-two meetings, allowed start slots exclude the last usable period and
cross-day boundaries. Occupancy indices include both covered periods.

### 12.3 Teacher Variables

For fixed staffing, a meeting lane directly claims its fixed teacher whenever
`x[m,s]` is true.

For candidate staffing:

```text
teach[m,lane,t] = 1 iff teacher t is selected for the lane
sum(teach[m,lane,t]) == 1
```

The combined occupancy relationship may use sparse linking literals:

```text
teach_at[m,lane,t,s] = teach[m,lane,t] AND x[m,s]
```

These variables are created only for eligible teacher/slot pairs. The fixed-staffing
MVP avoids this additional dimension; integrated staffing is a later milestone.

### 12.4 Room Variables

For each meeting lane, slot, and eligible room:

```text
room_at[m,lane,r,s] = 1 iff the meeting lane uses room r at slot s
```

The room variables imply `x[m,s]`, and each required lane chooses exactly one room
when its meeting is present. If a meeting has a fixed room, no room-choice variable is
needed.

### 12.5 Resource Exclusivity

For every group and occupied slot:

```text
sum(meetings using the group at the slot) <= 1
```

Equivalent constraints are added for teachers and rooms. Multi-period meetings
contribute to every covered slot.

The initial model should use sparse Boolean cardinality constraints. Interval variables
and `NoOverlap` may be benchmarked for multi-period meetings, but must not be adopted
without measured benefit on the acceptance dataset.

## 13. Hard Constraint Catalogue

Each item requires domain representation, solver encoding, validator implementation,
unit tests, and at least one invalid fixture.

### 13.1 Meeting Placement

- every required meeting is scheduled exactly once;
- no unexpected meeting is generated;
- duration is preserved;
- simultaneous lanes share the same start slot;
- fixed assignments are honored.

### 13.2 Student Groups

- no group overlap;
- allowed shift and slot domain;
- maximum lessons per day;
- optional minimum lessons on an active day;
- forbidden idle gaps where configured;
- subgroup parent and lane consistency.

### 13.3 Teachers

- no teacher overlap;
- unavailable slots and methodical days;
- maximum weekly load;
- maximum daily load;
- qualification for the subject;
- candidate membership for reconstructed assignments;
- optional minimum break between shifts.

### 13.4 Rooms

- no room overlap;
- availability;
- capacity;
- required features;
- allowed room sets;
- separate physical gyms and laboratories;
- one room per required lane.

### 13.5 Subject Distribution

- exact weekly occurrence count;
- at most one occurrence per day when configured;
- required double blocks;
- forbidden first/last periods;
- conflicting subjects not placed on the same day;
- maximum repeated subject occurrences per day;
- required separation between occurrences where configured.

### 13.6 Sessions

- all shifts share one global calendar;
- each group receives an allowed-slot domain;
- teachers and rooms may participate in multiple shifts without a post-solve merge;
- transition/break constraints apply directly between adjacent shift slots.

## 14. Soft Objective Catalogue

Every soft objective produces both penalty literals and an independently calculated
report component.

Initial objective components:

1. teacher gaps within a day;
2. group gaps within a day;
3. isolated teacher lessons;
4. excessive consecutive teacher lessons;
5. excessive consecutive group lessons;
6. late lessons;
7. first lessons where undesirable;
8. Saturday lessons by grade;
9. uneven subject distribution across days;
10. consecutive heavy subjects;
11. room changes for a teacher or group;
12. deviation from preferred rooms;
13. deviation from preferred teacher assignments;
14. deviation from a reference or previously accepted timetable;
15. imbalance of daily lesson counts.

Weights are named configuration values with documented units. For example, one
`teacher_gap` penalty means one empty period between the first and last lesson of one
teacher on one day.

## 15. Multi-Stage Solve Strategy

A single weighted objective is easy to misconfigure. The production workflow will be
lexicographic or staged:

### Stage A: Feasibility

- include hard constraints only;
- use a strict short time limit;
- emit the first valid solution immediately;
- validate it independently;
- stop early if the user requested feasibility only.

### Stage B: Critical Quality

- use the Stage A schedule as a solution hint;
- minimize teacher gaps and severe policy penalties;
- preserve the best solution through observers;
- independently validate every accepted incumbent before export.

### Stage C: Student and Operational Quality

- constrain the critical objective to its accepted bound or allowed tolerance;
- minimize group gaps, late periods, Saturday usage, and distribution penalties;
- warm-start from Stage B.

### Stage D: Stability and Polish

- constrain earlier objective tiers;
- minimize room changes and deviation from the reference schedule;
- stop at the global deadline.

The result records the objective value and best bound for each completed stage. If a
later stage times out, the last validated incumbent remains the output.

## 16. Reference Schedules and Hints

A reference schedule can provide:

- preferred meeting slots;
- preferred teachers;
- preferred rooms;
- a full or partial CP-SAT hint;
- a stability objective measuring changes.

Hints are not constraints. A bad historical schedule must be repairable. The solver
configuration may enable hint repair with a bounded conflict budget, followed by
normal portfolio search.

An imported reference must pass the independent validator. Violations are reported,
but the reference may still be used as a partial preference source if explicitly
allowed.

## 17. Uncertain Staffing Strategy

The reconstructed dataset contains confidence levels rather than authoritative staff
assignments. The model must preserve that distinction.

### 17.1 Confidence Mapping

Suggested initial mapping:

- historical exact unique: preferred candidate with the lowest deviation penalty;
- historical exact ambiguous: all matched candidates with equal or near-equal
  preference;
- historical subject-only: qualified candidate with a moderate deviation penalty;
- synthetic `TBD`: explicit open-staffing candidate pool with a high penalty;
- user-confirmed assignment: fixed hard assignment.

### 17.2 Delivery Order

1. Fixed-staffing scheduling proves the core time/room model.
2. A staffing-only feasibility model checks qualifications and coarse load.
3. Integrated staffing and scheduling are added for ambiguous assignments.
4. If integrated model size becomes excessive, evaluate an iterative decomposition:
   staffing proposal, timetable solve, conflict cut, and staffing retry.

The decomposition must have a bounded iteration count and preserve diagnostics. It
must not become another unbounded randomized loop.

## 18. Infeasibility Diagnostics

The solver must help users fix data rather than merely return `INFEASIBLE`.

### 18.1 Pre-Solve Diagnostics

Detect inexpensive contradictions before model construction:

- meeting with no allowed slot;
- fixed teacher not qualified;
- fixed room missing required features;
- weekly teacher load above available periods;
- group load above allowed slot capacity;
- room-feature demand above aggregate capacity;
- a duration-two meeting with no adjacent slot pair;
- conflicting fixed meetings.

### 18.2 Assumption Groups

Major optional rule families receive assumption literals, such as:

- `teacher_availability:<teacher-id>`;
- `room_capacity:<room-id>`;
- `subject_distribution:<group-id>:<subject-id>`;
- `first_last_policy:<subject-id>`;
- `shift_domain:<group-id>`;
- `fixed_staffing:<meeting-id>:<lane>`.

When infeasible, the solver-provided sufficient assumption set is translated into
domain diagnostics. The result is not guaranteed to be a minimum explanation, so the
application may run a bounded shrinking pass to remove redundant assumption groups.

### 18.3 Relaxation Analysis

An optional diagnostic solve can turn selected rules into high-penalty soft
constraints. The report then says, for example, which availability or room rules must
be relaxed to obtain a schedule. This mode must be explicitly labeled diagnostic and
must never export its result as a valid production timetable.

## 19. Independent Validation

`ScheduleValidator` is a separate implementation of every hard rule. It receives only
the canonical project and completed schedule.

It must not:

- inspect CP-SAT variables;
- reuse CP-SAT expressions;
- assume that a solver status implies correctness;
- mutate the schedule;
- print directly to stdout.

Each violation contains:

```text
code
severity
constraint_id
message
day/period
meeting_ids
teacher_ids
group_ids
room_ids
suggested_action
```

The validator also calculates every soft-objective component independently. Solver
and validator totals are compared in integration tests.

## 20. Solver API

```cpp
enum class SolveStatus {
  kOptimal,
  kFeasible,
  kInfeasible,
  kModelInvalid,
  kTimeLimit,
  kCancelled,
  kUnknown,
};

struct SolveOptions {
  std::chrono::milliseconds feasibility_limit;
  std::chrono::milliseconds total_limit;
  int workers;
  std::uint32_t random_seed;
  bool deterministic;
  bool enable_optimization;
  bool enable_infeasibility_analysis;
};

struct SolveStatistics {
  std::chrono::milliseconds model_build_time;
  std::chrono::milliseconds first_solution_time;
  std::chrono::milliseconds total_time;
  std::int64_t branches;
  std::int64_t conflicts;
  std::int64_t variables;
  std::int64_t constraints;
  double objective;
  double best_bound;
};

struct SolveResult {
  SolveStatus status;
  std::optional<Schedule> best_schedule;
  ValidationReport validation;
  ObjectiveBreakdown objective;
  SolveStatistics statistics;
  std::vector<Diagnostic> diagnostics;
};

class Solver {
 public:
  virtual ~Solver() = default;
  virtual SolveResult Solve(const Project&, const SolveOptions&,
                            std::stop_token) = 0;
};
```

The interface makes timeout and cancellation normal outcomes.

## 21. Concurrency, Progress, and Cancellation

- production solves use CP-SAT portfolio workers explicitly configured from
  `SolveOptions`;
- deterministic tests use one worker and a fixed seed;
- the application uses `std::jthread` or an equivalent RAII worker;
- cancellation uses a stop token connected to `StopSearch`;
- solution observers copy accepted incumbents into canonical schedules;
- callbacks do minimal work and never perform file I/O;
- progress events are rate-limited and include elapsed time, best objective, best
  bound, and solution count;
- the CLI handles Ctrl+C and requests cancellation before forced termination;
- a future Qt layer observes progress through an application-level callback or event
  queue, never from the solver thread directly.

No configuration may permit an infinite unobservable loop.

## 22. Performance Strategy

Optimization begins with model quality, not parameter guessing.

### 22.1 Domain Reduction

- omit forbidden meeting-slot variables;
- omit ineligible teacher and room choices;
- intersect group, teacher, room, and duration availability early;
- detect fixed values and avoid creating redundant variables;
- share precomputed occupancy lists;
- use dense runtime indices and flat vectors in the solver adapter;
- reserve container sizes from preprocessing counts.

### 22.2 Symmetry Breaking

Repeated occurrences of the same subject can be interchangeable. Add safe ordering
constraints such as nondecreasing assigned slots for indistinguishable meeting
occurrences. Equivalent rooms or open `TBD` resources may also require canonical
ordering.

Symmetry-breaking constraints must be proven not to remove semantically distinct
solutions and require dedicated tests.

### 22.3 Search Guidance

- feed the reference schedule as hints;
- prioritize highly constrained meetings only if benchmarks improve;
- keep CP-SAT automatic portfolio search as the default;
- do not hardcode experimental solver parameters without benchmark evidence;
- record the complete parameter set in every solve report.

### 22.4 Benchmark Metrics

For each fixture record:

- import and validation time;
- model build time;
- variable and constraint counts;
- presolve time;
- time to first feasible solution;
- time to accepted quality threshold;
- final objective and best bound;
- conflicts and branches;
- peak memory where available;
- validation time;
- deterministic seed and worker count.

Performance regressions are assessed against medians of repeated runs, not a single
wall-clock sample.

## 23. Testing Strategy

### 23.1 Unit Tests

- strong ID behavior and stable serialization;
- calendar and slot arithmetic;
- meeting expansion;
- candidate-domain filtering;
- each importer normalization rule;
- each validator rule;
- each objective component;
- diagnostic formatting;
- status mapping and deadline accounting.

### 23.2 Solver Micro-Fixtures

Create public fixtures with synthetic names:

- `tiny_feasible`: one group, two teachers, two rooms;
- `tiny_infeasible_teacher`: forced teacher collision;
- `tiny_infeasible_room`: required room collision;
- `tiny_double_lesson`: adjacency requirement;
- `tiny_subgroups`: two simultaneous lanes;
- `tiny_two_shifts`: shared teacher across shifts;
- `tiny_first_last`: forbidden edge periods;
- `tiny_methodical_day`: teacher unavailability;
- `tiny_candidate_staffing`: solver-selected teacher;
- `tiny_objective_gaps`: known optimal gap count.

Tiny tests run with one worker and fixed seeds. Where practical, they assert an
optimal objective and not one exact timetable arrangement.

### 23.3 Validator Mutation Tests

Start from a valid schedule and introduce exactly one defect:

- duplicate teacher;
- duplicate room;
- duplicate group;
- missing meeting;
- extra meeting;
- wrong teacher;
- wrong room feature;
- forbidden slot;
- broken double block;
- daily distribution violation.

The validator must report the intended code and affected entities.

### 23.4 Integration Tests

- legacy CSV to canonical JSON migration;
- canonical JSON round-trip;
- solve then independently validate;
- reference hint import;
- timeout with incumbent preservation;
- cancellation with clean shutdown;
- infeasible assumptions translated to diagnostics;
- objective solver/validator reconciliation;
- CSV export and re-import where supported.

### 23.5 Private Acceptance Tests

The reconstructed school dataset is executed locally or in an explicitly private CI
environment. Public CI never requires or receives the personal-data fixture.

The acceptance test asserts properties:

- project imports without fatal errors;
- all required meetings are scheduled;
- zero hard violations;
- expected entity and weekly-load counts;
- solve terminates by its deadline;
- solve report and exported schedule are created;
- no personal names appear in solver variable names or generic logs.

It must not assert one exact timetable.

## 24. Testing and Quality Tooling

Recommended initial tooling:

- GoogleTest for unit and integration tests;
- CTest for test orchestration;
- clang-format for formatting;
- clang-tidy for static analysis;
- compiler warnings at high levels;
- sanitizer jobs on Linux;
- optional code coverage for domain and validator layers;
- a small benchmark executable separate from correctness tests.

Dependencies should be kept minimal. A dependency is added only when it clearly
replaces project code that would otherwise be error-prone, such as JSON parsing,
testing, or command-line parsing.

## 25. CLI Design

Initial commands:

```text
schedmesh import  --input data.csv --config settings.conf --output project.json
schedmesh validate --project project.json
schedmesh solve    --project project.json --output schedule.json
schedmesh export   --project project.json --schedule schedule.json --format csv
schedmesh explain  --project project.json
schedmesh benchmark --fixture small
```

Important solve options:

```text
--time-limit
--feasibility-limit
--workers
--seed
--feasibility-only
--reference-schedule
--objective-preset
--report
--log-level
```

Exit codes are stable and documented. Human output goes to stderr/stdout as
appropriate; JSON reports remain clean machine-readable artifacts.

## 26. Observability and Solve Reports

Every solve writes or can return a report containing:

- application and OR-Tools versions;
- project schema version and content hash;
- sanitized project counts;
- solver parameters;
- preprocessing warnings;
- status;
- start/end timestamps and durations;
- model size before/after presolve where available;
- first-solution time;
- solution count;
- objective values by stage;
- best bounds;
- conflicts and branches;
- cancellation or timeout reason;
- independent validation summary;
- infeasibility diagnostics;
- output artifact hashes.

Logs must use stable IDs by default. Display names are enabled only in explicitly
local verbose output.

## 27. Privacy and Repository Safety

The current local reconstruction branch contains personal data in Git history. A
later deletion commit does not remove that history.

Required workflow:

1. Create solver-development branches from a public-safe commit on `main`, not from
   the personal-data branch.
2. Keep private projects outside the repository or under an ignored local directory.
3. Commit only anonymized or synthetic fixtures to public-safe branches.
4. Never merge the local personal-data branch into a branch intended for GitHub.
5. Install a local pre-push guard that rejects `refs/heads/local/*` and other explicitly
   private branch patterns.
6. Avoid `git push --all` and mirror pushes from this clone.
7. Ensure solve reports and logs use stable IDs rather than names.
8. Treat generated schedules as personal data when they contain staff names.
9. If the private commit is ever pushed accidentally, deleting the branch is
   insufficient; repository history must be rewritten and credentials/links audited.

Suggested local layout:

```text
D:/SchedMesh-private/
  reconstructed-school.project.json
  reference-schedule.json
  outputs/
```

The public-safe binary accepts these paths at runtime.

## 28. Migration Strategy

The rewrite will proceed beside the legacy program until the acceptance gates pass.

### Phase 1: Preserve

- document the legacy executable and input files;
- record known failure behavior;
- retain representative non-personal fixtures;
- stop adding new features to the legacy generator.

### Phase 2: Parallel Executable

- create a new executable target such as `schedmesh-next`;
- keep legacy `SchedMesh` buildable temporarily;
- share no mutable domain structures between them;
- compare imported counts and constraint behavior.

### Phase 3: Acceptance Cutover

- solve all public fixtures;
- solve and validate the private acceptance fixture;
- compare exports and objective reports;
- make the new CLI the default executable;
- move legacy files under `src/legacy` or a tagged historical location.

### Phase 4: Removal from Active Build

- remove legacy sources from default build targets;
- retain history and explanatory documentation;
- delete compatibility code only when no supported input depends on it.

There will be no attempt to gradually transform `generate_timetable()` into CP-SAT.
The new model is built from canonical data, not from legacy runtime structures.

## 29. Milestones

### M0 — Safe Rewrite Foundation

Tasks:

- create a public-safe rewrite branch from `main`;
- add the privacy workflow and local push guard;
- pin C++20 and OR-Tools;
- add test, formatting, and warning infrastructure;
- add a minimal `schedmesh-next --version` executable;
- preserve the legacy target unchanged.

Exit criteria:

- clean configure/build/test on Windows and one CI platform;
- no personal data in the branch;
- OR-Tools version printed by the executable;
- one trivial CP-SAT smoke test passes.

### M1 — Canonical Domain and JSON

Tasks:

- implement strong IDs and entity types;
- implement calendar and global slots;
- implement meetings and simultaneous lanes;
- define schema version 1;
- implement deterministic JSON read/write;
- implement structural project validation;
- add Tiny fixtures.

Exit criteria:

- public fixtures round-trip byte-stably after normalization;
- invalid projects return precise diagnostics;
- domain headers contain no OR-Tools types.

### M2 — Legacy Import and Independent Validator

Tasks:

- replace the split-based CSV parser;
- import shifts, groups, teachers, subjects, weekly hours, rooms, availability, and
  special rules;
- generate a migration report;
- implement independent hard validation;
- implement objective evaluation skeleton;
- compare known weekly totals with legacy fixtures.

Exit criteria:

- historical-format public fixture imports deterministically;
- every imported value is consumed or diagnosed;
- validator mutation suite passes;
- canonical model has no fixed cardinality limits inherited from legacy arrays.

### M3 — Fixed-Staffing CP-SAT Feasibility

Tasks:

- implement candidate-slot preprocessing;
- create time assignment variables;
- add group and teacher exclusivity;
- add fixed room exclusivity;
- add duration and double-block behavior;
- add distribution, shift, and availability constraints;
- implement time limits, cancellation, and status mapping;
- extract and independently validate incumbents.

Exit criteria:

- all Tiny feasible fixtures solve;
- all Tiny infeasible fixtures are proven infeasible;
- cancellation terminates cleanly;
- a time-limited run never exceeds its deadline beyond a documented shutdown margin;
- every returned schedule has zero independent hard violations.

### M4 — Rooms and Full Historical Hard Rules

Tasks:

- add alternative room assignment;
- add capacity and feature rules;
- model subgroup room lanes;
- implement every legacy hard rule from the inventory;
- remove the special gym model;
- build public Medium fixture.

Exit criteria:

- public Medium fixture is feasible within the target budget;
- all hard rules have solver and validator tests;
- solver/validator disagreement is zero.

### M5 — Diagnostics

Tasks:

- add pre-solve capacity diagnostics;
- add assumption groups;
- translate infeasible assumption sets;
- implement bounded core shrinking;
- add optional relaxation analysis;
- expose `schedmesh explain`.

Exit criteria:

- each infeasible Tiny fixture reports its intended rule family and entity;
- diagnostic solve is clearly marked invalid for production export;
- infeasibility analysis obeys a separate deadline.

### M6 — Staged Soft Optimization

Tasks:

- implement teacher gap variables;
- implement group gap and daily balance penalties;
- implement late/Saturday/distribution penalties;
- implement room stability;
- implement reference deviation;
- add objective presets;
- reconcile solver and validator objective totals;
- preserve incumbents across stages.

Exit criteria:

- known-optimum Tiny objective tests pass;
- each objective component appears independently in reports;
- timeout during optimization retains the last feasible schedule;
- no objective tier can sacrifice hard feasibility.

### M7 — Candidate Staffing

Tasks:

- represent qualifications and candidate pools;
- add teacher-choice variables;
- add load constraints;
- map reconstruction confidence to preferences;
- add explicit open-staffing resources;
- benchmark integrated and decomposed approaches;
- select one bounded production workflow.

Exit criteria:

- ambiguous staffing fixture finds a conflict-free assignment and timetable;
- user-confirmed assignments remain fixed;
- the report explains every selected non-primary candidate;
- no arbitrary 99-teacher limit remains.

### M8 — Private Acceptance and Performance

Tasks:

- import the reconstructed project outside the repository;
- establish baseline model sizes and solve times;
- tune candidate domains and safe symmetry breaking;
- evaluate hints from the reference schedule;
- run repeated benchmarks;
- fix all validator failures;
- document remaining reconstruction uncertainty.

Exit criteria:

- a feasible private schedule is produced within the agreed deadline;
- independent validator reports zero hard violations;
- solve report contains status, time, objective, best bound, and model statistics;
- repeated runs meet the performance budget at an agreed percentile;
- no private artifact appears in public Git status or CI logs.

### M9 — CLI Cutover

Tasks:

- finalize import, validate, solve, explain, and export commands;
- document exit codes and file formats;
- package a Windows CLI artifact;
- remove the legacy target from the default build;
- update README and user workflow documentation.

Exit criteria:

- the complete workflow runs without developer tools;
- package includes licenses for dependencies;
- legacy behavior is documented but no longer the default.

### M10 — Optional Qt Desktop Application

Tasks:

- build project editors and schedule views in Qt 6;
- run solves on a worker thread/process;
- expose progress and cancellation;
- implement validated move with undo/redo;
- display diagnostics and objective breakdown;
- export through shared application services.

Exit criteria:

- UI never freezes during solve;
- manual changes pass through the independent validator;
- no constraint logic is duplicated in Qt models or widgets.

## 30. Performance Budgets

Initial budgets are targets and must be revised with benchmark evidence:

| Fixture | First feasible | Total solve limit | Expected result |
| --- | ---: | ---: | --- |
| Tiny | < 1 second | 5 seconds | Optimal |
| Small | < 5 seconds | 30 seconds | Optimal or proven near-optimal |
| Medium public | < 30 seconds | 2 minutes | Feasible, improving |
| Private school acceptance | < 2 minutes | 10 minutes | Feasible, validator-clean |

Additional budgets:

- import and structural validation under 2 seconds for the acceptance dataset;
- model build under 10 seconds;
- cancellation acknowledged within 2 seconds under normal operation;
- no unbounded retry loop;
- peak memory measured and reported before setting a hard production budget.

A miss is reported as a benchmark failure, not hidden by increasing the default time
limit indefinitely.

## 31. Risks and Mitigations

### Risk: Model Size Explosion

Cause: meeting × teacher × room × slot cross-products.

Mitigation:

- fixed-staffing MVP;
- sparse candidates;
- preprocess domains;
- link variables only where necessary;
- benchmark alternative room encodings;
- integrated staffing only after fixed model is stable.

### Risk: Reconstructed Data Is Actually Infeasible

Mitigation:

- preserve confidence and candidate sets;
- pre-solve capacity checks;
- assumptions and relaxation diagnostics;
- do not treat historical guesses as hard facts;
- validate the reference schedule separately.

### Risk: Objective Weights Produce Bad Schedules

Mitigation:

- staged objectives;
- named penalty units;
- independent objective evaluator;
- objective breakdown in every report;
- user-visible presets instead of unexplained constants.

### Risk: Solver and Validator Disagree

Mitigation:

- separate implementations;
- mutation tests;
- solver/validator reconciliation tests;
- reject any incumbent with a hard validation failure;
- treat disagreement as a release blocker.

### Risk: Nondeterministic Tests

Mitigation:

- one worker and fixed seed in correctness tests;
- assert properties rather than exact arrangements;
- separate performance tests from correctness tests;
- record all parameters.

### Risk: OR-Tools Packaging Complexity on Windows

Mitigation:

- pin and document one installation method first;
- add a clean-machine CI build;
- isolate OR-Tools includes to the adapter target;
- package required runtime libraries and licenses;
- do not support multiple dependency managers initially.

### Risk: Personal Data Is Pushed

Mitigation:

- public-safe branch lineage;
- external private fixture path;
- local pre-push branch guard;
- anonymized logs and reports;
- explicit review of `git diff --cached` before every public commit.

## 32. Architecture Decision Records

Create ADRs for decisions that would be costly to reverse:

- ADR-001: C++20 as the core language;
- ADR-002: OR-Tools CP-SAT as the primary solver;
- ADR-003: canonical JSON project format;
- ADR-004: independent validator boundary;
- ADR-005: unified global time axis for shifts;
- ADR-006: sparse Boolean time assignment model;
- ADR-007: staged objective optimization;
- ADR-008: private acceptance data outside public Git history;
- ADR-009: fixed staffing before integrated staffing;
- ADR-010: Qt 6 only after CLI acceptance.

Each ADR records context, decision, alternatives, consequences, and evidence.

## 33. Proposed Initial Commit Sequence

The first implementation series should remain small and reviewable:

1. `build: add C++20 next-generation target and test infrastructure`
2. `docs: add architecture decisions for CP-SAT rewrite`
3. `domain: add strong IDs, calendar, and resource entities`
4. `domain: add meeting and schedule models`
5. `io: add versioned canonical JSON round-trip`
6. `validation: add structural project validator`
7. `solver: add CP-SAT smoke fixture with bounded solve`
8. `solver: add meeting placement and group exclusivity`
9. `solver: add teacher and room exclusivity`
10. `validation: add independent schedule validator`
11. `io: add strict legacy CSV importer and migration report`
12. `solver: add shifts, availability, and double lessons`
13. `solver: add solve reports, timeout, and cancellation`
14. `solver: add infeasibility assumption groups`
15. `solver: add staged teacher-gap objective`

Every commit must build and pass tests. Personal-data fixtures are never included in
this sequence.

## 34. Definition of Done for the Solver Rewrite

The solver rewrite is complete when all of the following are true:

1. The new C++20 executable is the default build target.
2. The legacy source is preserved but excluded from normal execution.
3. Historical-format input migrates deterministically into a documented canonical
   project.
4. Every hard constraint has domain, solver, validator, and test coverage.
5. The private acceptance dataset produces a feasible schedule within the agreed
   bounded runtime.
6. The independent validator reports zero hard violations.
7. Solver and validator objective components reconcile.
8. Timeout and cancellation always return cleanly with the best validated incumbent,
   if one exists.
9. Infeasible fixtures produce actionable diagnostics.
10. There are no domain cardinality sentinels or fixed-size capacity arrays.
11. There is no mutable global solver state or owning raw pointer.
12. Public branches and fixtures contain no personal staff data.
13. CI covers build, formatting, static analysis, tests, sanitizers, and packaging.
14. The CLI imports, validates, solves, explains, and exports without developer tools.
15. Architecture, schema, constraints, objective meanings, and limitations are
    documented.

## 35. Immediate Next Action

The next implementation task should be M0 on a new public-safe branch created from
`main`:

1. add a C++20 `schedmesh-next` target;
2. integrate a pinned OR-Tools build;
3. add GoogleTest and one trivial bounded CP-SAT test;
4. add `--version` and `solve-smoke` CLI behavior;
5. verify clean Windows configure/build/test instructions;
6. confirm that the branch contains no reconstructed personal-data files.

Only after this foundation passes should domain modeling begin.
