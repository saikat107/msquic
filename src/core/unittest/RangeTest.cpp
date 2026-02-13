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

// DeepTest: Tests for QuicRangeCompact
TEST(RangeTest, CompactEmptyRange)
{
    // Test Path 1: Empty range (0 subranges)
    SmartRange range;
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, CompactSingleSubrange)
{
    // Test Path 2: Single subrange
    SmartRange range;
    range.Add(100, 50);
    ASSERT_EQ(range.ValidCount(), 1u);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
}

TEST(RangeTest, CompactTwoAdjacentSubranges)
{
    // Test Path 3: Two adjacent subranges that should merge
    SmartRange range;
    range.Add(100, 50);
    range.Add(150, 50);
    ASSERT_EQ(range.ValidCount(), 1u); // Already merged by Add
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

TEST(RangeTest, CompactTwoOverlappingSubranges)
{
    // Test Path 4: Two overlapping subranges
    SmartRange range;
    range.Add(100, 100);
    range.Add(150, 100);
    ASSERT_EQ(range.ValidCount(), 1u); // Already merged by Add
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 249u);
}

TEST(RangeTest, CompactNonAdjacentSubranges)
{
    // Test Path 5: Two non-adjacent subranges (no merge)
    SmartRange range;
    range.Add(100, 50);
    range.Add(200, 50);
    ASSERT_EQ(range.ValidCount(), 2u);
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 249u);
}

TEST(RangeTest, CompactMultipleSubranges)
{
    // Test Path 6: Multiple subranges requiring multiple merges
    SmartRange range;
    // Create gaps that can be filled
    range.Add(100, 10);
    range.Add(200, 10);
    range.Add(300, 10);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // Fill gaps to create adjacent ranges
    range.Add(110, 90);
    range.Add(210, 90);
    ASSERT_EQ(range.ValidCount(), 1u); // Should merge all
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 309u);
}

TEST(RangeTest, CompactWithShrinking)
{
    // Test Path 7: Compact triggers shrinking
    const uint32_t LargeCount = 100;
    SmartRange range;
    
    // Add many subranges to grow allocation
    for (uint32_t i = 0; i < LargeCount; i++) {
        range.Add(i * 10, 5);
    }
    ASSERT_EQ(range.ValidCount(), LargeCount);
    uint32_t allocBefore = range.range.AllocLength;
    
    // Remove most subranges
    for (uint32_t i = 0; i < LargeCount - 2; i++) {
        range.Remove(i * 10, 5);
    }
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Compact should trigger shrinking
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 2u);
    // Allocation should potentially shrink
    ASSERT_LE(range.range.AllocLength, allocBefore);
}

// DeepTest: Tests for QuicRangeShrink
TEST(RangeTest, ShrinkToInitialSize)
{
    SmartRange range;
    
    // Grow the range
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(i * 10, 5);
    }
    uint32_t allocBefore = range.range.AllocLength;
    ASSERT_GT(allocBefore, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most entries
    for (uint32_t i = 0; i < 18; i++) {
        range.Remove(i * 10, 5);
    }
    
    // Shrink to initial size
    ASSERT_TRUE(QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT));
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, ShrinkToCustomSize)
{
    SmartRange range;
    
    // Grow significantly
    for (uint32_t i = 0; i < 40; i++) {
        range.Add(i * 10, 5);
    }
    ASSERT_GT(range.range.AllocLength, 16u);
    
    // Remove many entries
    for (uint32_t i = 0; i < 35; i++) {
        range.Remove(i * 10, 5);
    }
    
    // Shrink to custom size
    uint32_t targetSize = 16;
    ASSERT_TRUE(QuicRangeShrink(&range.range, targetSize));
    ASSERT_EQ(range.range.AllocLength, targetSize);
    ASSERT_EQ(range.ValidCount(), 5u);
}

TEST(RangeTest, ShrinkPreservesData)
{
    SmartRange range;
    
    // Add specific ranges
    range.Add(100, 10);
    range.Add(200, 10);
    range.Add(300, 10);
    
    // Grow allocation by adding more
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(1000 + i * 10, 5);
    }
    
    // Remove the extra ones
    for (uint32_t i = 0; i < 20; i++) {
        range.Remove(1000 + i * 10, 5);
    }
    
    uint32_t allocBefore = range.range.AllocLength;
    
    // Shrink
    if (allocBefore > QUIC_RANGE_INITIAL_SUB_COUNT) {
        ASSERT_TRUE(QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT));
        
        // Verify data integrity
        ASSERT_EQ(range.ValidCount(), 3u);
        ASSERT_EQ(range.Min(), 100u);
        ASSERT_EQ(range.Max(), 309u);
    }
}

// DeepTest: Edge cases and stress tests
TEST(RangeTest, CompactAfterRemoveCreatesMergeOpportunity)
{
    SmartRange range;
    
    // Create: [100-109] [110-119] [120-129]
    range.Add(100, 10);
    range.Add(110, 10);
    range.Add(120, 10);
    ASSERT_EQ(range.ValidCount(), 1u); // Already merged
    
    // These should all be merged into one
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 129u);
}

TEST(RangeTest, CompactHandlesComplexOverlaps)
{
    SmartRange range;
    
    // Create overlapping ranges
    range.Add(100, 50);
    range.Add(120, 50);
    range.Add(140, 50);
    
    // All should merge into one
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 189u);
}

TEST(RangeTest, ShrinkAfterManyOperations)
{
    SmartRange range;
    
    // Perform many add/remove operations
    for (uint32_t cycle = 0; cycle < 3; cycle++) {
        for (uint32_t i = 0; i < 30; i++) {
            range.Add(i * 100, 10);
        }
        
        for (uint32_t i = 0; i < 25; i++) {
            range.Remove(i * 100, 10);
        }
    }
    
    uint32_t usedBefore = range.ValidCount();
    uint32_t allocBefore = range.range.AllocLength;
    
    // Compact should help reduce allocation
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), usedBefore); // Same data
    // Allocation may shrink
    ASSERT_LE(range.range.AllocLength, allocBefore);
}

// DeepTest: Additional tests for better coverage
TEST(RangeTest, ResetRange)
{
    SmartRange range;
    range.Add(100, 50);
    range.Add(200, 50);
    ASSERT_EQ(range.ValidCount(), 2u);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, GetRangeFunction)
{
    SmartRange range;
    range.Add(100, 100);
    
    uint64_t count;
    BOOLEAN isLast;
    
    // Test getting a valid range
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 100, &count, &isLast));
    ASSERT_EQ(count, 100u);
    ASSERT_TRUE(isLast);
    
    // Test getting from middle of range
    ASSERT_TRUE(QuicRangeGetRange(&range.range, 150, &count, &isLast));
    ASSERT_EQ(count, 50u);
    ASSERT_TRUE(isLast);
    
    // Test getting invalid range
    ASSERT_FALSE(QuicRangeGetRange(&range.range, 250, &count, &isLast));
}

TEST(RangeTest, SetMinFunction)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Set min to middle of first range
    QuicRangeSetMin(&range.range, 150);
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 150u);
    ASSERT_EQ(range.Max(), 399u);
    
    // Set min to remove first range completely
    QuicRangeSetMin(&range.range, 300);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 300u);
}

TEST(RangeTest, RemoveMiddleOfRange)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.ValidCount(), 1u);
    
    // Remove middle section (should split into two ranges)
    range.Remove(140, 20);
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

TEST(RangeTest, RemoveOverlappingMultipleRanges)
{
    SmartRange range;
    range.Add(100, 50);
    range.Add(200, 50);
    range.Add(300, 50);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // Remove range that overlaps multiple subranges
    range.Remove(120, 160);
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 349u);
}

TEST(RangeTest, AddRangeUpdatedFlag)
{
    SmartRange range;
    BOOLEAN rangeUpdated;
    
    // First add should set updated flag
    ASSERT_NE(QuicRangeAddRange(&range.range, 100, 50, &rangeUpdated), (QUIC_SUBRANGE*)NULL);
    ASSERT_TRUE(rangeUpdated);
    
    // Adding exact same range should not update
    ASSERT_NE(QuicRangeAddRange(&range.range, 100, 50, &rangeUpdated), (QUIC_SUBRANGE*)NULL);
    ASSERT_FALSE(rangeUpdated);
    
    // Adding overlapping range that extends should update
    ASSERT_NE(QuicRangeAddRange(&range.range, 120, 50, &rangeUpdated), (QUIC_SUBRANGE*)NULL);
    ASSERT_TRUE(rangeUpdated);
}

TEST(RangeTest, GetMinMaxSafeFunctions)
{
    SmartRange range;
    uint64_t value;
    
    // Empty range should fail
    ASSERT_FALSE(QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_FALSE(QuicRangeGetMaxSafe(&range.range, &value));
    
    // Non-empty range should succeed
    range.Add(100, 50);
    ASSERT_TRUE(QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_EQ(value, 100u);
    ASSERT_TRUE(QuicRangeGetMaxSafe(&range.range, &value));
    ASSERT_EQ(value, 149u);
}

TEST(RangeTest, AddRangeWithCompact)
{
    SmartRange range;
    
    // Create fragmented ranges
    range.Add(100, 10);
    range.Add(200, 10);
    range.Add(300, 10);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    // Add range that fills gaps - should trigger compacting
    range.Add(110, 90);
    // Already merged by AddRange
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, RemoveRangeWithCompact)
{
    SmartRange range;
    range.Add(100, 200);
    ASSERT_EQ(range.ValidCount(), 1u);
    
    // Remove creates multiple ranges
    range.Remove(150, 50);
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Compact should not merge non-adjacent ranges
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), 2u);
}

TEST(RangeTest, ShrinkFailureHandling)
{
    SmartRange range;
    
    // Add some data
    range.Add(100, 10);
    uint32_t allocBefore = range.range.AllocLength;
    
    // Try to shrink to same size (should succeed trivially)
    ASSERT_TRUE(QuicRangeShrink(&range.range, allocBefore));
    ASSERT_EQ(range.range.AllocLength, allocBefore);
}

TEST(RangeTest, CompactBoundaryConditions)
{
    SmartRange range;
    
    // Test with ranges that are exactly adjacent (NextLow == CurrentHigh + 1)
    range.Add(100, 50); // 100-149
    range.Add(150, 50); // 150-199 (adjacent)
    ASSERT_EQ(range.ValidCount(), 1u); // Should already be merged
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

TEST(RangeTest, LargeScaleCompaction)
{
    SmartRange range;
    
    // Create many small ranges
    for (uint32_t i = 0; i < 50; i++) {
        range.Add(i * 100, 10);
    }
    
    // Fill all gaps
    for (uint32_t i = 0; i < 49; i++) {
        range.Add(i * 100 + 10, 90);
    }
    
    // Should merge into single range
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 4909u);
}

// DeepTest: More edge cases to push coverage higher
TEST(RangeTest, ManySmallRangesToTriggerGrowth)
{
    SmartRange range;
    
    // Add many separate ranges to force multiple growth operations
    for (uint32_t i = 0; i < 200; i++) {
        range.Add(i * 1000, 1);
    }
    
    // Verify all were added
    ASSERT_EQ(range.ValidCount(), 200u);
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 199000u);
}

TEST(RangeTest, AddRangeAtVeryBeginning)
{
    SmartRange range;
    
    // Add ranges in reverse order to test NextIndex == 0 path
    range.Add(1000, 100);
    range.Add(500, 100);
    range.Add(0, 100);
    
    ASSERT_EQ(range.ValidCount(), 3u);
    ASSERT_EQ(range.Min(), 0u);
}

TEST(RangeTest, RemoveFromEmptyRange)
{
    SmartRange range;
    
    // Removing from empty range should succeed (no-op)
    range.Remove(100, 50);
    ASSERT_EQ(range.ValidCount(), 0u);
}

TEST(RangeTest, AddRangeWithPriorOverlap)
{
    SmartRange range;
    
    // Create pattern where new range overlaps prior range
    range.Add(200, 100);
    range.Add(100, 150); // Overlaps and extends before
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 299u);
}

TEST(RangeTest, ComplexMergePattern)
{
    SmartRange range;
    
    // Create complex overlap pattern
    range.Add(100, 50);
    range.Add(300, 50);
    range.Add(500, 50);
    range.Add(700, 50);
    ASSERT_EQ(range.ValidCount(), 4u);
    
    // Add large range that merges multiple subranges
    range.Add(150, 500); // Overlaps multiple ranges
    ASSERT_EQ(range.ValidCount(), 2u); // Should merge first 3
}

TEST(RangeTest, ShrinkWithPreallocatedBuffer)
{
    SmartRange range;
    
    // Start with just the preallocated buffer
    range.Add(100, 10);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Grow beyond preallocated
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(1000 + i * 10, 5);
    }
    uint32_t allocAfterGrowth = range.range.AllocLength;
    ASSERT_GT(allocAfterGrowth, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most entries
    for (uint32_t i = 0; i < 19; i++) {
        range.Remove(1000 + i * 10, 5);
    }
    
    // Shrink back to initial should use preallocated buffer
    ASSERT_TRUE(QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT));
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
}

TEST(RangeTest, CompactWithNoMergeOpportunity)
{
    SmartRange range;
    
    // Create ranges with gaps that can't merge
    range.Add(100, 10);
    range.Add(200, 10);
    range.Add(300, 10);
    uint32_t countBefore = range.ValidCount();
    
    QuicRangeCompact(&range.range);
    
    // Should remain unchanged
    ASSERT_EQ(range.ValidCount(), countBefore);
}

TEST(RangeTest, SetMinAtBoundary)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    
    // Set min exactly at the start of second range
    QuicRangeSetMin(&range.range, 300);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 300u);
    ASSERT_EQ(range.Max(), 399u);
}

TEST(RangeTest, RemoveRangePartialOverlaps)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    range.Add(500, 100);
    
    // Remove range that partially overlaps first and last
    range.Remove(150, 400);
    
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 599u);
}
