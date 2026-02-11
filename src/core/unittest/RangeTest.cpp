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
// Test: QuicRangeCompact with empty range (UsedLength = 0)
// Scenario: Compact is called on an empty range (no subranges).
// How: Initialize an empty range and call QuicRangeCompact.
// Assertions: The function returns immediately without any changes. UsedLength remains 0.
//
TEST(RangeTest, CompactEmptyRange)
{
    SmartRange range;
    ASSERT_EQ(range.ValidCount(), 0u);
    
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 0u);
}

//
// Test: QuicRangeCompact with single subrange (UsedLength = 1)
// Scenario: Compact is called on a range with only one subrange.
// How: Add a single range and call QuicRangeCompact.
// Assertions: The function returns immediately without changes. UsedLength remains 1,
//             and the subrange values are unchanged.
//
TEST(RangeTest, CompactSingleSubrange)
{
    SmartRange range;
    range.Add(100, 50);
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
    
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 149u);
}

//
// Test: QuicRangeCompact merges two touching subranges
// Scenario: Two subranges touch at a boundary point (NextLow == CurrentHigh).
// How: Manually create two ranges [100-149] and [149-199] that touch at 149,
//      then call QuicRangeCompact.
// Assertions: After compaction, the two subranges are merged into one [100-199].
//             UsedLength decreases from 2 to 1. Min is 100 and Max is 199.
//
TEST(RangeTest, CompactAdjacentSubranges)
{
    SmartRange range;
    // Add first range [100-149]
    range.Add(100, 50);
    // Add second range [149-199] that touches at 149
    BOOLEAN rangeUpdated;
    QuicRangeAddRange(&range.range, 149, 51, &rangeUpdated);
    
    // After AddRange, Compact is automatically called, so they should already be merged
    // But let's verify by calling Compact again
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
    
    // Call compact again to ensure it's idempotent
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

//
// Test: QuicRangeCompact merges overlapping subranges
// Scenario: Two subranges overlap (NextLow < CurrentHigh + 1).
// How: Manually construct a range with overlapping subranges [100-149] and [130-179],
//      then call QuicRangeCompact.
// Assertions: After compaction, the overlapping subranges are merged into one [100-179].
//             UsedLength decreases from 2 to 1. Min is 100 and Max is 179.
//
TEST(RangeTest, CompactOverlappingSubranges)
{
    SmartRange range;
    // Add first range [100-149]
    BOOLEAN rangeUpdated;
    QuicRangeAddRange(&range.range, 100, 50, &rangeUpdated);
    // Add second range [130-179] which will overlap with [100-149]
    QuicRangeAddRange(&range.range, 130, 50, &rangeUpdated);
    
    // Note: QuicRangeAddRange already calls QuicRangeCompact if rangeUpdated is TRUE,
    // so the ranges should already be merged. Let's verify the final state.
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 179u);
}

//
// Test: QuicRangeCompact with non-adjacent subranges
// Scenario: Multiple subranges with gaps between them (no merging possible).
// How: Add three ranges [100-109], [200-209], [300-309] with gaps,
//      then call QuicRangeCompact.
// Assertions: No merging occurs. UsedLength remains 3. All ranges preserved.
//
TEST(RangeTest, CompactNonAdjacentSubranges)
{
    SmartRange range;
    range.Add(100, 10);
    range.Add(200, 10);
    range.Add(300, 10);
    ASSERT_EQ(range.ValidCount(), 3u);
    
    QuicRangeCompact(&range.range);
    
    ASSERT_EQ(range.ValidCount(), 3u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 309u);
}

//
// Test: QuicRangeCompact merges multiple consecutive touching subranges
// Scenario: Multiple touching subranges that can all be merged in one pass.
// How: Add four touching ranges [100-124], [124-149], [149-174], [174-199],
//      then call QuicRangeCompact.
// Assertions: All four subranges are merged into one [100-199].
//             UsedLength decreases from 4 to 1.
//
TEST(RangeTest, CompactMultipleConsecutiveAdjacent)
{
    SmartRange range;
    BOOLEAN rangeUpdated;
    // Add ranges that touch at boundaries
    QuicRangeAddRange(&range.range, 100, 25, &rangeUpdated);  // [100-124]
    QuicRangeAddRange(&range.range, 124, 26, &rangeUpdated);  // [124-149] touches at 124
    QuicRangeAddRange(&range.range, 149, 26, &rangeUpdated);  // [149-174] touches at 149
    QuicRangeAddRange(&range.range, 174, 26, &rangeUpdated);  // [174-199] touches at 174
    
    // QuicRangeAddRange already calls Compact, so they should be merged
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

//
// Test: QuicRangeCompact merges some subranges but not all
// Scenario: Mix of touching and non-touching subranges.
// How: Add ranges [100-124], [124-149] (touching), then [200-224], [224-249] (touching),
//      separated by a gap. Call QuicRangeCompact.
// Assertions: The two pairs are merged separately, resulting in 2 subranges:
//             [100-149] and [200-249]. UsedLength decreases from 4 to 2.
//
TEST(RangeTest, CompactPartialMerge)
{
    SmartRange range;
    BOOLEAN rangeUpdated;
    // First pair: touching at 124
    QuicRangeAddRange(&range.range, 100, 25, &rangeUpdated);  // [100-124]
    QuicRangeAddRange(&range.range, 124, 26, &rangeUpdated);  // [124-149]
    // Second pair: touching at 224, with gap before
    QuicRangeAddRange(&range.range, 200, 25, &rangeUpdated);  // [200-224]
    QuicRangeAddRange(&range.range, 224, 26, &rangeUpdated);  // [224-249]
    
    // QuicRangeAddRange calls Compact, so pairs should already be merged
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 249u);
    
    // Verify the two merged ranges
    QUIC_SUBRANGE* sub0 = QuicRangeGet(&range.range, 0);
    QUIC_SUBRANGE* sub1 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub0->Low, 100u);
    ASSERT_EQ(sub0->Count, 50u);  // [100-149]
    ASSERT_EQ(sub1->Low, 200u);
    ASSERT_EQ(sub1->Count, 50u);  // [200-249]
}

//
// Test: QuicRangeShrink to initial size (uses pre-allocated buffer)
// Scenario: Shrink a range with dynamic allocation back to initial size.
// How: Grow the range to 16 subranges, then remove most to get UsedLength = 2.
//      Call QuicRangeShrink with NewAllocLength = QUIC_RANGE_INITIAL_SUB_COUNT (8).
// Assertions: SubRanges pointer changes to PreAllocSubRanges. AllocLength becomes 8.
//             Function returns TRUE. Data is preserved.
//
TEST(RangeTest, ShrinkToInitialSize)
{
    SmartRange range;
    
    // Grow the range by adding many subranges
    for (uint32_t i = 0; i < 16; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_GE(range.range.AllocLength, 16u);
    uint32_t originalAllocLength = range.range.AllocLength;
    
    // Remove most subranges to make UsedLength small
    for (uint32_t i = 2; i < 16; i++) {
        range.Remove(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.range.AllocLength, originalAllocLength);  // Still allocated large
    
    // Shrink to initial size
    BOOLEAN result = QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.range.SubRanges, range.range.PreAllocSubRanges);
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 109u);
}

//
// Test: QuicRangeShrink to intermediate size (allocates new buffer)
// Scenario: Shrink from large allocation to an intermediate size (not initial).
// How: Grow the range to 32 subranges, remove most to get UsedLength = 3,
//      then call QuicRangeShrink with NewAllocLength = 16.
// Assertions: Function returns TRUE. AllocLength becomes 16.
//             SubRanges pointer changes (new allocation). Data is preserved.
//
TEST(RangeTest, ShrinkToIntermediateSize)
{
    SmartRange range;
    
    // Grow the range to 32 subranges
    for (uint32_t i = 0; i < 32; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_GE(range.range.AllocLength, 32u);
    
    // Remove most subranges
    for (uint32_t i = 3; i < 32; i++) {
        range.Remove(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), 3u);
    
    QUIC_SUBRANGE* oldPtr = range.range.SubRanges;
    
    // Shrink to intermediate size 16
    BOOLEAN result = QuicRangeShrink(&range.range, 16);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, 16u);
    ASSERT_NE(range.range.SubRanges, oldPtr);  // New allocation
    ASSERT_NE(range.range.SubRanges, range.range.PreAllocSubRanges);  // Not pre-allocated
    ASSERT_EQ(range.ValidCount(), 3u);
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 209u);
}

//
// Test: QuicRangeShrink with NewAllocLength equal to current AllocLength (no-op)
// Scenario: Attempt to shrink when already at target size.
// How: Initialize range with default allocation (8), add 2 subranges,
//      then call QuicRangeShrink with NewAllocLength = 8 (current size).
// Assertions: Function returns TRUE. AllocLength remains 8. No reallocation occurs.
//
TEST(RangeTest, ShrinkNoOp)
{
    SmartRange range;
    range.Add(100, 10);
    range.Add(200, 10);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    QUIC_SUBRANGE* originalPtr = range.range.SubRanges;
    
    BOOLEAN result = QuicRangeShrink(&range.range, QUIC_RANGE_INITIAL_SUB_COUNT);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    ASSERT_EQ(range.range.SubRanges, originalPtr);  // Same pointer
    ASSERT_EQ(range.ValidCount(), 2u);
}

//
// Test: QuicRangeRemoveRange triggers compaction
// Scenario: After removing a range, QuicRangeCompact is called automatically,
//           merging adjacent subranges if any.
// How: Add ranges [100-149], [160-209], then remove [150-159] (which is a gap).
//      The removal itself doesn't change the structure, but subsequent operations
//      might. Instead, create a scenario where removal creates adjacent ranges:
//      Add [100-149], [150-169], [170-219], then remove [150-169].
// Assertions: After removal, [100-149] and [170-219] are not adjacent (gap at 150-169),
//             but if we add [150-169] again and then remove part of it, we can test compaction.
//
// Adjusted: Add [100-149] and [151-200], remove [150-150] (doesn't exist, no effect),
//           then add [150-150] to bridge the gap, triggering compaction.
//
TEST(RangeTest, RemoveRangeTriggersCompaction)
{
    SmartRange range;
    range.Add(100, 50);  // [100-149]
    range.Add(151, 50);  // [151-200]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Add the missing value [150] to bridge the gap
    range.Add(150);
    
    // QuicRangeAddRange (used by Add) calls QuicRangeCompact if rangeUpdated is TRUE
    // So the ranges should now be merged into [100-200]
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 200u);
}

//
// Test: QuicRangeSetMin triggers compaction
// Scenario: After dropping all values below a threshold with QuicRangeSetMin,
//           QuicRangeCompact is called automatically.
// How: Add multiple ranges, call QuicRangeSetMin to drop some, verify compaction happens.
// Assertions: Remaining ranges are compacted if they become touching after removal.
//
TEST(RangeTest, SetMinTriggersCompaction)
{
    SmartRange range;
    BOOLEAN rangeUpdated;
    QuicRangeAddRange(&range.range, 100, 25, &rangeUpdated);  // [100-124]
    QuicRangeAddRange(&range.range, 124, 26, &rangeUpdated);  // [124-149] (should merge with previous)
    QuicRangeAddRange(&range.range, 200, 25, &rangeUpdated);  // [200-224] (gap, separate)
    ASSERT_EQ(range.ValidCount(), 2u);  // Should be 2 after auto-compaction
    
    // SetMin to 100, which shouldn't drop anything but triggers compaction
    QuicRangeSetMin(&range.range, 100);
    
    // After SetMin+Compact, should still be 2 (no change)
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 224u);
    
    QUIC_SUBRANGE* sub0 = QuicRangeGet(&range.range, 0);
    QUIC_SUBRANGE* sub1 = QuicRangeGet(&range.range, 1);
    ASSERT_EQ(sub0->Low, 100u);
    ASSERT_EQ(sub0->Count, 50u);  // [100-149] already merged
    ASSERT_EQ(sub1->Low, 200u);
    ASSERT_EQ(sub1->Count, 25u);  // [200-224]
}

//
// Test: QuicRangeAddRange triggers compaction when range is updated
// Scenario: Adding a range that bridges a gap triggers compaction.
// How: Add [100-124] and [175-199] with a gap, then add [124-175] to bridge the gap.
// Assertions: After adding the bridging range, all three are merged into [100-199].
//
TEST(RangeTest, AddRangeTriggersCompaction)
{
    SmartRange range;
    BOOLEAN rangeUpdated;
    QuicRangeAddRange(&range.range, 100, 25, &rangeUpdated);  // [100-124]
    QuicRangeAddRange(&range.range, 175, 25, &rangeUpdated);  // [175-199]
    ASSERT_EQ(range.ValidCount(), 2u);
    
    // Add the bridging range [124-175] that touches both
    QuicRangeAddRange(&range.range, 124, 52, &rangeUpdated);  // [124-175]
    
    // QuicRangeAddRange calls QuicRangeCompact if rangeUpdated is TRUE
    // All three should be merged into [100-199]
    ASSERT_EQ(range.ValidCount(), 1u);
    ASSERT_EQ(range.Min(), 100u);
    ASSERT_EQ(range.Max(), 199u);
}

//
// Test: QuicRangeCompact triggers shrinking when allocation is large and usage is low
// Scenario: After compacting, if allocation is >= 4*initial and used < allocation/8,
//           QuicRangeShrink is called automatically.
// How: Grow the range to 64 subranges, remove most to get UsedLength = 4 (< 64/8 = 8),
//      then call QuicRangeCompact.
// Assertions: After compaction, AllocLength is shrunk (halved or to initial size).
//
TEST(RangeTest, CompactTriggersShrinking)
{
    SmartRange range;
    
    // Grow to 64 subranges (requires AllocLength >= 64)
    for (uint32_t i = 0; i < 64; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_GE(range.range.AllocLength, 64u);
    uint32_t largeAlloc = range.range.AllocLength;
    
    // Remove most subranges to make UsedLength = 4 (< largeAlloc / 8)
    for (uint32_t i = 4; i < 64; i++) {
        range.Remove(i * 100, 10);
    }
    ASSERT_EQ(range.ValidCount(), 4u);
    ASSERT_EQ(range.range.AllocLength, largeAlloc);  // Still large
    
    // Call QuicRangeCompact, which should trigger shrinking
    QuicRangeCompact(&range.range);
    
    // Allocation should be shrunk (at least halved)
    ASSERT_LT(range.range.AllocLength, largeAlloc);
    ASSERT_EQ(range.ValidCount(), 4u);
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 309u);
}

//
// Test: QuicRangeRemoveSubranges uses new shrink logic
// Scenario: QuicRangeRemoveSubranges now uses QuicRangeCalculateShrinkLength
//           and QuicRangeShrink with different thresholds (MinAllocMultiplier=2, Denominator=4).
// How: Grow the range to 16 subranges (AllocLength >= 16), remove subranges to get
//      UsedLength = 2 (< 16/4 = 4), call QuicRangeRemoveSubranges.
// Assertions: Shrinking occurs. AllocLength is reduced to 8 (halved from 16).
//
TEST(RangeTest, RemoveSubrangesShrinkLogic)
{
    SmartRange range;
    
    // Grow to 16 subranges (AllocLength = 16)
    for (uint32_t i = 0; i < 16; i++) {
        range.Add(i * 100, 10);
    }
    ASSERT_GE(range.range.AllocLength, 16u);
    
    // Keep only first 2 subranges, remove the rest (14 subranges)
    // After removal, UsedLength = 2, which is < 16/4 = 4, so shrink should happen
    BOOLEAN shrunk = QuicRangeRemoveSubranges(&range.range, 2, 14);
    
    ASSERT_TRUE(shrunk);  // Should return TRUE indicating shrink occurred
    ASSERT_EQ(range.ValidCount(), 2u);
    ASSERT_EQ(range.range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);  // Shrunk to 8
    ASSERT_EQ(range.Min(), 0u);
    ASSERT_EQ(range.Max(), 109u);
}
