# M3 acceptance evidence

M3 delivers deterministic fixed-staffing CP-SAT feasibility over the canonical project
model. Candidate preprocessing, model construction, schedule extraction, and independent
validation remain separate layers. Weighted soft optimization is intentionally deferred to
M6 of the detailed rewrite plan.

## Feasibility coverage

The solver creates exactly one start mode per meeting and selects one eligible resource per
teacher and room lane. It enforces full multi-period occupancy, group, teacher, and room
exclusivity, resource availability and features, teacher daily and weekly loads, repeated
subject policy, and same-day subject conflicts.

Tiny fixtures cover feasible schedules and proven infeasibility for group, teacher, and room
collisions, alternative rooms, consecutive periods, load limits, repeated subjects, and
conflicting subjects. Every feasible or optimal response is converted to `domain::Schedule`
and must pass `ScheduleValidator` before it can be returned or exported.

## Runtime boundaries

`SolveRequest` provides a positive deadline, worker count, deterministic seed, and cooperative
`std::stop_token`. A short-deadline regression allows a one-second shutdown margin. Active
cancellation is exercised against the known-long historical search and must terminate within
three seconds after the one-second cancellation delay without returning a schedule.

The production command is:

```text
schedmesh-next solve <project.json> <schedule.json> \
  [--time-limit-ms N] [--workers N] [--seed N]
```

It prints status, elapsed milliseconds, branches, and conflicts. Schedule JSON is written only
after independent validation succeeds.

## Historical baseline

The public legacy dataset migrates deterministically into 947 meeting occurrences covering 964
teaching periods. With one worker, seed 1, and a 30-second budget, the M3 model proves the
reconstructed project infeasible and does not emit a schedule. The preserved legacy generator
also failed to complete this dataset.

This is a recorded baseline, not a fabricated success. M4 subsequently reconstructs ambiguous
room identities and full room semantics without changing that outcome. M5 owns bounded
rule-family diagnostics for the remaining global infeasibility; a validator-approved historical
schedule remains required before the release acceptance milestone.
