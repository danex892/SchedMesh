# Built-in legacy school example

`data/input5-11.csv`, `data/classrooms.csv`, and `data/settings.conf` form the repository's
anonymized school example. The CSV remains the source fixture because preserving the old input is
useful for migration tests. `make example` converts it to canonical project JSON and solves it with
the current CP-SAT engine. Generated files stay under `.tools/examples/legacy-school/`.

XHSTT is not the canonical storage format for this example. It is an external interchange and
benchmark format, while SchedMesh JSON represents the native model without an unnecessary
CSV-to-XHSTT-to-JSON conversion.

## Legacy matrix semantics

- Rows with the same subject and weekly hours but different teachers are simultaneous subgroups.
  This is used for large English and Informatics classes.
- Different hour batches of the same subject are separate courses. For example, five whole-class
  English lessons and three subgroup English lessons may both occur on one day, but each course is
  still spread to at most one occurrence per day.
- A slash such as `4/3` describes two parallel profile curricula, not seven sequential lessons.
  Whole-class lessons occupy both profile groups; paired profile lessons start simultaneously.

The cleaned fixture also fixes duplicated `5c` and `10b` headers to `5b` and `10a`. A trailing-space
subject identity was replaced by the explicit name `Informatics elective`; depending on invisible
whitespace to distinguish two courses made migration unstable.

## Run

```powershell
make example
```

The acceptance run migrates 29 scheduling groups (27 classes, with two profile classes represented
by two groups each), 39 teachers, 41 subjects, 41 rooms, and 952 meeting occurrences. The generated
schedule is independently validated before it is written. It then creates three local artifacts:

- `project.json` — the canonical migrated model;
- `schedule.json` — the validated solver result;
- `timetable.xlsx` — the formatted workbook for people.

The workbook contains a summary plus separate first- and second-shift sheets. Classes are columns;
days and lesson numbers are rows. Each occupied cell shows the subject, teacher, and room. Parallel
English and Informatics subgroups remain together in a numbered cell, profile lessons are labelled,
and both periods of a double lesson are marked explicitly.
