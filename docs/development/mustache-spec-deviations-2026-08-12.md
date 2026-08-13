# Mustache specification conformance ledger

**Date:** 2026-08-12  
**Specification revision:** `5d3b58ea35ae309c40d7a8111bfedc4c5bcd43a6`

This ledger records the exact conformance baseline before the memory-safety
modernization. It replaces the previous false-green result, which removed all
whitespace from expected and actual output and silently omitted unsupported
tests.

The executable source of truth is
[`tests/spec_expectations.hpp`](../../tests/spec_expectations.hpp). A known
failure that starts passing is treated as an unexpected pass so its ledger
entry must be reviewed and removed. An unlisted failure, a missing expectation,
or a change in the pinned suite inventory fails the test executable.

## Current result

| Suite | Exact passes | Explicit skips | Known failures | Total |
| --- | ---: | ---: | ---: | ---: |
| Comments | 5 | 0 | 7 | 12 |
| Delimiters | 6 | 0 | 8 | 14 |
| Interpolation | 39 | 0 | 0 | 39 |
| Inverted sections | 16 | 0 | 6 | 22 |
| Partials | 7 | 0 | 4 | 11 |
| Sections | 22 | 1 | 7 | 30 |
| Dynamic names | 0 | 21 | 0 | 21 |
| Inheritance | 0 | 22 | 0 | 22 |
| Lambdas | 9 | 0 | 1 | 10 |
| **Total** | **104** | **44** | **33** | **181** |

CI succeeds only when there are zero unexpected failures and zero unexpected
passes. Known failures remain visible in the summary and are not counted as
passes.

## Known conformance failures

All 33 currently executed failures are exact-output whitespace failures. They
fall into these related implementation gaps:

- standalone section and inverted-section lines are not removed;
- standalone comment lines are not removed;
- standalone delimiter-change lines are not removed; and
- standalone partial lines are not removed and partial indentation is not
  propagated to every rendered line.

These gaps also explain failures whose names are not themselves "Standalone",
including deeply nested and doubled sections, delimiter changes surrounding
sections, and the alternate-delimiter interpolation lambda. The complete list
of suite and test names is kept in the executable ledger rather than duplicated
here.

## Explicitly unsupported cases

- All 21 dynamic partial-name extension tests are skipped because dynamic
  partial names are not implemented.
- All 22 inheritance extension tests are skipped because parent and block tags
  are not implemented.
- `sections.yml / Implicit Iterator - Array` is skipped because nested-array
  implicit iteration is not implemented.

Each skipped case is parsed far enough to record its suite and name, then
reported with its reason. The expected per-suite inventory prevents an updated
or incomplete specification checkout from silently changing the totals.

## Updating this ledger

When behavior is fixed or the pinned specification changes:

1. Run the specification test with byte-for-byte comparison enabled.
2. Review every unexpected pass, unexpected failure, and inventory mismatch.
3. Update the executable expectation only after confirming the behavior.
4. Update this summary if any suite total changes.
5. Keep behavioral corrections separate from representation-only changes when
   practical.

Diagnostics escape newlines, carriage returns, tabs, control bytes, and
backslashes so whitespace regressions remain visible in test logs.
