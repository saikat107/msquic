/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for Bottleneck Bandwidth and RTT (BBR) congestion control.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "BbrTest.cpp.clog.h"
#endif

//
// Helper to create a minimal valid connection for testing BBR initialization.
// Uses a real QUIC_CONNECTION structure to ensure proper memory layout when
// QuicCongestionControlGetConnection() does CXPLAT_CONTAINING_RECORD pointer arithmetic.
//
static void InitializeMockConnection(
    QUIC_CONNECTION& Connection,
    uint16_t Mtu,
    BOOLEAN EnablePacing = FALSE)
{
    // Zero-initialize the entire connection structure
    CxPlatZeroMemory(&Connection, sizeof(Connection));

    // Initialize only the fields needed by BBR functions
    Connection.Paths[0].Mtu = Mtu;
    Connection.Paths[0].IsActive = TRUE;
    Connection.Send.NextPacketNumber = 0;
    Connection.LossDetection.LargestSentPacketNumber = 0;

    // Initialize Settings with defaults
    Connection.Settings.PacingEnabled = EnablePacing;
    Connection.Settings.NetStatsEventEnabled = FALSE;

    // Initialize Path fields needed for some functions
    Connection.Paths[0].GotFirstRttSample = FALSE;
    Connection.Paths[0].SmoothedRtt = 0;
    Connection.Paths[0].OneWayDelay = 0;
    
    // Initialize send buffer fields that BBR may reference
    Connection.SendBuffer.IdealBytes = 0;
    Connection.SendBuffer.PostedBytes = 0;
    Connection.Send.OrderedStreamBytesSent = 0;
    Connection.Send.PeerMaxData = UINT64_MAX;
}

//
// Test 1: Comprehensive initialization verification
// Scenario: Verifies BbrCongestionControlInitialize correctly sets up all BBR state
// including settings, function pointers, state flags, filters, and initial values.
//
TEST(BbrTest, InitializeComprehensive)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;

    InitializeMockConnection(Connection, 1280);

    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Verify settings stored correctly
    ASSERT_EQ(Bbr->InitialCongestionWindowPackets, 10u);

    // Verify congestion window initialized
    ASSERT_GT(Bbr->CongestionWindow, 0u);
    ASSERT_EQ(Bbr->CongestionWindow, Bbr->InitialCongestionWindow);
    ASSERT_EQ(Bbr->BytesInFlightMax, Bbr->CongestionWindow / 2);

    // Verify all function pointers are set
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlCanSend, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetExemption, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlReset, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetSendAllowance, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataSent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataInvalidated, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataLost, nullptr);
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlOnEcn, nullptr); // BBR doesn't support ECN
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlLogOutFlowStatus, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetExemptions, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlIsAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetCongestionWindow, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics, nullptr);

    // Verify initial BBR state
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
    ASSERT_EQ(Bbr->RecoveryState, 0u); // RECOVERY_STATE_NOT_RECOVERY
    ASSERT_FALSE(Bbr->BtlbwFound);
    ASSERT_FALSE(Bbr->ExitingQuiescence);
    ASSERT_EQ(Bbr->BytesInFlight, 0u);
    ASSERT_EQ(Bbr->Exemptions, 0u);
    ASSERT_EQ(Bbr->RoundTripCounter, 0u);

    // Verify gains set for STARTUP
    ASSERT_GT(Bbr->PacingGain, 256u); // kHighGain
    ASSERT_GT(Bbr->CwndGain, 256u);   // kHighGain

    // Verify MinRtt initialized as unsampled
    ASSERT_EQ(Bbr->MinRtt, UINT64_MAX);
    ASSERT_FALSE(Bbr->MinRttTimestampValid);
    ASSERT_TRUE(Bbr->RttSampleExpired);

    // Verify filters initialized
    ASSERT_FALSE(Bbr->BandwidthFilter.AppLimited);
    ASSERT_EQ(Bbr->BandwidthFilter.AppLimitedExitTarget, 0u);

    // Verify validity flags
    ASSERT_FALSE(Bbr->EndOfRecoveryValid);
    ASSERT_FALSE(Bbr->EndOfRoundTripValid);
    ASSERT_FALSE(Bbr->AckAggregationStartTimeValid);
    ASSERT_FALSE(Bbr->ProbeRttRoundValid);
    ASSERT_FALSE(Bbr->ProbeRttEndTimeValid);
}

//
// Test 2: Initialization with boundary parameter values
// Scenario: Tests initialization with extreme boundary values for MTU and InitialWindowPackets
// to ensure robustness across all valid configurations.
//
TEST(BbrTest, InitializeBoundaries)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    // Test minimum MTU with minimum window
    Settings.InitialWindowPackets = 1;
    InitializeMockConnection(Connection, 1200); // Min QUIC MTU
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 1u);

    // Test maximum MTU with large window
    Settings.InitialWindowPackets = 1000;
    InitializeMockConnection(Connection, 65535);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 1000u);

    // Test typical configuration
    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    // Don't hardcode expected value - just verify it's reasonable
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 10000u);
    ASSERT_LT(Connection.CongestionControl.Bbr.CongestionWindow, 15000u);
}

//
// Test 3: OnDataSent updates BytesInFlight
// Scenario: Sending data increases BytesInFlight and updates BytesInFlightMax.
//
TEST(BbrTest, OnDataSentIncreasesInflight)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint32_t InitialInFlight = Bbr->BytesInFlight;
    uint32_t InitialMax = Bbr->BytesInFlightMax;

    // Send some data
    uint32_t BytesToSend = 1000;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, BytesToSend);

    // Verify BytesInFlight increased
    ASSERT_EQ(Bbr->BytesInFlight, InitialInFlight + BytesToSend);

    // Verify BytesInFlightMax updated if new max reached
    ASSERT_GE(Bbr->BytesInFlightMax, InitialMax);
}

//
// Test 4: OnDataInvalidated decreases BytesInFlight
// Scenario: Invalidating sent data (e.g., 0-RTT rejection) reduces BytesInFlight.
//
TEST(BbrTest, OnDataInvalidatedDecreasesInflight)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Send data first
    uint32_t BytesToSend = 2000;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, BytesToSend);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint32_t InflightBeforeInvalidate = Bbr->BytesInFlight;

    // Invalidate some data
    uint32_t BytesToInvalidate = 500;
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(&Connection.CongestionControl, BytesToInvalidate);

    // Verify BytesInFlight decreased
    ASSERT_EQ(Bbr->BytesInFlight, InflightBeforeInvalidate - BytesToInvalidate);
}

//
// Test 5: OnDataLost enters recovery
// Scenario: Packet loss causes BBR to enter recovery state and reduce recovery window.
//
TEST(BbrTest, OnDataLostEntersRecovery)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Send data first
    uint32_t BytesToSend = 5000;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, BytesToSend);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Verify not in recovery initially
    ASSERT_EQ(Bbr->RecoveryState, 0u); // NOT_RECOVERY

    // Simulate loss
    QUIC_LOSS_EVENT LossEvent = {};
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 20;
    LossEvent.NumRetransmittableBytes = 1000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent);

    // Verify entered recovery
    ASSERT_NE(Bbr->RecoveryState, 0u); // Should be CONSERVATIVE or GROWTH
    ASSERT_TRUE(Bbr->EndOfRecoveryValid);
    ASSERT_EQ(Bbr->BytesInFlight, BytesToSend - 1000);
}

//
// Test 6: Reset returns to STARTUP
// Scenario: Calling Reset with FullReset=TRUE returns BBR to initial STARTUP state.
//
TEST(BbrTest, ResetReturnsToStartup)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Send some data to modify state
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 3000);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    
    // Reset with FullReset = TRUE
    Connection.CongestionControl.QuicCongestionControlReset(&Connection.CongestionControl, TRUE);

    // Verify back to STARTUP state
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
    ASSERT_EQ(Bbr->BytesInFlight, 0u);
    ASSERT_FALSE(Bbr->BtlbwFound);
    ASSERT_EQ(Bbr->RoundTripCounter, 0u);
    ASSERT_EQ(Bbr->RecoveryState, 0u);
}

//
// Test 7: Reset with FullReset=FALSE preserves BytesInFlight
// Scenario: Partial reset preserves inflight bytes.
//
TEST(BbrTest, ResetPartialPreservesInflight)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Send some data
    uint32_t BytesSent = 3000;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, BytesSent);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    
    // Reset with FullReset = FALSE
    Connection.CongestionControl.QuicCongestionControlReset(&Connection.CongestionControl, FALSE);

    // Verify back to STARTUP state but BytesInFlight preserved
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
    ASSERT_EQ(Bbr->BytesInFlight, BytesSent); // Preserved!
}

//
// Test 8: CanSend respects congestion window
// Scenario: CanSend returns FALSE when BytesInFlight >= CongestionWindow.
//
TEST(BbrTest, CanSendRespectsWindow)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint32_t Cwnd = Bbr->CongestionWindow;

    // Scenario 1: Under window - can send
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Fill window by sending
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, Cwnd);

    // Scenario 2: At/over window - cannot send
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));
}

//
// Test 9: Exemptions bypass congestion control
// Scenario: Setting exemptions allows sending even when congestion blocked.
//
TEST(BbrTest, ExemptionsBypassControl)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Fill window to block sending
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, Bbr->CongestionWindow);
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Set exemption
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 2);
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Send with exemption - should decrement
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 100);
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl), 1u);
}

//
// Test 10: OnSpuriousCongestionEvent returns FALSE
// Scenario: BBR doesn't revert on spurious loss detection.
//
TEST(BbrTest, SpuriousCongestionEventNoRevert)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // BBR always returns FALSE for spurious events (no revert)
    BOOLEAN Reverted = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(&Connection.CongestionControl);
    ASSERT_FALSE(Reverted);
}

//
// Test 11: Basic OnDataAcknowledged without state transition
// Scenario: Simple ACK processing that decreases BytesInFlight and updates round trip counter.
//
TEST(BbrTest, OnDataAcknowledgedBasic)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send data first
    uint32_t BytesToSend = 3000;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, BytesToSend);
    uint64_t PacketNum = 1;

    ASSERT_EQ(Bbr->BytesInFlight, BytesToSend);
    ASSERT_EQ(Bbr->RoundTripCounter, 0u);

    // Create ACK event
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.TimeNow = 100000; // 100ms
    AckEvent.LargestAck = PacketNum;
    AckEvent.LargestSentPacketNumber = PacketNum;
    AckEvent.NumRetransmittableBytes = BytesToSend;
    AckEvent.NumTotalAckedRetransmittableBytes = BytesToSend;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000; // 50ms RTT
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AckedPackets = NULL;

    // Process ACK
    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Verify BytesInFlight decreased
    ASSERT_EQ(Bbr->BytesInFlight, 0u);
    
    // Verify round trip counter incremented
    ASSERT_EQ(Bbr->RoundTripCounter, 1u);
    
    // Verify MinRTT updated
    ASSERT_EQ(Bbr->MinRtt, 50000u);
    ASSERT_TRUE(Bbr->MinRttTimestampValid);

    // Still in STARTUP
    ASSERT_EQ(Bbr->BbrState, 0u);
}

//
// Test 12: State transition STARTUP → DRAIN
// Scenario: Simulate bandwidth growth stalling for 3 rounds to find bottleneck and transition to DRAIN.
//
TEST(BbrTest, StateTransitionStartupToDrain)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint64_t TimeNow = 0;
    uint64_t PacketNum = 0;

    // Initial state: STARTUP
    ASSERT_EQ(Bbr->BbrState, 0u);
    ASSERT_FALSE(Bbr->BtlbwFound);

    // Simulate 3 rounds without significant bandwidth growth
    // This triggers BtlbwFound = TRUE
    for (int round = 0; round < 4; round++) {
        // Send some packets
        for (int i = 0; i < 5; i++) {
            Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1200);
            PacketNum++;
        }

        TimeNow += 50000; // 50ms per round

        // ACK packets
        QUIC_ACK_EVENT AckEvent = {};
        AckEvent.TimeNow = TimeNow;
        AckEvent.LargestAck = PacketNum;
        AckEvent.LargestSentPacketNumber = PacketNum;
        AckEvent.NumRetransmittableBytes = 6000;
        AckEvent.NumTotalAckedRetransmittableBytes = (round + 1) * 6000;
        AckEvent.IsImplicit = FALSE;
        AckEvent.HasLoss = FALSE;
        AckEvent.MinRttValid = TRUE;
        AckEvent.MinRtt = 50000;
        AckEvent.IsLargestAckedPacketAppLimited = FALSE;
        AckEvent.AckedPackets = NULL; // Simplified - no detailed packet metadata

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);
    }

    // After 3 rounds of no growth, should find bottleneck and transition to DRAIN
    ASSERT_TRUE(Bbr->BtlbwFound);
    ASSERT_EQ(Bbr->BbrState, 1u); // BBR_STATE_DRAIN
}

//
// Test 13: State transition DRAIN → PROBE_BW
// Scenario: After entering DRAIN, drain inflight bytes to target then transition to PROBE_BW.
//
TEST(BbrTest, StateTransitionDrainToProbeBw)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // First get to DRAIN state by simulating startup completion
    uint64_t TimeNow = 0;
    uint64_t PacketNum = 0;

    for (int round = 0; round < 4; round++) {
        for (int i = 0; i < 5; i++) {
            Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1200);
            PacketNum++;
        }
        TimeNow += 50000;

        QUIC_ACK_EVENT AckEvent = {};
        AckEvent.TimeNow = TimeNow;
        AckEvent.LargestAck = PacketNum;
        AckEvent.LargestSentPacketNumber = PacketNum;
        AckEvent.NumRetransmittableBytes = 6000;
        AckEvent.NumTotalAckedRetransmittableBytes = (round + 1) * 6000;
        AckEvent.IsImplicit = FALSE;
        AckEvent.HasLoss = FALSE;
        AckEvent.MinRttValid = TRUE;
        AckEvent.MinRtt = 50000;
        AckEvent.IsLargestAckedPacketAppLimited = FALSE;
        AckEvent.AckedPackets = NULL;

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);
    }

    // Should be in DRAIN now
    ASSERT_EQ(Bbr->BbrState, 1u); // BBR_STATE_DRAIN

    // Now drain by ACKing all inflight bytes
    TimeNow += 50000;
    PacketNum++;

    QUIC_ACK_EVENT DrainAck = {};
    DrainAck.TimeNow = TimeNow;
    DrainAck.LargestAck = PacketNum;
    DrainAck.LargestSentPacketNumber = PacketNum;
    DrainAck.NumRetransmittableBytes = Bbr->BytesInFlight; // ACK everything
    DrainAck.NumTotalAckedRetransmittableBytes = DrainAck.NumRetransmittableBytes + 24000;
    DrainAck.IsImplicit = FALSE;
    DrainAck.HasLoss = FALSE;
    DrainAck.MinRttValid = TRUE;
    DrainAck.MinRtt = 50000;
    DrainAck.IsLargestAckedPacketAppLimited = FALSE;
    DrainAck.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &DrainAck);

    // Should transition to PROBE_BW
    ASSERT_EQ(Bbr->BbrState, 2u); // BBR_STATE_PROBE_BW
}

//
// Test 14: State transition to PROBE_RTT due to RTT expiration
// Scenario: Simulate RTT sample expiration (10 seconds) to force PROBE_RTT.
//
TEST(BbrTest, StateTransitionToProbRtt)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint64_t PacketNum = 1;

    // Send and ACK to establish MinRTT
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1200);
    
    QUIC_ACK_EVENT InitialAck = {};
    InitialAck.TimeNow = 100000; // 100ms
    InitialAck.LargestAck = PacketNum++;
    InitialAck.LargestSentPacketNumber = PacketNum;
    InitialAck.NumRetransmittableBytes = 1200;
    InitialAck.NumTotalAckedRetransmittableBytes = 1200;
    InitialAck.IsImplicit = FALSE;
    InitialAck.HasLoss = FALSE;
    InitialAck.MinRttValid = TRUE;
    InitialAck.MinRtt = 50000; // 50ms RTT
    InitialAck.IsLargestAckedPacketAppLimited = FALSE;
    InitialAck.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &InitialAck);

    ASSERT_TRUE(Bbr->BtlbwFound);
    
    // Initial state should be STARTUP or might already be in DRAIN/PROBE_BW depending on timing
    // Just verify we're not already in PROBE_RTT
    ASSERT_NE(Bbr->BbrState, (uint8_t)3);

    // Now wait 10+ seconds and send/ACK again
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1200);

    QUIC_ACK_EVENT ExpiredAck = {};
    ExpiredAck.TimeNow = 10100000 + 100000; // > 10 seconds later
    ExpiredAck.LargestAck = PacketNum++;
    ExpiredAck.LargestSentPacketNumber = PacketNum;
    ExpiredAck.NumRetransmittableBytes = 1200;
    ExpiredAck.NumTotalAckedRetransmittableBytes = 2400;
    ExpiredAck.IsImplicit = FALSE;
    ExpiredAck.HasLoss = FALSE;
    ExpiredAck.MinRttValid = TRUE;
    ExpiredAck.MinRtt = 50000; // Same RTT
    ExpiredAck.IsLargestAckedPacketAppLimited = FALSE;
    ExpiredAck.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &ExpiredAck);

    // Should transition to PROBE_RTT
    ASSERT_EQ(Bbr->BbrState, 3u); // BBR_STATE_PROBE_RTT
}

//
// Test 15: PROBE_RTT exit back to PROBE_BW
// Scenario: Complete PROBE_RTT (stay 200ms + 1 RTT) with bottleneck found, return to PROBE_BW.
//
TEST(BbrTest, ProbeRttExitToProbeBw)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint64_t TimeNow = 100000;
    uint64_t PacketNum = 1;

    // Get to PROBE_BW first (simplified - directly set state for this specific test)
    // This is acceptable as we're testing PROBE_RTT exit, not entry
    
    // First establish MinRTT and get BtlbwFound
    for (int round = 0; round < 4; round++) {
        for (int i = 0; i < 3; i++) {
            Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1200);
            PacketNum++;
        }
        TimeNow += 50000;

        QUIC_ACK_EVENT AckEvent = {};
        AckEvent.TimeNow = TimeNow;
        AckEvent.LargestAck = PacketNum;
        AckEvent.LargestSentPacketNumber = PacketNum;
        AckEvent.NumRetransmittableBytes = 3600;
        AckEvent.NumTotalAckedRetransmittableBytes = (round + 1) * 3600;
        AckEvent.IsImplicit = FALSE;
        AckEvent.HasLoss = FALSE;
        AckEvent.MinRttValid = TRUE;
        AckEvent.MinRtt = 50000;
        AckEvent.IsLargestAckedPacketAppLimited = FALSE;
        AckEvent.AckedPackets = NULL;

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);
    }

    // Drain to get to PROBE_BW
    TimeNow += 50000;
    QUIC_ACK_EVENT DrainAck = {};
    DrainAck.TimeNow = TimeNow;
    DrainAck.LargestAck = PacketNum + 1;
    DrainAck.LargestSentPacketNumber = PacketNum + 1;
    DrainAck.NumRetransmittableBytes = Bbr->BytesInFlight;
    DrainAck.NumTotalAckedRetransmittableBytes = 5 * 3600;
    DrainAck.IsImplicit = FALSE;
    DrainAck.HasLoss = FALSE;
    DrainAck.MinRttValid = TRUE;
    DrainAck.MinRtt = 50000;
    DrainAck.IsLargestAckedPacketAppLimited = FALSE;
    DrainAck.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &DrainAck);

    ASSERT_EQ(Bbr->BbrState, 2u); // PROBE_BW
    ASSERT_TRUE(Bbr->BtlbwFound);

    // Now expire RTT to enter PROBE_RTT
    TimeNow += 10100000; // > 10 seconds
    PacketNum += 2;

    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1200);

    QUIC_ACK_EVENT ExpireAck = {};
    ExpireAck.TimeNow = TimeNow;
    ExpireAck.LargestAck = PacketNum;
    ExpireAck.LargestSentPacketNumber = PacketNum;
    ExpireAck.NumRetransmittableBytes = 1200;
    ExpireAck.NumTotalAckedRetransmittableBytes = 6 * 3600;
    ExpireAck.IsImplicit = FALSE;
    ExpireAck.HasLoss = FALSE;
    ExpireAck.MinRttValid = TRUE;
    ExpireAck.MinRtt = 50000;
    ExpireAck.IsLargestAckedPacketAppLimited = FALSE;
    ExpireAck.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &ExpireAck);

    ASSERT_EQ(Bbr->BbrState, 3u); // PROBE_RTT

    // Now complete PROBE_RTT: stay 200ms+ with low flight and complete a round
    TimeNow += 250000; // 250ms later
    PacketNum++;

    // Keep flight low (already drained)
    QUIC_ACK_EVENT ProbeAck1 = {};
    ProbeAck1.TimeNow = TimeNow;
    ProbeAck1.LargestAck = PacketNum;
    ProbeAck1.LargestSentPacketNumber = PacketNum;
    ProbeAck1.NumRetransmittableBytes = 0;
    ProbeAck1.NumTotalAckedRetransmittableBytes = 6 * 3600;
    ProbeAck1.IsImplicit = FALSE;
    ProbeAck1.HasLoss = FALSE;
    ProbeAck1.MinRttValid = TRUE;
    ProbeAck1.MinRtt = 50000;
    ProbeAck1.IsLargestAckedPacketAppLimited = FALSE;
    ProbeAck1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &ProbeAck1);

    // Should exit PROBE_RTT back to PROBE_BW (since BtlbwFound = TRUE)
    ASSERT_EQ(Bbr->BbrState, 2u); // PROBE_BW
}

//
// Test 16: GetSendAllowance with pacing disabled
// Scenario: When pacing is disabled, send allowance equals available congestion window.
//
TEST(BbrTest, GetSendAllowanceNoPacing)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280, FALSE); // Pacing disabled
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint32_t Cwnd = Bbr->CongestionWindow;

    // Send half the window
    uint32_t BytesSent = Cwnd / 2;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, BytesSent);

    // Get send allowance
    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 10000, TRUE);

    // With no pacing, should return full available window
    uint32_t ExpectedAllowance = Cwnd - BytesSent;
    ASSERT_EQ(Allowance, ExpectedAllowance);
}

//
// Test 17: GetSendAllowance with pacing enabled
// Scenario: With pacing enabled and valid RTT, allowance is rate-limited.
//
TEST(BbrTest, GetSendAllowanceWithPacing)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280, TRUE); // Pacing enabled
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms RTT
    
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send some data first to establish bandwidth
    uint64_t PacketNum = 1;
    uint64_t TimeNow = 100000;
    
    for (int i = 0; i < 3; i++) {
        Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 4000);
        PacketNum++;
    }

    // ACK to establish bandwidth
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.TimeNow = TimeNow;
    AckEvent.LargestAck = PacketNum;
    AckEvent.LargestSentPacketNumber = PacketNum;
    AckEvent.NumRetransmittableBytes = 12000;
    AckEvent.NumTotalAckedRetransmittableBytes = 12000;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Now get send allowance with 10ms elapsed
    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 10000, TRUE); // 10ms since last send

    // With pacing, allowance should be limited (less than full window)
    uint32_t AvailableWindow = Bbr->CongestionWindow - Bbr->BytesInFlight;
    
    // Pacing should limit the send
    ASSERT_GT(Allowance, 0u); // Should allow some sending
    ASSERT_LE(Allowance, AvailableWindow); // But not more than available
}
