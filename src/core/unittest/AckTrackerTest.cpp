/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for the QUIC_ACK_TRACKER interface.

    The Ack Tracker manages packet number tracking for duplicate detection
    and ACK frame generation. These tests verify the public interface behavior
    for initialization, reset, duplicate detection, and reordering threshold logic.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "AckTrackerTest.cpp.clog.h"
#endif

//
// Helper class for RAII-style QUIC_ACK_TRACKER management in tests.
// Ensures proper initialization and cleanup of the tracker.
//
struct SmartAckTracker {
    QUIC_ACK_TRACKER tracker;
    bool initialized;

    SmartAckTracker() : initialized(false) {
        CxPlatZeroMemory(&tracker, sizeof(tracker));
        QuicAckTrackerInitialize(&tracker);
        initialized = true;
    }

    ~SmartAckTracker() {
        if (initialized) {
            QuicAckTrackerUninitialize(&tracker);
        }
    }

    void Reset() {
        QuicAckTrackerReset(&tracker);
    }

    //
    // Returns TRUE if packet is a duplicate.
    //
    BOOLEAN AddPacketNumber(uint64_t PacketNumber) {
        return QuicAckTrackerAddPacketNumber(&tracker, PacketNumber);
    }

    BOOLEAN HasPacketsToAck() {
        return QuicAckTrackerHasPacketsToAck(&tracker);
    }

    //
    // Directly add a value to PacketNumbersToAck range for testing
    // reordering threshold logic.
    //
    bool AddToAckRange(uint64_t Value) {
        return QuicRangeAddValue(&tracker.PacketNumbersToAck, Value) != FALSE;
    }

    //
    // Add a contiguous range to PacketNumbersToAck for testing.
    //
    bool AddToAckRange(uint64_t Low, uint64_t Count) {
        BOOLEAN RangeUpdated;
        return QuicRangeAddRange(&tracker.PacketNumbersToAck, Low, Count, &RangeUpdated) != NULL;
    }

    uint32_t GetAckRangeSize() {
        return QuicRangeSize(&tracker.PacketNumbersToAck);
    }

    uint32_t GetReceivedRangeSize() {
        return QuicRangeSize(&tracker.PacketNumbersReceived);
    }
};

//
// Test: QuicAckTrackerInitialize creates a properly initialized tracker
// with empty ranges and zeroed state.
//
// Scenario: Verify that after initialization, the tracker has no packets
// in either the received or to-ack ranges.
//
// Assertions: Both PacketNumbersReceived and PacketNumbersToAck ranges
// are empty (size = 0).
//
TEST(AckTrackerTest, InitializeCreatesEmptyTracker)
{
    SmartAckTracker tracker;

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 0u);
    ASSERT_EQ(tracker.GetAckRangeSize(), 0u);
    ASSERT_EQ(tracker.tracker.AckElicitingPacketsToAcknowledge, 0u);
    ASSERT_EQ(tracker.tracker.LargestPacketNumberAcknowledged, 0u);
    ASSERT_EQ(tracker.tracker.LargestPacketNumberRecvTime, 0u);
    ASSERT_EQ(tracker.tracker.AlreadyWrittenAckFrame, FALSE);
    ASSERT_EQ(tracker.tracker.NonZeroRecvECN, FALSE);
}

//
// Test: QuicAckTrackerReset clears all state back to initial values.
//
// Scenario: Add packets to tracker, then reset and verify all state is cleared.
//
// Assertions: After reset, all counters are zero, flags are FALSE, and
// ranges are empty.
//
TEST(AckTrackerTest, ResetClearsAllState)
{
    SmartAckTracker tracker;

    // Add some packets to both ranges
    tracker.AddPacketNumber(100);
    tracker.AddPacketNumber(101);
    tracker.AddToAckRange(100);
    tracker.AddToAckRange(101);

    // Modify other state fields
    tracker.tracker.AckElicitingPacketsToAcknowledge = 5;
    tracker.tracker.LargestPacketNumberAcknowledged = 100;
    tracker.tracker.LargestPacketNumberRecvTime = 12345;
    tracker.tracker.AlreadyWrittenAckFrame = TRUE;
    tracker.tracker.NonZeroRecvECN = TRUE;
    tracker.tracker.ReceivedECN.ECT_0_Count = 1;
    tracker.tracker.ReceivedECN.ECT_1_Count = 2;
    tracker.tracker.ReceivedECN.CE_Count = 3;

    ASSERT_GT(tracker.GetReceivedRangeSize(), 0u);
    ASSERT_GT(tracker.GetAckRangeSize(), 0u);

    // Reset and verify
    tracker.Reset();

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 0u);
    ASSERT_EQ(tracker.GetAckRangeSize(), 0u);
    ASSERT_EQ(tracker.tracker.AckElicitingPacketsToAcknowledge, 0u);
    ASSERT_EQ(tracker.tracker.LargestPacketNumberAcknowledged, 0u);
    ASSERT_EQ(tracker.tracker.LargestPacketNumberRecvTime, 0u);
    ASSERT_EQ(tracker.tracker.AlreadyWrittenAckFrame, FALSE);
    ASSERT_EQ(tracker.tracker.NonZeroRecvECN, FALSE);
    ASSERT_EQ(tracker.tracker.ReceivedECN.ECT_0_Count, 0u);
    ASSERT_EQ(tracker.tracker.ReceivedECN.ECT_1_Count, 0u);
    ASSERT_EQ(tracker.tracker.ReceivedECN.CE_Count, 0u);
}

//
// Test: QuicAckTrackerAddPacketNumber returns FALSE for new (non-duplicate) packets.
//
// Scenario: Add a single packet number that hasn't been seen before.
//
// Assertions: Return value is FALSE (not a duplicate), and the packet is
// added to the received range.
//
TEST(AckTrackerTest, AddPacketNumberNonDuplicate)
{
    SmartAckTracker tracker;

    BOOLEAN isDuplicate = tracker.AddPacketNumber(100);

    ASSERT_EQ(isDuplicate, FALSE);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u);
}

//
// Test: QuicAckTrackerAddPacketNumber returns TRUE for duplicate packets.
//
// Scenario: Add the same packet number twice.
//
// Assertions: First call returns FALSE (new), second call returns TRUE (duplicate).
// Range size remains 1 after duplicate.
//
TEST(AckTrackerTest, AddPacketNumberDuplicate)
{
    SmartAckTracker tracker;

    BOOLEAN firstAdd = tracker.AddPacketNumber(100);
    BOOLEAN secondAdd = tracker.AddPacketNumber(100);

    ASSERT_EQ(firstAdd, FALSE);
    ASSERT_EQ(secondAdd, TRUE);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u);
}

//
// Test: QuicAckTrackerAddPacketNumber handles sequential packets correctly.
//
// Scenario: Add consecutive packet numbers 100, 101, 102.
//
// Assertions: All packets are added as non-duplicates, and they coalesce
// into a single contiguous range.
//
TEST(AckTrackerTest, AddMultipleSequentialPackets)
{
    SmartAckTracker tracker;

    ASSERT_EQ(tracker.AddPacketNumber(100), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(101), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(102), FALSE);

    // Sequential packets should merge into one range
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u);
}

//
// Test: QuicAckTrackerAddPacketNumber handles out-of-order packets correctly.
//
// Scenario: Add packets in non-sequential order: 100, 102, 101.
//
// Assertions: All packets marked as non-duplicates. After adding the
// middle packet (101), ranges should merge into one.
//
TEST(AckTrackerTest, AddOutOfOrderPackets)
{
    SmartAckTracker tracker;

    ASSERT_EQ(tracker.AddPacketNumber(100), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(102), FALSE);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 2u); // Two separate ranges

    ASSERT_EQ(tracker.AddPacketNumber(101), FALSE);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u); // Merged into one range
}

//
// Test: QuicAckTrackerAddPacketNumber handles gaps correctly.
//
// Scenario: Add packets 100, 105, 110 with gaps between them.
//
// Assertions: Each packet creates a separate range, resulting in 3 ranges.
//
TEST(AckTrackerTest, AddPacketsWithGaps)
{
    SmartAckTracker tracker;

    ASSERT_EQ(tracker.AddPacketNumber(100), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(105), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(110), FALSE);

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 3u);
}

//
// Test: QuicAckTrackerHasPacketsToAck returns FALSE for empty tracker.
//
// Scenario: Check HasPacketsToAck on a freshly initialized tracker.
//
// Assertions: Returns FALSE since no packets have been added to ACK range.
//
TEST(AckTrackerTest, HasPacketsToAckEmpty)
{
    SmartAckTracker tracker;

    ASSERT_EQ(tracker.HasPacketsToAck(), FALSE);
}

//
// Test: QuicAckTrackerHasPacketsToAck returns TRUE when packets are pending.
//
// Scenario: Add packets to the ACK range, then check HasPacketsToAck.
//
// Assertions: Returns TRUE when PacketNumbersToAck is non-empty and
// AlreadyWrittenAckFrame is FALSE.
//
TEST(AckTrackerTest, HasPacketsToAckWithPackets)
{
    SmartAckTracker tracker;

    tracker.AddToAckRange(100);

    ASSERT_EQ(tracker.HasPacketsToAck(), TRUE);
}

//
// Test: QuicAckTrackerHasPacketsToAck returns FALSE after ACK frame written.
//
// Scenario: Add packets, set AlreadyWrittenAckFrame = TRUE, check result.
//
// Assertions: Returns FALSE even though packets exist, because ACK was written.
//
TEST(AckTrackerTest, HasPacketsToAckAfterFrameWritten)
{
    SmartAckTracker tracker;

    tracker.AddToAckRange(100);
    ASSERT_EQ(tracker.HasPacketsToAck(), TRUE);

    tracker.tracker.AlreadyWrittenAckFrame = TRUE;
    ASSERT_EQ(tracker.HasPacketsToAck(), FALSE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold returns FALSE when threshold is 0.
//
// Scenario: Set threshold to 0 (disables reordering detection).
//
// Assertions: Always returns FALSE regardless of packet ranges.
//
TEST(AckTrackerTest, ReorderingThresholdZeroReturnsFalse)
{
    SmartAckTracker tracker;

    tracker.AddToAckRange(100);
    tracker.AddToAckRange(105);

    BOOLEAN result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 0);
    ASSERT_EQ(result, FALSE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold returns FALSE for single range.
//
// Scenario: Only one contiguous range exists (no gaps).
//
// Assertions: Returns FALSE because at least 2 ranges needed for reordering.
//
TEST(AckTrackerTest, ReorderingThresholdSingleRangeReturnsFalse)
{
    SmartAckTracker tracker;

    tracker.AddToAckRange(100, 5); // Range [100, 104]

    BOOLEAN result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 3);
    ASSERT_EQ(result, FALSE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold correctly detects reordering.
//
// Scenario: Multiple ranges with gap exceeding threshold.
// Per draft-ietf-quic-frequence-10, we check if the gap between the smallest
// unreported missing packet and the largest unacked packet >= threshold.
//
// Assertions: Returns TRUE when reordering threshold is exceeded.
//
TEST(AckTrackerTest, ReorderingThresholdExceeded)
{
    SmartAckTracker tracker;

    // Create ranges: [0,1] and [5] (missing 2,3,4)
    // LargestUnacked = 5, SmallestMissing = 2, Gap = 5-2 = 3
    tracker.AddToAckRange(0, 2); // [0, 1]
    tracker.AddToAckRange(5);     // [5]
    tracker.tracker.LargestPacketNumberAcknowledged = 0;

    // With threshold=3, gap (5-2=3) should trigger
    BOOLEAN result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 3);
    ASSERT_EQ(result, TRUE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold returns FALSE below threshold.
//
// Scenario: Gap exists but is less than threshold.
//
// Assertions: Returns FALSE when gap < threshold.
//
TEST(AckTrackerTest, ReorderingThresholdNotExceeded)
{
    SmartAckTracker tracker;

    // Create ranges: [0,1] and [4] (missing 2,3)
    // LargestUnacked = 4, SmallestMissing = 2, Gap = 4-2 = 2
    tracker.AddToAckRange(0, 2); // [0, 1]
    tracker.AddToAckRange(4);     // [4]
    tracker.tracker.LargestPacketNumberAcknowledged = 0;

    // With threshold=3, gap (4-2=2) should NOT trigger
    BOOLEAN result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 3);
    ASSERT_EQ(result, FALSE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold with LargestPacketNumberAcknowledged > 0.
//
// Scenario: Test the LargestReported calculation when prior ACKs have been sent.
// The logic adjusts based on what was previously acknowledged.
//
// Assertions: Threshold check accounts for LargestPacketNumberAcknowledged.
//
TEST(AckTrackerTest, ReorderingThresholdWithPriorAck)
{
    SmartAckTracker tracker;

    // Ranges: [0,1], [3,5], [8]
    // With LargestPacketNumberAcknowledged = 5 and threshold 3:
    // LargestReported = 5 - 3 + 1 = 3
    // Check gaps after packet 3
    tracker.AddToAckRange(0, 2); // [0, 1]
    tracker.AddToAckRange(3, 3); // [3, 5]
    tracker.AddToAckRange(8);     // [8]
    tracker.tracker.LargestPacketNumberAcknowledged = 5;

    // Gap from [5] to [8] is 2 (missing 6,7), plus the range check
    // SmallestMissing after LargestReported (3) = 6, LargestUnacked = 8
    // Gap = 8 - 6 = 2 < 3, should be FALSE
    BOOLEAN result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 3);
    ASSERT_EQ(result, FALSE);

    // Add packet 9 to increase gap
    tracker.AddToAckRange(9);
    // Now gap = 9 - 6 = 3 >= 3, should be TRUE
    result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 3);
    ASSERT_EQ(result, TRUE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold with complex multi-range scenario.
//
// Scenario: Multiple gaps exist; test that the smallest missing packet after
// LargestReported is correctly identified.
//
// Assertions: Correctly identifies the relevant gap for threshold comparison.
//
TEST(AckTrackerTest, ReorderingThresholdMultipleGaps)
{
    SmartAckTracker tracker;

    // Ranges: [0], [2], [4], [6,7]
    // Gaps at: 1, 3, 5
    tracker.AddToAckRange(0);
    tracker.AddToAckRange(2);
    tracker.AddToAckRange(4);
    tracker.AddToAckRange(6, 2); // [6, 7]
    tracker.tracker.LargestPacketNumberAcknowledged = 0;

    // LargestUnacked = 7, SmallestMissing = 1, Gap = 7-1 = 6 >= 5
    BOOLEAN result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 5);
    ASSERT_EQ(result, TRUE);

    // With higher threshold, should not trigger
    result = QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 7);
    ASSERT_EQ(result, FALSE);
}

//
// Test: Duplicate detection with large packet numbers.
//
// Scenario: Test with packet numbers near uint64_t max to ensure no overflow issues.
//
// Assertions: Correctly handles large packet numbers.
//
TEST(AckTrackerTest, AddLargePacketNumbers)
{
    SmartAckTracker tracker;

    uint64_t largeNum = UINT64_MAX - 1000;

    ASSERT_EQ(tracker.AddPacketNumber(largeNum), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(largeNum + 1), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(largeNum), TRUE); // Duplicate

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u);
}

//
// Test: QuicAckTrackerReset on already empty tracker.
//
// Scenario: Reset a tracker that has never had packets added.
//
// Assertions: Reset succeeds and tracker remains in valid empty state.
//
TEST(AckTrackerTest, ResetEmptyTracker)
{
    SmartAckTracker tracker;

    // Tracker is already empty
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 0u);
    ASSERT_EQ(tracker.GetAckRangeSize(), 0u);

    // Reset should be safe
    tracker.Reset();

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 0u);
    ASSERT_EQ(tracker.GetAckRangeSize(), 0u);
}

//
// Test: Multiple resets are safe.
//
// Scenario: Call reset multiple times in succession.
//
// Assertions: Multiple resets don't cause issues.
//
TEST(AckTrackerTest, MultipleResets)
{
    SmartAckTracker tracker;

    tracker.AddPacketNumber(100);
    tracker.Reset();
    tracker.Reset();
    tracker.Reset();

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 0u);

    // Verify tracker still works after multiple resets
    ASSERT_EQ(tracker.AddPacketNumber(200), FALSE);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u);
}

//
// Test: Adding packets after reset.
//
// Scenario: Add packets, reset, then add more packets.
//
// Assertions: Previously seen packets are no longer considered duplicates
// after reset.
//
TEST(AckTrackerTest, AddPacketsAfterReset)
{
    SmartAckTracker tracker;

    // Add and verify
    ASSERT_EQ(tracker.AddPacketNumber(100), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(100), TRUE);

    // Reset
    tracker.Reset();

    // Same packet number is now new again
    ASSERT_EQ(tracker.AddPacketNumber(100), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(100), TRUE);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold with boundary values.
//
// Scenario: Test exact threshold boundary conditions.
//
// Assertions: Returns FALSE when gap == threshold - 1, TRUE when gap == threshold.
//
TEST(AckTrackerTest, ReorderingThresholdBoundary)
{
    SmartAckTracker tracker;

    // Create gap of exactly threshold - 1
    // Ranges: [0] and [4] with threshold 5
    // Gap = 4 - 1 = 3 < 5-1=4... wait let me recalculate
    // SmallestMissing = 1, LargestUnacked = 4, gap = 4-1 = 3
    tracker.AddToAckRange(0);
    tracker.AddToAckRange(4);
    tracker.tracker.LargestPacketNumberAcknowledged = 0;

    // With threshold=4, gap=3 < 4, should be FALSE
    ASSERT_EQ(QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 4), FALSE);

    // Add packet 5 to increase gap to 4
    tracker.AddToAckRange(5);
    // Gap = 5 - 1 = 4 >= 4, should be TRUE
    ASSERT_EQ(QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 4), TRUE);
}

//
// Test: Interleaved add and duplicate check operations.
//
// Scenario: Mix of adds and duplicate checks in various orders.
//
// Assertions: Duplicate detection works correctly throughout.
//
TEST(AckTrackerTest, InterleavedAddAndDuplicateCheck)
{
    SmartAckTracker tracker;

    // Add some packets
    ASSERT_EQ(tracker.AddPacketNumber(10), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(20), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(30), FALSE);

    // Check duplicates
    ASSERT_EQ(tracker.AddPacketNumber(20), TRUE);
    ASSERT_EQ(tracker.AddPacketNumber(10), TRUE);
    ASSERT_EQ(tracker.AddPacketNumber(30), TRUE);

    // Add new packet
    ASSERT_EQ(tracker.AddPacketNumber(15), FALSE);

    // Check new and old duplicates
    ASSERT_EQ(tracker.AddPacketNumber(15), TRUE);
    ASSERT_EQ(tracker.AddPacketNumber(25), FALSE);
    ASSERT_EQ(tracker.AddPacketNumber(25), TRUE);
}
