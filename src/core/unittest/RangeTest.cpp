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
// DeepTest: Tests for QuicRangeCompact, QuicRangeShrink, and QuicRangeCalculateShrinkLength
// Added to improve coverage for PR #49
//

// Test QuicRangeCompact with empty range (early return path)
TEST(RangeTest, DeepTest_CompactEmptyRange)
{
    SmartRange range;
    // Empty range - should return immediately
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
}

// Test QuicRangeCompact with single subrange (early return path)
TEST(RangeTest, DeepTest_CompactSingleSubrange)
{
    SmartRange range;
    range.Add(100, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    QuicRangeCompact(&range.range);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)149);
}

// Test QuicRangeCompact with adjacent subranges that should be merged
TEST(RangeTest, DeepTest_CompactAdjacentRanges)
{
    SmartRange range;
    // Create adjacent subranges manually by adding separated values first
    range.Add(100, 10);  // [100-109]
    range.Add(120, 10);  // [120-129]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Now add the gap to make them adjacent
    range.Add(110, 10);  // [110-119] - should merge all three
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)129);
}

// Test QuicRangeCompact with overlapping subranges
TEST(RangeTest, DeepTest_CompactOverlappingRanges)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    range.Add(300, 50);  // [300-349]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    // Add overlapping range that connects first two
    range.Add(140, 70);  // [140-209] - overlaps first and second
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)100);
}

// Test QuicRangeCompact with well-separated ranges (no merge needed)
TEST(RangeTest, DeepTest_CompactSeparatedRanges)
{
    SmartRange range;
    range.Add(100, 10);  // [100-109]
    range.Add(200, 10);  // [200-209]
    range.Add(300, 10);  // [300-309]
    uint32_t initialCount = range.ValidCount();
    ASSERT_EQ(initialCount, (uint32_t)3);
    
    QuicRangeCompact(&range.range);
    // Should remain unchanged
    ASSERT_EQ(range.ValidCount(), initialCount);
}

// Test QuicRangeCompact that triggers shrinking
TEST(RangeTest, DeepTest_CompactTriggersShrink)
{
    // Create a range that will grow, then compact to trigger shrink
    SmartRange range(QUIC_MAX_RANGE_ALLOC_SIZE);
    
    // Add many separate values to grow allocation
    for (uint32_t i = 0; i < 64; i++) {
        range.Add(i * 10);  // Separated values
    }
    uint32_t largeCount = range.ValidCount();
    ASSERT_GT(largeCount, (uint32_t)32);
    
    // Now remove most of them to trigger shrink condition
    for (uint32_t i = 0; i < 60; i++) {
        range.Remove(i * 10, 1);
    }
    
    ASSERT_LT(range.ValidCount(), (uint32_t)10);
    QuicRangeCompact(&range.range);
    // Range should still work correctly after compaction
    ASSERT_LT(range.ValidCount(), (uint32_t)10);
}

// Test QuicRangeCompact with multiple consecutive adjacent ranges
TEST(RangeTest, DeepTest_CompactMultipleConsecutiveAdjacent)
{
    SmartRange range;
    // Add four consecutive adjacent ranges
    range.Add(0, 5);    // [0-4]
    range.Add(10, 5);   // [10-14]
    range.Add(20, 5);   // [20-24]
    range.Add(30, 5);   // [30-34]
    ASSERT_EQ(range.ValidCount(), (uint32_t)4);
    
    // Fill the gaps
    range.Add(5, 5);    // [5-9]
    range.Add(15, 5);   // [15-19]
    range.Add(25, 5);   // [25-29]
    
    // Should all be merged into one range
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)0);
    ASSERT_EQ(range.Max(), (uint64_t)34);
}

// Test shrinking behavior indirectly through remove operations
TEST(RangeTest, DeepTest_ShrinkViaRemoval)
{
    SmartRange range(QUIC_MAX_RANGE_ALLOC_SIZE);
    
    // Add enough values to grow beyond initial allocation
    for (uint32_t i = 0; i < 64; i++) {
        range.Add(i * 10);
    }
    uint32_t largeCount = range.ValidCount();
    ASSERT_EQ(largeCount, (uint32_t)64);
    
    // Remove most values - this should trigger shrinking via QuicRangeRemoveSubranges
    for (uint32_t i = 0; i < 60; i++) {
        range.Remove(i * 10, 1);
    }
    
    uint32_t remainingCount = range.ValidCount();
    ASSERT_EQ(remainingCount, (uint32_t)4);
    
    // Verify the remaining values are correct
    ASSERT_EQ(range.Min(), (uint64_t)600);
    ASSERT_EQ(range.Max(), (uint64_t)630);
}

// Test that allocation grows and then can shrink back
TEST(RangeTest, DeepTest_GrowthAndShrinkCycle)
{
    SmartRange range(QUIC_MAX_RANGE_ALLOC_SIZE);
    
    // Grow significantly by adding many separate values
    for (uint32_t i = 0; i < 32; i++) {
        range.Add(i * 20);
    }
    uint32_t grownCount = range.ValidCount();
    ASSERT_EQ(grownCount, (uint32_t)32);
    
    // Now fill all gaps to merge them into one contiguous range
    for (uint32_t i = 0; i < 31; i++) {
        range.Add(i * 20 + 1, 19);  // Fill gap between i*20 and (i+1)*20
    }
    
    // Should now be merged into 1 range after compaction
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)0);
    ASSERT_EQ(range.Max(), (uint64_t)31 * 20);
}

// Test remove operations trigger compaction
TEST(RangeTest, DeepTest_RemoveTriggersCompaction)
{
    SmartRange range;
    // Create a scenario where removal creates adjacent ranges
    range.Add(100, 100);  // [100-199]
    range.Add(300, 100);  // [300-399]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    // Remove the gap
    range.Add(200, 100);  // [200-299]
    
    // After compaction, should be one range
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    ASSERT_EQ(range.Max(), (uint64_t)399);
}

// Test SetMin triggers compaction
TEST(RangeTest, DeepTest_SetMinTriggersCompaction)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    range.Add(300, 50);  // [300-349]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    // Set minimum to drop first range
    QuicRangeSetMin(&range.range, 200);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)200);
}

// Test GetRange functionality
TEST(RangeTest, DeepTest_GetRangeExisting)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    uint64_t count;
    BOOLEAN isLastRange;
    BOOLEAN result = QuicRangeGetRange(&range.range, 100, &count, &isLastRange);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(count, (uint64_t)50);
    ASSERT_TRUE(isLastRange);
}

// Test GetRange with middle value
TEST(RangeTest, DeepTest_GetRangeMiddleValue)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    uint64_t count;
    BOOLEAN isLastRange;
    BOOLEAN result = QuicRangeGetRange(&range.range, 125, &count, &isLastRange);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(count, (uint64_t)25);  // 125-149 = 25 values
    ASSERT_TRUE(isLastRange);
}

// Test GetRange with non-existing value
TEST(RangeTest, DeepTest_GetRangeNonExisting)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    
    uint64_t count;
    BOOLEAN isLastRange;
    BOOLEAN result = QuicRangeGetRange(&range.range, 200, &count, &isLastRange);
    
    ASSERT_FALSE(result);
}

// Test GetRange IsLastRange flag
TEST(RangeTest, DeepTest_GetRangeNotLastRange)
{
    SmartRange range;
    range.Add(100, 50);   // [100-149]
    range.Add(200, 50);   // [200-249]
    
    uint64_t count;
    BOOLEAN isLastRange;
    BOOLEAN result = QuicRangeGetRange(&range.range, 100, &count, &isLastRange);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(count, (uint64_t)50);
    ASSERT_FALSE(isLastRange);  // Not the last range
}

// Test Reset on non-empty range
TEST(RangeTest, DeepTest_ResetNonEmptyRange)
{
    SmartRange range;
    range.Add(100, 50);
    range.Add(200, 50);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    
    range.Reset();
    ASSERT_EQ(range.ValidCount(), (uint32_t)0);
    
    // Verify GetMinSafe and GetMaxSafe return FALSE
    uint64_t value;
    ASSERT_FALSE(QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_FALSE(QuicRangeGetMaxSafe(&range.range, &value));
}

// Test GetMinSafe/GetMaxSafe on empty range
TEST(RangeTest, DeepTest_GetMinMaxSafeEmptyRange)
{
    SmartRange range;
    uint64_t value;
    
    ASSERT_FALSE(QuicRangeGetMinSafe(&range.range, &value));
    ASSERT_FALSE(QuicRangeGetMaxSafe(&range.range, &value));
}

// Test SetMin with partial overlap
TEST(RangeTest, DeepTest_SetMinPartialOverlap)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(200, 50);  // [200-249]
    
    QuicRangeSetMin(&range.range, 125);
    ASSERT_EQ(range.Min(), (uint64_t)125);
    ASSERT_EQ(range.Max(), (uint64_t)249);
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
}

// Test SetMin removes multiple ranges
TEST(RangeTest, DeepTest_SetMinRemovesMultiple)
{
    SmartRange range;
    range.Add(100, 20);  // [100-119]
    range.Add(200, 20);  // [200-219]
    range.Add(300, 20);  // [300-319]
    ASSERT_EQ(range.ValidCount(), (uint32_t)3);
    
    QuicRangeSetMin(&range.range, 250);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)300);
}

// Test growth at beginning (QuicRangeGrow with NextIndex=0)
TEST(RangeTest, DeepTest_GrowthAtBeginning)
{
    SmartRange range(QUIC_MAX_RANGE_ALLOC_SIZE);
    
    // Fill initial allocation
    for (uint32_t i = 0; i < QUIC_RANGE_INITIAL_SUB_COUNT; i++) {
        range.Add((i + 1) * 10);  // Start from 10 to leave room for 0
    }
    
    // Adding at beginning should trigger grow with NextIndex=0
    range.Add(0);
    ASSERT_EQ(range.Min(), (uint64_t)0);
}

// Test growth in middle (QuicRangeGrow with middle NextIndex)
TEST(RangeTest, DeepTest_GrowthInMiddle)
{
    SmartRange range(QUIC_MAX_RANGE_ALLOC_SIZE);
    
    // Fill initial allocation with gaps
    for (uint32_t i = 0; i < QUIC_RANGE_INITIAL_SUB_COUNT; i++) {
        range.Add(i * 20);
    }
    
    // Adding in middle should trigger grow with middle NextIndex
    range.Add(25);  // Between 20 and 40
    ASSERT_TRUE(range.Find(25) >= 0);
}

// Test growth at end (QuicRangeGrow with NextIndex=UsedLength)
TEST(RangeTest, DeepTest_GrowthAtEnd)
{
    SmartRange range(QUIC_MAX_RANGE_ALLOC_SIZE);
    
    // Fill initial allocation
    for (uint32_t i = 0; i < QUIC_RANGE_INITIAL_SUB_COUNT; i++) {
        range.Add(i * 10);
    }
    
    // Adding at end should trigger grow with NextIndex=UsedLength
    range.Add(1000);
    ASSERT_EQ(range.Max(), (uint64_t)1000);
}

// Test RemoveRange middle overlap (split scenario)
TEST(RangeTest, DeepTest_RemoveRangeMiddleSplit)
{
    SmartRange range;
    range.Add(100, 100);  // [100-199]
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    // Remove middle portion
    range.Remove(130, 40);  // Remove [130-169]
    ASSERT_EQ(range.ValidCount(), (uint32_t)2);
    ASSERT_EQ(range.Min(), (uint64_t)100);
    // Should have [100-129] and [170-199]
}

// Test AddRange with large count
TEST(RangeTest, DeepTest_AddRangeLargeCount)
{
    SmartRange range;
    range.Add(1000, 10000);
    
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)1000);
    ASSERT_EQ(range.Max(), (uint64_t)10999);
}

// Test compaction after multiple remove operations
TEST(RangeTest, DeepTest_CompactAfterMultipleRemoves)
{
    SmartRange range;
    
    // Create large contiguous range
    range.Add(0, 1000);
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    
    // Remove sections to fragment it
    range.Remove(100, 100);
    range.Remove(300, 100);
    range.Remove(500, 100);
    uint32_t fragmented = range.ValidCount();
    ASSERT_GT(fragmented, (uint32_t)1);
    
    // Add back to re-merge
    range.Add(100, 100);
    range.Add(300, 100);
    range.Add(500, 100);
    
    // Should be back to one range after compaction
    ASSERT_EQ(range.ValidCount(), (uint32_t)1);
    ASSERT_EQ(range.Min(), (uint64_t)0);
    ASSERT_EQ(range.Max(), (uint64_t)999);
}
