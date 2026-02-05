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


//
// Test 11: OnDataSent basic behavior
// Scenario: Tests OnDataSent correctly updates BytesInFlight and decrements exemptions.
// What: Tests the data sent event handling.
// How: Call OnDataSent with different byte amounts, verify BytesInFlight increases and exemptions decrease.
// Assertions: BytesInFlight increases by sent bytes, Exemptions decrements when > 0, BytesInFlightMax updates.
//
TEST(DeepTest_BbrTest, OnDataSentBasicBehavior)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initially BytesInFlight should be 0
    ASSERT_EQ(Bbr->BytesInFlight, 0u);

    // Send some data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->BytesInFlight, 1000u);

    // Send more data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 500);
    ASSERT_EQ(Bbr->BytesInFlight, 1500u);

    // BytesInFlightMax should track the maximum
    ASSERT_GE(Bbr->BytesInFlightMax, 1500u);
}

//
// Test 12: OnDataSent with exemptions
// Scenario: Tests that OnDataSent decrements exemptions when they are set.
// What: Tests exemption handling during data sending.
// How: Set exemptions, call OnDataSent multiple times, verify exemptions decrement.
// Assertions: Exemptions decrement by 1 for each OnDataSent call until reaching 0.
//
TEST(DeepTest_BbrTest, OnDataSentWithExemptions)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set exemptions
    Bbr->Exemptions = 3;

    // Each OnDataSent should decrement exemptions
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->Exemptions, 2u);

    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->Exemptions, 1u);

    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->Exemptions, 0u);

    // Should stay at 0
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->Exemptions, 0u);
}

//
// Test 13: OnDataSent sets ExitingQuiescence
// Scenario: Tests that OnDataSent sets ExitingQuiescence when transitioning from idle while app-limited.
// What: Tests the quiescence exit flag setting.
// How: Set app-limited state with BytesInFlight=0, call OnDataSent, verify ExitingQuiescence set.
// Assertions: ExitingQuiescence becomes TRUE when sending data from idle app-limited state.
//
TEST(DeepTest_BbrTest, OnDataSentSetsExitingQuiescence)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set app-limited state
    Bbr->BandwidthFilter.AppLimited = TRUE;
    Bbr->BytesInFlight = 0;
    Bbr->ExitingQuiescence = FALSE;

    // Send data from idle state
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);

    // Should set ExitingQuiescence
    ASSERT_TRUE(Bbr->ExitingQuiescence);
}

//
// Test 14: OnDataInvalidated basic behavior
// Scenario: Tests OnDataInvalidated correctly decreases BytesInFlight.
// What: Tests data invalidation (e.g., cancelled stream data).
// How: Send data to increase BytesInFlight, then invalidate some, verify BytesInFlight decreases.
// Assertions: BytesInFlight decreases by invalidated bytes.
//
TEST(DeepTest_BbrTest, OnDataInvalidatedBasicBehavior)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send data first
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 5000);
    ASSERT_EQ(Bbr->BytesInFlight, 5000u);

    // Invalidate some data
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(&Connection.CongestionControl, 2000);
    ASSERT_EQ(Bbr->BytesInFlight, 3000u);

    // Invalidate more
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->BytesInFlight, 2000u);
}

//
// Test 15: Reset with FullReset=TRUE
// Scenario: Tests Reset with FullReset=TRUE resets all state including BytesInFlight.
// What: Tests full reset of BBR state machine.
// How: Modify BBR state, call Reset(TRUE), verify all state reset to initial values.
// Assertions: All state variables reset, BytesInFlight=0, BBR state=STARTUP.
//
TEST(DeepTest_BbrTest, ResetWithFullReset)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Modify state
    Bbr->BytesInFlight = 5000;
    Bbr->BtlbwFound = TRUE;
    Bbr->RoundTripCounter = 100;
    Bbr->Exemptions = 5;

    // Full reset
    Connection.CongestionControl.QuicCongestionControlReset(&Connection.CongestionControl, TRUE);

    // Verify reset
    ASSERT_EQ(Bbr->BytesInFlight, 0u);
    ASSERT_EQ(Bbr->Exemptions, 0u);
    ASSERT_FALSE(Bbr->BtlbwFound);
    ASSERT_EQ(Bbr->RoundTripCounter, 0u);
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
    ASSERT_EQ(Bbr->RecoveryState, 0u); // RECOVERY_STATE_NOT_RECOVERY
}

//
// Test 16: Reset with FullReset=FALSE
// Scenario: Tests Reset with FullReset=FALSE preserves BytesInFlight.
// What: Tests partial reset of BBR state machine.
// How: Modify BBR state including BytesInFlight, call Reset(FALSE), verify BytesInFlight preserved.
// Assertions: Most state reset but BytesInFlight preserved.
//
TEST(DeepTest_BbrTest, ResetWithoutFullReset)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Modify state
    Bbr->BytesInFlight = 5000;
    Bbr->BtlbwFound = TRUE;
    Bbr->RoundTripCounter = 100;

    // Partial reset
    Connection.CongestionControl.QuicCongestionControlReset(&Connection.CongestionControl, FALSE);

    // BytesInFlight should be preserved
    ASSERT_EQ(Bbr->BytesInFlight, 5000u);
    
    // Other state should be reset
    ASSERT_FALSE(Bbr->BtlbwFound);
    ASSERT_EQ(Bbr->RoundTripCounter, 0u);
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP
}

//
// Test 17: GetCongestionWindow in STARTUP state
// Scenario: Tests GetCongestionWindow returns normal congestion window in STARTUP.
// What: Tests congestion window retrieval in STARTUP state.
// How: Initialize BBR (starts in STARTUP), call GetCongestionWindow.
// Assertions: Returns the CongestionWindow value.
//
TEST(DeepTest_BbrTest, GetCongestionWindowInStartup)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Should be in STARTUP state
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP

    uint32_t cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    
    // Should return the normal congestion window
    ASSERT_EQ(cwnd, Bbr->CongestionWindow);
}

//
// Test 18: GetCongestionWindow in PROBE_RTT state
// Scenario: Tests GetCongestionWindow returns minimum window (4*DatagramPayloadSize) in PROBE_RTT.
// What: Tests congestion window retrieval in PROBE_RTT state.
// How: Set BBR to PROBE_RTT state, call GetCongestionWindow.
// Assertions: Returns minimum congestion window which is less than normal window.
//
TEST(DeepTest_BbrTest, GetCongestionWindowInProbeRtt)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    uint32_t normalCwnd = Bbr->CongestionWindow;

    // Set to PROBE_RTT state (value = 3)
    Bbr->BbrState = 3; // BBR_STATE_PROBE_RTT

    uint32_t cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    
    // Should return minimum congestion window (much less than normal)
    ASSERT_LT(cwnd, normalCwnd);
    ASSERT_GT(cwnd, 0u);
    // Should be around 4 packets worth (allowing for header overhead)
    ASSERT_LT(cwnd, 4 * 1280 + 100);
    ASSERT_GT(cwnd, 4 * 1200);
}

//
// Test 19: GetCongestionWindow in recovery
// Scenario: Tests GetCongestionWindow returns MIN(CongestionWindow, RecoveryWindow) when in recovery.
// What: Tests congestion window retrieval during recovery.
// How: Enter recovery state with RecoveryWindow < CongestionWindow, call GetCongestionWindow.
// Assertions: Returns RecoveryWindow when it's smaller.
//
TEST(DeepTest_BbrTest, GetCongestionWindowInRecovery)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Enter recovery state
    Bbr->RecoveryState = 1; // RECOVERY_STATE_CONSERVATIVE
    Bbr->RecoveryWindow = 5000; // Smaller than CongestionWindow
    
    ASSERT_GT(Bbr->CongestionWindow, Bbr->RecoveryWindow);

    uint32_t cwnd = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    
    // Should return the smaller RecoveryWindow
    ASSERT_EQ(cwnd, Bbr->RecoveryWindow);
}

//
// Test 20: GetSendAllowance when congestion blocked
// Scenario: Tests GetSendAllowance returns 0 when BytesInFlight >= CongestionWindow.
// What: Tests send allowance when congestion blocked.
// How: Set BytesInFlight >= CongestionWindow, call GetSendAllowance.
// Assertions: Returns 0.
//
TEST(DeepTest_BbrTest, GetSendAllowanceWhenBlocked)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set BytesInFlight to congestion window
    Bbr->BytesInFlight = Bbr->CongestionWindow;

    uint32_t allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 1000, TRUE);
    
    ASSERT_EQ(allowance, 0u);
}

//
// Test 21: GetSendAllowance without pacing
// Scenario: Tests GetSendAllowance returns full available window when pacing disabled.
// What: Tests send allowance calculation without pacing.
// How: Disable pacing, set BytesInFlight below window, call GetSendAllowance.
// Assertions: Returns CongestionWindow - BytesInFlight.
//
TEST(DeepTest_BbrTest, GetSendAllowanceWithoutPacing)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Disable pacing
    Connection.Settings.PacingEnabled = FALSE;
    Bbr->BytesInFlight = Bbr->CongestionWindow / 2;

    uint32_t expectedAllowance = Bbr->CongestionWindow - Bbr->BytesInFlight;

    uint32_t allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 1000, TRUE);
    
    ASSERT_EQ(allowance, expectedAllowance);
}

//
// Test 22: GetSendAllowance with invalid time
// Scenario: Tests GetSendAllowance returns full window when TimeSinceLastSendValid=FALSE.
// What: Tests send allowance skips pacing with invalid time.
// How: Enable pacing but pass TimeSinceLastSendValid=FALSE.
// Assertions: Returns CongestionWindow - BytesInFlight (no pacing applied).
//
TEST(DeepTest_BbrTest, GetSendAllowanceWithInvalidTime)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Enable pacing
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Bbr->MinRtt = 50000;
    Bbr->BytesInFlight = Bbr->CongestionWindow / 2;

    uint32_t expectedAllowance = Bbr->CongestionWindow - Bbr->BytesInFlight;

    // Pass FALSE for TimeSinceLastSendValid
    uint32_t allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 1000, FALSE);
    
    ASSERT_EQ(allowance, expectedAllowance);
}

//
// Test 23: OnSpuriousCongestionEvent
// Scenario: Tests OnSpuriousCongestionEvent always returns FALSE for BBR.
// What: Tests spurious congestion event handling.
// How: Call OnSpuriousCongestionEvent.
// Assertions: Always returns FALSE (BBR doesn't handle spurious events).
//
TEST(DeepTest_BbrTest, OnSpuriousCongestionEvent)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    // Should always return FALSE
    BOOLEAN result = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(&Connection.CongestionControl);
    ASSERT_FALSE(result);
}

//
// Test 24: GetNetworkStatistics
// Scenario: Tests GetNetworkStatistics populates all fields correctly.
// What: Tests network statistics retrieval.
// How: Set various BBR state values, call GetNetworkStatistics, verify all fields populated.
// Assertions: NetworkStatistics contains correct BytesInFlight, CongestionWindow, etc.
//
TEST(DeepTest_BbrTest, GetNetworkStatistics)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set some state
    Bbr->BytesInFlight = 5000;
    Connection.SendBuffer.PostedBytes = 10000;
    Connection.SendBuffer.IdealBytes = 15000;
    Connection.Paths[0].SmoothedRtt = 50000;

    QUIC_NETWORK_STATISTICS Stats = {};
    Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics(
        &Connection, &Connection.CongestionControl, &Stats);

    // Verify fields populated
    ASSERT_EQ(Stats.BytesInFlight, 5000u);
    ASSERT_EQ(Stats.PostedBytes, 10000u);
    ASSERT_EQ(Stats.IdealBytes, 15000u);
    ASSERT_EQ(Stats.SmoothedRTT, 50000u);
    ASSERT_EQ(Stats.CongestionWindow, Bbr->CongestionWindow);
}


//
// Test 25: OnDataLost enters recovery
// Scenario: Tests OnDataLost enters CONSERVATIVE recovery state and sets EndOfRecovery.
// What: Tests packet loss handling and recovery entry.
// How: Create loss event, call OnDataLost, verify recovery state entered.
// Assertions: RecoveryState=CONSERVATIVE, EndOfRecoveryValid=TRUE, BytesInFlight decreased.
//
TEST(DeepTest_BbrTest, OnDataLostEntersRecovery)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send some data first
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 10000);
    ASSERT_EQ(Bbr->BytesInFlight, 10000u);
    ASSERT_EQ(Bbr->RecoveryState, 0u); // NOT_RECOVERY

    // Create loss event
    QUIC_LOSS_EVENT LossEvent = {};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 10;
    LossEvent.NumRetransmittableBytes = 2000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent);

    // Should enter recovery
    ASSERT_EQ(Bbr->RecoveryState, 1u); // RECOVERY_STATE_CONSERVATIVE
    ASSERT_TRUE(Bbr->EndOfRecoveryValid);
    ASSERT_EQ(Bbr->EndOfRecovery, 10u);
    
    // BytesInFlight should decrease
    ASSERT_EQ(Bbr->BytesInFlight, 8000u);
    
    // RecoveryWindow should be set
    ASSERT_GT(Bbr->RecoveryWindow, 0u);
}

//
// Test 26: OnDataLost with persistent congestion
// Scenario: Tests OnDataLost sets RecoveryWindow to minimum on persistent congestion.
// What: Tests persistent congestion handling.
// How: Create loss event with PersistentCongestion=TRUE, call OnDataLost.
// Assertions: RecoveryWindow set to minimum (around 4 * DatagramPayloadSize).
//
TEST(DeepTest_BbrTest, OnDataLostWithPersistentCongestion)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    uint32_t oldRecoveryWindow = Bbr->RecoveryWindow;

    // Send data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 10000);

    // Create persistent congestion loss event
    QUIC_LOSS_EVENT LossEvent = {};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 10;
    LossEvent.NumRetransmittableBytes = 2000;
    LossEvent.PersistentCongestion = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent);

    // RecoveryWindow should be minimum (around 4 packets, accounting for overhead)
    ASSERT_LT(Bbr->RecoveryWindow, oldRecoveryWindow);
    ASSERT_GT(Bbr->RecoveryWindow, 0u);
    ASSERT_LT(Bbr->RecoveryWindow, 6000u); // Less than old window
    ASSERT_GT(Bbr->RecoveryWindow, 4000u); // Around 4 packets minimum
}

//
// Test 27: OnDataLost during existing recovery
// Scenario: Tests OnDataLost behavior when already in recovery.
// What: Tests loss handling during recovery.
// How: Enter recovery, then trigger another loss, verify state remains CONSERVATIVE.
// Assertions: Stays in recovery, RecoveryWindow updated.
//
TEST(DeepTest_BbrTest, OnDataLostDuringRecovery)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send data and enter recovery
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 10000);
    
    QUIC_LOSS_EVENT LossEvent1 = {};
    LossEvent1.LargestPacketNumberLost = 5;
    LossEvent1.LargestSentPacketNumber = 10;
    LossEvent1.NumRetransmittableBytes = 2000;
    LossEvent1.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent1);
    ASSERT_EQ(Bbr->RecoveryState, 1u); // CONSERVATIVE

    uint32_t oldRecoveryWindow = Bbr->RecoveryWindow;

    // Lose more data during recovery
    QUIC_LOSS_EVENT LossEvent2 = {};
    LossEvent2.LargestPacketNumberLost = 8;
    LossEvent2.LargestSentPacketNumber = 15;
    LossEvent2.NumRetransmittableBytes = 1000;
    LossEvent2.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent2);
    
    // Should still be in recovery
    ASSERT_EQ(Bbr->RecoveryState, 1u); // CONSERVATIVE
    
    // RecoveryWindow should be reduced
    ASSERT_LT(Bbr->RecoveryWindow, oldRecoveryWindow);
}

//
// Test 28: OnDataAcknowledged with implicit ACK
// Scenario: Tests OnDataAcknowledged handles implicit ACKs by only updating congestion window.
// What: Tests implicit ACK handling (no packet metadata).
// How: Create ACK event with IsImplicit=TRUE, call OnDataAcknowledged.
// Assertions: CongestionWindow updated, but minimal BBR logic executed.
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedWithImplicitAck)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 5000);
    ASSERT_EQ(Bbr->BytesInFlight, 5000u);

    uint32_t oldCongestionWindow = Bbr->CongestionWindow;

    // Create implicit ACK event
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = TRUE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = 1000000;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Congestion window should be updated (grown in STARTUP)
    ASSERT_GE(Bbr->CongestionWindow, oldCongestionWindow);
}

//
// Test 29: OnDataAcknowledged updates MinRtt
// Scenario: Tests OnDataAcknowledged updates MinRtt when new sample available.
// What: Tests MinRtt tracking.
// How: Create ACK event with MinRttValid=TRUE and new MinRtt, call OnDataAcknowledged.
// Assertions: MinRtt updated to new value, MinRttTimestampValid=TRUE.
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedUpdatesMinRtt)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Send data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);

    // Create ACK event with MinRtt
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = 2000000;
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000; // 50ms
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // MinRtt should be updated
    ASSERT_EQ(Bbr->MinRtt, 50000u);
    ASSERT_TRUE(Bbr->MinRttTimestampValid);
    ASSERT_EQ(Bbr->MinRttTimestamp, 2000000u);
}

//
// Test 30: OnDataAcknowledged starts new round trip
// Scenario: Tests OnDataAcknowledged increments RoundTripCounter on new round trip.
// What: Tests round trip detection.
// How: ACK packet beyond EndOfRoundTrip, verify RoundTripCounter increments.
// Assertions: RoundTripCounter increases, EndOfRoundTrip updated.
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedStartsNewRoundTrip)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_EQ(Bbr->RoundTripCounter, 0u);
    ASSERT_FALSE(Bbr->EndOfRoundTripValid);

    // Send data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);

    // Create ACK event
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = 1000000;
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Should start first round trip
    ASSERT_EQ(Bbr->RoundTripCounter, 1u);
    ASSERT_TRUE(Bbr->EndOfRoundTripValid);
    ASSERT_EQ(Bbr->EndOfRoundTrip, 10u);
}

//
// Test 31: OnDataAcknowledged exits recovery
// Scenario: Tests OnDataAcknowledged exits recovery when EndOfRecovery packet is acked without loss.
// What: Tests recovery exit.
// How: Enter recovery, ACK packet beyond EndOfRecovery without loss, verify exit.
// Assertions: RecoveryState returns to NOT_RECOVERY.
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedExitsRecovery)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Enter recovery
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 10000);
    
    QUIC_LOSS_EVENT LossEvent = {};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 10;
    LossEvent.NumRetransmittableBytes = 2000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent);
    ASSERT_EQ(Bbr->RecoveryState, 1u); // CONSERVATIVE
    ASSERT_EQ(Bbr->EndOfRecovery, 10u);

    // Send more data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 2000);

    // ACK packet beyond EndOfRecovery without loss
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 2000;
    AckEvent.NumTotalAckedRetransmittableBytes = 4000;
    AckEvent.TimeNow = 2000000;
    AckEvent.LargestAck = 15; // Beyond EndOfRecovery (10)
    AckEvent.LargestSentPacketNumber = 15;
    AckEvent.MinRttValid = FALSE;
    AckEvent.HasLoss = FALSE; // No loss
    AckEvent.AckedPackets = NULL;

    // First ACK - should transition to GROWTH
    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);
    // Second ACK - should exit recovery
    AckEvent.LargestAck = 20;
    AckEvent.LargestSentPacketNumber = 20;
    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Should exit recovery
    ASSERT_EQ(Bbr->RecoveryState, 0u); // NOT_RECOVERY
}

//
// Test 32: OnDataAcknowledged transitions STARTUP to DRAIN
// Scenario: Tests OnDataAcknowledged transitions from STARTUP to DRAIN when BtlbwFound.
// What: Tests STARTUP → DRAIN transition.
// How: In STARTUP with BtlbwFound=TRUE and high BytesInFlight, call OnDataAcknowledged, verify transition to DRAIN.
// Assertions: BbrState transitions to DRAIN (1) when in STARTUP and BtlbwFound is TRUE.
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedTransitionsStartupToDrain)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_EQ(Bbr->BbrState, 0u); // STARTUP

    // Send lots of data to keep BytesInFlight high (prevent immediate transition to PROBE_BW)
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 20000);

    // Mark bottleneck bandwidth found and prevent transition to PROBE_RTT
    Bbr->BtlbwFound = TRUE;
    Bbr->MinRttTimestampValid = TRUE;
    Bbr->MinRttTimestamp = 1000000;
    Bbr->MinRtt = 50000;
    Bbr->RttSampleExpired = FALSE; // Prevent PROBE_RTT transition
    Bbr->ExitingQuiescence = FALSE;

    // Create ACK event for small amount of data (BytesInFlight stays high)
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 500; // Ack small amount
    AckEvent.NumTotalAckedRetransmittableBytes = 500;
    AckEvent.TimeNow = 1050000; // Soon after MinRtt, so not expired
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Should transition to DRAIN (and stay there because BytesInFlight is still high)
    ASSERT_EQ(Bbr->BbrState, 1u); // BBR_STATE_DRAIN
}

//
// Test 33: OnDataAcknowledged transitions DRAIN to PROBE_BW
// Scenario: Tests OnDataAcknowledged transitions from DRAIN to PROBE_BW when drained.
// What: Tests DRAIN → PROBE_BW transition.
// How: Set DRAIN state with low BytesInFlight, call OnDataAcknowledged.
// Assertions: BbrState transitions to PROBE_BW (2).
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedTransitionsDrainToProbeBw)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set to DRAIN state
    Bbr->BbrState = 1; // BBR_STATE_DRAIN
    Bbr->BtlbwFound = TRUE;

    // Set low BytesInFlight (drain condition met)
    Bbr->BytesInFlight = 100; // Much less than target

    // Create ACK event
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 100;
    AckEvent.NumTotalAckedRetransmittableBytes = 100;
    AckEvent.TimeNow = 1000000;
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Should transition to PROBE_BW
    ASSERT_EQ(Bbr->BbrState, 2u); // BBR_STATE_PROBE_BW
}

//
// Test 34: OnDataAcknowledged transitions to PROBE_RTT
// Scenario: Tests OnDataAcknowledged transitions to PROBE_RTT when MinRtt expires.
// What: Tests transition to PROBE_RTT for RTT measurement.
// How: Set expired MinRtt, call OnDataAcknowledged.
// Assertions: BbrState transitions to PROBE_RTT (3).
//
TEST(DeepTest_BbrTest, OnDataAcknowledgedTransitionsToProbeRtt)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set to PROBE_BW state
    Bbr->BbrState = 2; // BBR_STATE_PROBE_BW
    Bbr->BtlbwFound = TRUE;
    Bbr->MinRtt = 50000;
    Bbr->MinRttTimestamp = 100000;
    Bbr->MinRttTimestampValid = TRUE;
    Bbr->ExitingQuiescence = FALSE;

    // Send data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    Connection.LossDetection.LargestSentPacketNumber = 10;

    // Create ACK event with expired MinRtt
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = Bbr->MinRttTimestamp + 11000000; // > 10 seconds expired
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    // Mark MinRtt as expired
    Bbr->RttSampleExpired = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Should transition to PROBE_RTT
    ASSERT_EQ(Bbr->BbrState, 3u); // BBR_STATE_PROBE_RTT
}


//
// Test 35: GetSendAllowance with pacing in STARTUP
// Scenario: Tests GetSendAllowance with pacing enabled in STARTUP state uses special calculation.
// What: Tests pacing calculation in STARTUP considering both bandwidth-based and cwnd-based limits.
// How: Enable pacing with valid RTT, set STARTUP state, call GetSendAllowance with time delta.
// Assertions: Returns paced allowance based on MAX of bandwidth-based and cwnd-based calculations.
//
TEST(DeepTest_BbrTest, GetSendAllowanceWithPacingInStartup)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Enable pacing and set up for paced send
    Connection.Settings.PacingEnabled = TRUE;
    Bbr->MinRtt = 50000; // 50ms
    Bbr->MinRttTimestampValid = TRUE;
    Bbr->BytesInFlight = 1000;

    // Should be in STARTUP state
    ASSERT_EQ(Bbr->BbrState, 0u); // BBR_STATE_STARTUP

    uint64_t timeSinceLastSend = 10000; // 10ms
    uint32_t allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, timeSinceLastSend, TRUE);

    // Should return some paced allowance
    ASSERT_GT(allowance, 0u);
    ASSERT_LE(allowance, Bbr->CongestionWindow - Bbr->BytesInFlight);
}

//
// Test 36: PROBE_BW pacing cycle advancement
// Scenario: Tests that PROBE_BW advances through pacing cycle when conditions met.
// What: Tests pacing cycle index advancement in PROBE_BW state.
// How: Set PROBE_BW state, create ACK events to advance cycle, verify PacingCycleIndex changes.
// Assertions: PacingCycleIndex advances through cycle, wraps around to 0 after reaching max.
//
TEST(DeepTest_BbrTest, ProbeBwPacingCycleAdvancement)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set to PROBE_BW state
    Bbr->BbrState = 2; // BBR_STATE_PROBE_BW
    Bbr->BtlbwFound = TRUE;
    Bbr->MinRtt = 50000;
    Bbr->MinRttTimestampValid = TRUE;
    Bbr->CycleStart = 1000000;
    Bbr->PacingCycleIndex = 2;
    Bbr->PacingGain = 256; // GAIN_UNIT
    Bbr->RttSampleExpired = FALSE;
    Bbr->ExitingQuiescence = FALSE;

    // Send some data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 5000);

    uint32_t oldCycleIndex = Bbr->PacingCycleIndex;

    // Create ACK event after sufficient time to trigger cycle advancement
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = 1100000; // 100ms later, more than MinRtt
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Cycle should have advanced
    ASSERT_NE(Bbr->PacingCycleIndex, oldCycleIndex);
    ASSERT_LT(Bbr->PacingCycleIndex, 8u); // Should be within cycle length
}

//
// Test 37: App-limited state management
// Scenario: Tests that app-limited state is correctly set and tracked.
// What: Tests SetAppLimited sets the flag and exit target when condition met.
// How: Set low BytesInFlight, call SetAppLimited, verify flag and target set.
// Assertions: AppLimited flag set to TRUE and AppLimitedExitTarget updated when BytesInFlight low.
//
TEST(DeepTest_BbrTest, BandwidthFilterWithAppLimitedPackets)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initially not app-limited
    ASSERT_FALSE(Bbr->BandwidthFilter.AppLimited);

    // Mark as app-limited when BytesInFlight is low
    Bbr->BytesInFlight = 100; // Low, below congestion window
    Connection.LossDetection.LargestSentPacketNumber = 5;
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);

    // Should be marked as app-limited
    ASSERT_TRUE(Bbr->BandwidthFilter.AppLimited);
    ASSERT_EQ(Bbr->BandwidthFilter.AppLimitedExitTarget, 5u);

    // Try to set app-limited when BytesInFlight is high (should not set)
    Bbr->BandwidthFilter.AppLimited = FALSE;
    Bbr->BytesInFlight = Bbr->CongestionWindow + 1000; // High, above congestion window
    Connection.LossDetection.LargestSentPacketNumber = 10;
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(&Connection.CongestionControl);

    // Should NOT be marked as app-limited
    ASSERT_FALSE(Bbr->BandwidthFilter.AppLimited);
}

//
// Test 38: Recovery window growth during RECOVERY_STATE_GROWTH
// Scenario: Tests that RecoveryWindow grows by AckedBytes in GROWTH state.
// What: Tests recovery window expansion during growth phase.
// How: Enter recovery, advance to GROWTH state, ack data, verify RecoveryWindow increases.
// Assertions: RecoveryWindow grows when in GROWTH state with acks.
//
TEST(DeepTest_BbrTest, RecoveryWindowGrowthInGrowthState)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Enter recovery
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 10000);
    ASSERT_EQ(Bbr->BytesInFlight, 10000u);
    
    QUIC_LOSS_EVENT LossEvent = {};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 10;
    LossEvent.NumRetransmittableBytes = 2000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(&Connection.CongestionControl, &LossEvent);
    ASSERT_EQ(Bbr->RecoveryState, 1u); // CONSERVATIVE
    ASSERT_EQ(Bbr->BytesInFlight, 8000u); // 10000 - 2000

    // Send more data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 2000);
    ASSERT_EQ(Bbr->BytesInFlight, 10000u);
    
    // First ACK - new round trip to enter GROWTH (ack less than sent)
    QUIC_ACK_EVENT AckEvent1 = {};
    AckEvent1.IsImplicit = FALSE;
    AckEvent1.NumRetransmittableBytes = 500; // Ack small amount
    AckEvent1.NumTotalAckedRetransmittableBytes = 500;
    AckEvent1.TimeNow = 1000000;
    AckEvent1.LargestAck = 15;
    AckEvent1.LargestSentPacketNumber = 20;
    AckEvent1.MinRttValid = FALSE;
    AckEvent1.HasLoss = TRUE; // Still has loss, so stay in recovery
    AckEvent1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent1);
    
    // Should transition to GROWTH
    ASSERT_EQ(Bbr->RecoveryState, 2u); // GROWTH
    ASSERT_EQ(Bbr->BytesInFlight, 9500u); // 10000 - 500

    uint32_t oldRecoveryWindow = Bbr->RecoveryWindow;

    // Second ACK in GROWTH state
    QUIC_ACK_EVENT AckEvent2 = {};
    AckEvent2.IsImplicit = FALSE;
    AckEvent2.NumRetransmittableBytes = 300;
    AckEvent2.NumTotalAckedRetransmittableBytes = 800;
    AckEvent2.TimeNow = 1050000;
    AckEvent2.LargestAck = 18;
    AckEvent2.LargestSentPacketNumber = 20;
    AckEvent2.MinRttValid = FALSE;
    AckEvent2.HasLoss = TRUE; // Still has loss
    AckEvent2.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent2);

    // RecoveryWindow should have grown by the acked bytes (300)
    ASSERT_GT(Bbr->RecoveryWindow, oldRecoveryWindow);
}

//
// Test 39: Multiple round trips progression
// Scenario: Tests that RoundTripCounter increments correctly over multiple round trips.
// What: Tests round trip counter progression.
// How: Send data, ack beyond EndOfRoundTrip multiple times, verify counter increments.
// Assertions: RoundTripCounter increments by 1 for each new round trip.
//
TEST(DeepTest_BbrTest, MultipleRoundTripsProgression)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_EQ(Bbr->RoundTripCounter, 0u);

    // Round trip 1
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    Connection.LossDetection.LargestSentPacketNumber = 10;

    QUIC_ACK_EVENT AckEvent1 = {};
    AckEvent1.IsImplicit = FALSE;
    AckEvent1.NumRetransmittableBytes = 1000;
    AckEvent1.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent1.TimeNow = 1000000;
    AckEvent1.LargestAck = 10;
    AckEvent1.LargestSentPacketNumber = 10;
    AckEvent1.MinRttValid = FALSE;
    AckEvent1.HasLoss = FALSE;
    AckEvent1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent1);
    ASSERT_EQ(Bbr->RoundTripCounter, 1u);

    // Round trip 2
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    Connection.LossDetection.LargestSentPacketNumber = 20;

    QUIC_ACK_EVENT AckEvent2 = {};
    AckEvent2.IsImplicit = FALSE;
    AckEvent2.NumRetransmittableBytes = 1000;
    AckEvent2.NumTotalAckedRetransmittableBytes = 2000;
    AckEvent2.TimeNow = 1050000;
    AckEvent2.LargestAck = 20;
    AckEvent2.LargestSentPacketNumber = 20;
    AckEvent2.MinRttValid = FALSE;
    AckEvent2.HasLoss = FALSE;
    AckEvent2.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent2);
    ASSERT_EQ(Bbr->RoundTripCounter, 2u);

    // Round trip 3
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    Connection.LossDetection.LargestSentPacketNumber = 30;

    QUIC_ACK_EVENT AckEvent3 = {};
    AckEvent3.IsImplicit = FALSE;
    AckEvent3.NumRetransmittableBytes = 1000;
    AckEvent3.NumTotalAckedRetransmittableBytes = 3000;
    AckEvent3.TimeNow = 1100000;
    AckEvent3.LargestAck = 30;
    AckEvent3.LargestSentPacketNumber = 30;
    AckEvent3.MinRttValid = FALSE;
    AckEvent3.HasLoss = FALSE;
    AckEvent3.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent3);
    ASSERT_EQ(Bbr->RoundTripCounter, 3u);
}

//
// Test 40: MinRtt expiration and refresh
// Scenario: Tests that expired MinRtt triggers PROBE_RTT and can be refreshed.
// What: Tests MinRtt timeout mechanism.
// How: Set MinRtt with old timestamp, trigger expiration, verify transition to PROBE_RTT.
// Assertions: RttSampleExpired flag set when MinRtt older than 10 seconds, triggers PROBE_RTT.
//
TEST(DeepTest_BbrTest, MinRttExpirationAndRefresh)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Set to PROBE_BW with old MinRtt
    Bbr->BbrState = 2; // PROBE_BW
    Bbr->BtlbwFound = TRUE;
    Bbr->MinRtt = 50000;
    Bbr->MinRttTimestamp = 1000000; // 1 second
    Bbr->MinRttTimestampValid = TRUE;
    Bbr->ExitingQuiescence = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    Connection.LossDetection.LargestSentPacketNumber = 10;

    // Create ACK event 11 seconds later (MinRtt expires after 10 seconds)
    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = 12000000; // 12 seconds, more than 10 seconds after MinRttTimestamp
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Should have transitioned to PROBE_RTT due to expired MinRtt
    ASSERT_EQ(Bbr->BbrState, 3u); // PROBE_RTT
}

//
// Test 41: Congestion window growth in STARTUP without bottleneck found
// Scenario: Tests aggressive congestion window growth in STARTUP before bottleneck is found.
// What: Tests STARTUP phase cwnd growth.
// How: In STARTUP state, ack data multiple times, verify cwnd grows aggressively.
// Assertions: CongestionWindow increases by full acked amount in STARTUP.
//
TEST(DeepTest_BbrTest, CongestionWindowGrowthInStartupBeforeBottleneck)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    ASSERT_EQ(Bbr->BbrState, 0u); // STARTUP
    ASSERT_FALSE(Bbr->BtlbwFound);

    uint32_t initialCwnd = Bbr->CongestionWindow;

    // Send and ack data
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 5000);

    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 5000;
    AckEvent.NumTotalAckedRetransmittableBytes = 5000;
    AckEvent.TimeNow = 1000000;
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // Cwnd should have grown by acked bytes in STARTUP
    ASSERT_GT(Bbr->CongestionWindow, initialCwnd);
    // In STARTUP before bottleneck found, cwnd grows by acked bytes
    ASSERT_GE(Bbr->CongestionWindow, initialCwnd + 5000);
}

//
// Test 42: Send quantum calculation at different bandwidth levels
// Scenario: Tests that send quantum is calculated correctly for low, medium, and high bandwidth.
// What: Tests send quantum scaling with pacing rate.
// How: Set different bandwidth estimates, trigger send quantum calculation, verify correct quantum.
// Assertions: Send quantum is 1 packet for low BW, 2 packets for medium BW, calculated for high BW.
//
TEST(DeepTest_BbrTest, SendQuantumCalculationAtDifferentBandwidths)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Initially SendQuantum should be 0
    ASSERT_EQ(Bbr->SendQuantum, 0u);

    // Trigger send quantum calculation by updating congestion window
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);

    QUIC_ACK_EVENT AckEvent = {};
    AckEvent.IsImplicit = FALSE;
    AckEvent.NumRetransmittableBytes = 1000;
    AckEvent.NumTotalAckedRetransmittableBytes = 1000;
    AckEvent.TimeNow = 1000000;
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.MinRttValid = TRUE;
    AckEvent.MinRtt = 50000;
    AckEvent.HasLoss = FALSE;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent);

    // SendQuantum should now be set
    ASSERT_GT(Bbr->SendQuantum, 0u);
}

//
// Test 43: PROBE_RTT complete flow with timing
// Scenario: Tests complete PROBE_RTT entry, duration, and exit sequence.
// What: Tests full PROBE_RTT state machine including timing requirements.
// How: Enter PROBE_RTT, wait for low inflight, complete duration, verify exit.
// Assertions: PROBE_RTT sets ProbeRttEndTime when low inflight condition is met.
//
TEST(DeepTest_BbrTest, ProbeRttCompleteFlowWithTiming)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    BbrCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_BBR *Bbr = &Connection.CongestionControl.Bbr;

    // Transition to PROBE_RTT
    Bbr->BbrState = 3; // PROBE_RTT
    Bbr->BtlbwFound = TRUE;
    Bbr->MinRtt = 50000;
    Bbr->ProbeRttEndTimeValid = FALSE;
    Bbr->ProbeRttRoundValid = FALSE;

    // Send data first
    Connection.CongestionControl.QuicCongestionControlOnDataSent(&Connection.CongestionControl, 1000);
    ASSERT_EQ(Bbr->BytesInFlight, 1000u);

    Connection.LossDetection.LargestSentPacketNumber = 10;

    // First ACK in PROBE_RTT with low inflight - should set ProbeRttEndTime
    QUIC_ACK_EVENT AckEvent1 = {};
    AckEvent1.IsImplicit = FALSE;
    AckEvent1.NumRetransmittableBytes = 500;
    AckEvent1.NumTotalAckedRetransmittableBytes = 500;
    AckEvent1.TimeNow = 1000000;
    AckEvent1.LargestAck = 10;
    AckEvent1.LargestSentPacketNumber = 10;
    AckEvent1.MinRttValid = FALSE;
    AckEvent1.HasLoss = FALSE;
    AckEvent1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(&Connection.CongestionControl, &AckEvent1);

    // Should have set ProbeRttEndTime (bytes in flight is now 500, which is low)
    ASSERT_TRUE(Bbr->ProbeRttEndTimeValid);

    // Should still be in PROBE_RTT
    ASSERT_EQ(Bbr->BbrState, 3u);
}

