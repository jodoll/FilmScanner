# ADR-0006 — GoogleTest for native unit tests

- **Status:** Accepted
- **Date:** 2026-11-08
- **Deciders:** Project owner, firmware architect
- **Supersedes:** The native-test-framework part of
  [ADR-0001](0001-library-stack.md)

## Context

The native tests cover configuration validation, parsing, sensors, motion,
camera timing, persistence and the state machine. Many of these are table-driven:
every config key has boundaries and invalid values, every parser command has
variants, and every state transition needs a direct case.

The requirement is that **every test case runs and reports independently**. A
loop over several inputs inside one test does not satisfy this: it is one
reported case, and a fatal failure stops the remaining values.

Unity was the original choice because it is small and Arduino-friendly. It does
not provide parameterized tests, and it has no soft assertions without an
additional Ruby helper script. Emulating parameterization with a loop hides the
individual cases and produces poor failure reports.

Options considered:

1. **Unity plus a Ruby helper.** Keeps the existing framework, but adds a
   non-standard script to the workflow and still does not give native
   parameterized test cases.
2. **GoogleTest through PlatformIO's `native` platform.** Provides `TEST`,
   `TEST_F`, `TEST_P`, `INSTANTIATE_TEST_SUITE_P`, `EXPECT_*` and `ASSERT_*`.
   Each parameterized instance is a separate test case.
3. **Catch2 / doctest.** Also capable, but another dependency and a different
   macro style; PlatformIO's GoogleTest integration is sufficient and familiar.
4. **Custom harness.** Maximum control, but unnecessary maintenance and no
   standard reporting.

## Decision

Use **GoogleTest** for native unit tests, run through PlatformIO's `native`
platform. The exact environment definition is implementation and belongs in
`platformio.ini`, not in this record.

Rules specific to this decision:

- GoogleTest is host-only. It is not part of the Uno build and has no flash or
  RAM impact on the target.
- The native environment uses `gnu++17` because current GoogleTest requires it.
  The core sources remain C++11-compatible; only the host test build selects the
  newer standard.
- Test-writing conventions (one case per `TEST`/`TEST_F`/`TEST_P`, parameterized
  values, assertion choice) live in
  [09-testing.md §1](../09-testing.md#1-strategy). This record does not restate
  them.

## Consequences

Positive:

- Every parameterized value is a separately reported test case.
- `EXPECT_*` provides soft assertions without a helper script.
- Fixtures (`TEST_F`) and parameterized fixtures (`TEST_P`) are built in.
- Failure messages include the suite, test name and parameter index.
- Test filtering and listing are supported by the framework.

Negative:

- The native test binary is larger and the host build is heavier. This does not
  affect the Uno firmware.
- The team must use GoogleTest macros instead of Unity macros.
- PlatformIO downloads and builds GoogleTest for the native environment.

## Alternatives rejected

- **Unity with a Ruby helper:** extra tooling, no native parameterized cases,
  and still one reported case per loop.
- **Catch2 / doctest:** good frameworks, but GoogleTest is already supported by
  PlatformIO and fits the required reporting model.
- **Custom harness:** more maintenance for no project-specific benefit.
