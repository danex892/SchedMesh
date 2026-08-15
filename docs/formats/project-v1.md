# Canonical project format v1

SchedMesh project files are UTF-8 JSON objects with `schema_version` equal to `1`.
Canonical output uses two-space indentation, lexicographically ordered object keys,
preserves array order, and ends with one line feed. Reading and writing a valid
canonical file must be byte-stable.

The public example is [`tests/fixtures/tiny_project.json`](../../tests/fixtures/tiny_project.json).

## Root fields

| Field | Type | Meaning |
|---|---|---|
| `schema_version` | unsigned integer | Must be `1`. |
| `metadata` | object | Stable project `id` and presentation-only `display_name`. |
| `calendar` | object | Day and period axes plus their global slots. |
| `subjects` | array | Subject IDs, conflicts, consecutive-period requirements, and boundary restrictions. |
| `teachers` | array | Qualifications, unavailability, and load limits. |
| `student_groups` | array | Grade, allowed global slots, and repeated-subject policy. |
| `rooms` | array | Capacity, features, and unavailability. |
| `meetings` | array | Events to place, including simultaneous resource lanes. |

IDs are stable strings such as `teacher-001`; an array index is never a persistent
identifier. Times are integer minutes after midnight or `null` when unknown.

## Meetings and lanes

A meeting has one subject, one or more student groups, allowed start slots, a
positive duration, and teacher/room requirements. Parallel subgroups are represented
as multiple numbered lanes inside the same meeting. Lane numbers are zero-based,
non-negative, and unique within each requirement type.

A resource requirement contains either one fixed resource or a non-empty candidate
list, never both. Room requirements may additionally list required feature strings.

## Compatibility policy

- Unknown root fields are errors because they may indicate a different schema.
- Unknown nested fields are ignored when reading and removed by canonical writing.
  This permits additive presentation metadata without affecting solver semantics.
- Missing required fields and incompatible value types are fatal parse diagnostics;
  no partial project is returned.
- A syntactically valid project is independently checked for duplicate IDs, broken
  references, invalid capacities and loads, invalid slots, and malformed meetings.
- Future incompatible changes require a new `schema_version` and an explicit upgrade
  path.

Display names are presentation data. Diagnostics, solver reports, and logs identify
entities using stable IDs.
