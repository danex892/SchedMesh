# M2 acceptance evidence

M2 delivers deterministic legacy import plus solver-independent project, schedule,
and objective validation. The implementation contains no OR-Tools types outside the
solver layer and does not inherit the legacy fixed array limits.

## Historical fixture baseline

Migrating `data/settings.conf` twice produces byte-identical canonical JSON with:

- 6 days, 13 global periods, and 78 slots;
- 27 student groups;
- 39 teachers;
- 40 subjects;
- 37 explicit rooms;
- 947 meeting occurrences covering 964 teaching periods.

The migration report explicitly warns about `S` and `T` room codes because they do
not identify concrete facilities. It also reports `entire_course_per_day` as ignored:
the preserved generator parsed that setting but never used it during scheduling.

## Independent validation

`ProjectValidator` checks canonical references, resource domains, qualifications,
subject policies, durations, and whether each meeting retains a feasible start.

`ScheduleValidator` checks complete assignment, multi-period occupancy, group,
teacher, and room exclusivity, availability, resource lanes and features, teacher
loads, repeated subjects, and same-day subject conflicts.

Room capacity remains conditional on group-size data. The historical format does
not store student counts, so migration does not invent them; explicit group sizes
and capacity enforcement can be added without changing the imported baseline.

`ObjectiveEvaluator` independently reports raw teacher and group idle periods,
late-period load, and optional last-day load. Weighting these components remains a
solver concern for M3.

The mutation and boundary suite is registered in CTest, while `make lint` checks all
next-generation production and test translation units.
