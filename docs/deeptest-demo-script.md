# DeepTest Workflow Demo Script

Video walkthrough script for demonstrating the DeepTest GitHub Agentic Workflow on the MsQuic project.

---

## Opening — GitHub repo page

> "Hey everyone — today I'm going to walk you through a GitHub Agentic Workflow we've built for the MsQuic project. It uses **DeepTest**, a custom agent that runs on top of the Copilot CLI. What makes DeepTest special is that it builds a **semantic index** of the entire codebase — it understands the structure, the call graphs, the dependencies — and uses that deep understanding to generate high-quality, idiomatic tests that actually exercise meaningful code paths. We've wired it into a GitHub Agentic Workflow so it triggers automatically on pull requests and iterates until it hits a 95% line coverage target."

## Navigate to the Actions tab. Show the workflow in the Actions list, or trigger it from a PR

> "It triggers automatically when a PR is opened or updated. But first, [let's check out what changes are in the PR](#appendix-code-changes-summary) — this one adds range compaction and shrinking functions to `range.c`. Now let me show you a run from this PR."

## Click into a workflow run

> "The workflow has several jobs. Let me walk through them."
>
> "First, **activation** — a lightweight check that validates the workflow."
>
> "Then **resolve-params** and **list-files** — these identify which source files were changed in the PR and save that file list as an artifact for the agent."

## Click into the agent job

> "Now the **agent** job — this is where DeepTest actually runs."
>
> "It starts with the usual setup — checking out the repo, downloading the PR file list, and preparing the machine for code coverage instrumentation."

## Scroll to the Copilot CLI execution step

> "Here's the key step — 'Execute GitHub Copilot CLI.' This launches the Copilot CLI with DeepTest loaded as a **custom agent**. Unlike a generic LLM prompt, DeepTest brings its own specialized skills. It builds a **semantic index** over the repository — understanding not just the text of the code, but the relationships between components, functions, and test harnesses. The semantic index allows the agent to build context of program functionality and inferred contracts — like **pre- and post-conditions** — to test deeper paths in the system. The agent also does **neural path analysis** to identify what paths remain uncovered with respect to existing test collateral."

## Expand the step to show logs

### Phase 1 — PR analysis and test harness discovery

> "Let's look at what the agent did. It starts by reading the PR file list — `pr-files.json` — which tells it that `range.c` and `range.h` were modified. It reads both files in full to understand the component, then fetches the PR diff from the GitHub API to isolate exactly what changed: three new functions — `QuicRangeCompact`, `QuicRangeCalculateShrinkLength`, and `QuicRangeShrink`."
>
> "Next is **test harness discovery** — this is exploratory, not pre-indexed. The agent runs `find` and `grep` across the repo, looking for test files that reference `QuicRange` symbols. It checks `src/test/bin/`, reads `quic_gtest.cpp`, and eventually discovers `src/core/unittest/RangeTest.cpp` — a dedicated GTest file with a `SmartRange` RAII wrapper and 41 existing tests. It reads the full file to understand the test patterns, fixture style, and assertion conventions."

### Phase 2 — Skill invocation and semantic indexing

> "Once the agent understands both the production code and the test harness, it invokes the **unit-test skill** — one of DeepTest's built-in skills. The skill drives a structured process: first it calls into the **semantic-indexer skill**, which uses **tree-sitter** to parse the C source files and build an actual call graph stored in a SQLite database. For `QuicRangeCompact`, the indexer traces every function it calls — `QuicRangeGetSafe`, `QuicRangeGetHigh`, `QuicRangeRemoveSubranges`, `QuicRangeCalculateShrinkLength`, `QuicRangeShrink` — and recursively indexes their callees too. This gives the agent a programmatic understanding of the code's structure, not just a textual one."
>
> "Once the index is built, the agent **queries** it to retrieve a specification for the focal function — a summary that includes what the function does, what its callees do, and the inferred **pre-conditions** (e.g., 'Range must be initialized,' 'UsedLength > 1 for merging to occur') and **post-conditions** (e.g., 'no overlapping subranges remain,' 'allocation may shrink if usage drops below threshold'). This specification is what the agent consults *before* writing any tests — it's not just reading the source as text, it's working from a structured contract derived from the call graph and bottom-up summarization."
>
> "The agent then performs **neural path analysis** — a detailed breakdown of every execution path through the focal function, cross-referenced against the existing test collateral. This identifies which paths remain uncovered: for example, 'the shrink-to-PreAllocSubRanges path in `QuicRangeShrink` is never exercised by the existing 41 tests.' Each uncovered path becomes a test generation target."
>
> "For a deeper look at how the semantic indexer works and what the call graph looks like, see [`docs/semantic-indexer-example.md`](semantic-indexer-example.md)."

### Phase 3 — Iterative test generation with coverage feedback

> "Now the iteration loop begins. The agent writes a batch of tests, then runs `make-coverage.sh` — which builds with coverage instrumentation, executes the tests, and produces a Cobertura XML report. The agent parses the XML to find uncovered lines, reads the source at those line numbers to understand what path they represent, and writes targeted tests for the next iteration."
>
> "In this run, there were three iterations:"

| Iteration | Tests added | Coverage | What happened |
|---|---|---|---|
| 1 | 13 tests | 81.12% | Initial batch covering the three new functions — Compact, Shrink, and related scenarios. One compile error (called an internal-only function `QuicRangeCalculateShrinkLength` directly), fixed and re-run. |
| 2 | 12 tests | 93.57% | Targeted uncovered lines: `QuicRangeReset`, `GetRange`, `GetMinSafe`/`GetMaxSafe`, `SetMin` variations, growth branches, and edge cases in add/remove. |
| 3 | 9 tests | 93.57% | Attempted to cover remaining gaps — allocation-failure paths and max-size limits. Coverage plateaued because these paths require fault-injection that the test infrastructure doesn't support. |

> "The agent printed `MAX ITERATIONS REACHED. Best coverage: 93.57%.` — just short of the 95% target, with the remaining 6.4% being allocation-failure branches that need mock allocator support."

## Point to coverage numbers in the logs

> "You can see the coverage climbing in the logs — 81% after iteration 1, then jumping to 93.5% after iteration 2 as the agent systematically targeted uncovered branches. The final 6.4% are all allocation-failure error paths — lines where `CXPLAT_ALLOC_NONPAGED` returns NULL — which are unreachable without a fault-injection harness."
>
> "The tests the agent writes are idiomatic to the existing suite — they use the same `SmartRange` RAII wrapper, the same GTest assertion macros, and follow the same naming conventions as the original 41 tests. That's the value of reading the existing harness first and understanding the patterns."

## Navigate to the PR created by the workflow, if available

> "And here's the result — a draft pull request with the generated tests, labeled 'deeptest.' The team can review these, run CI, and merge."

## Wrap up

> "So to recap — what you're seeing here is DeepTest, a custom agent running on the Copilot CLI, that uses **semantic indexing** to deeply understand the MsQuic codebase and generate high-quality, targeted unit tests. We've wrapped it in a GitHub Agentic Workflow so it runs automatically on every PR, iterates toward a 95% coverage target, and creates a draft PR with the results — all with proper security boundaries."
>
> "Thanks for watching!"

---

## Appendix: Code Changes Summary

### Production code change — `range.c` (commit `84acbe8b`)

**Commit:** `feat: add range compaction and shrinking functions to optimize memory usage`
**Author:** Steve Fan — **+191 / −20 lines** across `range.c` and `range.h`

The commit adds three new functions and integrates compaction into existing operations:

| Function | Purpose |
|---|---|
| `QuicRangeCompact()` | Iterates through all subranges and merges any that are overlapping or adjacent. Then evaluates whether the allocation can be shrunk (threshold: alloc ≥ 4× initial and used < alloc/8). |
| `QuicRangeCalculateShrinkLength()` | Parameterized helper that decides whether shrinking is warranted. Takes a minimum-allocation multiplier and a used-threshold denominator, returns the target allocation length (halved, clamped to `QUIC_RANGE_INITIAL_SUB_COUNT`). |
| `QuicRangeShrink()` | Reallocates the subranges array to a smaller buffer. Falls back to the pre-allocated inline buffer (`PreAllocSubRanges`) when shrinking all the way to the initial size. |

The commit also:
- **Refactored `QuicRangeRemoveSubranges()`** — replaced the inline shrink logic with calls to `QuicRangeCalculateShrinkLength()` + `QuicRangeShrink()`.
- **Injected `QuicRangeCompact()` calls** into `QuicRangeAddRange()`, `QuicRangeRemoveRange()`, and `QuicRangeSetMin()` so compaction happens automatically after mutations.
- Added forward declarations and public API declarations in `range.h`.

### New tests — `RangeTest.cpp` (local, uncommitted)

**29 new tests** added on top of the existing 41 (total: 70). They fall into six categories:

#### 1. Basic API coverage (previously untested functions)
| Test | Targets |
|---|---|
| `ResetNonEmptyRange` | `QuicRangeReset` — verifies UsedLength drops to 0 and safe queries return FALSE |
| `GetMinMaxSafeEmptyRange` | `QuicRangeGetMinSafe` / `QuicRangeGetMaxSafe` on empty range |
| `GetRangeExistingValue` | `QuicRangeGetRange` — full range query, verifies Count and IsLastRange |
| `GetRangeNonExistingValue` | `QuicRangeGetRange` — returns FALSE for absent value |
| `GetRangeMiddleValue` | `QuicRangeGetRange` — partial count from middle of a subrange |
| `GetRangeNotLastRange` | `QuicRangeGetRange` — verifies IsLastRange=FALSE for non-terminal subranges |

#### 2. `SetMin` operation
| Test | Targets |
|---|---|
| `SetMinDropsLowerValues` | Drops values below threshold in a contiguous range |
| `SetMinSplitsSubrange` | Splits a subrange at mid-point |
| `SetMinDropsMultipleSubranges` | Drops entire subranges below threshold |
| `SetMinBelowExisting` | No-op when threshold is below current minimum |

#### 3. `Compact` operation (new function)
| Test | Targets |
|---|---|
| `CompactMergesAdjacentRanges` | Adjacent subranges merge into one |
| `CompactMergesOverlappingRanges` | Overlapping subranges merge correctly |
| `CompactWithActualMerging` | Bridge ranges connect three separate subranges into one |
| `CompactMergesMultipleOverlaps` | Chain of four overlapping ranges → single subrange |
| `CompactTriggersShrink` | Grow large, remove most, verify compact triggers shrink |
| `CompactShrinkingLargeAllocation` | Grow to 48 subranges, remove 45, compact → shrink path |
| `CompactNoOpWhenNoAdjacent` | Well-separated ranges survive compaction unchanged |
| `CompactSingleSubrange` | Edge case — single subrange, no crash |
| `CompactEmptyRange` | Edge case — empty range, no crash |

#### 4. Growth / memory management
| Test | Targets |
|---|---|
| `GrowthAtBeginning` | `QuicRangeGrow` with `NextIndex = 0` (memcpy branch lines 101–104) |
| `GrowthInMiddle` | `QuicRangeGrow` with `0 < NextIndex < UsedLength` (split branch lines 112–118) |
| `GrowthAtEnd` | `QuicRangeGrow` with `NextIndex = UsedLength` (append branch lines 106–109) |
| `GrowthHitsMaxSizeLimit` | Growth prevented at `MaxAllocSize` cap (line 71) |

#### 5. Shrink operation (new function)
| Test | Targets |
|---|---|
| `ShrinkAfterManyRemovals` | Shrink triggered by `RemoveSubranges` threshold |
| `ShrinkToPreAllocatedBuffer` | Shrink all the way back to `PreAllocSubRanges` inline buffer |

#### 6. AddRange / RemoveRange edge cases
| Test | Targets |
|---|---|
| `AddRangeFindingFirstOverlap` | Binary search finding first overlap among multiple subranges (line 276) |
| `AddRangeAtMaxCapacity` | Adding beyond capacity ages out smallest value |
| `RemoveRangeCausingSplit` | Remove middle of large range → two subranges |
| `RemoveRangeMiddleOverlap` | Remove [125–175] from [100–200] → verify exact split boundaries |

### Quality evaluation

**Strengths:**
- **Structured commenting** — every test has a block comment documenting the scenario, the targeted lines/branches, and the expected assertions. This makes future maintenance straightforward.
- **Good edge-case coverage** — empty range, single subrange, no-op compaction, and max-capacity aging are all covered. These are exactly the paths a human reviewer would ask about.
- **Three growth branches tested** — the `NextIndex = 0`, middle, and end cases map directly to the three `memcpy` branches in `QuicRangeGrow`, which is the kind of line-level targeting DeepTest excels at.
- **Deterministic** — no timing dependencies; all tests use simple integer arithmetic and direct API calls.
- **Idiomatic** — tests use the existing `SmartRange` RAII wrapper and follow the same GTest assertion style as the original 41 tests.

**Weaknesses / gaps:**
- **Allocation-failure paths untested** — lines 87–92 (`CXPLAT_ALLOC_NONPAGED` returns NULL in `QuicRangeGrow`) and line 689 (alloc failure in `QuicRangeShrink`) are not exercised. These require a mock allocator or fault-injection harness that MsQuic doesn't currently expose in unit tests.
- **Soft assertions on shrink** — `ShrinkAfterManyRemovals` and `CompactTriggersShrink` end with comments ("allocation may have shrunk") rather than hard assertions on `AllocLength`. The `ShrinkToPreAllocatedBuffer` test guards with `if (AllocLength == INITIAL)` rather than asserting it. This makes these tests weaker as regression guards.
- **Line 276 coverage uncertain** — `AddRangeFindingFirstOverlap` exercises the binary search overlap path but may not hit the exact `while` loop back-tracking on line 276 depending on search result ordering.
- **No negative-count / zero-count edge cases** — no tests for `QuicRangeAddRange` with `Count = 0` or very large values near `UINT64_MAX`.
- **`QuicRangeCompact` merge-count bug potential** — the production code on line 601 sets `CurrentRange->Count = MergedHigh - CurrentRange->Low` but this should be `MergedHigh - CurrentRange->Low + 1` since Count represents number of values, not difference. The tests don't catch this because the existing `AddRange` logic already handles merging before Compact is called, so the Compact merge path may not be exercised independently. This is worth verifying.

**Overall verdict:** The tests are well-structured, idiomatic, and cover the major functional paths of the new code. The main gap is allocation-failure testing, which is a known limitation of the test infrastructure rather than a test quality issue. Coverage reached ~92% before the agent timed out — the remaining ~3% is primarily allocation-failure branches and specific binary-search edge cases.
