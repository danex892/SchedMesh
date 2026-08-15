# ADR 0001: C++20 CP-SAT parallel rewrite

- Status: accepted
- Date: 2026-08-14

## Context

The legacy C++17 generator mixes parsing, mutable schedule state, randomized search,
constraint checks, and output. Its fixed-size arrays and unbounded retry behavior
make incremental replacement risky, while the historical fixture is too large for
the current greedy search.

## Decision

SchedMesh will be rewritten as a parallel C++20 executable named `schedmesh-next`.
Google OR-Tools CP-SAT 9.15 is the first solver backend. The legacy `SchedMesh`
target remains buildable and unchanged until the new CLI passes the historical
acceptance test.

The canonical domain model and independent validator must not depend on OR-Tools.
Application surfaces call those components through application services instead of
constructing solver variables directly.

Dependencies are version-pinned. CMake first accepts installed config packages and
can otherwise fetch verified source archives. Every solve, including smoke tests,
has an explicit time limit.

## Consequences

- Rewrite work can be reviewed without destabilizing the legacy executable.
- Solver output will not be treated as proof of schedule validity.
- The first clean dependency build is relatively expensive, especially on Windows.
- Two executables coexist until the acceptance cutover.
