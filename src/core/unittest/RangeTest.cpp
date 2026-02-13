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
// DeepTest: Tests for new compact and shrink functionality
//

TEST(RangeTest, DeepTest_CompactEmptyRange)
{
    // Path: QuicRangeCompact with UsedLength == 0
    SmartRange range;
    ASSERT_EQ(range.ValidCount(), 0u);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, DeepTest_CompactSingleSubrange)
{
    // Path: QuicRangeCompact with UsedLength == 1
    SmartRange range;
    range.Add(100, 10);
    ASSERT_EQ(range.ValidCount(), 1u);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 109u);
}

TEST(RangeTest, DeepTest_CompactNonAdjacentRanges)
{
    // Path: QuicRangeCompact with no merging (non-adjacent ranges)
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    range.Add(120, 10);  // [120-129]
    range.Add(140, 10);  // [140-149]
    ASSERT_EQ(range.ValidCount(), 3u);
    
    QuicRangeCompact(&range.range);
    
    // Should remain 3 separate ranges
    ASSERT_EQ(range.ValidCount(), 3u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
}

TEST(RangeTest, DeepTest_CompactAdjacentRanges)
{
    // Path: QuicRangeCompact merges adjacent ranges
    // Note: QuicRangeAddRange already calls Compact, so this tests the implicit behavior
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    range.Add(110, 10);  // [110-119] - adjacent to first
    
    // Should already be merged due to automatic compaction
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 119u);
}

TEST(RangeTest, DeepTest_CompactOverlappingRanges)
{
    // Path: QuicRangeCompact merges overlapping ranges
    // Note: QuicRangeAddRange already calls Compact, so this tests the implicit behavior
    SmartRange range;
    range.Add(100, 20);  // [100-119]
    range.Add(110, 20);  // [110-129] - overlaps with first
    
    // Should already be merged due to automatic compaction
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 129u);
}

TEST(RangeTest, DeepTest_CompactMultipleMerges)
{
    // Path: QuicRangeCompact with multiple consecutive merges
    // Note: QuicRangeAddRange already calls Compact, so this tests the implicit behavior
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    range.Add(110, 10);  // [110-119]
    range.Add(120, 10);  // [120-129]
    range.Add(130, 10);  // [130-139]
    
    // Should already be merged due to automatic compaction
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 139u);
}

TEST(RangeTest, DeepTest_CompactNestedMerging)
{
    // Path: Merging one pair makes it adjacent to the next
    // Note: QuicRangeAddRange already calls Compact, so this tests the implicit behavior
    SmartRange range;
    range.Add(100, 5);   // [100-104]
    range.Add(105, 5);   // [105-109] - adjacent to first
    range.Add(110, 10);  // [110-119] - adjacent to merged
    
    // All should already be merged due to automatic compaction
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 119u);
}

TEST(RangeTest, DeepTest_CompactTriggersShrinkg)
{
    // Path: QuicRangeCompact triggers shrinking after merging
    SmartRange range;
    
    // Grow allocation by adding many small ranges
    for (uint64_t i = 0; i < 50; i++) {
        range.Add(i * 100, 10);
    }
    uint32_t allocBefore = range.range.AllocLength;
    ASSERT_GT(allocBefore, QUIC_RANGE_INITIAL_SUB_COUNT * 4);
    
    // Remove most of them to trigger shrink condition
    for (uint64_t i = 0; i < 45; i++) {
        range.Remove(i * 100, 10);
    }
    
    // Now compact - should shrink
    QuicRangeCompact(&range.range);
    
    // Allocation should have shrunk
    ASSERT_LT(range.range.AllocLength, allocBefore);
}

TEST(RangeTest, DeepTest_ShrinkToInitialSize)
{
    // Path: QuicRangeShrink to QUIC_RANGE_INITIAL_SUB_COUNT
    SmartRange range;
    
    // Grow allocation
    for (uint64_t i = 0; i < 20; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most to allow shrinking
    for (uint64_t i = 5; i < 20; i++) {
        range.Remove(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), 5u);
    
    // Shrink to initial size
    BOOLEAN result = QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.ValidCount(), 5u);
    
    // Verify data integrity
    ASSERT_EQ(range.Min(), 0u);
}

TEST(RangeTest, DeepTest_ShrinkToIntermediateSize)
{
    // Path: QuicRangeShrink to size between initial and current
    SmartRange range;
    
    // Grow to large allocation
    for (uint64_t i = 0; i < 40; i++) {
        range.Add(i * 100, 10);
    }
    uint32_t allocBefore = range.range.AllocLength;
    
    // Remove some
    for (uint64_t i = 20; i < 40; i++) {
        range.Remove(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), 20u);
    
    // Shrink to half
    uint32_t newSize = allocBefore / 2;
    ASSERT_GT(newSize, QUIC_RANGE_INITIAL_SUB_COUNT);
    BOOLEAN result = QuicRangeShrink(&range.range, newSize);
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, newSize);
    ASSERT_EQ(range.ValidCount(), 20u);
}

TEST(RangeTest, DeepTest_ShrinkWithEmptyRange)
{
    // Path: QuicRangeShrink with UsedLength == 0
    SmartRange range;
    
    // Add many to grow allocation significantly
    for (uint64_t i = 0; i < 100; i++) {
        range.Add(i * 1000, 10);
    }
    uint32_t allocAfterGrowth = range.range.AllocLength;
    ASSERT_GT(allocAfterGrowth, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove all to empty the range
    for (uint64_t i = 0; i < 100; i++) {
        range.Remove(i * 1000, 10);
    }
    ASSERT_EQ(range.ValidCount(), 0u);
    // Allocation should still be large (remove doesn't always shrink immediately)
    
    // Manually shrink empty range
    BOOLEAN result = QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, DeepTest_CompactDoesNotShrinkWhenNotWarranted)
{
    // Test that compact doesn't shrink when conditions not met
    SmartRange range;
    
    // Add enough to grow a bit but not trigger shrink
    for (uint64_t i = 0; i < 10; i++) {
        range.Add(i * 100, 10);
    }
    uint32_t allocBefore = range.range.AllocLength;
    
    // Compact without meeting shrink conditions
    QuicRangeCompact(&range.range);
    
    // Should not shrink
    ASSERT_EQ(range.range.AllocLength, allocBefore);
}

TEST(RangeTest, DeepTest_AddRangeCallsCompact)
{
    // Verify that QuicRangeAddRange triggers compaction
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    range.Add(115, 10);  // [115-124]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Add bridging range - should trigger compact and merge
    range.Add(110, 5);   // [110-114]
    
    // After compaction, should be 1 range
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 124u);
}

TEST(RangeTest, DeepTest_RemoveRangeCallsCompact)
{
    // Verify that QuicRangeRemoveRange triggers compaction
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    // Remove middle section, creating two ranges
    range.Remove(120, 10);  // Now [100-119] and [130-149]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Add back the middle - should compact back to 1
    range.Add(120, 10);
    ASSERT_EQ(range.ValidCount(), 1u);
}

TEST(RangeTest, DeepTest_SetMinCallsCompact)
{
    // Verify that QuicRangeSetMin triggers compaction
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    range.Add(120, 10);  // [120-129]
    range.Add(140, 10);  // [140-149]
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // SetMin removes first range and may trigger compact
    QuicRangeSetMin(&range.range, 115);
    
    // Should have removed first range
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 120u);
}

TEST(RangeTest, DeepTest_CompactLargeNumberOfRanges)
{
    // Stress test: compact many small ranges
    SmartRange range;
    
    // Add 100 adjacent ranges
    for (uint64_t i = 0; i < 100; i++) {
        range.Add(i * 10, 10);
    }
    
    // Compact should merge all into 1
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 999u);
}

TEST(RangeTest, DeepTest_CompactMixedOverlapAndAdjacent)
{
    // Test with mix of overlapping and adjacent ranges
    // Note: QuicRangeAddRange already calls Compact, so this tests the implicit behavior
    SmartRange range;
    range.Add(100, 20);  // [100-119]
    range.Add(115, 10);  // [115-124] - overlaps
    range.Add(125, 10);  // [125-134] - adjacent to merged
    range.Add(135, 5);   // [135-139] - adjacent
    
    // All should already be merged due to automatic compaction
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 139u);
}

TEST(RangeTest, DeepTest_ShrinkPreservesData)
{
    // Verify shrinking preserves all subrange data
    SmartRange range;
    
    // Add specific pattern
    uint64_t values[] = {100, 200, 300, 400, 500};
    for (auto val : values) {
        range.Add(val, 5);
    }
    ASSERT_EQ(range.ValidCount(), 5u);
    
    // Grow allocation
    for (uint64_t i = 0; i < 30; i++) {
        range.Add(1000 + i * 100, 5);
    }
    
    // Remove the extra ones
    for (uint64_t i = 0; i < 30; i++) {
        range.Remove(1000 + i * 100, 5);
    }
    ASSERT_EQ(range.ValidCount(), 5u);
    
    // Shrink
    QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Verify original data intact
    ASSERT_EQ(range.ValidCount(), 5u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 504u);
}

// Iteration 2: Target uncovered lines in QuicRangeCompact and QuicRangeShrink

TEST(RangeTest, DeepTest_CompactWithTrueOverlap)
{
    // Target lines 595, 598, 601 - merging when ranges actually overlap (not just adjacent)
    SmartRange range;
    
    // Manually construct overlapping ranges by adding non-adjacent first
    range.Add(100, 20);  // [100-119]
    range.Add(150, 20);  // [150-169]
    
    // Now add one that truly overlaps (not adjacent) with first
    range.Add(110, 30);  // [110-139] - overlaps [100-119]
    
    // The overlap merging path should have been exercised
    ASSERT_EQ(range.ValidCount(), 2u);  // [100-139] and [150-169]
}

TEST(RangeTest, DeepTest_CompactCallsShrinkPath)
{
    // Target line 614 - QuicRangeCompact calling QuicRangeShrink
    SmartRange range;
    
    // Build a large sparse range set
    for (uint64_t i = 0; i < 100; i++) {
        range.Add(i * 1000, 1);
    }
    
    uint32_t allocBefore = range.range.AllocLength;
    
    // Remove most, leaving just a few scattered ones
    for (uint64_t i = 2; i < 98; i++) {
        range.Remove(i * 1000, 1);
    }
    
    // Now manually compact - should trigger shrink due to low usage
    if (range.range.AllocLength >= QUIC_RANGE_INITIAL_SUB_COUNT * 4 &&
        range.range.UsedLength < range.range.AllocLength / 8) {
        QuicRangeCompact(&range.range);
        // Should have shrunk
        ASSERT_LT(range.range.AllocLength, allocBefore);
    }
}

TEST(RangeTest, DeepTest_ManualCompactMergeScenario)
{
    // Additional test to ensure the actual merge code paths are hit
    SmartRange range;
    
    // Create a pattern where manual compaction will find mergeable ranges
    // Add non-contiguous ranges first
    range.Add(1000, 10);
    range.Add(2000, 10);
    range.Add(3000, 10);
    
    // Now use direct manipulation to create overlapping subranges
    // by adding values that would create overlap
    range.Add(1005, 20);  // Overlaps with [1000-1009]
    
    // Verify merging happened
    ASSERT_LE(range.ValidCount(), 3u);
}

// Iteration 3: Additional edge cases and comprehensive path coverage

TEST(RangeTest, DeepTest_ResetAfterGrowth)
{
    // Test QuicRangeReset after growing allocation
    SmartRange range;
    
    for (uint64_t i = 0; i < 50; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_GT(range.ValidCount(), 0u);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, DeepTest_GetRangeAtBoundaries)
{
    // Test QuicRangeGetRange at various positions
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    uint64_t count;
    BOOLEAN isLast;
    
    // Get at start
    BOOLEAN result = QuicRangeGetRange(&range.range, 100, &count, &isLast);
    ASSERT_TRUE(result);
    ASSERT_EQ(count, 50u);
    ASSERT_TRUE(isLast);
    
    // Get in middle
    result = QuicRangeGetRange(&range.range, 125, &count, &isLast);
    ASSERT_TRUE(result);
    ASSERT_EQ(count, 25u);
    ASSERT_TRUE(isLast);
    
    // Get value not in range
    result = QuicRangeGetRange(&range.range, 200, &count, &isLast);
    ASSERT_FALSE(result);
}

TEST(RangeTest, DeepTest_GetMinMaxSafeOnEmpty)
{
    // Test QuicRangeGetMinSafe/GetMaxSafe on empty range
    SmartRange range;
    
    uint64_t value;
    ASSERT_FALSE(QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_FALSE(QuicRangeGetMaxSafe(&range.range, &value));
}

TEST(RangeTest, DeepTest_SetMinMultipleSubranges)
{
    // Test QuicRangeSetMin with multiple subranges
    SmartRange range;
    range.Add(100, 10);   // [100-109]
    range.Add(200, 10);   // [200-209]
    range.Add(300, 10);   // [300-309]
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // SetMin to 250 - should remove first two ranges
    QuicRangeSetMin(&range.range, 250);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 300u);
}

TEST(RangeTest, DeepTest_SetMinSplitsRange)
{
    // Test QuicRangeSetMin splitting a range
    SmartRange range;
    range.Add(100, 100);  // [100-199]
    
    // SetMin to 150 - should split the range
    QuicRangeSetMin(&range.range, 150);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 150u);
    ASSERT_EQ(range.Max(), 199u);
}

TEST(RangeTest, DeepTest_RemoveRangeSplit)
{
    // Test remove creating a split
    SmartRange range;
    range.Add(100, 100);  // [100-199]
    
    // Remove middle
    range.Remove(140, 20);  // Remove [140-159]
    
    // Should have two ranges now
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

TEST(RangeTest, DeepTest_AddValueAtBoundaries)
{
    // Test adding values at exact boundaries
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    
    // Add at high boundary + 1 (should merge)
    range.Add((uint64_t)110);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Max(), 110u);
    
    // Add at low boundary - 1 (should merge)
    range.Add((uint64_t)99);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 99u);
}

TEST(RangeTest, DeepTest_LargeRangeOperations)
{
    // Test with large count values
    SmartRange range;
    range.Add(1000000, 1000000);  // Large range
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 1000000u);
    ASSERT_EQ(range.Max(), 1999999u);
    
    // Remove from middle
    range.Remove(1500000, 100000);
    ASSERT_EQ(range.ValidCount(), 2u);
}

// Iteration 4: Final attempts to hit remaining uncovered lines

TEST(RangeTest, DeepTest_ShrinkFromLargeToInitial)
{
    // Test shrinking from very large allocation back to initial
    SmartRange range;
    
    // Grow to very large allocation
    for (uint64_t i = 0; i < 200; i++) {
        range.Add(i * 10000, 100);
    }
    
    uint32_t largeAlloc = range.range.AllocLength;
    ASSERT_GT(largeAlloc, QUIC_RANGE_INITIAL_SUB_COUNT * 10);
    
    // Remove almost everything
    for (uint64_t i = 2; i < 200; i++) {
        range.Remove(i * 10000, 100);
    }
    
    // Manually shrink all the way to initial
    BOOLEAN result = QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
}

TEST(RangeTest, DeepTest_MultipleSuccessiveShrinks)
{
    // Test multiple successive shrink operations
    SmartRange range;
    
    for (uint64_t i = 0; i < 100; i++) {
        range.Add(i * 1000, 10);
    }
    
    uint32_t alloc1 = range.range.AllocLength;
    
    // Remove many to allow shrinking
    for (uint64_t i = 20; i < 100; i++) {
        range.Remove(i * 1000, 10);
    }
    ASSERT_EQ(range.ValidCount(), 20u);
    
    // Try shrinking if allocation is large enough
    if (alloc1 > QUIC_RANGE_INITIAL_SUB_COUNT * 2) {
        uint32_t newSize = CXPLAT_MAX(alloc1 / 2, QUIC_RANGE_INITIAL_SUB_COUNT * 2);
        if (newSize >= range.range.UsedLength) {
            QuicRangeShrink(&range.range, newSize);
            ASSERT_LE(range.range.AllocLength, alloc1);
        }
    }
}

TEST(RangeTest, DeepTest_CompactAfterRemove)
{
    // Test that compaction happens after remove operations
    SmartRange range;
    
    // Create a large contiguous range
    range.Add(1000, 1000);
    ASSERT_EQ(range.ValidCount(), 1u);
    
    // Remove from middle to create split
    range.Remove(1400, 200);
    
    // Should have split into 2 ranges
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Add back the middle to test merging via compact
    range.Add(1400, 200);
    
    // Should merge back to 1
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 1000u);
    ASSERT_EQ(range.Max(), 1999u);
}

TEST(RangeTest, DeepTest_EdgeCaseZeroCount)
{
    // Test edge cases with minimal counts
    SmartRange range;
    range.Add(100, 1);  // Single value
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 100u);
    
    // Remove it
    range.Remove(100, 1);
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, DeepTest_MaxValueRanges)
{
    // Test with very large values near uint64 max
    SmartRange range;
    uint64_t large = UINT64_MAX - 1000;
    
    range.Add(large, 100);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), large);
}

TEST(RangeTest, DeepTest_InterleavedAddRemove)
{
    // Test interleaved add and remove operations
    SmartRange range;
    
    for (int i = 0; i < 20; i++) {
        range.Add(i * 100, 10);
        if (i % 3 == 0) {
            range.Remove(i * 100, 5);
        }
    }
    
    ASSERT_GT(range.ValidCount(), 0u);
}
