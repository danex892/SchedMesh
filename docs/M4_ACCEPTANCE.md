# M4 acceptance evidence

M4 replaces implicit legacy classroom behavior with explicit canonical room resources and
independently enforced room requirements. The fixed-staffing solver and ScheduleValidator now
agree on ordinary room exclusivity, availability, features, per-lane minimum capacity,
alternative assignment, simultaneous subgroup lanes, and the historical gym sharing rule.

## Historical reconstruction

Ordinary classroom names are interned into stable shared identities. Alternative lists preserve
source order, remove duplicates, and deterministically disambiguate normalized-ID collisions.
Every room-using meeting must declare exactly one room requirement for each teacher lane.

The legacy `S` code becomes two ordinary rooms with the `gym` feature, matching the preserved
generator's two concurrent gym lanes. Concurrent gym meetings are permitted only for groups in
the same or adjacent grades. The legacy `T` code becomes a dedicated `technology` room for each
mapped teacher, preserving the old absence of cross-teacher workshop contention. Migration emits
an explicit warning because private facility identities must still be confirmed by the school.

The public historical migration now produces 41 rooms and 952 meetings with no roomless meetings.
It contains both alternative and feature-constrained room lanes. The deterministic room audit
finds no aggregate candidate-pool capacity overload, so a simple classroom shortage does not
constrain the corrected fixture beyond feasibility.

## Public Medium fixture

`tests/fixtures/medium_project.h` defines a source-controlled, deterministic project with:

- two student groups across adjacent grades;
- ordinary small and large rooms with explicit capacities;
- a laboratory and a technology workshop with required features;
- two interchangeable gym lanes;
- two simultaneous language subgroup lanes competing for two language rooms;
- alternative room selection and a consecutive two-period technology meeting.

CP-SAT solves the fixture to optimal feasibility, returns every meeting exactly once, and the
independent validator reports zero diagnostics. The accompanying room audit reports complete room
coverage and no proven aggregate overload.

## Verification boundary

The full local suite builds with the repository-managed clang-cl/Ninja toolchain. Formatting and
clang-tidy cover every production and test translation unit. CTest covers the Medium fixture,
historical migration and solve, capacity boundaries, feature
selection, subgroup lanes, room identity, gym compatibility, cancellation, and independent
schedule validation.

The built-in fixture now solves after preserving legacy subgroup and profile semantics explicitly.
Assumption groups, bounded core shrinking, relaxation analysis, and `schedmesh explain` still
belong to M5 for genuinely infeasible projects. The special-room reconstruction also remains
visibly marked for confirmation against a real facility inventory rather than being presented as
undisputed source data.
