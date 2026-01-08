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
    uint16_t Mtu)
{
    // Zero-initialize the entire connection structure
    CxPlatZeroMemory(&Connection, sizeof(Connection));

    // Initialize only the fields needed by BBR functions
    Connection.Paths[0].Mtu = Mtu;
    Connection.Paths[0].IsActive = TRUE;
    Connection.Send.NextPacketNumber = 0;
    Connection.LossDetection.LargestSentPacketNumber = 0;

    // Initialize Settings with defaults
    Connection.Settings.PacingEnabled = FALSE;  // Disable pacing by default for simpler tests
    Connection.Settings.NetStatsEventEnabled = FALSE;

    // Initialize Path fields needed for some functions
    Connection.Paths[0].GotFirstRttSample = FALSE;
    Connection.Paths[0].SmoothedRtt = 0;
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

    // Pre-set some fields to verify they get reset
    Connection.CongestionControl.Bbr.BytesInFlight = 12345;
    Connection.CongestionControl.Bbr.Exemptions = 5;

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
// Test 3: Re-initialization behavior
// Scenario: Tests that BBR can be re-initialized with different settings and correctly
// updates its state. Verifies that calling BbrCongestionControlInitialize() multiple times
// properly resets state and applies new settings.
//
TEST(BbrTest, Reinitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    // First initialization
    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    uint32_t FirstCwnd = Connection.CongestionControl.Bbr.CongestionWindow;
    ASSERT_GT(FirstCwnd, 0u);

    // Modify some state
    Connection.CongestionControl.Bbr.BytesInFlight = 5000;
    Connection.CongestionControl.Bbr.BtlbwFound = TRUE;
    Connection.CongestionControl.Bbr.RoundTripCounter = 10;

    // Re-initialize with different settings
    Settings.InitialWindowPackets = 20;
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    uint32_t SecondCwnd = Connection.CongestionControl.Bbr.CongestionWindow;

    // Verify new congestion window reflects new settings
    ASSERT_GT(SecondCwnd, FirstCwnd);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 20u);

    // Verify state reset
    ASSERT_EQ(Connection.CongestionControl.Bbr.BytesInFlight, 0u);
    ASSERT_FALSE(Connection.CongestionControl.Bbr.BtlbwFound);
    ASSERT_EQ(Connection.CongestionControl.Bbr.RoundTripCounter, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.BbrState, 0u); // STARTUP
}

//
// Test 4: CanSend scenarios
// Scenario: Tests BbrCongestionControlCanSend under different conditions to verify
// send permission logic based on congestion window and exemptions.
//
TEST(BbrTest, CanSendScenarios)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Scenario 1: No bytes in flight - can send
    Bbr->BytesInFlight = 0;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 2: Below window - can send
    Bbr->BytesInFlight = Bbr->CongestionWindow / 2;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 3: At window - cannot send
    Bbr->BytesInFlight = Bbr->CongestionWindow;
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 4: Exceeding window - still blocked
    Bbr->BytesInFlight = Bbr->CongestionWindow + 100;
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 5: With exemptions - can send even when blocked
    Bbr->Exemptions = 2;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));
}

//
// Test 5: SetExemption and GetExemptions
// Scenario: Tests exemption setting and retrieval to verify probe packet bypass mechanism.
//
TEST(BbrTest, ExemptionHandling)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Initially should be 0
    uint8_t Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 0u);

    // Set exemptions
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 5);
    Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 5u);

    // Set to zero
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 0);
    Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 0u);

    // Set to max
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 255);
    Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 255u);
}

//
// Test 6: GetCongestionWindow in different BBR states
// Scenario: Tests that congestion window calculation respects BBR state machine,
// returning minimum window in PROBE_RTT and considering recovery window.
//
TEST(BbrTest, GetCongestionWindowByState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 100;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint32_t InitialCwnd = Bbr->CongestionWindow;

    // State 1: STARTUP - returns full congestion window
    uint32_t Cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    ASSERT_EQ(Cwnd, InitialCwnd);

    // State 2: PROBE_RTT - returns minimum congestion window (4 MSS)
    Bbr->BbrState = 3; // BBR_STATE_PROBE_RTT
    Cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    // MinCwnd is 4 * DatagramPayloadSize - don't hardcode, just verify it's small
    ASSERT_LT(Cwnd, InitialCwnd);
    ASSERT_GT(Cwnd, 4000u); // At least 4 * ~1200
    ASSERT_LT(Cwnd, 6000u); // But less than typical MTU

    // State 3: Back to PROBE_BW - returns full window
    Bbr->BbrState = 2; // BBR_STATE_PROBE_BW
    Cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    ASSERT_EQ(Cwnd, InitialCwnd);

    // State 4: In recovery with smaller recovery window
    Bbr->RecoveryState = 1; // RECOVERY_STATE_CONSERVATIVE
    Bbr->RecoveryWindow = InitialCwnd / 2;
    Cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    ASSERT_EQ(Cwnd, Bbr->RecoveryWindow);

    // State 5: In recovery with larger recovery window
    Bbr->RecoveryWindow = InitialCwnd * 2;
    Cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    ASSERT_EQ(Cwnd, InitialCwnd); // Returns min of two
}

//
// Test 7: GetBytesInFlightMax
// Scenario: Tests that maximum bytes in flight tracking returns correct value.
//
TEST(BbrTest, GetBytesInFlightMax)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;
    uint32_t InitialMax = Bbr->BytesInFlightMax;

    uint32_t Max = Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax(&Connection.CongestionControl);
    ASSERT_EQ(Max, InitialMax);

    // Modify and verify
    Bbr->BytesInFlightMax = 99999;
    Max = Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax(&Connection.CongestionControl);
    ASSERT_EQ(Max, 99999u);
}

//
// Test 8: GetNetworkStatistics with no bandwidth samples
// Scenario: Tests network statistics retrieval when no samples collected yet (initial state).
// Uses the public API GetNetworkStatistics which internally uses bandwidth estimation.
//
TEST(BbrTest, GetNetworkStatisticsInitialState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_NETWORK_STATISTICS Stats = {};
    Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics(
        &Connection, &Connection.CongestionControl, &Stats);

    // Initially, no bandwidth samples, bandwidth should be 0
    ASSERT_EQ(Stats.Bandwidth, 0u);
    ASSERT_EQ(Stats.BytesInFlight, 0u);
    ASSERT_GT(Stats.CongestionWindow, 0u);
}

//
// Test 9: IsAppLimited and SetAppLimited
// Scenario: Tests app-limited state tracking which affects bandwidth estimation.
//
TEST(BbrTest, AppLimitedState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initially not app limited
    BOOLEAN AppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl);
    ASSERT_FALSE(AppLimited);

    // Scenario 1: Set app limited when under congestion window
    Bbr->BytesInFlight = Bbr->CongestionWindow / 2;
    Connection.LossDetection.LargestSentPacketNumber = 100;
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);
    AppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl);
    ASSERT_TRUE(AppLimited);
    ASSERT_EQ(Bbr->BandwidthFilter.AppLimitedExitTarget, 100u);

    // Scenario 2: Try to set app limited when at/above congestion window (should not set)
    Bbr->BandwidthFilter.AppLimited = FALSE;
    Bbr->BytesInFlight = Bbr->CongestionWindow + 1; // Above congestion window
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);
    AppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl);
    ASSERT_FALSE(AppLimited); // Still limited by congestion, not app
}

//
// Test 10: OnSpuriousCongestionEvent always returns FALSE
// Scenario: BBR doesn't revert on spurious loss detection, always returns FALSE.
//
TEST(BbrTest, SpuriousCongestionEvent)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // BBR always returns FALSE for spurious events
    BOOLEAN Reverted = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(&Connection.CongestionControl);
    ASSERT_FALSE(Reverted);
}
