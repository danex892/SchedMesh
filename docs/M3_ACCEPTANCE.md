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

## Historical acceptance

The public legacy dataset now migrates deterministically into 952 meeting occurrences covering 969
teaching periods. The importer preserves simultaneous English and Informatics subgroups, separate
courses of the same subject, and the two parallel profile curricula encoded by slash-separated
hours. With one worker and seed 1, the acceptance project solves within the 30-second test budget,
writes a schedule, and passes independent validation.

The old greedy generator did not complete this dataset. The earlier CP-SAT infeasibility was also
real for the model it received, but that model had incorrectly serialized parallel profile hours as
sequential whole-class hours. The acceptance test now protects the corrected interpretation.
