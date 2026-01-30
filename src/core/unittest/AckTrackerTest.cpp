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
    // Returns true if packet is a duplicate.
    //
    bool AddPacketNumber(uint64_t PacketNumber) {
        return QuicAckTrackerAddPacketNumber(&tracker, PacketNumber) != FALSE;
    }

    bool HasPacketsToAck() {
        return QuicAckTrackerHasPacketsToAck(&tracker) != FALSE;
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

    bool isDuplicate = tracker.AddPacketNumber(100);

    ASSERT_EQ(isDuplicate, false);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 1u);
}

//
// Test: QuicAckTrackerAddPacketNumber returns TRUE for duplicate packets.
//
// Scenario: Add the same packet number twice.
//
// Assertions: First call returns false (new), second call returns true (duplicate).
// Range size remains 1 after duplicate.
//
TEST(AckTrackerTest, AddPacketNumberDuplicate)
{
    SmartAckTracker tracker;

    bool firstAdd = tracker.AddPacketNumber(100);
    bool secondAdd = tracker.AddPacketNumber(100);

    ASSERT_EQ(firstAdd, false);
    ASSERT_EQ(secondAdd, true);
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

    ASSERT_EQ(tracker.AddPacketNumber(100), false);
    ASSERT_EQ(tracker.AddPacketNumber(101), false);
    ASSERT_EQ(tracker.AddPacketNumber(102), false);

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

    ASSERT_EQ(tracker.AddPacketNumber(100), false);
    ASSERT_EQ(tracker.AddPacketNumber(102), false);
    ASSERT_EQ(tracker.GetReceivedRangeSize(), 2u); // Two separate ranges

    ASSERT_EQ(tracker.AddPacketNumber(101), false);
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

    ASSERT_EQ(tracker.AddPacketNumber(100), false);
    ASSERT_EQ(tracker.AddPacketNumber(105), false);
    ASSERT_EQ(tracker.AddPacketNumber(110), false);

    ASSERT_EQ(tracker.GetReceivedRangeSize(), 3u);
}

//
// Test: QuicAckTrackerHasPacketsToAck returns false for empty tracker.
//
// Scenario: Check HasPacketsToAck on a freshly initialized tracker.
//
// Assertions: Returns false since no packets have been added to ACK range.
//
TEST(AckTrackerTest, HasPacketsToAckEmpty)
{
    SmartAckTracker tracker;

    ASSERT_EQ(tracker.HasPacketsToAck(), false);
}

//
// Test: QuicAckTrackerHasPacketsToAck returns true when packets are pending.
//
// Scenario: Add packets to the ACK range, then check HasPacketsToAck.
//
// Assertions: Returns true when PacketNumbersToAck is non-empty and
// AlreadyWrittenAckFrame is false.
//
TEST(AckTrackerTest, HasPacketsToAckWithPackets)
{
    SmartAckTracker tracker;

    tracker.AddToAckRange(100);

    ASSERT_EQ(tracker.HasPacketsToAck(), true);
}

//
// Test: QuicAckTrackerHasPacketsToAck returns false after ACK frame written.
//
// Scenario: Add packets, set AlreadyWrittenAckFrame = TRUE, check result.
//
// Assertions: Returns false even though packets exist, because ACK was written.
//
TEST(AckTrackerTest, HasPacketsToAckAfterFrameWritten)
{
    SmartAckTracker tracker;

    tracker.AddToAckRange(100);
    ASSERT_EQ(tracker.HasPacketsToAck(), true);

    tracker.tracker.AlreadyWrittenAckFrame = TRUE;
    ASSERT_EQ(tracker.HasPacketsToAck(), false);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold returns false when threshold is 0.
//
// Scenario: Set threshold to 0 (disables reordering detection).
//
// Assertions: Always returns false regardless of packet ranges.
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
// Test: QuicAckTrackerDidHitReorderingThreshold returns false for single range.
//
// Scenario: Only one contiguous range exists (no gaps).
//
// Assertions: Returns false because at least 2 ranges needed for reordering.
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

    ASSERT_EQ(tracker.AddPacketNumber(largeNum), false);
    ASSERT_EQ(tracker.AddPacketNumber(largeNum + 1), false);
    ASSERT_EQ(tracker.AddPacketNumber(largeNum), true); // Duplicate

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
    ASSERT_EQ(tracker.AddPacketNumber(200), false);
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
    ASSERT_EQ(tracker.AddPacketNumber(100), false);
    ASSERT_EQ(tracker.AddPacketNumber(100), true);

    // Reset
    tracker.Reset();

    // Same packet number is now new again
    ASSERT_EQ(tracker.AddPacketNumber(100), false);
    ASSERT_EQ(tracker.AddPacketNumber(100), true);
}

//
// Test: QuicAckTrackerDidHitReorderingThreshold with boundary values.
//
// Scenario: Test exact threshold boundary conditions.
//
// Assertions: Returns false when gap == threshold - 1, true when gap == threshold.
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

    // With threshold=4, gap=3 < 4, should be false
    ASSERT_EQ(QuicAckTrackerDidHitReorderingThreshold(&tracker.tracker, 4), FALSE);

    // Add packet 5 to increase gap to 4
    tracker.AddToAckRange(5);
    // Gap = 5 - 1 = 4 >= 4, should be true
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
    ASSERT_EQ(tracker.AddPacketNumber(10), false);
    ASSERT_EQ(tracker.AddPacketNumber(20), false);
    ASSERT_EQ(tracker.AddPacketNumber(30), false);

    // Check duplicates
    ASSERT_EQ(tracker.AddPacketNumber(20), true);
    ASSERT_EQ(tracker.AddPacketNumber(10), true);
    ASSERT_EQ(tracker.AddPacketNumber(30), true);

    // Add new packet
    ASSERT_EQ(tracker.AddPacketNumber(15), false);

    // Check new and old duplicates
    ASSERT_EQ(tracker.AddPacketNumber(15), true);
    ASSERT_EQ(tracker.AddPacketNumber(25), false);
    ASSERT_EQ(tracker.AddPacketNumber(25), true);
}

//
// ============================================================================
// Tests requiring QUIC_CONNECTION context
//
// These tests use a MockPacketSpaceWithConnection structure that embeds the
// QUIC_PACKET_SPACE (containing the AckTracker) alongside a QUIC_CONNECTION.
// This satisfies the pointer arithmetic in QuicAckTrackerGetPacketSpace().
// ============================================================================
//

//
// Mock structure that properly embeds QUIC_PACKET_SPACE with its Connection pointer.
// The AckTracker is accessed via CXPLAT_CONTAINING_RECORD to get PacketSpace,
// then PacketSpace->Connection is used.
//
struct MockPacketSpaceWithConnection {
    QUIC_PACKET_SPACE PacketSpace;
    QUIC_CONNECTION Connection;

    MockPacketSpaceWithConnection() {
        CxPlatZeroMemory(&PacketSpace, sizeof(PacketSpace));
        CxPlatZeroMemory(&Connection, sizeof(Connection));

        // Link packet space to connection
        PacketSpace.Connection = &Connection;

        // Initialize the AckTracker
        QuicAckTrackerInitialize(&PacketSpace.AckTracker);

        // Initialize connection settings needed by AckTracker functions
        Connection.Settings.MaxAckDelayMs = 25;  // Default ACK delay
        Connection.PacketTolerance = 2;           // Default packet tolerance
        Connection.ReorderingThreshold = 0;       // Disabled by default
        Connection.AckDelayExponent = 3;          // Default exponent

        // Initialize Send state to avoid validation issues
        // Set Uninitialized to skip QuicSendValidate in DEBUG builds
        Connection.Send.Uninitialized = TRUE;

        // Initialize list head for send streams (needed by some functions)
        CxPlatListInitializeHead(&Connection.Send.SendStreams);
    }

    ~MockPacketSpaceWithConnection() {
        QuicAckTrackerUninitialize(&PacketSpace.AckTracker);
    }

    QUIC_ACK_TRACKER* Tracker() {
        return &PacketSpace.AckTracker;
    }
};

//
// Test: QuicAckTrackerAckPacket with NON_ACK_ELICITING packet type.
//
// Scenario: Add a non-ACK-eliciting packet and verify it doesn't increment
// the AckElicitingPacketsToAcknowledge counter.
//
// Assertions: Packet is added to the ack range, counter remains 0.
//
TEST(AckTrackerTest, AckPacketNonAckEliciting)
{
    MockPacketSpaceWithConnection mock;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,                            // PacketNumber
        1000,                           // RecvTimeUs
        CXPLAT_ECN_NON_ECT,            // ECN
        QUIC_ACK_TYPE_NON_ACK_ELICITING // AckType
    );

    ASSERT_EQ(mock.Tracker()->AckElicitingPacketsToAcknowledge, 0u);
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 1u);
    ASSERT_EQ(mock.Tracker()->LargestPacketNumberRecvTime, 1000u);
    ASSERT_EQ(mock.Tracker()->AlreadyWrittenAckFrame, FALSE);
}

//
// Test: QuicAckTrackerAckPacket with ACK_ELICITING packet type.
//
// Scenario: Add an ACK-eliciting packet and verify the counter increments.
//
// Assertions: AckElicitingPacketsToAcknowledge counter increments.
//
TEST(AckTrackerTest, AckPacketAckEliciting)
{
    MockPacketSpaceWithConnection mock;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,                        // PacketNumber
        1000,                       // RecvTimeUs
        CXPLAT_ECN_NON_ECT,        // ECN
        QUIC_ACK_TYPE_ACK_ELICITING // AckType
    );

    ASSERT_EQ(mock.Tracker()->AckElicitingPacketsToAcknowledge, 1u);
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 1u);
}

//
// Test: QuicAckTrackerAckPacket with ECN ECT_0.
//
// Scenario: Add packet with ECN ECT_0 codepoint.
//
// Assertions: ECT_0 counter increments, NonZeroRecvECN flag set.
//
TEST(AckTrackerTest, AckPacketEcnEct0)
{
    MockPacketSpaceWithConnection mock;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        1000,
        CXPLAT_ECN_ECT_0,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->NonZeroRecvECN, TRUE);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_0_Count, 1u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_1_Count, 0u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.CE_Count, 0u);
}

//
// Test: QuicAckTrackerAckPacket with ECN ECT_1.
//
// Scenario: Add packet with ECN ECT_1 codepoint.
//
// Assertions: ECT_1 counter increments, NonZeroRecvECN flag set.
//
TEST(AckTrackerTest, AckPacketEcnEct1)
{
    MockPacketSpaceWithConnection mock;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        1000,
        CXPLAT_ECN_ECT_1,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->NonZeroRecvECN, TRUE);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_0_Count, 0u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_1_Count, 1u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.CE_Count, 0u);
}

//
// Test: QuicAckTrackerAckPacket with ECN CE (Congestion Experienced).
//
// Scenario: Add packet with ECN CE codepoint.
//
// Assertions: CE counter increments, NonZeroRecvECN flag set.
//
TEST(AckTrackerTest, AckPacketEcnCE)
{
    MockPacketSpaceWithConnection mock;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        1000,
        CXPLAT_ECN_CE,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->NonZeroRecvECN, TRUE);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_0_Count, 0u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_1_Count, 0u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.CE_Count, 1u);
}

//
// Test: QuicAckTrackerAckPacket detects reordered packets.
//
// Scenario: Add packets out of order (higher packet first, then lower).
//
// Assertions: Connection stats show reordered packet count incremented.
//
TEST(AckTrackerTest, AckPacketDetectsReordering)
{
    MockPacketSpaceWithConnection mock;

    // Add packet 200 first
    QuicAckTrackerAckPacket(
        mock.Tracker(),
        200,
        1000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Connection.Stats.Recv.ReorderedPackets, 0u);

    // Add packet 100 (older than 200) - this is reordering
    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        2000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Connection.Stats.Recv.ReorderedPackets, 1u);
}

//
// Test: QuicAckTrackerAckPacket updates receive time only for new largest packet.
//
// Scenario: Add packets in both orders and verify receive time is only
// updated when the new packet is the largest.
//
// Assertions: LargestPacketNumberRecvTime only updates for new largest.
//
TEST(AckTrackerTest, AckPacketRecvTimeOnlyForLargest)
{
    MockPacketSpaceWithConnection mock;

    // Add packet 100
    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        1000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->LargestPacketNumberRecvTime, 1000u);

    // Add packet 50 (not the largest) - should NOT update recv time
    QuicAckTrackerAckPacket(
        mock.Tracker(),
        50,
        2000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->LargestPacketNumberRecvTime, 1000u);

    // Add packet 200 (new largest) - should update recv time
    QuicAckTrackerAckPacket(
        mock.Tracker(),
        200,
        3000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->LargestPacketNumberRecvTime, 3000u);
}

//
// Test: QuicAckTrackerAckPacket clears AlreadyWrittenAckFrame flag.
//
// Scenario: Set AlreadyWrittenAckFrame = TRUE, add packet, verify it's cleared.
//
// Assertions: AlreadyWrittenAckFrame is FALSE after adding packet.
//
TEST(AckTrackerTest, AckPacketClearsAlreadyWrittenFlag)
{
    MockPacketSpaceWithConnection mock;
    mock.Tracker()->AlreadyWrittenAckFrame = TRUE;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        1000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_NON_ACK_ELICITING
    );

    ASSERT_EQ(mock.Tracker()->AlreadyWrittenAckFrame, FALSE);
}

//
// Test: QuicAckTrackerOnAckFrameAcked removes acknowledged packets.
//
// Scenario: Add packets to tracker, then call OnAckFrameAcked to remove them.
//
// Assertions: Packets up to LargestAckedPacketNumber are removed.
//
TEST(AckTrackerTest, OnAckFrameAckedRemovesPackets)
{
    MockPacketSpaceWithConnection mock;

    // Add packets 100, 101, 102, 103, 104
    for (uint64_t i = 100; i <= 104; i++) {
        QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, i);
    }
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 1u);

    // Ack up to packet 102
    QuicAckTrackerOnAckFrameAcked(mock.Tracker(), 102);

    // Only packets 103, 104 should remain
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 1u);
    uint64_t minValue;
    ASSERT_TRUE(QuicRangeGetMinSafe(&mock.Tracker()->PacketNumbersToAck, &minValue));
    ASSERT_EQ(minValue, 103u);
}

//
// Test: QuicAckTrackerOnAckFrameAcked removes all packets.
//
// Scenario: Acknowledge all packets, verify range becomes empty.
//
// Assertions: PacketNumbersToAck becomes empty.
//
TEST(AckTrackerTest, OnAckFrameAckedRemovesAllPackets)
{
    MockPacketSpaceWithConnection mock;

    // Add packets 100, 101, 102
    QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, 100);
    QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, 101);
    QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, 102);

    // Set AckElicitingPacketsToAcknowledge to verify it gets cleared
    mock.Tracker()->AckElicitingPacketsToAcknowledge = 3;

    // Ack all packets
    QuicAckTrackerOnAckFrameAcked(mock.Tracker(), 102);

    // Range should be empty
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 0u);
    // Counter should be cleared when no packets left
    ASSERT_EQ(mock.Tracker()->AckElicitingPacketsToAcknowledge, 0u);
}

//
// Test: QuicAckTrackerOnAckFrameAcked with gaps - packets remain after ack.
//
// Scenario: Add non-contiguous packets, ack middle, verify remaining.
//
// Assertions: Only packets > LargestAckedPacketNumber remain.
//
TEST(AckTrackerTest, OnAckFrameAckedWithGaps)
{
    MockPacketSpaceWithConnection mock;

    // Add packets 100, 105, 110 (with gaps)
    QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, 100);
    QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, 105);
    QuicRangeAddValue(&mock.Tracker()->PacketNumbersToAck, 110);
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 3u);

    // Ack up to 105
    QuicAckTrackerOnAckFrameAcked(mock.Tracker(), 105);

    // Only 110 should remain
    ASSERT_EQ(QuicRangeSize(&mock.Tracker()->PacketNumbersToAck), 1u);
    uint64_t minValue;
    ASSERT_TRUE(QuicRangeGetMinSafe(&mock.Tracker()->PacketNumbersToAck, &minValue));
    ASSERT_EQ(minValue, 110u);
}

//
// Test: QuicAckTrackerAckPacket handles multiple ECN types in sequence.
//
// Scenario: Add packets with different ECN types and verify all counters.
//
// Assertions: All ECN counters increment correctly.
//
TEST(AckTrackerTest, AckPacketMultipleEcnTypes)
{
    MockPacketSpaceWithConnection mock;

    // Add packets with different ECN types
    QuicAckTrackerAckPacket(mock.Tracker(), 100, 1000, CXPLAT_ECN_ECT_0, QUIC_ACK_TYPE_NON_ACK_ELICITING);
    QuicAckTrackerAckPacket(mock.Tracker(), 101, 1001, CXPLAT_ECN_ECT_1, QUIC_ACK_TYPE_NON_ACK_ELICITING);
    QuicAckTrackerAckPacket(mock.Tracker(), 102, 1002, CXPLAT_ECN_CE, QUIC_ACK_TYPE_NON_ACK_ELICITING);
    QuicAckTrackerAckPacket(mock.Tracker(), 103, 1003, CXPLAT_ECN_NON_ECT, QUIC_ACK_TYPE_NON_ACK_ELICITING);
    QuicAckTrackerAckPacket(mock.Tracker(), 104, 1004, CXPLAT_ECN_ECT_0, QUIC_ACK_TYPE_NON_ACK_ELICITING);

    ASSERT_EQ(mock.Tracker()->NonZeroRecvECN, TRUE);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_0_Count, 2u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.ECT_1_Count, 1u);
    ASSERT_EQ(mock.Tracker()->ReceivedECN.CE_Count, 1u);
}

//
// Test: QuicAckTrackerAckPacket skips send flag work if ACK already queued.
//
// Scenario: Pre-set QUIC_CONN_SEND_FLAG_ACK, add packet, verify counter increments
// but we take the early exit path (line 243).
//
// Assertions: Counter increments, flag remains set, no additional work done.
//
TEST(AckTrackerTest, AckPacketAckAlreadyQueued)
{
    MockPacketSpaceWithConnection mock;

    // Pre-set ACK flag
    mock.Connection.Send.SendFlags = QUIC_CONN_SEND_FLAG_ACK;

    QuicAckTrackerAckPacket(
        mock.Tracker(),
        100,
        1000,
        CXPLAT_ECN_NON_ECT,
        QUIC_ACK_TYPE_ACK_ELICITING
    );

    // Counter should still increment
    ASSERT_EQ(mock.Tracker()->AckElicitingPacketsToAcknowledge, 1u);
    // Flag should still be set
    ASSERT_TRUE(mock.Connection.Send.SendFlags & QUIC_CONN_SEND_FLAG_ACK);
}

//
// Test: QuicAckTrackerAckPacket multiple ACK eliciting packets.
//
// Scenario: Add multiple ACK-eliciting packets and verify counter accumulates.
//
// Assertions: AckElicitingPacketsToAcknowledge increments for each packet.
//
TEST(AckTrackerTest, AckPacketMultipleAckEliciting)
{
    MockPacketSpaceWithConnection mock;

    // Pre-set ACK flag to avoid triggering complex send logic
    mock.Connection.Send.SendFlags = QUIC_CONN_SEND_FLAG_ACK;

    QuicAckTrackerAckPacket(mock.Tracker(), 100, 1000, CXPLAT_ECN_NON_ECT, QUIC_ACK_TYPE_ACK_ELICITING);
    QuicAckTrackerAckPacket(mock.Tracker(), 101, 1001, CXPLAT_ECN_NON_ECT, QUIC_ACK_TYPE_ACK_ELICITING);
    QuicAckTrackerAckPacket(mock.Tracker(), 102, 1002, CXPLAT_ECN_NON_ECT, QUIC_ACK_TYPE_ACK_ELICITING);

    ASSERT_EQ(mock.Tracker()->AckElicitingPacketsToAcknowledge, 3u);
}
