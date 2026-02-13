# Semantic Indexer — How It Works

This document explains how the DeepTest **semantic-indexer** skill builds a programmatic understanding of source code, using `QuicRangeCompact` from MsQuic's `range.c` as a concrete example.

---

## What the semantic indexer does

Unlike the LLM simply "reading" source files as text, the semantic indexer runs **actual Python scripts** that use [tree-sitter](https://tree-sitter.github.io/) to parse every `.c`, `.h`, and `.cpp` file in the repository into an AST. From the ASTs it extracts:

- Every function definition (name, file, line range)
- Every call expression inside each function (caller → callee edges)
- Type definitions, `#define` macros, and class structures

The results are stored in a **SQLite database** with a normalized schema, not just a markdown document.

---

## The three-stage pipeline

```
┌──────────────────┐     ┌───────────────────┐     ┌──────────────────────┐
│  1. Parse & Index │────▶│  2. Build Focal   │────▶│  3. Bottom-Up        │
│  (tree-sitter)    │     │  Call Graph        │     │  Summarization       │
│                   │     │                    │     │                      │
│  query_repo.py    │     │  build_focal.py    │     │  summarizer.py       │
│  • Walk all files │     │  • Pick a focal fn │     │  • Start at leaves   │
│  • Extract funcs  │     │  • Trace callees   │     │  • LLM reads source  │
│  • Build call map │     │  • Store in SQLite │     │    + callee summaries │
│  • Cache results  │     │  • Handle cycles   │     │  • Write summary     │
└──────────────────┘     └───────────────────┘     │  • Annotate pre/post │
                                                    └──────────────────────┘
```

**Stage 1** is fully automated — tree-sitter parses thousands of files in seconds.  
**Stage 2** is automated — the script walks the call graph recursively.  
**Stage 3** is LLM-assisted — the summarizer presents source code + callee context to the LLM, which writes a natural-language summary stored back in the database.

---

## Example: Call graph for `QuicRangeCompact`

When the agent runs:
```bash
python scripts/build_focal.py \
  --focal "QuicRangeCompact" \
  --file "range.c" \
  --project /home/runner/work/msquic/msquic \
  --db .deeptest/range.db
```

tree-sitter parses the project, locates `QuicRangeCompact` in `range.c`, and traces every function it calls — then recursively traces their callees. The result is this call graph:

### Call graph diagram

```
                        QuicRangeCompact
                        (range.c:573-616)
                              │
              ┌───────────────┼───────────────────┐
              │               │                   │
              ▼               ▼                   ▼
     QuicRangeGetSafe  QuicRangeGetHigh  QuicRangeRemoveSubranges
      (range.h:113)     (range.h:41)       (range.c:186-217)
              │                                   │
              ▼                         ┌─────────┼─────────┐
        QuicRangeSize                   │         │         │
         (range.h:89)                   ▼         ▼         ▼
                                   memmove   CalcShrink  QuicRangeShrink
                                              Length      (range.c:672-708)
                                           (range.c:       │
                                            638-651)       │
                                                     ┌─────┼──────┐
                                                     ▼     ▼      ▼
                                              CXPLAT_  memcpy  CXPLAT_
                                              ALLOC_          FREE
                                              NONPAGED
              ┌───────────────┐
              │               │
              ▼               ▼
  QuicRangeCalculate    QuicRangeShrink
   ShrinkLength          (same as above)
   (range.c:638-651)
```

### What each node does

| Function | File | Role in the graph |
|---|---|---|
| **QuicRangeCompact** | range.c:573 | **Focal** — iterates subranges, merges adjacent/overlapping ones, then evaluates shrinking |
| QuicRangeGetSafe | range.h:113 | Bounds-checked accessor — returns `NULL` if index is out of range |
| QuicRangeGetHigh | range.h:41 | Computes `Low + Count - 1` to get the highest value in a subrange |
| QuicRangeSize | range.h:89 | Returns `UsedLength` — number of active subranges |
| QuicRangeRemoveSubranges | range.c:186 | Removes N contiguous subranges from the array via `memmove`, then evaluates shrinking |
| QuicRangeCalculateShrinkLength | range.c:638 | Decides if allocation is oversized relative to usage; returns target length |
| QuicRangeShrink | range.c:672 | Reallocates the subranges array to a smaller buffer, or falls back to the inline `PreAllocSubRanges` |

---

## SQLite database schema

The indexer stores everything in a normalized SQLite database:

```sql
CREATE TABLE functions (
    function_id  INTEGER PRIMARY KEY,
    name         TEXT NOT NULL,
    file         TEXT NOT NULL,
    start_line   INTEGER,
    end_line     INTEGER,
    summary      TEXT DEFAULT ''   -- filled by bottom-up summarization
);

CREATE TABLE call_edges (
    caller_id  INTEGER NOT NULL,
    callee_id  INTEGER NOT NULL,
    PRIMARY KEY (caller_id, callee_id),
    FOREIGN KEY (caller_id) REFERENCES functions(function_id),
    FOREIGN KEY (callee_id) REFERENCES functions(function_id)
);

CREATE TABLE preconditions (
    function_id      INTEGER NOT NULL,
    condition_text   TEXT NOT NULL,
    sequence_order   INTEGER NOT NULL
);

CREATE TABLE postconditions (
    function_id      INTEGER NOT NULL,
    condition_text   TEXT NOT NULL,
    sequence_order   INTEGER NOT NULL
);
```

For the `QuicRangeCompact` focal, the database would contain rows like:

| function_id | name | file | start_line | end_line |
|---|---|---|---|---|
| 1 | QuicRangeCompact | range.c | 573 | 616 |
| 2 | QuicRangeGetSafe | range.h | 113 | 117 |
| 3 | QuicRangeGetHigh | range.h | 41 | 45 |
| 4 | QuicRangeRemoveSubranges | range.c | 186 | 217 |
| 5 | QuicRangeCalculateShrinkLength | range.c | 638 | 651 |
| 6 | QuicRangeShrink | range.c | 672 | 708 |
| 7 | QuicRangeSize | range.h | 89 | 93 |

And `call_edges`:

| caller_id | callee_id | Meaning |
|---|---|---|
| 1 | 2 | Compact → GetSafe |
| 1 | 3 | Compact → GetHigh |
| 1 | 4 | Compact → RemoveSubranges |
| 1 | 5 | Compact → CalculateShrinkLength |
| 1 | 6 | Compact → Shrink |
| 4 | 5 | RemoveSubranges → CalculateShrinkLength |
| 4 | 6 | RemoveSubranges → Shrink |
| 2 | 7 | GetSafe → Size |

---

## Bottom-up summarization walkthrough

After building the call graph, the summarizer works **leaves-first**:

```
Step 1:  Summarize QuicRangeSize (leaf — no callees)
         → "Returns the number of active subranges (UsedLength)."

Step 2:  Summarize QuicRangeGetSafe (calls QuicRangeSize)
         → "Bounds-checked subrange accessor. Returns pointer to
            SubRanges[Index] if Index < UsedLength, otherwise NULL."

Step 3:  Summarize QuicRangeGetHigh (leaf)
         → "Returns the highest value in a subrange: Low + Count - 1."

Step 4:  Summarize QuicRangeCalculateShrinkLength (leaf)
         → "Returns halved AllocLength (clamped to INITIAL_SUB_COUNT)
            if AllocLength >= MinAllocMultiplier * INITIAL and
            UsedLength < AllocLength / UsedThresholdDenominator.
            Otherwise returns current AllocLength (no-op)."

Step 5:  Summarize QuicRangeShrink (calls CXPLAT_ALLOC, memcpy, CXPLAT_FREE)
         → "Reallocates SubRanges array to NewAllocLength. Uses
            PreAllocSubRanges if shrinking to initial size. Copies
            UsedLength entries. Frees old buffer. Returns FALSE on
            allocation failure."

Step 6:  Summarize QuicRangeRemoveSubranges (calls memmove, CalcShrink, Shrink)
         → "Removes Count subranges starting at Index via memmove.
            Evaluates shrinking with multiplier=2, denominator=4.
            Returns TRUE if reallocation occurred."

Step 7:  Summarize QuicRangeCompact (calls all of the above)
         → "Iterates subranges, merging any that are overlapping
            (High_i >= Low_{i+1}) or adjacent (High_i + 1 == Low_{i+1}).
            Removes merged duplicates via RemoveSubranges. Then evaluates
            shrinking with multiplier=4, denominator=8 — a more
            conservative threshold than RemoveSubranges uses."
```

At each step, the LLM sees **actual source code** plus the already-computed callee summaries — so by the time it summarizes the focal function, it has deep context about every dependency.

---

## How the semantic index helps test generation

With this index, the agent knows:

1. **What paths exist** — `QuicRangeCompact` can call `RemoveSubranges`, which can call `Shrink`, which can fail on allocation. That's a 3-level-deep error path the agent needs to reason about.

2. **Which functions are internal** — `QuicRangeCalculateShrinkLength` has no public header declaration, so tests must reach it indirectly through `Compact` or `RemoveSubranges`.

3. **What the preconditions are** — `QuicRangeGetSafe` handles out-of-bounds gracefully (returns NULL), but `QuicRangeGet` does not (undefined behavior). Tests must use the right accessor.

4. **Where the interesting branches are** — the summarizer notes that `Shrink` has a special case for `NewAllocLength == QUIC_RANGE_INITIAL_SUB_COUNT` (use inline buffer). That's a specific scenario the test should exercise.

This is fundamentally different from the LLM just reading `range.c` as text — the indexer gives the agent a **queryable, structured graph** of the code's architecture.

---

## Comparison: Semantic indexer vs. component-test RCI

| Aspect | Semantic Indexer | Component-Test RCI |
|---|---|---|
| **Parsing** | tree-sitter AST (programmatic) | LLM reads source as text |
| **Call graph** | Extracted from AST, stored in SQLite | LLM infers by reading code |
| **Storage** | SQLite with normalized tables | Markdown + JSON files |
| **Summarization** | Bottom-up, callee-context-aware | Single-pass LLM analysis |
| **Incremental** | Yes — database grows as you index more focals | No — regenerated each run |
| **Accuracy** | High — AST-level precision | Good — but can miss indirect calls |
| **Speed** | Parsing: seconds; summarization: minutes | Minutes (single LLM pass) |
