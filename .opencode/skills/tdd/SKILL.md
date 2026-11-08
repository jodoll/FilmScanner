---
name: Core TDD
description: Test-first workflow for lib/scanner_core. Use when adding or changing behavior, a method, a state transition, a config rule or an error path in the pure core.
---

# Core TDD

Apply this to every behavior change in `lib/scanner_core/`. The HAL in `src/hal/`
is covered by the manual checklist in `docs/09-testing.md` §8; do not add host
unit tests for it.

Canonical rules: `docs/09-testing.md` §1–3. This skill is the working loop; if
they ever differ, that document wins.

## Before the first test

1. Find the behavior in `docs/`. The docs define behavior; the headers define
   declarations.
2. If it is missing, ambiguous or contradicts the docs, stop and report the gap,
   the options and the proposed doc change. Do not encode a guess.
3. Read the header you are about to implement. Its signature is the contract; do
   not redesign it inside the test. If it must change, change the header and
   every caller in the same step.

## The loop

1. **Red.** Add one small `TEST` (or `TEST_F`/`TEST_P`) for the next behavior.
   Name it after the behavior, not the method:
   `RejectsOutOfRangeSeekSpeed`, `CompletesShutterPulseAfterAbort`.
2. **Run it.**

   ```bash
   pio test -e native -v
   ```

   Confirm it fails on the expected assertion. A compile error is not red; fix
   the test or its includes first.
3. **Green.** Write the minimum production code that passes. No speculative
   methods, unused parameters or drive-by changes.
4. **Run everything.** `pio test -e native` must be green before you continue.
5. **Refactor** with the test green. The test stays as the specification.
6. **Check the target.** `pio run -e uno` proves the core still compiles as
   C++11 for AVR; the native build uses C++17 and will not catch that.
7. **Stop before git.** Do not run `git add`, `git commit` or `git push`.
   Instead, report the changed files, propose one Conventional Commit message in
   the style of `docs/11-implementation-plan.md` §15, and show the exact
   command:

   ```bash
   git add <changed files> && git commit -m "feat(core): ..."
   ```

   Wait for the user to approve or run it.

One behavior per loop. Do not batch tests, and do not batch code.

## Worked example

Behavior: `bootCompleted()` moves the machine from `BOOT` to `IDLE`
(`docs/03-architecture.md` §6.1 and §7.4).

Red:

```cpp
TEST(StateMachineBoot, MovesFromBootToIdle) {
  StateMachine machine;
  machine.begin();

  const Result result = machine.bootCompleted();

  ASSERT_EQ(result.state, State::IDLE);
  ASSERT_EQ(result.error, ErrorCode::NONE);
}
```

`pio test -e native -v` fails because `result.state` is still `State::BOOT`.
That is the expected failure. A compile error, or a failure in another
assertion, means the test is not ready yet.

Then implement only the `BOOT` → `IDLE` transition, rerun until green, and
refactor. A step can contain several red-green loops; each loop is one behavior.

## Test rules

- One `TEST`/`TEST_F`/`TEST_P` instance is one case, and one case is one behavior.
- Never loop over inputs inside a test. Use `TEST_P` with
  `INSTANTIATE_TEST_SUITE_P`; each value runs and reports separately.
- `ASSERT_*` for preconditions and single-outcome checks. `EXPECT_*` only for
  several independent facts of one coherent behavior.
- Include `<gtest/gtest.h>`. Do not define `main()`; `test_main.cpp` owns it.
- Fakes live in `test/fakes/`. No real time, no `delay()`, no `sleep()`; drive
  time with `FakeClock`.
- Assert observable behavior: return values, emitted events, output states. Not
  private members.
- If a fake is wrong, fix the fake, not the production code.
- Where the docs give a concrete number, test it (`expectedFrameSteps = 1290`).
- Cover every public method, branch, boundary and error code, and every
  transition in `docs/03-architecture.md` §6.1.

## Layout

| Thing | Path |
|---|---|
| Declarations (contract) | `lib/scanner_core/include/scanner_core/` |
| Implementation | `lib/scanner_core/src/` |
| Tests | `test/test_native/` |
| Fakes | `test/fakes/` |

## Commands

```bash
pio test -e native                                  # all tests
pio test -e native -v                               # one line per case
pio test -e native --program-arg=--gtest_filter='StateMachine.*'
pio run -e uno                                      # C++11 / AVR check
```

## Done for this step

- The new test failed for the right reason before the code existed.
- `pio test -e native` is green.
- `pio run -e uno` is clean, with no warnings from project code.
- No `Arduino.h`, heap, exceptions, RTTI, STL container or `String` in the core.
- No behavior that the docs do not state.
- Nothing is staged or committed; a commit message and command are proposed.

## Pitfalls

- `-std=gnu++17` on native hides C++17-only code in the core. The Uno build is
  the real check.
- Moving logic into an adapter to avoid a test is a spec violation, not a
  shortcut.
- Changing a test to match a bug turns the safety net off.
- Tests batched at the end of a milestone usually miss branches and boundaries.
- Do not paste declarations into `docs/`; link the header instead.
