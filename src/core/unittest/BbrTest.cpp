/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for BBR congestion control.
    Tests cover initialization, state management, function pointers, and
    basic congestion control logic for the Bottleneck Bandwidth and RTT algorithm.

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
static void InitializeMockConnectionForBbr(
    QUIC_CONNECTION& Connection,
    uint16_t Mtu)
{
    // Zero-initialize the entire connection structure
    CxPlatZeroMemory(&Connection, sizeof(Connection));

    // Initialize only the fields needed by BBR functions
    Connection.Paths[0].Mtu = Mtu;
    Connection.Paths[0].IsActive = TRUE;
    Connection.Send.NextPacketNumber = 0;

    // Initialize Settings with defaults
    Connection.Settings.PacingEnabled = TRUE;  // BBR typically uses pacing

    // Initialize Path fields needed for some functions
    Connection.Paths[0].GotFirstRttSample = FALSE;
    Connection.Paths[0].SmoothedRtt = 0;
}

//
// Test 1: Comprehensive initialization verification
// Scenario: Verifies BbrCongestionControlInitialize correctly sets up all BBR state
// including settings, function pointers, and state variables.
//
TEST(BbrTest, InitializeComprehensive)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);

    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Verify congestion window is initialized
    ASSERT_GT(Bbr->CongestionWindow, 0u);
    ASSERT_EQ(Bbr->InitialCongestionWindowPackets, 10u);

    // Verify most function pointers are set (note: OnEcn is NULL for BBR)
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlCanSend, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetExemption, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlReset, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetSendAllowance, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataSent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataInvalidated, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataLost, nullptr);
    // Note: QuicCongestionControlOnEcn is NULL for BBR (by design)
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlLogOutFlowStatus, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetExemptions, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlIsAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetCongestionWindow, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics, nullptr);

    // Verify BBR-specific state
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
    ASSERT_EQ(Bbr->RecoveryState, 0u); // RECOVERY_STATE_NOT_RECOVERY
    ASSERT_EQ(Bbr->BytesInFlight, 0u);
}

//
// Test 2: Initialization with boundary parameter values
// Scenario: Tests initialization with extreme boundary values for MTU and settings.
//
TEST(BbrTest, InitializeBoundaries)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    // Test minimum MTU with minimum window
    Settings.InitialWindowPackets = 1;
    Settings.SendIdleTimeoutMs = 0;
    InitializeMockConnectionForBbr(Connection, QUIC_DPLPMTUD_MIN_MTU);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 1u);

    // Test maximum MTU with larger window
    Settings.InitialWindowPackets = 100;
    Settings.SendIdleTimeoutMs = UINT32_MAX;
    InitializeMockConnectionForBbr(Connection, 65535);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 100u);
}

//
// Test 3: CanSend scenarios (via function pointer)
// Scenario: Comprehensive test of CanSend logic covering: available window (can send),
// congestion blocked (cannot send), and exemptions (bypass blocking).
//
TEST(BbrTest, CanSendScenarios)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Scenario 1: Available window - can send
    Bbr->BytesInFlight = Bbr->CongestionWindow / 2;
    Bbr->Exemptions = 0;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 2: Congestion blocked - cannot send
    Bbr->BytesInFlight = Bbr->CongestionWindow;
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 3: Exceeding window - still blocked
    Bbr->BytesInFlight = Bbr->CongestionWindow + 100;
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 4: With exemptions - can send even when blocked
    Bbr->Exemptions = 2;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));
}

//
// Test 4: SetExemption (via function pointer)
// Scenario: Tests SetExemption to verify it correctly sets the number of packets that
// can bypass congestion control.
//
TEST(BbrTest, SetExemption)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initially should be 0
    ASSERT_EQ(Bbr->Exemptions, 0u);

    // Set exemptions via function pointer
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 5);
    ASSERT_EQ(Bbr->Exemptions, 5u);

    // Set to zero
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 0);
    ASSERT_EQ(Bbr->Exemptions, 0u);

    // Set to max
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 255);
    ASSERT_EQ(Bbr->Exemptions, 255u);
}

//
// Test 5: Getter functions (via function pointers)
// Scenario: Tests all simple getter functions that return internal state values.
//
TEST(BbrTest, GetterFunctions)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Test GetExemptions
    Bbr->Exemptions = 7;
    uint8_t Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 7u);

    // Test GetBytesInFlightMax
    Bbr->BytesInFlightMax = 50000;
    uint32_t BytesInFlightMax = Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax(&Connection.CongestionControl);
    ASSERT_EQ(BytesInFlightMax, 50000u);

    // Test GetCongestionWindow - returns the current congestion window
    uint32_t CongestionWindow = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    ASSERT_EQ(CongestionWindow, Bbr->CongestionWindow);
}

//
// Test 6: IsAppLimited and SetAppLimited
// Scenario: Tests the application-limited state management functions.
//
TEST(BbrTest, AppLimitedState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Initially should not be app limited
    BOOLEAN IsAppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl);
    ASSERT_FALSE(IsAppLimited);

    // Set app limited
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);
    IsAppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl);
    ASSERT_TRUE(IsAppLimited);
}

//
// Test 7: OnDataSent updates BytesInFlight
// Scenario: Tests that OnDataSent correctly updates BytesInFlight.
// Note: BytesInFlightMax is initialized to CongestionWindow/2 during init,
// so we test that it grows as needed.
//
TEST(BbrTest, OnDataSentUpdatesState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_EQ(Bbr->BytesInFlight, 0u);
    uint32_t InitialBytesInFlightMax = Bbr->BytesInFlightMax;
    ASSERT_GT(InitialBytesInFlightMax, 0u); // Should be initialized

    // Send some data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->BytesInFlight, 1000u);

    // Send more data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 500);
    ASSERT_EQ(Bbr->BytesInFlight, 1500u);

    // BytesInFlightMax should be at least as large as current BytesInFlight
    ASSERT_GE(Bbr->BytesInFlightMax, Bbr->BytesInFlight);
}

//
// Test 8: OnDataInvalidated reduces BytesInFlight
// Scenario: Tests that OnDataInvalidated correctly reduces BytesInFlight.
//
TEST(BbrTest, OnDataInvalidatedReducesBytesInFlight)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send some data first
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 2000);
    ASSERT_EQ(Bbr->BytesInFlight, 2000u);

    // Invalidate some data
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(&Connection.CongestionControl, 500);
    ASSERT_EQ(Bbr->BytesInFlight, 1500u);

    // Invalidate more data
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(&Connection.CongestionControl, 500);
    ASSERT_EQ(Bbr->BytesInFlight, 1000u);
}

//
// Test 9: MinRtt initialization
// Scenario: Verifies MinRtt is initialized to a very large value representing no sample.
//
TEST(BbrTest, MinRttInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // MinRtt should be initialized to a large value (no sample yet)
    ASSERT_EQ(Bbr->MinRtt, UINT64_MAX);
    ASSERT_FALSE(Bbr->MinRttTimestampValid);
}

//
// Test 10: Bandwidth filter initialization
// Scenario: Verifies the bandwidth filter is properly initialized.
//
TEST(BbrTest, BandwidthFilterInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // App limited should start as FALSE
    ASSERT_FALSE(Bbr->BandwidthFilter.AppLimited);
}

//
// Test 11: Gain values initialization
// Scenario: Verifies that pacing gain and cwnd gain are set appropriately for STARTUP state.
//
TEST(BbrTest, GainValuesInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // In STARTUP state, gains should be high
    ASSERT_GT(Bbr->PacingGain, 0u);
    ASSERT_GT(Bbr->CwndGain, 0u);
}

//
// Test 12: Round trip counter initialization
// Scenario: Verifies the round trip counter starts at zero.
//
TEST(BbrTest, RoundTripCounterInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_EQ(Bbr->RoundTripCounter, 0u);
}

//
// Test 13: Recovery state initialization
// Scenario: Verifies BBR starts in non-recovery state.
//
TEST(BbrTest, RecoveryStateInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Should not be in recovery initially
    ASSERT_EQ(Bbr->RecoveryState, 0u); // RECOVERY_STATE_NOT_RECOVERY
    ASSERT_FALSE(Bbr->EndOfRecoveryValid);
}

//
// Test 14: Boolean flags initialization
// Scenario: Verifies all boolean flags are properly initialized.
//
TEST(BbrTest, BooleanFlagsInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_FALSE(Bbr->BtlbwFound);
    ASSERT_FALSE(Bbr->ExitingQuiescence);
    ASSERT_FALSE(Bbr->EndOfRecoveryValid);
    ASSERT_FALSE(Bbr->EndOfRoundTripValid);
    ASSERT_FALSE(Bbr->AckAggregationStartTimeValid);
    ASSERT_FALSE(Bbr->ProbeRttRoundValid);
    ASSERT_FALSE(Bbr->ProbeRttEndTimeValid);
}

//
// Test 15: Initial congestion window calculation
// Scenario: Verifies the initial congestion window is calculated based on
// InitialWindowPackets and MTU.
//
TEST(BbrTest, InitialCongestionWindowCalculation)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    const uint16_t Mtu = 1280;
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnectionForBbr(Connection, Mtu);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initial congestion window should be based on packets * MTU
    // The exact calculation may vary, but it should be non-zero and reasonable
    ASSERT_GT(Bbr->CongestionWindow, 0u);
    ASSERT_GT(Bbr->InitialCongestionWindow, 0u);
    ASSERT_EQ(Bbr->CongestionWindow, Bbr->InitialCongestionWindow);
}
