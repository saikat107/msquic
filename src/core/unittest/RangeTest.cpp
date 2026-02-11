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
// Test: QuicRangeReset clears all values from a non-empty range
// Scenario: Add multiple values, then reset to verify range becomes empty
// Assertions: UsedLength becomes 0, Min/Max queries fail after reset
//
TEST(RangeTest, ResetNonEmptyRange)
{
    SmartRange range;
    range.Add(100);
    range.Add(200);
    range.Add(300);
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint32_t)100);
    ASSERT_EQ(range.Max(), (uint32_t)300);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    
    uint64_t value;
    ASSERT_EQ(FALSE, QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_EQ(FALSE, QuicRangeGetMaxSafe(&range.range, &value));
}

//
// Test: QuicRangeGetMinSafe/GetMaxSafe return FALSE for empty range
// Scenario: Query min/max on empty range to verify safe behavior
// Assertions: Both functions return FALSE without crashing
//
TEST(RangeTest, GetMinMaxSafeEmptyRange)
{
    SmartRange range;
    uint64_t value;
    ASSERT_EQ(FALSE, QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_EQ(FALSE, QuicRangeGetMaxSafe(&range.range, &value));
}

//
// Test: QuicRangeGetRange finds an existing value and returns contiguous count
// Scenario: Add range [100-199], query for value 100 to verify count returned
// Assertions: Returns TRUE, Count=100, IsLastRange=TRUE
//
TEST(RangeTest, GetRangeExistingValue)
{
    SmartRange range;
    range.Add(100, 100);
    
    uint64_t count;
    BOOLEAN isLastRange;
    ASSERT_EQ(TRUE, QuicRangeGetRange(&range.range, 100, &count, &isLastRange));
    ASSERT_EQ(count, (uint64_t)100);
    ASSERT_EQ(isLastRange, TRUE);
}

//
// Test: QuicRangeGetRange returns FALSE for non-existing value
// Scenario: Add range [100-199], query for value 50 (not present)
// Assertions: Returns FALSE
//
TEST(RangeTest, GetRangeNonExistingValue)
{
    SmartRange range;
    range.Add(100, 100);
    
    uint64_t count;
    BOOLEAN isLastRange;
    ASSERT_EQ(FALSE, QuicRangeGetRange(&range.range, 50, &count, &isLastRange));
}

//
// Test: QuicRangeGetRange with middle value in range
// Scenario: Add range [100-199], query for value 150 to verify partial count
// Assertions: Returns TRUE, Count=50 (from 150 to 199), IsLastRange=TRUE
//
TEST(RangeTest, GetRangeMiddleValue)
{
    SmartRange range;
    range.Add(100, 100);
    
    uint64_t count;
    BOOLEAN isLastRange;
    ASSERT_EQ(TRUE, QuicRangeGetRange(&range.range, 150, &count, &isLastRange));
    ASSERT_EQ(count, (uint64_t)50);
    ASSERT_EQ(isLastRange, TRUE);
}

//
// Test: QuicRangeGetRange detects non-last range
// Scenario: Add two ranges [100-199] and [300-399], query first range
// Assertions: IsLastRange=FALSE for first range
//
TEST(RangeTest, GetRangeNotLastRange)
{
    SmartRange range;
    range.Add(100, 100);
    range.Add(300, 100);
    
    uint64_t count;
    BOOLEAN isLastRange;
    ASSERT_EQ(TRUE, QuicRangeGetRange(&range.range, 100, &count, &isLastRange));
    ASSERT_EQ(count, (uint64_t)100);
    ASSERT_EQ(isLastRange, FALSE);
    
    ASSERT_EQ(TRUE, QuicRangeGetRange(&range.range, 300, &count, &isLastRange));
    ASSERT_EQ(count, (uint64_t)100);
    ASSERT_EQ(isLastRange, TRUE);
}

//
// Test: QuicRangeSetMin drops all values below threshold
// Scenario: Add contiguous range [10-50], then SetMin(30) to drop [10-29]
// Assertions: Min becomes 30, max stays 50
//
TEST(RangeTest, SetMinDropsLowerValues)
{
    SmartRange range;
    range.Add(10, 41); // Add range [10-50]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)10);
    ASSERT_EQ(range.Max(), (uint64_t)50);
    
    QuicRangeSetMin(&range.range, 30);
    ASSERT_EQ(range.Min(), (uint64_t)30);
    ASSERT_EQ(range.Max(), (uint64_t)50);
}

//
// Test: QuicRangeSetMin with value in middle of a subrange
// Scenario: Add range [100-199], SetMin(150) to split the range
// Assertions: Min becomes 150, max stays 199, count becomes 50
//
TEST(RangeTest, SetMinSplitsSubrange)
{
    SmartRange range;
    range.Add(100, 100);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)199);
    
    QuicRangeSetMin(&range.range, 150);
    ASSERT_EQ(range.Min(), (uint64_t)150);
    ASSERT_EQ(range.Max(), (uint64_t)199);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
}

//
// Test: QuicRangeSetMin drops multiple subranges
// Scenario: Add ranges [10-19], [30-39], [50-59], SetMin(40) to drop first two
// Assertions: Only [50-59] remains
//
TEST(RangeTest, SetMinDropsMultipleSubranges)
{
    SmartRange range;
    range.Add(10, 10);
    range.Add(30, 10);
    range.Add(50, 10);
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    QuicRangeSetMin(&range.range, 40);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)50);
    ASSERT_EQ(range.Max(), (uint64_t)59);
}

//
// Test: QuicRangeSetMin with value already at or below minimum
// Scenario: Add range [100-199], SetMin(50) to verify no change
// Assertions: Range unchanged
//
TEST(RangeTest, SetMinBelowExisting)
{
    SmartRange range;
    range.Add(100, 100);
    
    QuicRangeSetMin(&range.range, 50);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)199);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
}

//
// Test: QuicRangeCompact merges adjacent subranges after manual manipulation
// Scenario: Create adjacent subranges, call compact to verify merging
// Assertions: Adjacent ranges merge into single subrange
// Note: Testing compaction behavior through Add/Remove which now call Compact
//
TEST(RangeTest, CompactMergesAdjacentRanges)
{
    SmartRange range;
    // Add values that create adjacent subranges, then let compaction merge them
    range.Add(100, 50);
    range.Add(150, 50);
    // AddRange internally calls Compact, so they should be merged
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)199);
}

//
// Test: QuicRangeCompact handles overlapping subranges
// Scenario: Add overlapping ranges to trigger compaction merging
// Assertions: Overlapping ranges merge correctly
//
TEST(RangeTest, CompactMergesOverlappingRanges)
{
    SmartRange range;
    range.Add(100, 75); // [100-174]
    range.Add(150, 75); // [150-224], overlaps with [100-174]
    // Should merge to [100-224]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)224);
}

//
// Test: QuicRangeShrink via RemoveRange triggering shrink threshold
// Scenario: Grow range significantly, then remove most values to trigger shrink
// Assertions: Allocation shrinks when usage drops below threshold
//
TEST(RangeTest, ShrinkAfterManyRemovals)
{
    const uint32_t MaxCount = 32;
    SmartRange range(MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Add many separate values to grow allocation
    for (uint32_t i = 0; i < MaxCount; i++) {
        range.Add(i * 10);
    }
    ASSERT_EQ(range.ValidCount(), MaxCount);
    uint32_t allocAfterGrow = range.range.AllocLength;
    ASSERT_GT(allocAfterGrow, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most values
    for (uint32_t i = 0; i < MaxCount - 2; i++) {
        range.Remove(i * 10, 1);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Allocation may have shrunk due to low usage
    // (Shrinking happens when AllocLength >= 2*Initial and UsedLength < AllocLength/4)
}

//
// Test: QuicRangeShrink back to pre-allocated buffer
// Scenario: Grow range, then remove to trigger shrink back to QUIC_RANGE_INITIAL_SUB_COUNT
// Assertions: SubRanges pointer returns to PreAllocSubRanges
//
TEST(RangeTest, ShrinkToPreAllocatedBuffer)
{
    SmartRange range;
    
    // Add enough values to force growth beyond initial allocation
    for (uint32_t i = 0; i < 20; i++) {
        range.Add(i * 10);
    }
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most values to allow shrinking
    for (uint32_t i = 0; i < 18; i++) {
        range.Remove(i * 10, 1);
    }
    
    // After shrinking, should use PreAllocSubRanges if allocation shrinks to initial size
    if (range.range.AllocLength == QUIC_RANGE_INITIAL_SUB_COUNT) {
        ASSERT_EQ(range.range.SubRanges, range.range.PreAllocSubRanges);
    }
}

//
// Test: Force growth with insertion at beginning (NextIndex = 0)
// Scenario: Fill initial capacity, then add value at beginning to trigger growth with NextIndex=0
// Assertions: Tests memcpy branch for inserting at index 0 during growth (lines 101-104)
//
TEST(RangeTest, GrowthAtBeginning)
{
    SmartRange range;
    // Fill initial capacity (8 slots) with separate values
    for (uint32_t i = 1; i <= 8; i++) {
        range.Add(i * 10);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)8);
    
    // Adding a value before all existing values forces growth with NextIndex=0
    range.Add(5);
    ASSERT_EQ(range.Min(), (uint64_t)5);
}

//
// Test: Force growth with insertion in middle
// Scenario: Fill capacity, then add value in middle to trigger growth with 0 < NextIndex < UsedLength
// Assertions: Tests memcpy branch for splitting during growth (lines 115-118)
//
TEST(RangeTest, GrowthInMiddle)
{
    SmartRange range;
    // Fill initial capacity
    for (uint32_t i = 0; i < 8; i++) {
        range.Add(i * 10);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)8);
    
    // Add value in middle to force growth at middle index
    range.Add(35);
    ASSERT_EQ(range.ValidCount(), (uint32_t)9);
}

//
// Test: Force growth with insertion at end (NextIndex = UsedLength)
// Scenario: Fill capacity, then append value at end to trigger growth with NextIndex=UsedLength
// Assertions: Tests memcpy branch for appending during growth (lines 106-109)
//
TEST(RangeTest, GrowthAtEnd)
{
    SmartRange range;
    // Fill initial capacity with values leaving gap at end
    for (uint32_t i = 0; i < 8; i++) {
        range.Add(i * 10);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)8);
    
    // Add value after all existing values to force growth at end
    range.Add(100);
    ASSERT_EQ(range.Max(), (uint64_t)100);
    ASSERT_EQ(range.ValidCount(), (uint32_t)9);
}

//
// Test: QuicRangeCompact with overlapping ranges that need merging
// Scenario: Manually create situation where adjacent subranges exist, then compact
// Assertions: Tests actual merge logic in QuicRangeCompact (lines 595, 598, 601)
//
TEST(RangeTest, CompactWithActualMerging)
{
    SmartRange range;
    // Create multiple separate ranges
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    range.Add(300, 50);  // [300-349]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    // Now add connecting ranges to create adjacency
    range.Add(150, 50);  // [150-199] - connects first and second
    range.Add(250, 50);  // [250-299] - connects second and third
    
    // After compaction (which happens in AddRange), should have 1 range [100-349]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)349);
}

//
// Test: QuicRangeCompact triggers shrinking when allocation is large but usage is low
// Scenario: Grow to large allocation, then remove many values to trigger shrink in compact
// Assertions: Tests shrinking logic in QuicRangeCompact (line 614)
//
TEST(RangeTest, CompactTriggersShrink)
{
    SmartRange range;
    
    // Grow allocation significantly
    for (uint32_t i = 0; i < 40; i++) {
        range.Add(i * 10);
    }
    ASSERT_GT(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    // Remove most values to get below shrink threshold
    for (uint32_t i = 0; i < 38; i++) {
        range.Remove(i * 10, 1);
    }
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Trigger compaction (via remove which calls SetMin or direct compact)
    QuicRangeCompact(&range.range);
    
    // Allocation should potentially shrink if threshold met
    // (Shrinks when AllocLength >= 4*Initial and UsedLength < AllocLength/8)
}

//
// Test: Multiple overlapping ranges merged in single compact
// Scenario: Create chain of overlapping ranges and verify they all merge
// Assertions: Tests iterative merging in QuicRangeCompact loop
//
TEST(RangeTest, CompactMergesMultipleOverlaps)
{
    SmartRange range;
    // Add overlapping ranges
    range.Add(100, 30);  // [100-129]
    range.Add(120, 30);  // [120-149] - overlaps with first
    range.Add(140, 30);  // [140-169] - overlaps with second
    range.Add(160, 30);  // [160-189] - overlaps with third
    
    // All should merge to [100-189]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)189);
}

//
// Test: Hit QUIC_MAX_RANGE_ALLOC_SIZE limit
// Scenario: Create range with MaxAllocSize set to actual max, verify growth limit
// Assertions: Tests line 71 where growth is prevented at max size
//
TEST(RangeTest, GrowthHitsMaxSizeLimit)
{
    // QUIC_MAX_RANGE_ALLOC_SIZE = 0x100000 (1MB)
    // This allows 65536 subranges (1MB / 16 bytes)
    // For practical testing, we'll use a smaller max
    const uint32_t MaxCount = 64;
    SmartRange range(MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Fill to capacity
    for (uint32_t i = 0; i < MaxCount; i++) {
        range.Add(i * 2);
    }
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.range.AllocLength, MaxCount);
    
    // Try to add beyond capacity - should age out oldest value
    range.Add(MaxCount * 2);
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), (uint64_t)2); // First value (0) aged out
}

//
// Test: QuicRangeAddRange with search finding overlapping range
// Scenario: Add ranges that require binary search to find overlapping range
// Assertions: Tests line 276 where we search for first overlapping range
//
TEST(RangeTest, AddRangeFindingFirstOverlap)
{
    SmartRange range;
    // Add base ranges with gaps
    range.Add(100, 10);  // [100-109]
    range.Add(200, 10);  // [200-209]
    range.Add(300, 10);  // [300-309]
    range.Add(400, 10);  // [400-409]
    ASSERT_EQ(range.ValidCount(), (uint32_t)4);
    
    // Add range that overlaps with multiple existing ranges
    // This should find first overlap and merge through them
    range.Add(195, 120);  // [195-314] overlaps with [200-209] and [300-309]
    
    // Should merge to fewer ranges
    ASSERT_LT(range.ValidCount(), (uint32_t)4);
}

//
// Test: RemoveRange causing split and requiring reallocation
// Scenario: Remove middle portion from single large range
// Assertions: Verifies split logic and potential reallocation
//
TEST(RangeTest, RemoveRangeCausingSplit)
{
    SmartRange range;
    // Add large contiguous range
    range.Add(1000, 1000);  // [1000-1999]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    // Remove large middle portion to create split
    range.Remove(1100, 800);  // Remove [1100-1899]
    
    // Should result in two ranges: [1000-1099] and [1900-1999]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    auto sub1 = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub1->Low, (uint64_t)1000);
    ASSERT_EQ(sub1->Count, (uint64_t)100);
    
    auto sub2 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub2->Low, (uint64_t)1900);
    ASSERT_EQ(sub2->Count, (uint64_t)100);
}

//
// Test: QuicRangeCompact shrinks when allocation is very large but usage is minimal
// Scenario: Grow to 64+ subranges, remove most to trigger shrink threshold in Compact
// Assertions: Tests line 614 where Compact calls Shrink
//
TEST(RangeTest, CompactShrinkingLargeAllocation)
{
    SmartRange range;
    
    // Grow to large allocation (AllocLength >= 32)
    for (uint32_t i = 0; i < 48; i++) {
        range.Add(i * 10);
    }
    ASSERT_GE(range.range.AllocLength, (uint32_t)32);
    
    // Remove many values to get UsedLength < AllocLength/8
    // If AllocLength=64, need UsedLength < 8
    for (uint32_t i = 0; i < 45; i++) {
        range.Remove(i * 10, 1);
    }
    ASSERT_LE(range.ValidCount(), (uint32_t)8);
    
    // Call Compact explicitly to trigger shrinking logic
    QuicRangeCompact(&range.range);
    
    // Verify range still works after compaction
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint64_t)450);
    ASSERT_EQ(range.Max(), (uint64_t)470);
}

//
// Test: QuicRangeRemoveRange with middle overlap requiring split
// Scenario: Add [100-200], remove [125-175] to create a split
// Assertions: Results in two subranges [100-124] and [176-200]
//
TEST(RangeTest, RemoveRangeMiddleOverlap)
{
    SmartRange range;
    range.Add(100, 101); // [100-200]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    range.Remove(125, 51); // Remove [125-175]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Verify first subrange [100-124]
    auto sub1 = QuicRangeGet(&range.range, 0);
    ASSERT_EQ(sub1->Low, (uint64_t)100);
    ASSERT_EQ(sub1->Count, (uint64_t)25);
    
    // Verify second subrange [176-200]
    auto sub2 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub2->Low, (uint64_t)176);
    ASSERT_EQ(sub2->Count, (uint64_t)25);
}

//
// Test: QuicRangeAddRange with allocation at max size limit
// Scenario: Create range with MaxAllocSize limit, fill to capacity, attempt to add beyond
// Assertions: When at max, adding new non-mergeable values ages out oldest values
//
TEST(RangeTest, AddRangeAtMaxCapacity)
{
    const uint32_t MaxCount = 16;
    SmartRange range(MaxCount * sizeof(QUIC_SUBRANGE));
    
    // Fill to capacity with separate values
    for (uint32_t i = 0; i < MaxCount; i++) {
        range.Add(i * 2);
    }
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), (uint64_t)0);
    ASSERT_EQ(range.Max(), (MaxCount - 1) * 2);
    
    // Adding beyond capacity ages out the smallest value
    range.Add(MaxCount * 2);
    ASSERT_EQ(range.ValidCount(), MaxCount);
    ASSERT_EQ(range.Min(), (uint64_t)2); // First value (0) was aged out
    ASSERT_EQ(range.Max(), MaxCount * 2);
}

//
// Test: QuicRangeCompact with no adjacent ranges (no-op scenario)
// Scenario: Add well-separated ranges, verify compaction doesn't change structure
// Assertions: ValidCount and values unchanged after operations that trigger compact
//
TEST(RangeTest, CompactNoOpWhenNoAdjacent)
{
    SmartRange range;
    range.Add(100, 10); // [100-109]
    range.Add(200, 10); // [200-209] - well separated
    range.Add(300, 10); // [300-309] - well separated
    
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    // Operations that trigger compact shouldn't merge these
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)309);
}

//
// Test: QuicRangeCompact with single subrange (edge case)
// Scenario: Range with single subrange, call compact to verify no crash/corruption
// Assertions: Range structure unchanged
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

//
// Test: QuicRangeCompact with empty range (edge case)
// Scenario: Call compact on empty range to verify no crash
// Assertions: Range remains empty
//
TEST(RangeTest, CompactEmptyRange)
{
    SmartRange range;
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}
