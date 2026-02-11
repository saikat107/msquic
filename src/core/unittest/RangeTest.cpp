/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit test for the QUIC_RANGE multirange tracker interface.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "RangeTest.cpp.clog.h"
#endif

struct SmartRange {
    QUIC_RANGE range;
    SmartRange(uint32_t MaxAllocSize = QUIC_MAX_RANGE_ALLOC_SIZE) {
        QuicRangeInitialize(MaxAllocSize, &range);
    }
    ~SmartRange() {
        QuicRangeUninitialize(&range);
    }
    void Reset() {
        QuicRangeReset(&range);
    }
    bool TryAdd(uint64_t value) {
        return QuicRangeAddValue(&range, value) != FALSE;
    }
    bool TryAdd(uint64_t low, uint64_t count) {
        BOOLEAN rangeUpdated;
        return QuicRangeAddRange(&range, low, count, &rangeUpdated) != FALSE;
    }
    void Add(uint64_t value) {
        ASSERT_TRUE(TryAdd(value));
    #ifndef LOG_ONLY_FAILURES
        Dump();
    #endif
    }
    void Add(uint64_t low, uint64_t count) {
        ASSERT_TRUE(TryAdd(low, count));
    #ifndef LOG_ONLY_FAILURES
        Dump();
    #endif
    }
    void Remove(uint64_t low, uint64_t count) {
        ASSERT_TRUE(QuicRangeRemoveRange(&range, low, count));
    #ifndef LOG_ONLY_FAILURES
        Dump();
    #endif
    }
    int Find(uint64_t value) {
        QUIC_RANGE_SEARCH_KEY Key = { value, value };
        return QuicRangeSearch(&range, &Key);
    }
    int FindRange(uint64_t value, uint64_t count) {
        QUIC_RANGE_SEARCH_KEY Key = { value, value + count - 1 };
        return QuicRangeSearch(&range, &Key);
    }
    uint64_t Min() {
        uint64_t value;
        EXPECT_EQ(TRUE, QuicRangeGetMinSafe(&range, &value));
        return value;
    }
    uint64_t Max() {
        uint64_t value;
        EXPECT_EQ(TRUE, QuicRangeGetMaxSafe(&range, &value));
        return value;
    }
    uint32_t ValidCount() {
        return QuicRangeSize(&range);
    }
    void Dump() {
#if 0
        std::cerr << ("== Dump == ") << std::endl;
        for (uint32_t i = 0; i < QuicRangeSize(&range); i++) {
            auto cur = QuicRangeGet(&range, i);
            std::cerr << "[" << cur->Low << ":" << cur->Count << "]" << std::endl;
        }
#endif
    }
};

TEST(RangeTest, AddSingle)
{
    SmartRange range;
    range.Add(100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)100);
}

TEST(RangeTest, AddTwoAdjacentBefore)
{
    SmartRange range;
    range.Add(101);
    range.Add(100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)101);
}

TEST(RangeTest, AddTwoAdjacentAfter)
{
    SmartRange range;
    range.Add(100);
    range.Add(101);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)101);
}

TEST(RangeTest, AddTwoSeparateBefore)
{
    SmartRange range;
    range.Add(102);
    range.Add(100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)102);
}

TEST(RangeTest, AddTwoSeparateAfter)
{
    SmartRange range;
    range.Add(100);
    range.Add(102);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)102);
}

TEST(RangeTest, AddThreeMerge)
{
    SmartRange range;
    range.Add(100);
    range.Add(102);
    range.Add(101);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)102);
}

TEST(RangeTest, AddBetween)
{
    SmartRange range;
    range.Add(100);
    range.Add(104);
    range.Add(102);
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)104);
}

TEST(RangeTest, AddRangeSingle)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
}

TEST(RangeTest, AddRangeBetween)
{
    SmartRange range;
    range.Add(100, 50);
    range.Add(300, 50);
    range.Add(200, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)349);
}

TEST(RangeTest, AddRangeTwoAdjacentBefore)
{
    SmartRange range;
    range.Add(200, 100);
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeTwoAdjacentAfter)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(200, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeTwoSeparateBefore)
{
    SmartRange range;
    range.Add(300, 100);
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)399);
}

TEST(RangeTest, AddRangeTwoSeparateAfter)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)399);
}

TEST(RangeTest, AddRangeTwoOverlapBefore1)
{
    SmartRange range;
    range.Add(200, 100);
    range.Add(100, 150);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeTwoOverlapBefore2)
{
    SmartRange range;
    range.Add(200, 100);
    range.Add(100, 200);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeTwoOverlapBefore3)
{
    SmartRange range;
    range.Add(200, 50);
    range.Add(100, 200);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeTwoOverlapAfter1)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(150, 150);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeTwoOverlapAfter2)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(100, 200);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeThreeMerge)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    range.Add(200, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)399);
}

TEST(RangeTest, AddRangeThreeOverlapAndAdjacentAfter1)
{
    SmartRange range;
    range.Add(100, 1);
    range.Add(200, 100);
    range.Add(101, 150);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)299);
}

TEST(RangeTest, AddRangeThreeOverlapAndAdjacentAfter2)
{
    SmartRange range;
    range.Add(100, 1);
    range.Add(200, 100);
    range.Add(101, 299);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)399);
}

TEST(RangeTest, AddRangeThreeOverlapAndAdjacentAfter3)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    range.Add(150, 150);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)399);
}

TEST(RangeTest, AddRangeThreeOverlapAndAdjacentAfter4)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    range.Add(50, 250);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)50);
    ASSERT_EQ(range.Max(), (uint32_t)399);
}

TEST(RangeTest, RemoveRangeBefore)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(0, 99);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(0, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
}

TEST(RangeTest, RemoveRangeAfter)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(201, 99);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(200, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
}

TEST(RangeTest, RemoveRangeFront)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(100, 20);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)120);
    ASSERT_EQ(range.Max(), (uint32_t)199);
}

TEST(RangeTest, RemoveRangeBack)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(180, 20);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)179);
}

TEST(RangeTest, RemoveRangeAll)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)199);
    range.Remove(100, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}

TEST(RangeTest, ExampleAckTest)
{
    SmartRange range;
    range.Add(10000);
    range.Add(10001);
    range.Add(10003);
    range.Add(10002);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10000, 2);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10000, 4);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    range.Add(10005);
    range.Add(10006);
    range.Add(10004);
    range.Add(10007);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10005, 2);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    range.Remove(10004, 1);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10007, 1);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}

TEST(RangeTest, ExampleAckWithLossTest)
{
    SmartRange range;
    range.Add(10000);
    range.Add(10001);
    range.Add(10003);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    range.Add(10002);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10000, 2);
    range.Remove(10003, 1);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10002, 1);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    range.Add(10004);
    range.Add(10005);
    range.Add(10006);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10004, 3);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    range.Add(10008);
    range.Add(10009);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    range.Remove(10008, 2);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}

TEST(RangeTest, AddLots)
{
    SmartRange range;
    for (uint32_t i = 0; i < 400; i += 2) {
        range.Add(i);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)200);
    for (uint32_t i = 0; i < 398; i += 2) {
        range.Remove(i, 1);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
}

TEST(RangeTest, HitMax)
{
    const uint32_t MaxCount = 16;
    SmartRange range(MaxCount * sizeof(QUIC_SUBRANGE));
    for (uint32_t i = 0; i < MaxCount; i++) {
        range.Add(i*2);
    }
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), 0ull);
    ASSERT_EQ(range.Max(), (MaxCount - 1)*2);
    range.Add(MaxCount*2);
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), 2ull);
    ASSERT_EQ(range.Max(), MaxCount*2);
    range.Remove(2, 1);
    ASSERT_EQ(range.ValidCount(), MaxCount - 1);
    ASSERT_EQ(range.Min(), 4ull);
    ASSERT_EQ(range.Max(), MaxCount*2);
    range.Add(0);
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), 0ull);
    ASSERT_EQ(range.Max(), MaxCount*2);
}

TEST(RangeTest, SearchZero)
{
    SmartRange range;
    auto index = range.Find(25);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);
}

TEST(RangeTest, SearchOne)
{
    SmartRange range;
    range.Add(25);

    auto index = range.Find(27);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.Find(26);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.Find(24);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);
    index = range.Find(23);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.Find(25);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
}

TEST(RangeTest, SearchTwo)
{
    SmartRange range;
    range.Add(25);
    range.Add(27);

    auto index = range.Find(28);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.Find(26);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.Find(24);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.Find(27);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.Find(25);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
}

TEST(RangeTest, SearchThree)
{
    SmartRange range;
    range.Add(25);
    range.Add(27);
    range.Add(29);

    auto index = range.Find(30);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 3ul);
    index = range.Find(28);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.Find(26);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.Find(24);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.Find(29);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 2);
    index = range.Find(27);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.Find(25);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
}

TEST(RangeTest, SearchFour)
{
    SmartRange range;
    range.Add(25);
    range.Add(27);
    range.Add(29);
    range.Add(31);

    auto index = range.Find(32);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 4ul);
    index = range.Find(30);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 3ul);
    index = range.Find(28);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.Find(26);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.Find(24);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.Find(29);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 2);
    index = range.Find(27);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.Find(25);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
}

TEST(RangeTest, SearchRangeZero)
{
    SmartRange range;
    auto index = range.FindRange(25, 17);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);
}

TEST(RangeTest, SearchRangeOne)
{
    SmartRange range;
    range.Add(25);

    auto index = range.FindRange(27, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(26, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(22, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);
    index = range.FindRange(21, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.FindRange(23, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(24, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(25, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
}

TEST(RangeTest, SearchRangeTwo)
{
    SmartRange range;
    range.Add(25);
    range.Add(30);

    auto index = range.FindRange(32, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.FindRange(31, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.FindRange(26, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(27, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(28, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(22, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);
    index = range.FindRange(23, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.FindRange(24, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(24, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(25, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(29, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.FindRange(29, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.FindRange(30, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);

    index = range.FindRange(24, 7);
    ASSERT_TRUE(IS_FIND_INDEX(index));
#if QUIC_RANGE_USE_BINARY_SEARCH
    ASSERT_EQ(index, 0);
#else
    ASSERT_EQ(index, 1);
#endif
    index = range.FindRange(25, 6);
    ASSERT_TRUE(IS_FIND_INDEX(index));
#if QUIC_RANGE_USE_BINARY_SEARCH
    ASSERT_EQ(index, 0);
#else
    ASSERT_EQ(index, 1);
#endif
}

TEST(RangeTest, SearchRangeThree)
{
    SmartRange range;
    range.Add(25);
    range.Add(30);
    range.Add(35);

    auto index = range.FindRange(36, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 3ul);
    index = range.FindRange(32, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.FindRange(31, 3);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 2ul);
    index = range.FindRange(26, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(27, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(28, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 1ul);
    index = range.FindRange(22, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);
    index = range.FindRange(23, 2);
    ASSERT_TRUE(IS_INSERT_INDEX(index));
    ASSERT_EQ(INSERT_INDEX_TO_FIND_INDEX(index), 0ul);

    index = range.FindRange(24, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(24, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(25, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 0);
    index = range.FindRange(29, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.FindRange(29, 3);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.FindRange(30, 2);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);

    index = range.FindRange(24, 7);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);
    index = range.FindRange(25, 6);
    ASSERT_TRUE(IS_FIND_INDEX(index));
    ASSERT_EQ(index, 1);

    index = range.FindRange(29, 7);
    ASSERT_TRUE(IS_FIND_INDEX(index));
#if QUIC_RANGE_USE_BINARY_SEARCH
    ASSERT_EQ(index, 1);
#else
    ASSERT_EQ(index, 2);
#endif
    index = range.FindRange(30, 6);
    ASSERT_TRUE(IS_FIND_INDEX(index));
#if QUIC_RANGE_USE_BINARY_SEARCH
    ASSERT_EQ(index, 1);
#else
    ASSERT_EQ(index, 2);
#endif

    index = range.FindRange(24, 12);
    ASSERT_TRUE(IS_FIND_INDEX(index));
#if QUIC_RANGE_USE_BINARY_SEARCH
    ASSERT_EQ(index, 1);
#else
    ASSERT_EQ(index, 2);
#endif
    index = range.FindRange(25, 11);
    ASSERT_TRUE(IS_FIND_INDEX(index));
#if QUIC_RANGE_USE_BINARY_SEARCH
    ASSERT_EQ(index, 1);
#else
    ASSERT_EQ(index, 2);
#endif
}

//
// Tests for new compaction and shrinking functionality (PR #18)
//

// Scenario: Compact an empty range (should be a no-op)
// Tests QuicRangeCompact with empty range - early exit path
// Assertions: No crashes, UsedLength remains 0
TEST(RangeTest, CompactEmptyRange)
{
    SmartRange range;
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 0u);
}

// Scenario: Compact a range with single subrange (should be a no-op)
// Tests QuicRangeCompact with single subrange - early exit path
// Assertions: UsedLength remains 1, subrange unchanged
TEST(RangeTest, CompactSingleSubrange)
{
    SmartRange range;
    range.Add(100, 10);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 109u);
}

// Scenario: Compact with adjacent subranges that should merge
// Tests QuicRangeCompact merging adjacent subranges after manual manipulation
// Assertions: Adjacent subranges merge into one, UsedLength decreases
TEST(RangeTest, CompactAdjacentSubranges)
{
    SmartRange range;
    // Create adjacent ranges
    range.Add(10, 5);    // [10-14]
    range.Add(20, 5);    // [20-24]
    range.Add(15, 5);    // [15-19] fills gap, should merge all three
    
    // After adds, they should already be merged, but test explicit compact
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 24u);
}

// Scenario: Compact with mixed adjacent and non-adjacent subranges
// Tests QuicRangeCompact with realistic mix - only adjacent ones merge
// Assertions: Adjacent ranges merge, gaps remain
TEST(RangeTest, CompactMixedSubranges)
{
    SmartRange range;
    range.Add(10, 5);    // [10-14]
    range.Add(15, 5);    // [15-19] adjacent to first
    range.Add(30, 5);    // [30-34] gap
    range.Add(35, 5);    // [35-39] adjacent to third
    
    // Should result in 2 subranges after initial adds
    ASSERT_EQ(range.ValidCount(), 2u);
    
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(QuicRangeGet(&range.range, 0)->Count, 10u);  // [10-19]
    ASSERT_EQ(QuicRangeGet(&range.range, 1)->Low, 30u);
    ASSERT_EQ(QuicRangeGet(&range.range, 1)->Count, 10u);  // [30-39]
}

// Scenario: Shrink range explicitly using QuicRangeShrink
// Tests QuicRangeShrink with valid NewAllocLength after growth
// Assertions: Returns TRUE, AllocLength decreases, data preserved
TEST(RangeTest, ShrinkRangeExplicitly)
{
    SmartRange range;
    
    // Grow the range first by adding many non-adjacent values
    for (uint64_t i = 0; i < 10; i++) {
        range.Add(i * 10);
    }
    
    ASSERT_EQ(range.ValidCount(), 10u);
    uint32_t oldAlloc = range.range.AllocLength;
    ASSERT_GT(oldAlloc, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Shrink to a smaller allocation
    uint32_t newAlloc = QUIC_RANGE_INITIAL_SUB_COUNT + 2;
    ASSERT_TRUE(QuicRangeShrink(&range.range, newAlloc));
    
    ASSERT_LT(range.range.AllocLength, oldAlloc);
    ASSERT_EQ(range.range.AllocLength, newAlloc);
    ASSERT_EQ(range.ValidCount(), 10u);
    
    // Verify data is preserved
    for (uint64_t i = 0; i < 10; i++) {
        ASSERT_EQ(QuicRangeGet(&range.range, (uint32_t)i)->Low, i * 10);
    }
}

// Scenario: Shrink range back to initial capacity (pre-allocated buffer)
// Tests QuicRangeShrink to QUIC_RANGE_INITIAL_SUB_COUNT uses PreAllocSubRanges
// Assertions: Returns TRUE, SubRanges points to PreAllocSubRanges
TEST(RangeTest, ShrinkToInitialCapacity)
{
    SmartRange range;
    
    // Grow then shrink
    for (uint64_t i = 0; i < 10; i++) {
        range.Add(i * 10);
    }
    
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most values
    for (uint64_t i = 5; i < 10; i++) {
        range.Remove(i * 10, 1);
    }
    
    ASSERT_EQ(range.ValidCount(), 5u);
    
    // Shrink to initial capacity
    ASSERT_TRUE(QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT));
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.range.SubRanges, range.range.PreAllocSubRanges);
}

// Scenario: Test automatic shrinking triggered by removal operations
// Tests that QuicRangeRemoveSubranges can trigger shrinking
// Assertions: Allocation shrinks when usage drops below threshold
TEST(RangeTest, AutoShrinkOnRemoval)
{
    SmartRange range;
    
    // Add many non-adjacent ranges to grow allocation significantly
    for (uint64_t i = 0; i < 40; i++) {
        range.Add(i * 100, 1);
    }
    
    ASSERT_EQ(range.ValidCount(), 40u);
    uint32_t allocAfterGrow = range.range.AllocLength;
    ASSERT_GT(allocAfterGrow, QUIC_RANGE_INITIAL_SUB_COUNT * 2);
    
    // Remove most ranges (leave only a few)
    for (uint32_t i = 0; i < 35; i++) {
        QuicRangeRemoveSubranges(&range.range, 0, 1);
    }
    
    ASSERT_EQ(range.ValidCount(), 5u);
    
    // Allocation should eventually shrink (conditional based on thresholds)
    // The shrink happens when allocation is >= 4x initial and used < 1/8 of allocated
    // With 5 used and 40+ allocated, we should see shrinking
    ASSERT_LE(range.range.AllocLength, allocAfterGrow);
}

// Scenario: Test QuicRangeCompact with potential shrinking
// Tests QuicRangeCompact's shrinking logic when usage is low
// Assertions: Compact can shrink allocation if usage is significantly less than capacity
TEST(RangeTest, CompactWithShrinking)
{
    SmartRange range;
    
    // Add many values then remove most
    for (uint64_t i = 0; i < 50; i++) {
        range.Add(i * 2);
    }
    
    ASSERT_EQ(range.ValidCount(), 50u);
    
    // Remove most values
    for (uint64_t i = 40; i < 50; i++) {
        range.Remove(i * 2, 1);
    }
    for (uint64_t i = 0; i < 38; i++) {
        range.Remove(i * 2, 1);
    }
    
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Compact should trigger shrinking
    QuicRangeCompact(&range.range);
    
    // Verify data is still correct
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 76u);  // 38 * 2
    ASSERT_EQ(range.Max(), 78u);  // 39 * 2
}

// Scenario: Test QuicRangeSetMin with compaction
// Tests that QuicRangeSetMin calls QuicRangeCompact internally
// Assertions: SetMin removes values and potentially triggers compaction
TEST(RangeTest, SetMinWithCompaction)
{
    SmartRange range;
    
    // Add multiple ranges
    range.Add(10, 5);   // [10-14]
    range.Add(20, 5);   // [20-24]
    range.Add(30, 5);   // [30-34]
    range.Add(40, 5);   // [40-44]
    
    ASSERT_EQ(range.ValidCount(), 4u);
    
    // Set minimum to 22 - should remove first range and trim second
    QuicRangeSetMin(&range.range, 22);
    
    ASSERT_EQ(range.ValidCount(), 3u);
    ASSERT_EQ(range.Min(), 22u);
    ASSERT_EQ(range.Max(), 44u);
}

// Scenario: Test QuicRangeRemoveRange with middle split and compaction
// Tests that removing from middle and then compacting works correctly
// Assertions: Split creates two subranges, data is preserved
TEST(RangeTest, RemoveMiddleWithCompaction)
{
    SmartRange range;
    
    range.Add(100, 50);  // [100-149]
    
    ASSERT_EQ(range.ValidCount(), 1u);
    
    // Remove from middle, splitting into two
    range.Remove(120, 10);  // Remove [120-129]
    
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
    ASSERT_EQ(QuicRangeGet(&range.range, 0)->Count, 20u);  // [100-119]
    ASSERT_EQ(QuicRangeGet(&range.range, 1)->Low, 130u);
    ASSERT_EQ(QuicRangeGet(&range.range, 1)->Count, 20u);  // [130-149]
    
    // Compact should be a no-op (no adjacent ranges)
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 2u);
}

// Scenario: Stress test with many add/remove/compact operations
// Tests range behavior under repeated operations to verify memory management
// Assertions: No crashes, final state is consistent
TEST(RangeTest, StressTestWithCompaction)
{
    SmartRange range;
    
    // Add many values
    for (uint64_t i = 0; i < 100; i++) {
        range.Add(i * 2);  // Even numbers
    }
    
    ASSERT_EQ(range.ValidCount(), 100u);
    
    // Remove half
    for (uint64_t i = 0; i < 50; i++) {
        range.Remove(i * 4, 1);  // Remove every other
    }
    
    // Compact
    QuicRangeCompact(&range.range);
    
    // Verify we have a consistent state
    ASSERT_GT(range.ValidCount(), 0u);
    ASSERT_LE(range.ValidCount(), range.range.AllocLength);
    
    // Add more values
    for (uint64_t i = 0; i < 50; i++) {
        range.Add(i * 2 + 1);  // Odd numbers
    }
    
    // Final compact
    QuicRangeCompact(&range.range);
    
    ASSERT_GT(range.ValidCount(), 0u);
}

// Scenario: Test duplicate value addition with RangeUpdated flag
// Tests QuicRangeAddRange RangeUpdated output for duplicate values
// Assertions: RangeUpdated is FALSE when adding existing values
TEST(RangeTest, AddDuplicateCheckUpdatedFlag)
{
    SmartRange range;
    
    BOOLEAN updated;
    ASSERT_NE(QuicRangeAddRange(&range.range, 100, 10, &updated), nullptr);
    ASSERT_TRUE(updated);  // First add updates
    
    // Add overlapping/duplicate range
    ASSERT_NE(QuicRangeAddRange(&range.range, 105, 1, &updated), nullptr);
    ASSERT_FALSE(updated);  // Should not update since 105 is already in [100-109]
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 109u);
}

// Scenario: Test GetRange with IsLastRange flag for multiple subranges
// Tests QuicRangeGetRange IsLastRange output correctness
// Assertions: IsLastRange is TRUE only for last subrange
TEST(RangeTest, GetRangeIsLastFlag)
{
    SmartRange range;
    
    range.Add(100, 10);  // [100-109]
    range.Add(200, 10);  // [200-209]
    
    uint64_t count;
    BOOLEAN isLast;
    
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 100, &count, &isLast));
    ASSERT_FALSE(isLast);  // Not the last subrange
    ASSERT_EQ(count, 10u);
    
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 200, &count, &isLast));
    ASSERT_TRUE(isLast);   // Is the last subrange
    ASSERT_EQ(count, 10u);
}

// Scenario: Test range operations with maximum allocation limit enforced
// Tests that MaxAllocSize limits are respected during growth
// Assertions: Growth stops at MaxAllocSize, aging occurs when limit hit
TEST(RangeTest, MaxAllocationLimitWithCompaction)
{
    const uint32_t MaxCount = 20;
    SmartRange range(MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Add up to limit
    for (uint64_t i = 0; i < MaxCount; i++) {
        range.Add(i * 10);
    }
    
    // May be less than MaxCount if allocation reached limit and aged out values
    ASSERT_LE(range.ValidCount(), MaxCount);
    ASSERT_LE(range.range.AllocLength * sizeof(QUIC_SUBRANGE), MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Try to add more (should age out old values if at limit)
    for (uint64_t i = MaxCount; i < MaxCount + 5; i++) {
        range.Add(i * 10);
    }
    
    // Should still be at or under max allocation
    ASSERT_LE(range.range.AllocLength * sizeof(QUIC_SUBRANGE), MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Compact to clean up
    QuicRangeCompact(&range.range);
    ASSERT_LE(range.range.AllocLength * sizeof(QUIC_SUBRANGE), MaxCount * sizeof(QUIC_SUBRANGE));
}

// Scenario: Test QuicRangeReset preserves allocation but clears contents
// Tests Reset followed by operations to verify allocation reuse
// Assertions: After reset, AllocLength unchanged, can reuse allocation
TEST(RangeTest, ResetPreservesAllocation)
{
    SmartRange range;
    
    // Grow the range
    for (uint64_t i = 0; i < 15; i++) {
        range.Add(i * 10);
    }
    
    uint32_t allocBeforeReset = range.range.AllocLength;
    ASSERT_GT(allocBeforeReset, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Reset
    range.Reset();
    
    ASSERT_EQ(range.ValidCount(), 0u);
    ASSERT_EQ(range.range.AllocLength, allocBeforeReset);  // Allocation preserved
    
    // Reuse - should not need to reallocate
    for (uint64_t i = 0; i < 10; i++) {
        range.Add(i * 5);
    }
    
    ASSERT_EQ(range.ValidCount(), 10u);
    ASSERT_EQ(range.range.AllocLength, allocBeforeReset);  // Still same allocation
}
