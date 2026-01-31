/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for the QUIC_ACK_TRACKER component.
    Tests cover initialization, reset, packet number tracking, and duplicate detection.
    The AckTracker manages received packet numbers for duplicate detection and
    tracks packet numbers that need to be acknowledged via ACK frames.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "AckTrackerTest.cpp.clog.h"
#endif

//
// Test 1: Basic initialization and uninitialization
// Scenario: Verifies QuicAckTrackerInitialize correctly sets up the tracker's
// internal QUIC_RANGE structures for PacketNumbersReceived and PacketNumbersToAck.
// Tests the basic lifecycle of creating and destroying an AckTracker.
//
TEST(AckTrackerTest, InitializeAndUninitialize)
{
    QUIC_ACK_TRACKER Tracker;

    //
    // Initialize the tracker - this should set up both internal ranges
    //
    QuicAckTrackerInitialize(&Tracker);

    //
    // Verify the ranges are properly initialized (size should be 0)
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 0u);
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersToAck), 0u);

    //
    // Clean up
    //
    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 2: Reset functionality
// Scenario: Verifies QuicAckTrackerReset correctly clears all state fields
// including AckElicitingPacketsToAcknowledge, LargestPacketNumberAcknowledged,
// LargestPacketNumberRecvTime, flags, ECN counters, and both packet ranges.
//
TEST(AckTrackerTest, Reset)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Set some initial state that should be cleared by reset
    //
    Tracker.AckElicitingPacketsToAcknowledge = 5;
    Tracker.LargestPacketNumberAcknowledged = 100;
    Tracker.LargestPacketNumberRecvTime = 12345;
    Tracker.AlreadyWrittenAckFrame = TRUE;
    Tracker.NonZeroRecvECN = TRUE;
    Tracker.ReceivedECN.ECT_0_Count = 10;
    Tracker.ReceivedECN.ECT_1_Count = 20;
    Tracker.ReceivedECN.CE_Count = 5;

    //
    // Add some packet numbers to the ranges
    //
    QuicRangeAddValue(&Tracker.PacketNumbersReceived, 1);
    QuicRangeAddValue(&Tracker.PacketNumbersReceived, 2);
    QuicRangeAddValue(&Tracker.PacketNumbersToAck, 1);
    QuicRangeAddValue(&Tracker.PacketNumbersToAck, 2);

    ASSERT_GT(QuicRangeSize(&Tracker.PacketNumbersReceived), 0u);
    ASSERT_GT(QuicRangeSize(&Tracker.PacketNumbersToAck), 0u);

    //
    // Reset the tracker
    //
    QuicAckTrackerReset(&Tracker);

    //
    // Verify all fields are reset
    //
    ASSERT_EQ(Tracker.AckElicitingPacketsToAcknowledge, 0u);
    ASSERT_EQ(Tracker.LargestPacketNumberAcknowledged, 0u);
    ASSERT_EQ(Tracker.LargestPacketNumberRecvTime, 0u);
    ASSERT_FALSE(Tracker.AlreadyWrittenAckFrame);
    ASSERT_FALSE(Tracker.NonZeroRecvECN);
    ASSERT_EQ(Tracker.ReceivedECN.ECT_0_Count, 0u);
    ASSERT_EQ(Tracker.ReceivedECN.ECT_1_Count, 0u);
    ASSERT_EQ(Tracker.ReceivedECN.CE_Count, 0u);
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 0u);
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersToAck), 0u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 3: Add first packet number (not a duplicate)
// Scenario: When adding the first packet number to an empty tracker,
// QuicAckTrackerAddPacketNumber should return FALSE (not a duplicate)
// and the packet should be added to PacketNumbersReceived.
//
TEST(AckTrackerTest, AddFirstPacketNumber)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // First packet should not be a duplicate
    //
    BOOLEAN IsDuplicate = QuicAckTrackerAddPacketNumber(&Tracker, 100);
    ASSERT_FALSE(IsDuplicate);

    //
    // Verify the packet was added to the received range
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Min, Max;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Min, 100u);
    ASSERT_EQ(Max, 100u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 4: Detect duplicate packet number
// Scenario: When the same packet number is added twice, the second call to
// QuicAckTrackerAddPacketNumber should return TRUE indicating a duplicate.
//
TEST(AckTrackerTest, DetectDuplicatePacketNumber)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packet 100 first time - not a duplicate
    //
    BOOLEAN FirstAdd = QuicAckTrackerAddPacketNumber(&Tracker, 100);
    ASSERT_FALSE(FirstAdd);

    //
    // Add packet 100 again - should be detected as duplicate
    //
    BOOLEAN SecondAdd = QuicAckTrackerAddPacketNumber(&Tracker, 100);
    ASSERT_TRUE(SecondAdd);

    //
    // Range should still only have one entry (no duplicate added)
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 5: Add multiple sequential packet numbers
// Scenario: Adding sequential packet numbers should result in a single
// contiguous range in PacketNumbersReceived. None should be duplicates.
//
TEST(AckTrackerTest, AddSequentialPacketNumbers)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packets 100, 101, 102 in order
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 100));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 101));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 102));

    //
    // Should be merged into a single range
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Min, Max;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Min, 100u);
    ASSERT_EQ(Max, 102u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 6: Add packet numbers with gaps
// Scenario: Adding non-sequential packet numbers should create multiple
// ranges in PacketNumbersReceived. Each unique packet is not a duplicate.
//
TEST(AckTrackerTest, AddPacketNumbersWithGaps)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packets 100, 102, 104 (with gaps at 101 and 103)
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 100));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 102));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 104));

    //
    // Should have three separate ranges due to gaps
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 3u);

    uint64_t Min, Max;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Min, 100u);
    ASSERT_EQ(Max, 104u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 7: Fill gaps merges ranges
// Scenario: Adding the missing packet numbers between existing ranges
// should merge them into a single contiguous range.
//
TEST(AckTrackerTest, FillGapsMergesRanges)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packets 100, 102 (gap at 101)
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 100));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 102));
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 2u);

    //
    // Fill the gap with packet 101
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 101));

    //
    // Should now be merged into a single range
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Min, Max;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Min, 100u);
    ASSERT_EQ(Max, 102u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 8: Add packets in reverse order
// Scenario: Adding packet numbers in reverse order should still correctly
// track all packets and merge adjacent ranges.
//
TEST(AckTrackerTest, AddPacketsReverseOrder)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packets 102, 101, 100 in reverse order
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 102));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 101));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 100));

    //
    // Should be merged into a single range
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Min, Max;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Min, 100u);
    ASSERT_EQ(Max, 102u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 9: QuicAckTrackerHasPacketsToAck when empty
// Scenario: An empty tracker should report no packets to acknowledge
// when PacketNumbersToAck is empty.
//
TEST(AckTrackerTest, HasPacketsToAckWhenEmpty)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Empty tracker should have no packets to ack
    //
    ASSERT_FALSE(QuicAckTrackerHasPacketsToAck(&Tracker));

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 10: QuicAckTrackerHasPacketsToAck with packets
// Scenario: When packets are added to PacketNumbersToAck and
// AlreadyWrittenAckFrame is FALSE, HasPacketsToAck should return TRUE.
//
TEST(AckTrackerTest, HasPacketsToAckWithPackets)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add a packet to the ToAck range
    //
    QuicRangeAddValue(&Tracker.PacketNumbersToAck, 100);
    Tracker.AlreadyWrittenAckFrame = FALSE;

    //
    // Should indicate packets to ack
    //
    ASSERT_TRUE(QuicAckTrackerHasPacketsToAck(&Tracker));

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 11: QuicAckTrackerHasPacketsToAck after writing ACK frame
// Scenario: When AlreadyWrittenAckFrame is TRUE, HasPacketsToAck should
// return FALSE even if there are packets in PacketNumbersToAck.
//
TEST(AckTrackerTest, HasPacketsToAckAfterWritingFrame)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add a packet and mark ACK frame as written
    //
    QuicRangeAddValue(&Tracker.PacketNumbersToAck, 100);
    Tracker.AlreadyWrittenAckFrame = TRUE;

    //
    // Should not indicate packets to ack (already written)
    //
    ASSERT_FALSE(QuicAckTrackerHasPacketsToAck(&Tracker));

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 12: Packet number zero handling
// Scenario: Packet number 0 is a valid packet number and should be
// tracked correctly without being treated specially.
//
TEST(AckTrackerTest, PacketNumberZero)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Packet 0 should not be a duplicate on first add
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 0));

    //
    // Verify it was added
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Min;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_EQ(Min, 0u);

    //
    // Adding again should be a duplicate
    //
    ASSERT_TRUE(QuicAckTrackerAddPacketNumber(&Tracker, 0));

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 13: Large packet number handling
// Scenario: Large packet numbers near the maximum should be handled
// correctly without overflow issues.
//
TEST(AckTrackerTest, LargePacketNumbers)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Use a large packet number (close to max varint value)
    //
    const uint64_t LargePacketNumber = QUIC_VAR_INT_MAX - 10;

    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, LargePacketNumber));
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, LargePacketNumber + 1));

    //
    // Verify they were added correctly
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Max;
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Max, LargePacketNumber + 1);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 14: Multiple reset cycles
// Scenario: The tracker should work correctly across multiple reset cycles,
// maintaining proper state management.
//
TEST(AckTrackerTest, MultipleResetCycles)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    for (int cycle = 0; cycle < 3; cycle++) {
        //
        // Add some packets
        //
        uint64_t BasePacket = (uint64_t)cycle * 100;
        ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, BasePacket));
        ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, BasePacket + 1));

        //
        // Verify state
        //
        ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

        //
        // Reset and verify clean state
        //
        QuicAckTrackerReset(&Tracker);
        ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 0u);
        ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersToAck), 0u);
    }

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 15: ECN state reset verification
// Scenario: Verifies that ECN-related state is properly cleared on reset,
// including all three ECN counter types.
//
TEST(AckTrackerTest, EcnStateReset)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Set various ECN state
    //
    Tracker.NonZeroRecvECN = TRUE;
    Tracker.ReceivedECN.ECT_0_Count = 100;
    Tracker.ReceivedECN.ECT_1_Count = 200;
    Tracker.ReceivedECN.CE_Count = 50;

    //
    // Reset
    //
    QuicAckTrackerReset(&Tracker);

    //
    // Verify all ECN state is cleared
    //
    ASSERT_FALSE(Tracker.NonZeroRecvECN);
    ASSERT_EQ(Tracker.ReceivedECN.ECT_0_Count, 0u);
    ASSERT_EQ(Tracker.ReceivedECN.ECT_1_Count, 0u);
    ASSERT_EQ(Tracker.ReceivedECN.CE_Count, 0u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 16: Duplicate detection across resets
// Scenario: After a reset, the same packet number should not be detected
// as a duplicate since the tracking state is cleared.
//
TEST(AckTrackerTest, DuplicateDetectionAcrossReset)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packet 100
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 100));

    //
    // Verify duplicate detection works
    //
    ASSERT_TRUE(QuicAckTrackerAddPacketNumber(&Tracker, 100));

    //
    // Reset the tracker
    //
    QuicAckTrackerReset(&Tracker);

    //
    // After reset, the same packet should not be a duplicate
    //
    ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, 100));

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 17: Timing field reset verification
// Scenario: Verifies that timing-related fields like LargestPacketNumberRecvTime
// are properly reset to zero.
//
TEST(AckTrackerTest, TimingFieldsReset)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Set timing fields
    //
    Tracker.LargestPacketNumberRecvTime = 1234567890;
    Tracker.LargestPacketNumberAcknowledged = 500;
    Tracker.AckElicitingPacketsToAcknowledge = 10;

    //
    // Reset
    //
    QuicAckTrackerReset(&Tracker);

    //
    // Verify timing fields are cleared
    //
    ASSERT_EQ(Tracker.LargestPacketNumberRecvTime, 0u);
    ASSERT_EQ(Tracker.LargestPacketNumberAcknowledged, 0u);
    ASSERT_EQ(Tracker.AckElicitingPacketsToAcknowledge, 0u);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 18: Boolean flags reset verification
// Scenario: Verifies that all boolean flags are properly reset to FALSE.
//
TEST(AckTrackerTest, BooleanFlagsReset)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Set boolean flags to TRUE
    //
    Tracker.AlreadyWrittenAckFrame = TRUE;
    Tracker.NonZeroRecvECN = TRUE;

    //
    // Reset
    //
    QuicAckTrackerReset(&Tracker);

    //
    // Verify flags are FALSE
    //
    ASSERT_FALSE(Tracker.AlreadyWrittenAckFrame);
    ASSERT_FALSE(Tracker.NonZeroRecvECN);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 19: Add many unique packets
// Scenario: Adding a large number of sequential packets should work correctly,
// with all being tracked in a single contiguous range.
//
TEST(AckTrackerTest, AddManyUniquePackets)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    const uint64_t StartPacket = 1000;
    const uint64_t Count = 100;

    //
    // Add many sequential packets
    //
    for (uint64_t i = 0; i < Count; i++) {
        ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, StartPacket + i));
    }

    //
    // Should be a single contiguous range
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 1u);

    uint64_t Min, Max;
    ASSERT_TRUE(QuicRangeGetMinSafe(&Tracker.PacketNumbersReceived, &Min));
    ASSERT_TRUE(QuicRangeGetMaxSafe(&Tracker.PacketNumbersReceived, &Max));
    ASSERT_EQ(Min, StartPacket);
    ASSERT_EQ(Max, StartPacket + Count - 1);

    QuicAckTrackerUninitialize(&Tracker);
}

//
// Test 20: Alternating packet numbers create multiple ranges
// Scenario: Adding every other packet number should create many separate ranges.
//
TEST(AckTrackerTest, AlternatingPacketNumbers)
{
    QUIC_ACK_TRACKER Tracker;
    QuicAckTrackerInitialize(&Tracker);

    //
    // Add packets 0, 2, 4, 6, 8 (every other)
    //
    for (uint64_t i = 0; i < 10; i += 2) {
        ASSERT_FALSE(QuicAckTrackerAddPacketNumber(&Tracker, i));
    }

    //
    // Should have 5 separate ranges
    //
    ASSERT_EQ(QuicRangeSize(&Tracker.PacketNumbersReceived), 5u);

    QuicAckTrackerUninitialize(&Tracker);
}
