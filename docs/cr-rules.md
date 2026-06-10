# CR rules

The eight hard requirements every PR into `main` must satisfy.

## R1 — Tests first, evidence in PR body
The PR description MUST contain two fenced code blocks:
1. `scripts/test.py` output showing the new test(s) **FAILing** (run before the change).
2. Same script's output showing those tests **PASSing** (run after).
Without the FAIL log we cannot verify the test would catch a regression.

## R2 — Integration evidence for behavior changes
Any change that touches user-visible behavior (CLI output, error messages, runtime
behavior) MUST include a paste of the relevant stderr/stdout demonstrating the new
behavior in the ## Integration evidence section.

## R3 — Test coverage on touched logic
Every new or modified pure-logic path in `fjmReader.h` or `cpu.h` MUST have at
least one new test in `scripts/test.py` exercising that path.

## R4 — No undefined behavior introduced
Any arithmetic on template type `W` MUST use `static_cast<W>(1)` (not bare `1`)
when shifting, to avoid UB on 64-bit builds. Reviewer checks for `1 <<` patterns
on any `W`-typed expression.

## R5 — All file reads checked for failure
Every call that reads from a binary file (including `readTo` / `assertRead` macro
uses) in the segment-loading path MUST be followed by an implicit or explicit
stream-state check. Unguarded reads on truncated inputs are rejected.

## R6 — Module placement
Pure interpreter logic lives in `fjmReader.h` and `cpu.h`. `main.cpp` is entry-
point only: argument parsing + file open + dispatch. No interpreter logic in
`main.cpp`; no I/O setup in `fjmReader.h` / `cpu.h` beyond what `run()` already
accepts via `std::istream&` / `std::ostream&`.

## R7 — Branch & PR naming
- Branch: `fix/<slug>` for bug-fix / safety PRs; `mN-feature-slug` for milestones.
- PR title: `Fix: <short description>` for fixes; `M<N>: <feature>` for milestones.
- PR body MUST contain `## TDD evidence (R1)` and `## Integration evidence (R2)`.

## R8 — Zero new warnings
Build with `-Wall -Wpedantic` must introduce no new warning lines vs the
baseline in `docs/known-warnings.md`. Debug build uses `-fsanitize=address,undefined`.

---

## Verdict format

Approving: review body `APPROVED\nAll R1-R8 pass.`
Requesting changes: `CHANGES REQUESTED\nR<id> fail: <one-line reason>\n...`
Inline comments prefix: `R<id>:`
