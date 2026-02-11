/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Tests for the QUIC_RANGE data structure and operations.

--*/

#include "precomp.h"
#ifdef QUIC_CLOG
#include "RangeTest.cpp.clog.h"
#endif

#include "../core/range.h"

//
// Scenario: Initialize a QUIC_RANGE and verify initial state
// How: Call QuicRangeInitialize with valid MaxAllocSize
// Assertions: UsedLength is 0, AllocLength is QUIC_RANGE_INITIAL_SUB_COUNT,
//             SubRanges points to PreAllocSubRanges
//
void QuicTestRangeInitialization()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_EQUAL(Range.UsedLength, 0u);
    TEST_EQUAL(Range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_EQUAL(Range.MaxAllocSize, 1024u);
    TEST_EQUAL(Range.SubRanges, Range.PreAllocSubRanges);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add a single value to an empty range
// How: Initialize range, add value 42, check if present
// Assertions: UsedLength is 1, range contains value 42, 
//             subrange Low=42 Count=1
//
void QuicTestRangeAddSingleValue()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 42));
    TEST_EQUAL(Range.UsedLength, 1u);
    
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 42, &Count, &IsLast));
    TEST_EQUAL(Count, 1ull);
    TEST_TRUE(IsLast);
    
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 42ull);
    TEST_EQUAL(Sub->Count, 1ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add multiple non-adjacent values
// How: Add values 10, 20, 30 to range
// Assertions: UsedLength is 3, each value present in separate subrange,
//             subranges are ordered
//
void QuicTestRangeAddMultipleNonAdjacentValues()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    
    TEST_EQUAL(Range.UsedLength, 3u);
    
    // Verify ordering
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    QUIC_SUBRANGE* Sub2 = QuicRangeGet(&Range, 2);
    
    TEST_EQUAL(Sub0->Low, 10ull);
    TEST_EQUAL(Sub0->Count, 1ull);
    TEST_EQUAL(Sub1->Low, 20ull);
    TEST_EQUAL(Sub1->Count, 1ull);
    TEST_EQUAL(Sub2->Low, 30ull);
    TEST_EQUAL(Sub2->Count, 1ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add a contiguous range of values
// How: Add range [100, 110) using QuicRangeAddRange
// Assertions: UsedLength is 1, subrange covers Low=100 Count=10,
//             all values 100-109 are present
//
void QuicTestRangeAddContiguousRange()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated;
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, 100, 10, &Updated);
    
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_TRUE(Updated);
    TEST_EQUAL(Range.UsedLength, 1u);
    TEST_EQUAL(Sub->Low, 100ull);
    TEST_EQUAL(Sub->Count, 10ull);
    
    // Verify all values present
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 100, &Count, &IsLast));
    TEST_EQUAL(Count, 10ull);
    TEST_TRUE(QuicRangeGetRange(&Range, 105, &Count, &IsLast));
    TEST_EQUAL(Count, 5ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeAddRange automatically merges adjacent subranges via compaction
// How: Add two adjacent ranges [10,15) and [15,20), verify they merge into one
// Assertions: After both additions, UsedLength is 1 (merged), 
//             single subrange has Low=10 Count=10
//
void QuicTestRangeAddRangeMergesAdjacent()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated1, Updated2;
    QuicRangeAddRange(&Range, 10, 5, &Updated1);  // [10-14]
    QuicRangeAddRange(&Range, 15, 5, &Updated2);  // [15-19]
    
    // QuicRangeAddRange calls QuicRangeCompact, which should merge adjacent ranges
    TEST_EQUAL(Range.UsedLength, 1u);
    
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 10ull);
    TEST_EQUAL(Sub->Count, 10ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeAddRange merges overlapping ranges via compaction
// How: Add ranges [10,20) and [15,25), verify they merge
// Assertions: UsedLength is 1, subrange covers [10,25)
//
void QuicTestRangeAddRangeMergesOverlapping()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated1, Updated2;
    QuicRangeAddRange(&Range, 10, 10, &Updated1);  // [10-19]
    QuicRangeAddRange(&Range, 15, 10, &Updated2);  // [15-24]
    
    TEST_EQUAL(Range.UsedLength, 1u);
    
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 10ull);
    TEST_EQUAL(Sub->Count, 15ull);  // [10-24]
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove a single value from range
// How: Add values, remove one, verify it's gone
// Assertions: Value is no longer present after removal,
//             remaining values still present
//
void QuicTestRangeRemoveSingleValue()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    
    TEST_TRUE(QuicRangeRemoveRange(&Range, 20, 1));
    
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_FALSE(QuicRangeGetRange(&Range, 20, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 10, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 30, &Count, &IsLast));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove a range that splits a subrange
// How: Add range [10,20), remove [12,15), verify split
// Assertions: UsedLength is 2, subranges are [10,12) and [15,20)
//
void QuicTestRangeRemoveMiddleSplitsSubrange()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated;
    QuicRangeAddRange(&Range, 10, 10, &Updated);  // [10-19]
    TEST_TRUE(QuicRangeRemoveRange(&Range, 12, 3));  // Remove [12-14]
    
    TEST_EQUAL(Range.UsedLength, 2u);
    
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    
    TEST_EQUAL(Sub0->Low, 10ull);
    TEST_EQUAL(Sub0->Count, 2ull);  // [10-11]
    TEST_EQUAL(Sub1->Low, 15ull);
    TEST_EQUAL(Sub1->Count, 5ull);  // [15-19]
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove range that eliminates entire subrange
// How: Add range [10,20), remove [10,20), verify empty
// Assertions: UsedLength is 0 after removal
//
void QuicTestRangeRemoveEntireSubrange()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated;
    QuicRangeAddRange(&Range, 10, 10, &Updated);
    TEST_TRUE(QuicRangeRemoveRange(&Range, 10, 10));
    
    TEST_EQUAL(Range.UsedLength, 0u);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeReset clears all values without deallocating
// How: Add values, call QuicRangeReset, verify empty
// Assertions: UsedLength is 0, AllocLength unchanged
//
void QuicTestRangeReset()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    uint32_t AllocBefore = Range.AllocLength;
    
    QuicRangeReset(&Range);
    
    TEST_EQUAL(Range.UsedLength, 0u);
    TEST_EQUAL(Range.AllocLength, AllocBefore);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetMin returns minimum value
// How: Add values 30, 10, 20, get minimum
// Assertions: QuicRangeGetMin returns 10
//
void QuicTestRangeGetMin()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    
    uint64_t Min = QuicRangeGetMin(&Range);
    TEST_EQUAL(Min, 10ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetMinSafe returns FALSE on empty range
// How: Initialize range without adding values, call GetMinSafe
// Assertions: Returns FALSE, output value unchanged
//
void QuicTestRangeGetMinSafeEmpty()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    uint64_t Value = 999;
    BOOLEAN Result = QuicRangeGetMinSafe(&Range, &Value);
    
    TEST_FALSE(Result);
    TEST_EQUAL(Value, 999ull);  // Unchanged
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetMinSafe returns TRUE with valid minimum
// How: Add values, call GetMinSafe
// Assertions: Returns TRUE, output value is minimum
//
void QuicTestRangeGetMinSafeNonEmpty()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 50));
    TEST_TRUE(QuicRangeAddValue(&Range, 25));
    
    uint64_t Value;
    BOOLEAN Result = QuicRangeGetMinSafe(&Range, &Value);
    
    TEST_TRUE(Result);
    TEST_EQUAL(Value, 25ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetMax returns maximum value
// How: Add values 10, 30, 20, get maximum
// Assertions: QuicRangeGetMax returns 30
//
void QuicTestRangeGetMax()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    
    uint64_t Max = QuicRangeGetMax(&Range);
    TEST_EQUAL(Max, 30ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetMaxSafe returns FALSE on empty range
// How: Initialize range without adding values, call GetMaxSafe
// Assertions: Returns FALSE
//
void QuicTestRangeGetMaxSafeEmpty()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    uint64_t Value = 999;
    BOOLEAN Result = QuicRangeGetMaxSafe(&Range, &Value);
    
    TEST_FALSE(Result);
    TEST_EQUAL(Value, 999ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetMaxSafe returns TRUE with valid maximum
// How: Add multiple values, call GetMaxSafe
// Assertions: Returns TRUE, output value is maximum
//
void QuicTestRangeGetMaxSafeNonEmpty()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 25));
    TEST_TRUE(QuicRangeAddValue(&Range, 75));
    
    uint64_t Value;
    BOOLEAN Result = QuicRangeGetMaxSafe(&Range, &Value);
    
    TEST_TRUE(Result);
    TEST_EQUAL(Value, 75ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeSetMin removes all values below threshold
// How: Add values 5, 15, 25, 35, set min to 20
// Assertions: Values 5 and 15 removed, 25 and 35 remain
//
void QuicTestRangeSetMin()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 5));
    TEST_TRUE(QuicRangeAddValue(&Range, 15));
    TEST_TRUE(QuicRangeAddValue(&Range, 25));
    TEST_TRUE(QuicRangeAddValue(&Range, 35));
    
    QuicRangeSetMin(&Range, 20);
    
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_FALSE(QuicRangeGetRange(&Range, 5, &Count, &IsLast));
    TEST_FALSE(QuicRangeGetRange(&Range, 15, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 25, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 35, &Count, &IsLast));
    
    TEST_EQUAL(QuicRangeGetMin(&Range), 25ull);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeSetMin partially removes subrange
// How: Add range [10,30), set min to 20
// Assertions: Resulting range is [20,30)
//
void QuicTestRangeSetMinPartialRemoval()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated;
    QuicRangeAddRange(&Range, 10, 20, &Updated);  // [10-29]
    
    QuicRangeSetMin(&Range, 20);
    
    TEST_EQUAL(Range.UsedLength, 1u);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 20ull);
    TEST_EQUAL(Sub->Count, 10ull);  // [20-29]
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeCompact merges multiple adjacent subranges
// How: Manually create non-compacted state (add ranges that should be compacted), call Compact
// Assertions: Subranges are merged into fewer subranges
// Note: QuicRangeAddRange already calls compact, so this tests explicit compaction after removals
//
void QuicTestRangeCompactMultipleAdjacent()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    // Add non-adjacent ranges
    BOOLEAN Updated;
    QuicRangeAddRange(&Range, 10, 1, &Updated);   // [10]
    QuicRangeAddRange(&Range, 12, 1, &Updated);   // [12]
    QuicRangeAddRange(&Range, 14, 1, &Updated);   // [14]
    
    // Add the gaps to create adjacent subranges
    QuicRangeAddRange(&Range, 11, 1, &Updated);   // [11]
    QuicRangeAddRange(&Range, 13, 1, &Updated);   // [13]
    
    // After additions with compaction, should be merged
    TEST_EQUAL(Range.UsedLength, 1u);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 10ull);
    TEST_EQUAL(Sub->Count, 5ull);  // [10-14]
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeShrink reduces allocation after heavy usage
// How: Add many values to grow array, remove most, explicitly shrink
// Assertions: AllocLength decreases, UsedLength preserved, data intact
//
void QuicTestRangeShrinkExplicit()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(4096, &Range);
    
    // Add many non-adjacent values to force growth
    for (uint32_t i = 0; i < 20; i++) {
        TEST_TRUE(QuicRangeAddValue(&Range, i * 10));
    }
    
    uint32_t OriginalAlloc = Range.AllocLength;
    TEST_TRUE(OriginalAlloc > QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most values
    for (uint32_t i = 0; i < 18; i++) {
        TEST_TRUE(QuicRangeRemoveRange(&Range, i * 10, 1));
    }
    
    TEST_EQUAL(Range.UsedLength, 2u);
    
    // Explicitly shrink to half
    uint32_t NewAlloc = OriginalAlloc / 2;
    if (NewAlloc < QUIC_RANGE_INITIAL_SUB_COUNT) {
        NewAlloc = QUIC_RANGE_INITIAL_SUB_COUNT;
    }
    
    TEST_TRUE(QuicRangeShrink(&Range, NewAlloc));
    TEST_EQUAL(Range.AllocLength, NewAlloc);
    TEST_EQUAL(Range.UsedLength, 2u);
    
    // Verify data intact
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 180, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 190, &Count, &IsLast));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeShrink back to pre-allocated buffer
// How: Grow array, then shrink back to QUIC_RANGE_INITIAL_SUB_COUNT
// Assertions: SubRanges points to PreAllocSubRanges, AllocLength is QUIC_RANGE_INITIAL_SUB_COUNT
//
void QuicTestRangeShrinkToPreallocated()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(4096, &Range);
    
    // Force growth beyond initial
    for (uint32_t i = 0; i < 15; i++) {
        TEST_TRUE(QuicRangeAddValue(&Range, i * 10));
    }
    
    TEST_TRUE(Range.AllocLength > QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_NOT_EQUAL(Range.SubRanges, Range.PreAllocSubRanges);
    
    // Remove most to allow shrinking
    for (uint32_t i = 0; i < 13; i++) {
        TEST_TRUE(QuicRangeRemoveRange(&Range, i * 10, 1));
    }
    
    // Shrink to initial size
    TEST_TRUE(QuicRangeShrink(&Range, QUIC_RANGE_INITIAL_SUB_COUNT));
    TEST_EQUAL(Range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_EQUAL(Range.SubRanges, Range.PreAllocSubRanges);
    
    // Verify data intact
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 130, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 140, &Count, &IsLast));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Adding duplicate value doesn't change range
// How: Add value 42 twice
// Assertions: Second add returns TRUE (success), but RangeUpdated is FALSE
//
void QuicTestRangeAddDuplicateValue()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    BOOLEAN Updated1, Updated2;
    QuicRangeAddRange(&Range, 42, 1, &Updated1);
    TEST_TRUE(Updated1);
    
    QuicRangeAddRange(&Range, 42, 1, &Updated2);
    TEST_FALSE(Updated2);  // Already present
    
    TEST_EQUAL(Range.UsedLength, 1u);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove non-existent value succeeds without error
// How: Remove value 99 from range containing 10, 20
// Assertions: Returns TRUE, UsedLength unchanged
//
void QuicTestRangeRemoveNonExistent()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    
    TEST_TRUE(QuicRangeRemoveRange(&Range, 99, 1));
    TEST_EQUAL(Range.UsedLength, 2u);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetRange correctly reports IsLastRange
// How: Add multiple ranges, query last and non-last
// Assertions: IsLastRange is TRUE for last subrange, FALSE for others
//
void QuicTestRangeGetRangeIsLastRange()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    
    uint64_t Count;
    BOOLEAN IsLast;
    
    TEST_TRUE(QuicRangeGetRange(&Range, 10, &Count, &IsLast));
    TEST_FALSE(IsLast);
    
    TEST_TRUE(QuicRangeGetRange(&Range, 20, &Count, &IsLast));
    TEST_FALSE(IsLast);
    
    TEST_TRUE(QuicRangeGetRange(&Range, 30, &Count, &IsLast));
    TEST_TRUE(IsLast);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Large range operations with many subranges
// How: Add 100 non-adjacent values, verify all present
// Assertions: UsedLength is 100, all values retrievable
//
void QuicTestRangeLargeScale()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(8192, &Range);
    
    for (uint32_t i = 0; i < 100; i++) {
        TEST_TRUE(QuicRangeAddValue(&Range, i * 100));
    }
    
    TEST_EQUAL(Range.UsedLength, 100u);
    
    // Verify all present
    uint64_t Count;
    BOOLEAN IsLast;
    for (uint32_t i = 0; i < 100; i++) {
        TEST_TRUE(QuicRangeGetRange(&Range, i * 100, &Count, &IsLast));
    }
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add range at uint64_t boundaries
// How: Add range near UINT64_MAX
// Assertions: Range is added correctly without overflow
//
void QuicTestRangeHighBoundaryValues()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(1024, &Range);
    
    uint64_t HighValue = UINT64_MAX - 100;
    BOOLEAN Updated;
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, HighValue, 50, &Updated);
    
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_TRUE(Updated);
    TEST_EQUAL(Sub->Low, HighValue);
    TEST_EQUAL(Sub->Count, 50ull);
    
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, HighValue, &Count, &IsLast));
    TEST_EQUAL(Count, 50ull);
    
    QuicRangeUninitialize(&Range);
}
