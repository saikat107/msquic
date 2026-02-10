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
// Test QuicRangeReset: Verify that reset clears all values from a non-empty range.
// The test adds multiple ranges, confirms they exist, then resets and verifies the
// range is empty. Asserts that UsedLength becomes 0 while allocation remains.
//
TEST(RangeTest, ResetNonEmptyRange)
{
    SmartRange range;
    range.Add(100, 50);
    range.Add(200, 50);
    range.Add(300, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)349);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    
    // After reset, should be able to add again
    range.Add(500);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint32_t)500);
    ASSERT_EQ(range.Max(), (uint32_t)500);
}

//
// Test QuicRangeGetRange: Query a value that exists in the range and verify
// the returned contiguous count and whether it's the last range. This tests
// the basic success path of GetRange with a value in the middle of a subrange.
//
TEST(RangeTest, GetRangeValueExists)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    
    uint64_t count;
    BOOLEAN isLast;
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 120, &count, &isLast));
    ASSERT_EQ(count, (uint64_t)30);  // 120 to 149 = 30 values
    ASSERT_FALSE(isLast);  // Not the last range
}

//
// Test QuicRangeGetRange: Query a value in the last subrange and verify
// IsLastRange is set to TRUE. This ensures the last range detection works.
//
TEST(RangeTest, GetRangeValueInLastRange)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    
    uint64_t count;
    BOOLEAN isLast;
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 220, &count, &isLast));
    ASSERT_EQ(count, (uint64_t)30);  // 220 to 249 = 30 values
    ASSERT_TRUE(isLast);  // This is the last range
}

//
// Test QuicRangeGetRange: Query a value that doesn't exist in the range.
// Verifies that GetRange returns FALSE for values not in any subrange.
//
TEST(RangeTest, GetRangeValueNotExists)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    
    uint64_t count;
    BOOLEAN isLast;
    ASSERT_FALSE(QuicRangeGetRange(&range.range, 175, &count, &isLast));
    ASSERT_FALSE(QuicRangeGetRange(&range.range, 50, &count, &isLast));
    ASSERT_FALSE(QuicRangeGetRange(&range.range, 300, &count, &isLast));
}

//
// Test QuicRangeGetRange: Query on an empty range.
// Verifies that GetRange returns FALSE when the range has no values.
//
TEST(RangeTest, GetRangeEmptyRange)
{
    SmartRange range;
    
    uint64_t count;
    BOOLEAN isLast;
    ASSERT_FALSE(QuicRangeGetRange(&range.range, 100, &count, &isLast));
}

//
// Test QuicRangeGetMinSafe/GetMaxSafe: Verify safe get operations on empty range.
// These functions should return FALSE when the range is empty, preventing undefined behavior.
//
TEST(RangeTest, GetMinMaxSafeEmptyRange)
{
    SmartRange range;
    uint64_t value;
    
    ASSERT_FALSE(QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_FALSE(QuicRangeGetMaxSafe(&range.range, &value));
}

//
// Test QuicRangeGetMinSafe/GetMaxSafe: Verify safe get operations on non-empty range.
// These functions should return TRUE and set the correct min/max values.
//
TEST(RangeTest, GetMinMaxSafeNonEmptyRange)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    
    uint64_t minVal, maxVal;
    ASSERT_TRUE(QuicRangeGetMinSafe(&range.range, &minVal));
    ASSERT_EQ(minVal, (uint64_t)100);
    
    ASSERT_TRUE(QuicRangeGetMaxSafe(&range.range, &maxVal));
    ASSERT_EQ(maxVal, (uint64_t)249);
}

//
// Test QuicRangeSetMin: Remove all values below a specified minimum.
// Verifies that SetMin removes subranges entirely below the min value
// and trims subranges that span the min value.
//
TEST(RangeTest, SetMinRemovesValuesBelow)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    range.Add(300, 50);  // [300-349]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    QuicRangeSetMin(&range.range, 220);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)220);
    ASSERT_EQ(range.Max(), (uint64_t)349);
}

//
// Test QuicRangeSetMin: Set min to a value below all existing values.
// Verifies that SetMin does nothing when the min is already below all values.
//
TEST(RangeTest, SetMinBelowAllValues)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    QuicRangeSetMin(&range.range, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)249);
}

//
// Test QuicRangeSetMin: Set min to trim the first subrange without removing it.
// Verifies that a subrange spanning the min value is correctly trimmed.
//
TEST(RangeTest, SetMinTrimsFirstSubrange)
{
    SmartRange range;
    range.Add(100, 100);  // [100-199]
    range.Add(300, 50);   // [300-349]
    
    QuicRangeSetMin(&range.range, 150);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)150);
    ASSERT_EQ(range.Max(), (uint64_t)349);
    
    // Verify the first subrange is trimmed correctly
    auto sub = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub->Low, (uint64_t)150);
    ASSERT_EQ(sub->Count, (uint64_t)50);  // 150-199 = 50 values
}

//
// Test QuicRangeRemoveRange: Remove a range in the middle of a subrange, causing a split.
// Verifies that removing from the middle creates two separate subranges.
//
TEST(RangeTest, RemoveRangeMiddleSplit)
{
    SmartRange range;
    range.Add(100, 100);  // [100-199]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Remove(130, 40);  // Remove [130-169]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Verify first subrange [100-129]
    auto sub1 = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub1->Low, (uint64_t)100);
    ASSERT_EQ(sub1->Count, (uint64_t)30);
    
    // Verify second subrange [170-199]
    auto sub2 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub2->Low, (uint64_t)170);
    ASSERT_EQ(sub2->Count, (uint64_t)30);
}

//
// Test QuicRangeRemoveRange: Remove a range spanning multiple subranges.
// Verifies that remove can handle removal across multiple disjoint subranges.
//
TEST(RangeTest, RemoveRangeSpanningMultiple)
{
    SmartRange range;
    range.Add(100, 50);   // [100-149]
    range.Add(200, 50);   // [200-249]
    range.Add(300, 50);   // [300-349]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    range.Remove(120, 200);  // Remove [120-319] - spans all three
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Should have [100-119] and [320-349]
    ASSERT_EQ(range.Min(), (uint64_t)100);
    auto sub1 = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub1->Low, (uint64_t)100);
    ASSERT_EQ(sub1->Count, (uint64_t)20);
    
    auto sub2 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub2->Low, (uint64_t)320);
    ASSERT_EQ(sub2->Count, (uint64_t)30);
}

//
// Test QuicRangeCompact: Explicitly test compaction merging adjacent subranges.
// Manually creates adjacent subranges and verifies they are merged by Compact.
//
TEST(RangeTest, CompactMergesAdjacentRanges)
{
    SmartRange range;
    // Add ranges that will become adjacent after another operation
    range.Add(100, 50);   // [100-149]
    range.Add(150, 50);   // [150-199] - should already be merged by Add
    range.Add(200, 50);   // [200-249] - should already be merged
    
    // All should be merged into one range
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)249);
}

//
// Test QuicRangeCompact: Test compaction on a range with no adjacent subranges.
// Verifies that Compact doesn't incorrectly merge non-adjacent ranges.
//
TEST(RangeTest, CompactNoMergeNeeded)
{
    SmartRange range;
    range.Add(100, 10);   // [100-109]
    range.Add(200, 10);   // [200-209]
    range.Add(300, 10);   // [300-309]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    QuicRangeCompact(&range.range);
    
    // Should still be 3 separate ranges
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)309);
}

//
// Test growth from pre-allocated buffer to heap allocation.
// Verifies that the range can grow beyond the initial 8 subranges by allocating
// heap memory. Asserts that after adding many separate values, the allocation grows.
//
TEST(RangeTest, GrowFromPreAllocToHeap)
{
    SmartRange range;
    // Add 10 separate values to force growth beyond initial 8
    for (uint32_t i = 0; i < 10; i++) {
        range.Add(i * 100, 1);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)10);
    ASSERT_EQ(range.Min(), (uint64_t)0);
    ASSERT_EQ(range.Max(), (uint64_t)900);
}

//
// Test that adding values when at maximum capacity causes oldest values to be evicted.
// Sets a limited MaxAllocSize, fills to capacity, then adds more values to trigger
// aging out of oldest values. Asserts that oldest values are removed.
//
TEST(RangeTest, HitMaxAndAgeOut)
{
    const uint32_t MaxCount = 8;
    SmartRange range(MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Fill to max with separate values
    for (uint32_t i = 0; i < MaxCount; i++) {
        range.Add(i * 10, 1);
    }
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), (uint64_t)0);
    
    // Add a value that should cause aging out of oldest
    range.Add(MaxCount * 10, 1);
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), (uint64_t)10);  // First value (0) should be aged out
    ASSERT_EQ(range.Max(), (uint64_t)MaxCount * 10);
}

//
// Test shrinking after removing many subranges.
// Creates many subranges, then removes most of them to trigger shrinking.
// Verifies that the range still functions correctly after shrinking.
//
TEST(RangeTest, ShrinkAfterManyRemovals)
{
    SmartRange range;
    
    // Add many separate ranges to force heap allocation
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)20);
    
    // Remove most of them to trigger shrinking
    for (uint32_t i = 0; i < 18; i++) {
        range.Remove(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)1800);
    ASSERT_EQ(range.Max(), (uint64_t)1909);
}

//
// Test adding a range that merges with multiple existing subranges.
// Creates several disjoint subranges, then adds a range that bridges them all.
// Asserts that all subranges are merged into one.
//
TEST(RangeTest, AddRangeMergingMultipleSubranges)
{
    SmartRange range;
    range.Add(100, 10);   // [100-109]
    range.Add(120, 10);   // [120-129]
    range.Add(140, 10);   // [140-149]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    // Add range [105-145] that overlaps/bridges all three
    range.Add(105, 41);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)149);
}

//
// Test QuicRangeGetRange at the exact start of a subrange.
// Verifies that querying the first value of a subrange returns the full count.
//
TEST(RangeTest, GetRangeAtSubrangeStart)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    uint64_t count;
    BOOLEAN isLast;
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 100, &count, &isLast));
    ASSERT_EQ(count, (uint64_t)50);  // Full range from start
    ASSERT_TRUE(isLast);  // Only one range
}

//
// Test QuicRangeGetRange at the exact end of a subrange.
// Verifies that querying the last value returns count of 1.
//
TEST(RangeTest, GetRangeAtSubrangeEnd)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    uint64_t count;
    BOOLEAN isLast;
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 149, &count, &isLast));
    ASSERT_EQ(count, (uint64_t)1);  // Only one value left
    ASSERT_TRUE(isLast);
}

//
// Test SetMin that removes all subranges entirely.
// Sets min above all existing values, resulting in an empty range.
//
TEST(RangeTest, SetMinRemovesAllSubranges)
{
    SmartRange range;
    range.Add(100, 50);   // [100-149]
    range.Add(200, 50);   // [200-249]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    QuicRangeSetMin(&range.range, 300);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}

//
// Test adding range with Count=1 (edge case equivalent to AddValue).
// Verifies that AddRange with Count=1 behaves correctly.
//
TEST(RangeTest, AddRangeWithCountOne)
{
    SmartRange range;
    BOOLEAN updated;
    
    auto result = QuicRangeAddRange(&range.range, 100, 1, &updated);
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(updated);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)100);
}

//
// Test adding a range that's already present (no update).
// Verifies that RangeUpdated is set to FALSE when adding an existing range.
//
TEST(RangeTest, AddRangeAlreadyPresent)
{
    SmartRange range;
    BOOLEAN updated1, updated2;
    
    auto result1 = QuicRangeAddRange(&range.range, 100, 50, &updated1);
    ASSERT_NE(result1, nullptr);
    ASSERT_TRUE(updated1);
    
    // Add the same range again
    auto result2 = QuicRangeAddRange(&range.range, 110, 20, &updated2);
    ASSERT_NE(result2, nullptr);
    ASSERT_FALSE(updated2);  // Should not be updated, already present
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
}

//
// Test removing a single value from a larger range (creates split).
// Removes one value from the middle of a range, splitting it into two.
//
TEST(RangeTest, RemoveSingleValueFromMiddle)
{
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Remove(105, 1);  // Remove just 105
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Should have [100-104] and [106-109]
    auto sub1 = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub1->Low, (uint64_t)100);
    ASSERT_EQ(sub1->Count, (uint64_t)5);
    
    auto sub2 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub2->Low, (uint64_t)106);
    ASSERT_EQ(sub2->Count, (uint64_t)4);
}

//
// Test edge case with large values near UINT64_MAX.
// Verifies that the range can handle values close to the maximum 64-bit value.
//
TEST(RangeTest, LargeValuesNearMaxUint64)
{
    SmartRange range;
    uint64_t largeValue = UINT64_MAX - 1000;
    
    range.Add(largeValue, 100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), largeValue);
    ASSERT_EQ(range.Max(), largeValue + 99);
}

//
// Test adding duplicate values (should be idempotent).
// Adds the same value multiple times and verifies only one entry exists.
//
TEST(RangeTest, AddDuplicateValues)
{
    SmartRange range;
    range.Add(100);
    range.Add(100);
    range.Add(100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)100);
}

//
// Test QuicRangeRemoveSubranges directly.
// Manually creates subranges and removes a subset of them.
// Verifies that the specified subranges are removed correctly.
//
TEST(RangeTest, RemoveSubrangesDirectly)
{
    SmartRange range;
    range.Add(100, 10);   // [100-109]
    range.Add(200, 10);   // [200-209]
    range.Add(300, 10);   // [300-309]
    range.Add(400, 10);   // [400-409]
    ASSERT_EQ(range.ValidCount(), (uint32_t)4);
    
    // Remove 2 subranges starting at index 1 (removes [200-209] and [300-309])
    QuicRangeRemoveSubranges(&range.range, 1, 2);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Should have [100-109] and [400-409]
    auto sub1 = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub1->Low, (uint64_t)100);
    
    auto sub2 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub2->Low, (uint64_t)400);
}

//
// Test QuicRangeShrink explicitly.
// Grows the range, then explicitly calls shrink to reduce allocation.
// Verifies that shrink succeeds and range remains valid.
//
TEST(RangeTest, ExplicitShrink)
{
    SmartRange range;
    
    // Grow to heap allocation
    for (uint32_t i = 0; i < 16; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)16);
    
    // Remove most subranges
    for (uint32_t i = 0; i < 14; i++) {
        QuicRangeRemoveSubranges(&range.range, 0, 1);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Explicitly shrink
    BOOLEAN shrinkResult = QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_TRUE(shrinkResult);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
}

//
// Test multiple resets in sequence.
// Verifies that reset can be called multiple times and range remains valid.
//
TEST(RangeTest, MultipleResets)
{
    SmartRange range;
    
    range.Add(100, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    
    range.Add(200, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    
    range.Add(300, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)300);
}

//
// Test adding range that extends existing range backwards.
// Adds a range that overlaps the start of an existing range, extending it leftward.
//
TEST(RangeTest, AddRangeExtendingBackwards)
{
    SmartRange range;
    range.Add(200, 100);  // [200-299]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Add(150, 60);   // [150-209] - overlaps start
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)150);
    ASSERT_EQ(range.Max(), (uint64_t)299);
}

//
// Test adding range that extends existing range forwards.
// Adds a range that overlaps the end of an existing range, extending it rightward.
//
TEST(RangeTest, AddRangeExtendingForwards)
{
    SmartRange range;
    range.Add(100, 100);  // [100-199]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Add(180, 50);   // [180-229] - overlaps end
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)229);
}

//
// Test compact on empty range (edge case).
// Verifies that calling compact on an empty range doesn't crash.
//
TEST(RangeTest, CompactEmptyRange)
{
    SmartRange range;
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}

//
// Test compact on single subrange (should be no-op).
// Verifies that compact with one subrange doesn't change anything.
//
TEST(RangeTest, CompactSingleSubrange)
{
    SmartRange range;
    range.Add(100, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)149);
}
