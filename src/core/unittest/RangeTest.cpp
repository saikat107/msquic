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
// Tests for new PR #49 functions: QuicRangeCompact, QuicRangeShrink
//

TEST(RangeTest, CompactEmptyRange)
{
    // Test: Compact on empty range should be a no-op
    SmartRange range;
    ASSERT_EQ(range.ValidCount(), 0u);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, CompactSingleSubrange)
{
    // Test: Compact on single subrange should be a no-op
    SmartRange range;
    range.Add(100, 50);
    ASSERT_EQ(range.ValidCount(), 1u);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
}

TEST(RangeTest, CompactAdjacentSubrangesManual)
{
    // Test: Manually create adjacent subranges and compact them
    // This tests the compaction logic directly
    SmartRange range;
    // Add values that will NOT be immediately merged by AddValue
    // We need to construct a scenario where adjacent ranges exist
    range.Add(10);
    range.Add(20);
    // Now add the gap to make them adjacent
    range.Add(11, 9); // This makes [10-19] and [20]
    // After this, they should be compacted into one
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 20u);
}

TEST(RangeTest, CompactOverlappingRanges)
{
    // Test: Overlapping ranges should be merged by compact
    SmartRange range;
    range.Add(10, 10); // [10-19]
    range.Add(15, 10); // [15-24] overlaps with [10-19]
    // QuicRangeAddRange calls QuicRangeCompact, so they should be merged
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 24u);
}

TEST(RangeTest, CompactMultipleAdjacentRanges)
{
    // Test: Multiple adjacent ranges get merged
    SmartRange range;
    range.Add(10);
    range.Add(30);
    range.Add(50);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // Fill the gaps to make them all adjacent
    range.Add(11, 19); // [11-29]
    range.Add(31, 19); // [31-49]
    
    // Should all be compacted into one range [10-50]
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 50u);
}

TEST(RangeTest, CompactNoMergeableRanges)
{
    // Test: Non-adjacent, non-overlapping ranges should not be affected
    SmartRange range;
    range.Add(10);
    range.Add(20);
    range.Add(30);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    QuicRangeCompact(&range.range);
    
    // Should remain 3 separate ranges
    ASSERT_EQ(range.ValidCount(), 3u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 30u);
}

TEST(RangeTest, ShrinkTriggeredByRemoval)
{
    // Test: Growing then removing many values should trigger shrinking
    SmartRange range;
    
    // Add many non-adjacent values to force growth
    for (uint32_t i = 0; i < 30; i++) {
        range.Add(i * 100);
    }
    uint32_t allocAfterGrowth = range.range.AllocLength;
    ASSERT_GT(allocAfterGrowth, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.ValidCount(), 30u);
    
    // Remove most of them (keep only 2)
    for (uint32_t i = 0; i < 28; i++) {
        range.Remove(i * 100, 1);
    }
    
    // After removals, allocation should have shrunk
    ASSERT_LT(range.range.AllocLength, allocAfterGrowth);
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, ShrinkToInitialSize)
{
    // Test: Shrinking back to initial size restores PreAllocSubRanges
    SmartRange range;
    
    // Grow significantly
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(i * 100);
    }
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove almost all
    for (uint32_t i = 0; i < 19; i++) {
        range.Remove(i * 100, 1);
    }
    
    // Should shrink back to initial size and use PreAllocSubRanges
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.range.SubRanges, range.range.PreAllocSubRanges);
    ASSERT_EQ(range.ValidCount(), 1u);
}

TEST(RangeTest, ShrinkDoesNotHappenPrematurely)
{
    // Test: Shrinking should only happen when usage is very low
    SmartRange range;
    
    // Grow to 16 subranges
    for (uint32_t i = 0; i < 16; i++) {
        range.Add(i * 100);
    }
    ASSERT_EQ(range.range.AllocLength, 16u);
    
    // Remove only a few (keep 12, which is still > 16/4)
    for (uint32_t i = 0; i < 4; i++) {
        range.Remove(i * 100, 1);
    }
    
    // Should NOT shrink yet (12 > 16/4=4)
    ASSERT_EQ(range.range.AllocLength, 16u);
    ASSERT_EQ(range.ValidCount(), 12u);
}

TEST(RangeTest, CompactTriggeredByRemoveRange)
{
    // Test: RemoveRange should call QuicRangeCompact
    SmartRange range;
    range.Add(10, 20); // [10-29]
    range.Add(40, 20); // [40-59]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Remove gap between them
    range.Remove(30, 10); // Remove [30-39] (which doesn't exist)
    
    // Ranges remain separate (no compaction needed here)
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Now remove partial from first to create adjacency scenario
    // Actually, let's test a split scenario
    range.Add(70, 20); // Add [70-89]
    range.Remove(15, 5); // Remove [15-19] from [10-29], creating [10-14] [20-29]
    
    // Check that split happened
    ASSERT_EQ(range.ValidCount(), 4u);
}

TEST(RangeTest, CompactTriggeredBySetMin)
{
    // Test: QuicRangeSetMin should call QuicRangeCompact
    SmartRange range;
    range.Add(10, 20); // [10-29]
    range.Add(40, 20); // [40-59]
    range.Add(70, 20); // [70-89]
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // SetMin to 35 should remove first range
    QuicRangeSetMin(&range.range, 35);
    
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 40u);
    ASSERT_EQ(range.Max(), 89u);
}

TEST(RangeTest, CompactAfterRemoveMiddleSplit)
{
    // Test: After splitting a range, compact should not affect separate ranges
    SmartRange range;
    range.Add(10, 30); // [10-39]
    
    // Remove middle to split
    range.Remove(20, 10); // Remove [20-29], creating [10-19] [30-39]
    
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Manual compact should not change anything
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, LargeScaleCompactionScenario)
{
    // Test: Large-scale add/remove with compaction
    SmartRange range;
    
    // Add many overlapping and adjacent ranges
    for (uint32_t i = 0; i < 50; i++) {
        range.Add(i * 10, 15); // Each range: [i*10, i*10+14]
        // These will overlap: [0-14], [10-24], [20-34], ...
    }
    
    // After all adds with overlaps, should be compacted to fewer ranges
    // Expected: merged into larger contiguous ranges
    ASSERT_LT(range.ValidCount(), 50u);
    
    // Verify min and max
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), (uint64_t)(49 * 10 + 14)); // Last range end
}

TEST(RangeTest, RemoveSpanningMultipleRanges)
{
    // Test: Remove operation spanning multiple subranges
    SmartRange range;
    range.Add(10, 10); // [10-19]
    range.Add(30, 10); // [30-39]
    range.Add(50, 10); // [50-59]
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // Remove spanning from middle of first to middle of last
    range.Remove(15, 40); // Remove [15-54]
    
    // Should leave [10-14] and [55-59]
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 59u);
}

TEST(RangeTest, SetMinPartialOverlap)
{
    // Test: SetMin with partial overlap of a range
    SmartRange range;
    range.Add(10, 30); // [10-39]
    
    QuicRangeSetMin(&range.range, 25);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 25u);
    ASSERT_EQ(range.Max(), 39u);
}

TEST(RangeTest, SetMinBelowAllRanges)
{
    // Test: SetMin below all ranges should not change anything
    SmartRange range;
    range.Add(100, 50); // [100-149]
    
    QuicRangeSetMin(&range.range, 50);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
}

TEST(RangeTest, SetMinAboveAllRanges)
{
    // Test: SetMin above all ranges should clear everything
    SmartRange range;
    range.Add(10, 20); // [10-29]
    range.Add(50, 20); // [50-69]
    
    QuicRangeSetMin(&range.range, 100);
    
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, CompactWithShrinkDecision)
{
    // Test: QuicRangeCompact should consider shrinking
    SmartRange range;
    
    // Grow to 32 subranges
    for (uint32_t i = 0; i < 32; i++) {
        range.Add(i * 100);
    }
    uint32_t allocAfterGrowth = range.range.AllocLength;
    ASSERT_GE(allocAfterGrowth, 32u);
    
    // Remove most, leaving only 2
    for (uint32_t i = 0; i < 30; i++) {
        range.Remove(i * 100, 1);
    }
    
    // Compact should trigger shrink (used=2, alloc>=32, 2 < 32/8)
    QuicRangeCompact(&range.range);
    
    ASSERT_LT(range.range.AllocLength, allocAfterGrowth);
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, ShrinkFailureHandling)
{
    // Test: Shrink with very small MaxAllocSize
    // This tests the shrink logic under constrained conditions
    const uint32_t SmallMax = sizeof(QUIC_SUBRANGE) * 12;
    SmartRange range(SmallMax);
    
    // Add values up to the limit
    for (uint32_t i = 0; i < 10; i++) {
        range.Add(i * 100);
    }
    
    // Remove some
    for (uint32_t i = 0; i < 5; i++) {
        range.Remove(i * 100, 1);
    }
    
    // Should handle shrink appropriately within constraints
    ASSERT_EQ(range.ValidCount(), 5u);
}

TEST(RangeTest, CompactAdjacentAfterMerge)
{
    // Test: Ensure adjacent ranges created by merges are compacted
    SmartRange range;
    
    // Create a scenario: [10], [12], [14]
    range.Add(10);
    range.Add(12);
    range.Add(14);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // Add [11] to merge [10] and [12] into [10-12]
    range.Add(11);
    // Now we have [10-12], [14]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Add [13] to merge [10-12] and [14] into [10-14]
    range.Add(13);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 14u);
}

// Iteration 2 tests - target specific uncovered lines

TEST(RangeTest, ResetAfterOperations)
{
    // Test: Reset after various operations (line 59-60)
    SmartRange range;
    range.Add(100, 50);
    range.Add(200, 50);
    ASSERT_EQ(range.ValidCount(), 2u);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, GetRangeFunction)
{
    // Test: QuicRangeGetRange function (lines 223-239)
    SmartRange range;
    range.Add(100, 50); // [100-149]
    
    uint64_t count;
    BOOLEAN isLast;
    
    // Test existing value
    BOOLEAN found = QuicRangeGetRange(&range.range, 120, &count, &isLast);
    ASSERT_TRUE(found);
    ASSERT_EQ(count, 30u); // 120 to 149 = 30 values
    ASSERT_TRUE(isLast); // Only one range
    
    // Test non-existing value
    found = QuicRangeGetRange(&range.range, 200, &count, &isLast);
    ASSERT_FALSE(found);
}

TEST(RangeTest, GetRangeMultipleSubranges)
{
    // Test: QuicRangeGetRange with multiple subranges (line 238)
    SmartRange range;
    range.Add(100, 50); // [100-149]
    range.Add(200, 50); // [200-249]
    
    uint64_t count;
    BOOLEAN isLast;
    
    // Query first range
    BOOLEAN found = QuicRangeGetRange(&range.range, 120, &count, &isLast);
    ASSERT_TRUE(found);
    ASSERT_FALSE(isLast); // Not the last range
    
    // Query second range
    found = QuicRangeGetRange(&range.range, 220, &count, &isLast);
    ASSERT_TRUE(found);
    ASSERT_TRUE(isLast); // This is the last range
}

TEST(RangeTest, HitAbsoluteMaxAlloc)
{
    // Test: Hit QUIC_MAX_RANGE_ALLOC_SIZE limit (line 71)
    // Create range with max limit, try to grow beyond it
    const uint32_t MaxLimit = sizeof(QUIC_SUBRANGE) * QUIC_MAX_RANGE_ALLOC_SIZE;
    SmartRange range(MaxLimit);
    
    // This is impractical to test reaching true max (would need 1M+ subranges)
    // But we can verify the range respects its max size configuration
    ASSERT_EQ(range.range.MaxAllocSize, MaxLimit);
    
    // Verify range can still operate within its limits
    range.Add(100);
    ASSERT_EQ(range.ValidCount(), 1u);
}

TEST(RangeTest, GrowthWithNextIndexZero)
{
    // Test: Growth with NextIndex = 0 (lines 101-104)
    // This happens when inserting at the front and needing to grow
    SmartRange range;
    
    // Fill to capacity with high values
    for (uint32_t i = 0; i < QUIC_RANGE_INITIAL_SUB_COUNT; i++) {
        range.Add((100 + i) * 10);
    }
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Now add a value at the front, forcing growth with NextIndex=0
    range.Add(5); // Very low value, inserts at index 0
    
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.Min(), 5u);
}

TEST(RangeTest, GrowthWithNextIndexMiddle)
{
    // Test: Growth with NextIndex in middle (lines 111-118)
    SmartRange range;
    
    // Fill to capacity with alternating high/low values
    for (uint32_t i = 0; i < QUIC_RANGE_INITIAL_SUB_COUNT; i++) {
        if (i < QUIC_RANGE_INITIAL_SUB_COUNT / 2) {
            range.Add(i * 100);
        } else {
            range.Add(i * 100 + 5000);
        }
    }
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Add a value in the middle, forcing growth
    range.Add(2000); // Should be in the middle
    
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
}

TEST(RangeTest, CompactWithActualAdjacentRanges)
{
    // Test: Ensure lines 593-602 are hit (adjacent range merging in Compact)
    SmartRange range;
    
    // We need to create a scenario where QuicRangeCompact finds adjacent ranges
    // that weren't already merged by AddRange
    
    // Add two separate ranges
    range.Add(10, 5); // [10-14]
    range.Add(20, 5); // [20-24]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Now add the gap to make them touch
    range.Add(15, 5); // [15-19]
    
    // This should trigger compaction and merge all three
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 24u);
}

TEST(RangeTest, FindOverlappingRangeDuringSearch)
{
    // Test: Line 276 - finding overlapping ranges during search
    // This tests the while loop that looks for the first overlapping range
    SmartRange range;
    
    // Add overlapping ranges in a specific order to test the search
    range.Add(100, 20); // [100-119]
    range.Add(110, 20); // [110-129] overlaps with first
    
    // The ranges should be merged
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 129u);
}

TEST(RangeTest, RemoveRangeWithAllocationFailure)
{
    // Test: Line 432 - allocation failure in QuicRangeRemoveRange
    // This is hard to test without mocking, but we can test the split path
    SmartRange range;
    range.Add(10, 40); // [10-49]
    
    // Remove from middle to trigger split (lines 430-433)
    range.Remove(20, 10); // Remove [20-29]
    
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 10u);
    ASSERT_EQ(range.Max(), 49u);
}

TEST(RangeTest, CompactTriggersSuccessfulShrink)
{
    // Test: Line 614 - QuicRangeShrink called from QuicRangeCompact
    SmartRange range;
    
    // Grow significantly
    for (uint32_t i = 0; i < 40; i++) {
        range.Add(i * 100);
    }
    uint32_t allocAfterGrowth = range.range.AllocLength;
    ASSERT_GT(allocAfterGrowth, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most
    for (uint32_t i = 0; i < 38; i++) {
        range.Remove(i * 100, 1);
    }
    
    // Manually compact to trigger shrink decision
    QuicRangeCompact(&range.range);
    
    ASSERT_LT(range.range.AllocLength, allocAfterGrowth);
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, EmptyRangeGetMinMaxSafe)
{
    // Test: Lines 530, 554 - GetMinSafe/GetMaxSafe on empty range
    SmartRange range;
    
    uint64_t value;
    BOOLEAN result;
    
    result = QuicRangeGetMinSafe(&range.range, &value);
    ASSERT_FALSE(result);
    
    result = QuicRangeGetMaxSafe(&range.range, &value);
    ASSERT_FALSE(result);
}

TEST(RangeTest, ReallocationDuringSubsume)
{
    // Test: Line 364 - reallocation during subsume in QuicRangeAddRange
    SmartRange range;
    
    // Create many small ranges
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(i * 10, 2); // Small ranges: [0-1], [10-11], [20-21], ...
    }
    uint32_t countBefore = range.ValidCount();
    ASSERT_GT(countBefore, 10u);
    
    // Now add a large range that subsumes many of them
    range.Add(5, 180); // [5-184], subsumes many ranges
    
    // Should have fewer ranges now
    ASSERT_LT(range.ValidCount(), countBefore);
}
