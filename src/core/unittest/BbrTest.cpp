/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for BBR congestion control.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "DeepTest_BbrTest.cpp.clog.h"
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

    // Initialize Settings with defaults
    Connection.Settings.PacingEnabled = FALSE;  // Disable pacing by default for simpler tests

    // Initialize Path fields needed for some functions
    Connection.Paths[0].GotFirstRttSample = FALSE;
    Connection.Paths[0].SmoothedRtt = 0;
    Connection.LossDetection.LargestSentPacketNumber = 0;
}

//
// Test 1: Comprehensive initialization verification
// Scenario: Verifies BbrCongestionControlInitialize correctly sets up all BBR state
// including settings, function pointers, state flags, and zero-initialized fields.
// What: Tests the initialization of the BBR congestion control state machine.
// How: Calls BbrCongestionControlInitialize with valid connection and settings, then inspects all state.
// Assertions: All function pointers set, BBR state is STARTUP, initial window set correctly,
// all boolean flags initialized, bandwidth and ack filters initialized.
//
TEST(DeepTest_BbrTest, InitializeComprehensive)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);

    // Pre-set some fields to verify they get initialized
    Connection.CongestionControl.Bbr.BytesInFlight = 12345;
    Connection.CongestionControl.Bbr.Exemptions = 5;

    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Verify settings stored correctly
    ASSERT_EQ(Bbr->InitialCongestionWindowPackets, 10u);

    // Verify congestion window initialized
    ASSERT_GT(Bbr->CongestionWindow, 0u);
    ASSERT_EQ(Bbr->BytesInFlightMax, Bbr->CongestionWindow / 2);

    // Verify all 17 function pointers are set
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlCanSend, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetExemption, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlReset, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetSendAllowance, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataSent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataInvalidated, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataLost, nullptr);
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlOnEcn, nullptr);  // BBR doesn't implement ECN
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlLogOutFlowStatus, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetExemptions, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlIsAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetCongestionWindow, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics, nullptr);

    // Verify boolean state flags
    ASSERT_FALSE(Bbr->BtlbwFound);
    ASSERT_FALSE(Bbr->ExitingQuiescence);
    ASSERT_FALSE(Bbr->EndOfRecoveryValid);
    ASSERT_FALSE(Bbr->EndOfRoundTripValid);
    ASSERT_FALSE(Bbr->AckAggregationStartTimeValid);
    ASSERT_FALSE(Bbr->ProbeRttRoundValid);
    ASSERT_FALSE(Bbr->ProbeRttEndTimeValid);
    ASSERT_TRUE(Bbr->RttSampleExpired);
    ASSERT_FALSE(Bbr->MinRttTimestampValid);

    // Verify BBR state machine initial state
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
    ASSERT_EQ(Bbr->RecoveryState, 0u); // RECOVERY_STATE_NOT_RECOVERY

    // Verify counters initialized
    ASSERT_EQ(Bbr->BytesInFlight, 0u);
    ASSERT_EQ(Bbr->Exemptions, 0u);
    ASSERT_EQ(Bbr->RoundTripCounter, 0u);
    ASSERT_EQ(Bbr->SlowStartupRoundCounter, 0u);
    ASSERT_EQ(Bbr->PacingCycleIndex, 0u);
    ASSERT_EQ(Bbr->AggregatedAckBytes, 0u);
    ASSERT_EQ(Bbr->LastEstimatedStartupBandwidth, 0u);

    // Verify MinRtt initialized
    ASSERT_EQ(Bbr->MinRtt, UINT64_MAX);

    // Verify bandwidth filter initialized
    ASSERT_FALSE(Bbr->BandwidthFilter.AppLimited);
    ASSERT_EQ(Bbr->BandwidthFilter.AppLimitedExitTarget, 0u);
}

//
// Test 2: Initialization with boundary parameter values
// Scenario: Tests initialization with extreme boundary values for MTU and InitialWindowPackets
// to ensure robustness across all valid configurations.
// What: Tests BBR initialization with minimum and maximum valid input values.
// How: Calls BbrCongestionControlInitialize with minimum MTU and packets, then with maximum values.
// Assertions: Congestion window is properly calculated for all boundary cases.
//
TEST(DeepTest_BbrTest, InitializeBoundaries)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    // Test minimum window with minimum MTU
    Settings.InitialWindowPackets = 1;
    Settings.SendIdleTimeoutMs = 0;
    InitializeMockConnection(Connection, QUIC_DPLPMTUD_MIN_MTU);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 1u);

    // Test maximum window with large MTU
    Settings.InitialWindowPackets = 1000;
    Settings.SendIdleTimeoutMs = UINT32_MAX;
    InitializeMockConnection(Connection, 9000); // Jumbo frame MTU
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 1000u);

    // Test standard values
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Bbr.CongestionWindow, 0u);
}

//
// Test 3: Re-initialization behavior
// Scenario: Tests that BBR can be re-initialized with different settings and correctly
// updates its state. Verifies that calling BbrCongestionControlInitialize() multiple times
// properly resets state and applies new settings.
// What: Tests multiple sequential initializations.
// How: Initialize BBR, capture state, re-initialize with different settings, verify state updated.
// Assertions: Settings change properly, congestion window scales with new InitialWindowPackets.
//
TEST(DeepTest_BbrTest, MultipleSequentialInitializations)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);

    // Initialize first time
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    uint32_t FirstCongestionWindow = Connection.CongestionControl.Bbr.CongestionWindow;
    ASSERT_EQ(Connection.CongestionControl.Bbr.InitialCongestionWindowPackets, 10u);

    // Re-initialize with different settings
    Settings.InitialWindowPackets = 20;
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Should reflect new settings with doubled window
    ASSERT_EQ(Bbr->InitialCongestionWindowPackets, 20u);
    ASSERT_EQ(Bbr->CongestionWindow, FirstCongestionWindow * 2);
}

//
// Test 4: CanSend scenarios
// Scenario: Comprehensive test of CanSend logic covering: available window (can send),
// congestion blocked (cannot send), and exemptions (bypass blocking).
// What: Tests the core congestion control send decision logic.
// How: Set different BytesInFlight and Exemptions values, call CanSend.
// Assertions: Returns TRUE when window available or exemptions set, FALSE when blocked.
//
TEST(DeepTest_BbrTest, CanSendScenarios)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
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
// Test 5: SetExemption
// Scenario: Tests SetExemption to verify it correctly sets the number of packets that
// can bypass congestion control. Used for probe packets and other special cases.
// What: Tests the exemption counter setting.
// How: Call SetExemption with different values, verify Exemptions field updated.
// Assertions: Exemptions field matches the set value.
//
TEST(DeepTest_BbrTest, SetExemption)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
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
// Test 6: GetExemptions
// Scenario: Tests GetExemptions to verify it correctly returns the current exemption count.
// What: Tests reading the exemption counter.
// How: Set different exemption values, call GetExemptions, verify returned value.
// Assertions: GetExemptions returns the current Exemptions value.
//
TEST(DeepTest_BbrTest, GetExemptions)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initially 0
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl), 0u);

    // Set and read
    Bbr->Exemptions = 3;
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl), 3u);

    Bbr->Exemptions = 100;
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl), 100u);
}

//
// Test 7: GetBytesInFlightMax
// Scenario: Tests GetBytesInFlightMax returns the maximum bytes in flight seen.
// What: Tests reading the BytesInFlightMax value.
// How: Initialize BBR, verify initial max, update max, verify updated value.
// Assertions: GetBytesInFlightMax returns current BytesInFlightMax value.
//
TEST(DeepTest_BbrTest, GetBytesInFlightMax)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Should be initialized to CongestionWindow / 2
    uint32_t InitialMax = Bbr->CongestionWindow / 2;
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax(&Connection.CongestionControl), InitialMax);

    // Update and verify
    Bbr->BytesInFlightMax = 50000;
    ASSERT_EQ(Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax(&Connection.CongestionControl), 50000u);
}

//
// Test 8: IsAppLimited initial state
// Scenario: Tests IsAppLimited returns FALSE initially after initialization.
// What: Tests the initial app-limited state.
// How: Initialize BBR, call IsAppLimited immediately.
// Assertions: Returns FALSE after initialization.
//
TEST(DeepTest_BbrTest, IsAppLimitedInitialState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Should not be app-limited initially
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl));
}

//
// Test 9: SetAppLimited when condition met
// Scenario: Tests SetAppLimited marks the connection as app-limited when BytesInFlight <= CongestionWindow.
// What: Tests setting app-limited state when send window is available.
// How: Set BytesInFlight below CongestionWindow, call SetAppLimited, verify AppLimited flag set.
// Assertions: AppLimited becomes TRUE and AppLimitedExitTarget is set.
//
TEST(DeepTest_BbrTest, SetAppLimitedWhenConditionMet)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set BytesInFlight below CongestionWindow
    Bbr->BytesInFlight = Bbr->CongestionWindow / 2;
    Connection.LossDetection.LargestSentPacketNumber = 100;

    // Call SetAppLimited
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);

    // Should be marked as app-limited
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl));
    ASSERT_EQ(Bbr->BandwidthFilter.AppLimitedExitTarget, 100u);
}

//
// Test 10: SetAppLimited when condition not met
// Scenario: Tests SetAppLimited does nothing when BytesInFlight > CongestionWindow.
// What: Tests that app-limited is not set when congestion blocked.
// How: Set BytesInFlight above CongestionWindow, call SetAppLimited, verify no change.
// Assertions: AppLimited remains FALSE.
//
TEST(DeepTest_BbrTest, SetAppLimitedWhenConditionNotMet)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set BytesInFlight above CongestionWindow
    Bbr->BytesInFlight = Bbr->CongestionWindow + 1000;

    // Call SetAppLimited
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);

    // Should NOT be marked as app-limited
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlIsAppLimited(&Connection.CongestionControl));
}

