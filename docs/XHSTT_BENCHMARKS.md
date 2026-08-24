# XHSTT import support

`schedmesh-next import-xhstt` converts a supported XHSTT archive into canonical project JSON and
imports its last published solution as a reference schedule. The importer preserves hard time
availability, linked events, student clashes, daily course limits, and fixed or candidate teacher
and room assignments.

No third-party benchmark XML is stored in Git. Importer regression tests use compact synthetic XML
fixtures embedded in the test suite, so normal builds remain self-contained.
