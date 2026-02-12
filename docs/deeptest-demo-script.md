# DeepTest Workflow Demo Script

Video walkthrough script for demonstrating the DeepTest GitHub Agentic Workflow on the MsQuic project.

---

## Opening — GitHub repo page

> "Hey everyone — today I'm going to walk you through a GitHub Agentic Workflow we've built for the MsQuic project. It uses **DeepTest**, a custom agent that runs on top of the Copilot CLI. What makes DeepTest special is that it builds a **semantic index** of the entire codebase — it understands the structure, the call graphs, the dependencies — and uses that deep understanding to generate high-quality, idiomatic tests that actually exercise meaningful code paths. We've wired it into a GitHub Agentic Workflow so it triggers automatically on pull requests and iterates until it hits a 95% line coverage target."

## Navigate to the Actions tab

> "The workflow itself is defined in a simple markdown file — that's the GitHub Agentic Workflows approach. You describe what you want in natural language with some YAML frontmatter, compile it, and it runs as a standard GitHub Actions workflow. But the real intelligence comes from DeepTest as the agent behind it."

## Show the workflow in the Actions list, or trigger it from a PR

> "It triggers automatically when a PR is opened or updated. Let me show you a run from a recent PR."

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

> "Here's the key step — 'Execute GitHub Copilot CLI.' This launches the Copilot CLI with DeepTest loaded as a **custom agent**. Unlike a generic LLM prompt, DeepTest brings its own specialized skills. It builds a **semantic index** over the repository — understanding not just the text of the code, but the relationships between components, functions, and test harnesses. That's what allows it to generate tests that are idiomatic to the existing test suite and that target the right harness files, the right patterns, and the right APIs."

## Expand the step to show logs

> "Let's look at what the agent did. It first reads the PR file list to see what changed — in this case, `range.c` and `range.h`. Then it reads the full source and extracts the PR diff to understand what's new. Next comes test harness discovery — the agent searches the repo for existing test files that cover the changed component. It uses `find` and `grep` to locate files referencing the changed symbols, explores the directory structure, and reads candidate test files to understand their patterns. For this run, it discovered `RangeTest.cpp` under `src/core/unittest/` — a GTest harness with a `SmartRange` RAII wrapper and 41 existing tests. Once it finds the right harness, it invokes the **component-test skill**, which kicks off a structured process: it builds a **Repository Contract Index** — a detailed document cataloging every public API's signature, preconditions, postconditions, and call relationships — then derives a scenario catalog and starts generating tests against the gaps."
>
> "The agent then follows an iteration loop. In each iteration, it analyzes uncovered lines from the coverage report, generates new tests targeting those specific paths, and runs our `make-coverage.sh` script — which builds with coverage, executes the tests, and produces a Cobertura XML report."

## Point to coverage numbers in the logs

> "You can see the coverage results printed after each iteration. The agent starts at the baseline — say around 78% — and with each round, coverage climbs as it targets the remaining uncovered branches, error-handling paths, and edge cases."
>
> "Because DeepTest has that semantic understanding of the codebase, the tests it writes aren't just superficial — they follow the existing MsQuic test patterns, use the right helper classes like `TestConnection` and `TestStream`, and exercise paths that a naive approach would miss entirely."

## Scroll past the agent execution to safe outputs / conclusion

> "Once the agent finishes, it prepares a commit and requests a pull request through the **safe outputs** mechanism. The agent itself doesn't have write permissions — it emits a structured JSONL request, and a separate job with elevated permissions creates the draft PR. There's also a **threat detection** step in between that reviews the agent's output before any write operations happen."

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
