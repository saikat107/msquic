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
