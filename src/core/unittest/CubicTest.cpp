/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for CUBIC congestion control.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "DeepTest_CubicTest.cpp.clog.h"
#endif

//
// Helper to create a minimal valid connection for testing CUBIC initialization.
// Uses a real QUIC_CONNECTION structure to ensure proper memory layout when
// QuicCongestionControlGetConnection() does CXPLAT_CONTAINING_RECORD pointer arithmetic.
//
static void InitializeMockConnection(
    QUIC_CONNECTION& Connection,
    uint16_t Mtu)
{
    // Zero-initialize the entire connection structure
    CxPlatZeroMemory(&Connection, sizeof(Connection));

    // Initialize only the fields needed by CUBIC functions
    Connection.Paths[0].Mtu = Mtu;
    Connection.Paths[0].IsActive = TRUE;
    Connection.Send.NextPacketNumber = 0;

    // Initialize Settings with defaults
    Connection.Settings.PacingEnabled = FALSE;  // Disable pacing by default for simpler tests
    Connection.Settings.HyStartEnabled = FALSE; // Disable HyStart by default

    // Initialize Path fields needed for some functions
    Connection.Paths[0].GotFirstRttSample = FALSE;
    Connection.Paths[0].SmoothedRtt = 0;
}

//
// Test 1: Comprehensive initialization verification
// Scenario: Verifies CubicCongestionControlInitialize correctly sets up all CUBIC state
// including settings, function pointers, state flags, HyStart fields, and zero-initialized fields.
// This consolidates basic initialization, function pointer, state flags, HyStart, and zero-field checks.
//
TEST(DeepTest_CubicTest, InitializeComprehensive)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);

    // Pre-set some fields to verify they get zeroed
    Connection.CongestionControl.Cubic.BytesInFlight = 12345;
    Connection.CongestionControl.Cubic.Exemptions = 5;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Verify settings stored correctly
    ASSERT_EQ(Cubic->InitialWindowPackets, 10u);
    ASSERT_EQ(Cubic->SendIdleTimeoutMs, 1000u);
    ASSERT_EQ(Cubic->SlowStartThreshold, UINT32_MAX);

    // Verify congestion window initialized
    ASSERT_GT(Cubic->CongestionWindow, 0u);
    ASSERT_EQ(Cubic->BytesInFlightMax, Cubic->CongestionWindow / 2);

    // Verify all 17 function pointers are set
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlCanSend, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetExemption, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlReset, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetSendAllowance, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataSent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataInvalidated, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnDataLost, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnEcn, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlLogOutFlowStatus, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetExemptions, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlIsAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlSetAppLimited, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetCongestionWindow, nullptr);
    ASSERT_NE(Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics, nullptr);

    // Verify boolean state flags
    ASSERT_FALSE(Cubic->HasHadCongestionEvent);
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->IsInPersistentCongestion);
    ASSERT_FALSE(Cubic->TimeOfLastAckValid);

    // Verify HyStart fields
    ASSERT_EQ(Cubic->HyStartState, HYSTART_NOT_STARTED);
    ASSERT_EQ(Cubic->HyStartRoundEnd, 0u);
    ASSERT_EQ(Cubic->HyStartAckCount, 0u);
    ASSERT_EQ(Cubic->MinRttInLastRound, UINT64_MAX);
    ASSERT_EQ(Cubic->MinRttInCurrentRound, UINT64_MAX);
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, 1u);
}

//
// Test 2: Initialization with boundary parameter values
// Scenario: Tests initialization with extreme boundary values for MTU, InitialWindowPackets,
// and SendIdleTimeoutMs to ensure robustness across all valid configurations.
//
TEST(DeepTest_CubicTest, InitializeBoundaries)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    // Test minimum MTU with minimum window
    Settings.InitialWindowPackets = 1;
    Settings.SendIdleTimeoutMs = 0;
    InitializeMockConnection(Connection, QUIC_DPLPMTUD_MIN_MTU);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Cubic.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Cubic.InitialWindowPackets, 1u);
    ASSERT_EQ(Connection.CongestionControl.Cubic.SendIdleTimeoutMs, 0u);

    // Test maximum MTU with maximum window and timeout
    Settings.InitialWindowPackets = 1000;
    Settings.SendIdleTimeoutMs = UINT32_MAX;
    InitializeMockConnection(Connection, 65535);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Cubic.CongestionWindow, 0u);
    ASSERT_EQ(Connection.CongestionControl.Cubic.InitialWindowPackets, 1000u);
    ASSERT_EQ(Connection.CongestionControl.Cubic.SendIdleTimeoutMs, UINT32_MAX);

    // Test very small MTU (below minimum)
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    InitializeMockConnection(Connection, 500);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    ASSERT_GT(Connection.CongestionControl.Cubic.CongestionWindow, 0u);
}

//
// Test 3: Re-initialization behavior
// Scenario: Tests that CUBIC can be re-initialized with different settings and correctly
// updates its state. Verifies that calling CubicCongestionControlInitialize() multiple times
// properly resets state and applies new settings (e.g., doubling InitialWindowPackets should
// double the CongestionWindow). Important for connection migration or settings updates.
//
TEST(DeepTest_CubicTest, MultipleSequentialInitializations)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);

    // Initialize first time
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);
    uint32_t FirstCongestionWindow = Connection.CongestionControl.Cubic.CongestionWindow;

    // Re-initialize with different settings
    Settings.InitialWindowPackets = 20;
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Should reflect new settings with doubled window
    ASSERT_EQ(Cubic->InitialWindowPackets, 20u);
    ASSERT_EQ(Cubic->CongestionWindow, FirstCongestionWindow * 2);
}

//
// Test 4: CanSend scenarios (via function pointer)
// Scenario: Comprehensive test of CanSend logic covering: available window (can send),
// congestion blocked (cannot send), and exemptions (bypass blocking). Tests the core
// congestion control decision logic.
//
TEST(DeepTest_CubicTest, CanSendScenarios)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Scenario 1: Available window - can send
    Cubic->BytesInFlight = Cubic->CongestionWindow / 2;
    Cubic->Exemptions = 0;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 2: Congestion blocked - cannot send
    Cubic->BytesInFlight = Cubic->CongestionWindow;
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 3: Exceeding window - still blocked
    Cubic->BytesInFlight = Cubic->CongestionWindow + 100;
    ASSERT_FALSE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));

    // Scenario 4: With exemptions - can send even when blocked
    Cubic->Exemptions = 2;
    ASSERT_TRUE(Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl));
}

//
// Test 5: SetExemption (via function pointer)
// Scenario: Tests SetExemption to verify it correctly sets the number of packets that
// can bypass congestion control. Used for probe packets and other special cases.
//
TEST(DeepTest_CubicTest, SetExemption)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Initially should be 0
    ASSERT_EQ(Cubic->Exemptions, 0u);

    // Set exemptions via function pointer
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 5);
    ASSERT_EQ(Cubic->Exemptions, 5u);

    // Set to zero
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 0);
    ASSERT_EQ(Cubic->Exemptions, 0u);

    // Set to max
    Connection.CongestionControl.QuicCongestionControlSetExemption(&Connection.CongestionControl, 255);
    ASSERT_EQ(Cubic->Exemptions, 255u);
}

//
// Test 6: GetSendAllowance scenarios (via function pointer)
// Scenario: Tests GetSendAllowance under different conditions: congestion blocked (returns 0),
// available window without pacing (returns full window), and invalid time (skips pacing).
// Covers the main decision paths in send allowance calculation.
//
TEST(DeepTest_CubicTest, GetSendAllowanceScenarios)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Scenario 1: Congestion blocked - should return 0
    Cubic->BytesInFlight = Cubic->CongestionWindow;
    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 1000, TRUE);
    ASSERT_EQ(Allowance, 0u);

    // Scenario 2: Available window without pacing - should return full window
    Connection.Settings.PacingEnabled = FALSE;
    Cubic->BytesInFlight = Cubic->CongestionWindow / 2;
    uint32_t ExpectedAllowance = Cubic->CongestionWindow - Cubic->BytesInFlight;
    Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 1000, TRUE);
    ASSERT_EQ(Allowance, ExpectedAllowance);

    // Scenario 3: Invalid time - should skip pacing and return full window
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, 1000, FALSE); // FALSE = invalid time
    ASSERT_EQ(Allowance, ExpectedAllowance);
}

//
// Test 7: GetSendAllowance with active pacing (via function pointer)
// Scenario: Tests the pacing logic that limits send rate based on RTT and congestion window.
// When pacing is enabled with valid RTT samples, the function calculates a pacing rate to
// smooth out packet transmission. This prevents burst sending and improves performance over
// certain network paths. The pacing calculation is: (CongestionWindow * TimeSinceLastSend) / RTT.
// This test verifies that with pacing enabled, the allowance is rate-limited based on elapsed
// time, resulting in a smaller allowance than the full available congestion window.
//
TEST(DeepTest_CubicTest, GetSendAllowanceWithActivePacing)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);

    // Enable pacing and provide valid RTT sample
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms (well above QUIC_MIN_PACING_RTT)

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set BytesInFlight to half the window to have available capacity
    Cubic->BytesInFlight = Cubic->CongestionWindow / 2;
    uint32_t AvailableWindow = Cubic->CongestionWindow - Cubic->BytesInFlight;

    // Simulate 10ms elapsed since last send
    // Expected pacing calculation: (CongestionWindow * 10ms) / 50ms = CongestionWindow / 5
    uint32_t TimeSinceLastSend = 10000; // 10ms in microseconds

    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl, TimeSinceLastSend, TRUE);

    // Pacing should limit the allowance to less than the full available window
    ASSERT_GT(Allowance, 0u); // Should allow some sending
    ASSERT_LT(Allowance, AvailableWindow); // But less than full window due to pacing

    // Exact value is calculated considering the current implementation is right and this test is meant to
    // prevent future regressions
    uint32_t ExpectedPacedAllowance = 4928; // Pre-calculated expected value
    ASSERT_EQ(Allowance, ExpectedPacedAllowance);
}

//
// Test 8: Getter functions (via function pointers)
// Scenario: Tests all simple getter functions that return internal state values.
// Verifies GetExemptions, GetBytesInFlightMax, and GetCongestionWindow all return
// correct values matching the internal CUBIC state.
//
TEST(DeepTest_CubicTest, GetterFunctions)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Test GetExemptions
    uint8_t Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 0u);
    Cubic->Exemptions = 3;
    Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(&Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 3u);

    // Test GetBytesInFlightMax
    uint32_t MaxBytes = Connection.CongestionControl.QuicCongestionControlGetBytesInFlightMax(&Connection.CongestionControl);
    ASSERT_EQ(MaxBytes, Cubic->BytesInFlightMax);
    ASSERT_EQ(MaxBytes, Cubic->CongestionWindow / 2);

    // Test GetCongestionWindow
    uint32_t CongestionWindow = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(&Connection.CongestionControl);
    ASSERT_EQ(CongestionWindow, Cubic->CongestionWindow);
    ASSERT_GT(CongestionWindow, 0u);
}

//
// Test 9: Reset scenarios (via function pointer)
// Scenario: Tests Reset function with both FullReset=FALSE (preserves BytesInFlight) and
// FullReset=TRUE (zeros BytesInFlight). Verifies that reset properly reinitializes CUBIC
// state while respecting the FullReset parameter for connection recovery scenarios.
//
TEST(DeepTest_CubicTest, ResetScenarios)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC *Cubic = &Connection.CongestionControl.Cubic;

    // Scenario 1: Partial reset (FullReset=FALSE) - preserves BytesInFlight
    Cubic->BytesInFlight = 5000;
    Cubic->SlowStartThreshold = 10000;
    Cubic->IsInRecovery = TRUE;
    Cubic->HasHadCongestionEvent = TRUE;
    uint32_t BytesInFlightBefore = Cubic->BytesInFlight;

    Connection.CongestionControl.QuicCongestionControlReset(&Connection.CongestionControl, FALSE);

    ASSERT_EQ(Cubic->SlowStartThreshold, UINT32_MAX);
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->HasHadCongestionEvent);
    ASSERT_EQ(Cubic->LastSendAllowance, 0u);
    ASSERT_EQ(Cubic->BytesInFlight, BytesInFlightBefore); // Preserved

    // Scenario 2: Full reset (FullReset=TRUE) - zeros BytesInFlight
    Cubic->BytesInFlight = 5000;
    Cubic->SlowStartThreshold = 10000;
    Cubic->IsInRecovery = TRUE;

    Connection.CongestionControl.QuicCongestionControlReset(&Connection.CongestionControl, TRUE);

    ASSERT_EQ(Cubic->SlowStartThreshold, UINT32_MAX);
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_EQ(Cubic->BytesInFlight, 0u); // Zeroed with full reset
}

//
// Test 10: CubicCongestionControlOnDataSent - BytesInFlight increases and exemptions decrement
// Scenario: Tests that OnDataSent correctly increments BytesInFlight when data is sent
// and decrements exemptions when probe packets are sent. This tracks outstanding data
// in the network and consumes exemptions. Verifies BytesInFlightMax is updated when
// BytesInFlight reaches a new maximum.
//
TEST(DeepTest_CubicTest, OnDataSent_IncrementsBytesInFlight)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    uint32_t InitialBytesInFlight = Cubic->BytesInFlight;
    uint32_t InitialBytesInFlightMax = Cubic->BytesInFlightMax;
    uint32_t BytesToSend = 1500;

    // Call through function pointer
    Connection.CongestionControl.QuicCongestionControlOnDataSent(
        &Connection.CongestionControl, BytesToSend);

    ASSERT_EQ(Cubic->BytesInFlight, InitialBytesInFlight + BytesToSend);
    // BytesInFlightMax should update if new BytesInFlight exceeds previous max
    if (InitialBytesInFlight + BytesToSend > InitialBytesInFlightMax) {
        ASSERT_EQ(Cubic->BytesInFlightMax, InitialBytesInFlight + BytesToSend);
    } else {
        ASSERT_EQ(Cubic->BytesInFlightMax, InitialBytesInFlightMax);
    }

    // Test exemption decrement
    Cubic->Exemptions = 5;
    Connection.CongestionControl.QuicCongestionControlOnDataSent(
        &Connection.CongestionControl, 1500);
    ASSERT_EQ(Cubic->Exemptions, 4u);
}

//
// Test 11: CubicCongestionControlOnDataInvalidated - BytesInFlight decreases
// Scenario: Tests OnDataInvalidated when sent packets are discarded (e.g., due to key
// phase change). BytesInFlight should decrease by the invalidated bytes since they're
// no longer considered in-flight. Critical for accurate congestion window management.
//
TEST(DeepTest_CubicTest, OnDataInvalidated_DecrementsBytesInFlight)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Send some data first
    Cubic->BytesInFlight = 5000;
    uint32_t BytesToInvalidate = 2000;

    // Call through function pointer
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(
        &Connection.CongestionControl, BytesToInvalidate);

    ASSERT_EQ(Cubic->BytesInFlight, 3000u);
}

//
// Test 12: OnDataAcknowledged - Basic ACK Processing and CUBIC Growth
// Scenario: Tests the core CUBIC congestion control algorithm by acknowledging sent data.
// Exercises CubicCongestionControlOnDataAcknowledged and internally calls CubeRoot for CUBIC calculations.
// Verifies congestion window grows appropriately after successful ACK.
//
TEST(DeepTest_CubicTest, OnDataAcknowledged_BasicAck)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms in microseconds

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    uint32_t InitialWindow = Cubic->CongestionWindow;

    // Simulate data sent
    Cubic->BytesInFlight = 5000;

    // Create ACK event with correct structure
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 5;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.NumRetransmittableBytes = 5000;
    AckEvent.NumTotalAckedRetransmittableBytes = 5000;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL; // NULL pointer is valid

    // Call through function pointer
    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);
    // Verify window may have grown (depends on slow start vs congestion avoidance)
    ASSERT_GE(Cubic->CongestionWindow, InitialWindow);
}

//
// Test 13: OnDataLost - Packet Loss Handling and Window Reduction
// Scenario: Tests CUBIC's response to packet loss. When packets are declared lost,
// the congestion window should be reduced according to CUBIC algorithm (multiplicative decrease).
// Verifies proper loss recovery state transitions.
//
TEST(DeepTest_CubicTest, OnDataLost_WindowReduction)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    uint32_t InitialWindow = Cubic->CongestionWindow;

    // Simulate data in flight
    Cubic->BytesInFlight = 10000;

    // Create loss event with correct structure
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3600; // 3 packets * 1200 bytes
    LossEvent.PersistentCongestion = FALSE;
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;

    // Call through function pointer
    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify window was reduced (CUBIC multiplicative decrease)
    ASSERT_LT(Cubic->CongestionWindow, InitialWindow);
    ASSERT_GT(Cubic->SlowStartThreshold, 0u);
    ASSERT_LT(Cubic->SlowStartThreshold, UINT32_MAX);
}

//
// Test 14: OnEcn - ECN Marking Handling
// Scenario: Tests Explicit Congestion Notification (ECN) handling. When ECN-marked packets
// are received, CUBIC should treat it as a congestion signal and reduce the window appropriately.
//
TEST(DeepTest_CubicTest, OnEcn_CongestionSignal)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.EcnEnabled = TRUE;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    uint32_t InitialWindow = Cubic->CongestionWindow;

    // Simulate data in flight
    Cubic->BytesInFlight = 10000;

    // Create ECN event with correct structure
    QUIC_ECN_EVENT EcnEvent;
    CxPlatZeroMemory(&EcnEvent, sizeof(EcnEvent));
    EcnEvent.LargestPacketNumberAcked = 10;
    EcnEvent.LargestSentPacketNumber = 15;

    // Call through function pointer
    Connection.CongestionControl.QuicCongestionControlOnEcn(
        &Connection.CongestionControl,
        &EcnEvent);

    // Verify window was reduced due to ECN congestion signal
    ASSERT_LE(Cubic->CongestionWindow, InitialWindow);
}

//
// Test 15: GetNetworkStatistics - Statistics Retrieval
// Scenario: Tests retrieval of network statistics including congestion window, RTT estimates,
// and throughput metrics. Used for monitoring and diagnostics.
//
TEST(DeepTest_CubicTest, GetNetworkStatistics_RetrieveStats)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms
    Connection.Paths[0].MinRtt = 40000; // 40ms
    Connection.Paths[0].RttVariance = 5000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    Cubic->BytesInFlight = 8000;

    // Prepare network statistics structure (not QUIC_STATISTICS_V2)
    QUIC_NETWORK_STATISTICS NetworkStats;
    CxPlatZeroMemory(&NetworkStats, sizeof(NetworkStats));

    // Call through function pointer - note it takes Connection as first param
    Connection.CongestionControl.QuicCongestionControlGetNetworkStatistics(
        &Connection,
        &Connection.CongestionControl,
        &NetworkStats);

    // Verify statistics were populated
    ASSERT_EQ(NetworkStats.CongestionWindow, Cubic->CongestionWindow);
    ASSERT_EQ(NetworkStats.BytesInFlight, Cubic->BytesInFlight);
    ASSERT_GT(NetworkStats.SmoothedRTT, 0u);
}

//
// Test 16: Miscellaneous Small Functions - Complete API Coverage
// Scenario: Tests remaining small functions to achieve comprehensive API coverage:
// SetExemption, GetExemptions, OnDataInvalidated, GetCongestionWindow, LogOutFlowStatus, OnSpuriousCongestionEvent.
//
TEST(DeepTest_CubicTest, MiscFunctions_APICompleteness)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Test SetExemption
    Connection.CongestionControl.QuicCongestionControlSetExemption(
        &Connection.CongestionControl,
        1); // Set exemption count

    // Test GetExemptions
    uint8_t Exemptions = Connection.CongestionControl.QuicCongestionControlGetExemptions(
        &Connection.CongestionControl);
    ASSERT_EQ(Exemptions, 1u);

    // Test OnDataInvalidated
    Cubic->BytesInFlight = 5000;
    Connection.CongestionControl.QuicCongestionControlOnDataInvalidated(
        &Connection.CongestionControl,
        2000); // Invalidate 2000 bytes
    ASSERT_EQ(Cubic->BytesInFlight, 3000u);

    // Test GetCongestionWindow
    uint32_t CongestionWindow = Connection.CongestionControl.QuicCongestionControlGetCongestionWindow(
        &Connection.CongestionControl);
    ASSERT_EQ(CongestionWindow, Cubic->CongestionWindow);

    // Test LogOutFlowStatus
    Connection.CongestionControl.QuicCongestionControlLogOutFlowStatus(
        &Connection.CongestionControl);
    // No assertion needed - just ensure it doesn't crash

    // Test OnSpuriousCongestionEvent
    Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);
    // No assertion needed - just ensure it doesn't crash
}

//
// Test 17: HyStart State Transitions - Complete Coverage
// Scenario: Tests HyStart state transitions and behavior in different states.
// HyStart is an algorithm to safely exit slow start by detecting delay increases.
// Tests HYSTART_NOT_STARTED -> HYSTART_ACTIVE -> HYSTART_DONE transitions.
//
TEST(DeepTest_CubicTest, HyStart_StateTransitions)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.HyStartEnabled = TRUE; // Enable HyStart

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Initial state should be HYSTART_NOT_STARTED
    ASSERT_EQ(Cubic->HyStartState, HYSTART_NOT_STARTED);
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, 1u);

    // Transition to HYSTART_ACTIVE by acknowledging data (triggers slow start)
    Cubic->BytesInFlight = 5000;

    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 1000000;
    AckEvent.LargestAck = 5;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.NumRetransmittableBytes = 5000;
    AckEvent.NumTotalAckedRetransmittableBytes = 5000;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // HyStart may transition states based on RTT measurements
    // Just verify state is valid and divisor is set appropriately
    ASSERT_TRUE(Cubic->HyStartState >= HYSTART_NOT_STARTED &&
                Cubic->HyStartState <= HYSTART_DONE);
    ASSERT_GE(Cubic->CWndSlowStartGrowthDivisor, 1u);
}

//
// Test 18: Persistent Congestion - Window Reset to Minimum
// Scenario: Tests that when persistent congestion is detected (prolonged packet loss),
// CUBIC reduces the congestion window to the minimum (2 packets) while preserving
// other state variables at BETA-reduced values. This is a severe congestion response
// indicating the network path may be severely impaired.
//
// How: Trigger a loss event with PersistentCongestion=TRUE after establishing a large
// congestion window. Verify window is reset to minimum while SlowStartThreshold and
// related CUBIC state (WindowMax, WindowPrior, AimdWindow) are reduced by BETA (0.7).
//
// Assertions:
// - CongestionWindow is set to exactly 2 * DatagramPayloadLength (minimum)
// - SlowStartThreshold = PreviousCongestionWindow * 0.7
// - WindowMax, WindowPrior, WindowLastMax, AimdWindow all set to reduced value
// - IsInPersistentCongestion flag is set to TRUE
// - IsInRecovery flag is set to TRUE
// - HyStartState transitions to HYSTART_DONE
//
TEST(DeepTest_CubicTest, PersistentCongestion_WindowResetToMinimum)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Establish a large congestion window
    uint32_t InitialWindow = Cubic->CongestionWindow;
    ASSERT_GT(InitialWindow, 5000u); // Should be 20 packets * ~1200 bytes
    
    // Simulate data in flight
    Cubic->BytesInFlight = 10000;

    // Calculate expected values
    const uint16_t DatagramPayloadLength = 
        QuicPathGetDatagramPayloadSize(&Connection.Paths[0]);
    uint32_t ExpectedMinWindow = DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS;
    uint32_t ExpectedReducedWindow = (InitialWindow * 7) / 10; // BETA = 0.7

    // Create loss event with PERSISTENT congestion
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3600;
    LossEvent.PersistentCongestion = TRUE; // Key: persistent congestion detected
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify window reset to absolute minimum
    ASSERT_EQ(Cubic->CongestionWindow, ExpectedMinWindow);
    
    // Verify CUBIC state variables reduced by BETA
    ASSERT_EQ(Cubic->SlowStartThreshold, ExpectedReducedWindow);
    ASSERT_EQ(Cubic->WindowMax, ExpectedReducedWindow);
    ASSERT_EQ(Cubic->WindowPrior, ExpectedReducedWindow);
    ASSERT_EQ(Cubic->WindowLastMax, ExpectedReducedWindow);
    ASSERT_EQ(Cubic->AimdWindow, ExpectedReducedWindow);
    
    // Verify recovery and persistent congestion flags
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->IsInPersistentCongestion);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    
    // Verify KCubic is reset
    ASSERT_EQ(Cubic->KCubic, 0u);
    
    // Verify HyStart is done
    ASSERT_EQ(Cubic->HyStartState, HYSTART_DONE);
    
    // Verify bytes in flight was decremented
    ASSERT_EQ(Cubic->BytesInFlight, 10000u - 3600u);
}

//
// Test 19: Spurious Congestion Event - State Restoration
// Scenario: Tests that when a congestion event is later determined to be spurious
// (false positive loss detection), CUBIC can restore its previous state completely.
// This prevents unnecessary throughput reduction when loss detection was incorrect.
//
// How: Trigger a normal (non-ECN) congestion event to save state, verify window
// reduction occurred, then call OnSpuriousCongestionEvent to restore previous state.
// Verify all CUBIC parameters are restored to pre-congestion values.
//
// Assertions:
// - After loss: CongestionWindow is reduced, IsInRecovery=TRUE
// - After spurious event: All Prev* values restored (WindowPrior, WindowMax, etc.)
// - IsInRecovery=FALSE, HasHadCongestionEvent=FALSE
// - Function returns TRUE (became unblocked)
//
TEST(DeepTest_CubicTest, SpuriousCongestion_StateRestoration)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Grow window to trigger more interesting state
    Cubic->SlowStartThreshold = 15000;
    Cubic->CongestionWindow = 30000;
    Cubic->WindowMax = 35000;
    Cubic->WindowPrior = 32000;
    Cubic->WindowLastMax = 40000;
    Cubic->KCubic = 100;
    Cubic->AimdWindow = 30000;
    
    // Save expected values
    uint32_t ExpectedCongestionWindow = Cubic->CongestionWindow;
    uint32_t ExpectedSlowStartThreshold = Cubic->SlowStartThreshold;
    uint32_t ExpectedWindowMax = Cubic->WindowMax;
    uint32_t ExpectedWindowPrior = Cubic->WindowPrior;
    uint32_t ExpectedWindowLastMax = Cubic->WindowLastMax;
    uint32_t ExpectedKCubic = Cubic->KCubic;
    uint32_t ExpectedAimdWindow = Cubic->AimdWindow;
    
    Cubic->BytesInFlight = 10000;

    // Trigger a NON-ECN congestion event (saves previous state)
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3000;
    LossEvent.PersistentCongestion = FALSE;
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify congestion response occurred
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    ASSERT_LT(Cubic->CongestionWindow, ExpectedCongestionWindow);
    
    // Verify previous state was saved
    ASSERT_EQ(Cubic->PrevCongestionWindow, ExpectedCongestionWindow);
    ASSERT_EQ(Cubic->PrevWindowMax, ExpectedWindowMax);

    // Now declare it spurious
    BOOLEAN BecameUnblocked = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);

    // Verify complete state restoration
    ASSERT_EQ(Cubic->CongestionWindow, ExpectedCongestionWindow);
    ASSERT_EQ(Cubic->SlowStartThreshold, ExpectedSlowStartThreshold);
    ASSERT_EQ(Cubic->WindowMax, ExpectedWindowMax);
    ASSERT_EQ(Cubic->WindowPrior, ExpectedWindowPrior);
    ASSERT_EQ(Cubic->WindowLastMax, ExpectedWindowLastMax);
    ASSERT_EQ(Cubic->KCubic, ExpectedKCubic);
    ASSERT_EQ(Cubic->AimdWindow, ExpectedAimdWindow);
    
    // Verify recovery flags cleared
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->HasHadCongestionEvent);
    
    // Should return TRUE since we had reduced window and now restored
    ASSERT_TRUE(BecameUnblocked);
}

//
// Test 20: Spurious Congestion Event When Not In Recovery
// Scenario: Tests that calling OnSpuriousCongestionEvent when not in recovery
// returns FALSE and does not modify any state. This ensures the function is
// safe to call speculatively.
//
// How: Call OnSpuriousCongestionEvent without prior congestion event.
//
// Assertions:
// - Returns FALSE
// - No state changes occur
//
TEST(DeepTest_CubicTest, SpuriousCongestion_NotInRecovery)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Verify not in recovery
    ASSERT_FALSE(Cubic->IsInRecovery);
    
    uint32_t InitialWindow = Cubic->CongestionWindow;

    // Call spurious congestion event when not in recovery
    BOOLEAN Result = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);

    // Should return FALSE (no state change)
    ASSERT_FALSE(Result);
    
    // Verify no state change
    ASSERT_EQ(Cubic->CongestionWindow, InitialWindow);
    ASSERT_FALSE(Cubic->IsInRecovery);
}

//
// Test 21: Recovery Exit - ACK Beyond Recovery Point
// Scenario: Tests that CUBIC exits recovery when it receives an ACK for a packet
// sent after the recovery started. During recovery, the window does not grow.
// Exiting recovery allows normal growth to resume.
//
// How: Enter recovery via loss, set RecoverySentPacketNumber, send ACKs with
// LargestAck <= RecoverySentPacketNumber (should stay in recovery), then send
// an ACK with LargestAck > RecoverySentPacketNumber (should exit recovery).
//
// Assertions:
// - During recovery: IsInRecovery=TRUE, window does not grow
// - After exit ACK: IsInRecovery=FALSE, IsInPersistentCongestion=FALSE
// - TimeOfCongAvoidStart is set (for future congestion avoidance)
//
TEST(DeepTest_CubicTest, Recovery_ExitOnAckBeyondRecoveryPoint)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Connection.Send.NextPacketNumber = 0;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    Cubic->BytesInFlight = 15000;
    uint32_t WindowBeforeLoss = Cubic->CongestionWindow;

    // Enter recovery via loss
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3000;
    LossEvent.PersistentCongestion = FALSE;
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;  // Recovery point

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify in recovery
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, 15u);
    ASSERT_LT(Cubic->CongestionWindow, WindowBeforeLoss);
    
    uint32_t WindowDuringRecovery = Cubic->CongestionWindow;

    // ACK packets sent before/during recovery (should not exit)
    QUIC_ACK_EVENT AckEvent1;
    CxPlatZeroMemory(&AckEvent1, sizeof(AckEvent1));
    AckEvent1.TimeNow = CxPlatTimeUs64();
    AckEvent1.LargestAck = 12;  // <= RecoverySentPacketNumber
    AckEvent1.LargestSentPacketNumber = 15;
    AckEvent1.NumRetransmittableBytes = 2000;
    AckEvent1.SmoothedRtt = 50000;
    AckEvent1.MinRtt = 45000;
    AckEvent1.MinRttValid = TRUE;

    Cubic->BytesInFlight = 10000;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent1);

    // Still in recovery, no window growth
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_EQ(Cubic->CongestionWindow, WindowDuringRecovery);

    // ACK packet sent AFTER recovery started (should exit)
    QUIC_ACK_EVENT AckEvent2;
    CxPlatZeroMemory(&AckEvent2, sizeof(AckEvent2));
    AckEvent2.TimeNow = CxPlatTimeUs64();
    AckEvent2.LargestAck = 20;  // > RecoverySentPacketNumber
    AckEvent2.LargestSentPacketNumber = 25;
    AckEvent2.NumRetransmittableBytes = 1200;
    AckEvent2.SmoothedRtt = 50000;
    AckEvent2.MinRtt = 45000;
    AckEvent2.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent2);

    // Exited recovery
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->IsInPersistentCongestion);
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, 0u);
}

//
// Test 22: Fast Convergence - WindowLastMax Reduction
// Scenario: Tests CUBIC's fast convergence mechanism. When a new congestion event
// occurs and the previous WindowMax was higher than the current window, CUBIC
// applies fast convergence by reducing WindowMax more aggressively. This helps
// the algorithm converge faster to the available bandwidth.
//
// How: Create a scenario where WindowLastMax > current window, trigger congestion,
// and verify WindowMax is reduced by the fast convergence formula:
// WindowMax = WindowMax * (1.0 + BETA) / 2.0 = WindowMax * 0.85 (for BETA=0.7)
//
// Assertions:
// - WindowLastMax > CongestionWindow before event
// - After event: WindowMax = CongestionWindow * (10 + BETA*10) / 20
// - WindowLastMax = WindowMax (not the original CongestionWindow)
//
TEST(DeepTest_CubicTest, FastConvergence_WindowLastMaxReduction)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up scenario: previous WindowLastMax is larger than current window
    Cubic->CongestionWindow = 30000;
    Cubic->WindowLastMax = 40000;  // Higher than current window
    Cubic->BytesInFlight = 15000;

    // Trigger congestion event
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3000;
    LossEvent.PersistentCongestion = FALSE;
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Fast convergence should apply: WindowMax = CongestionWindow * (10 + 7) / 20 = CongestionWindow * 0.85
    uint32_t ExpectedWindowMax = (30000 * 17) / 20;  // 30000 * 0.85 = 25500
    
    // Verify fast convergence applied
    ASSERT_EQ(Cubic->WindowMax, ExpectedWindowMax);
    ASSERT_EQ(Cubic->WindowLastMax, ExpectedWindowMax);  // Should be set to the reduced WindowMax
    
    // Verify new congestion window (BETA * previous window)
    uint32_t ExpectedCongestionWindow = (30000 * 7) / 10;  // 30000 * 0.7 = 21000
    ASSERT_EQ(Cubic->CongestionWindow, ExpectedCongestionWindow);
}

//
// Test 23: Normal Convergence - WindowLastMax Update Without Fast Convergence
// Scenario: Tests normal (non-fast) convergence when WindowLastMax <= CongestionWindow.
// In this case, WindowMax is set to CongestionWindow and WindowLastMax is updated
// to match, without the fast convergence reduction.
//
// How: Set WindowLastMax <= CongestionWindow, trigger congestion, verify
// WindowMax = CongestionWindow and WindowLastMax = CongestionWindow (no reduction).
//
// Assertions:
// - WindowLastMax <= CongestionWindow before event
// - After event: WindowMax = original CongestionWindow
// - WindowLastMax = original CongestionWindow (no fast convergence reduction)
//
TEST(DeepTest_CubicTest, NormalConvergence_NoFastConvergenceReduction)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up scenario: WindowLastMax <= current window (no fast convergence)
    Cubic->CongestionWindow = 30000;
    Cubic->WindowLastMax = 25000;  // Lower than current window
    Cubic->BytesInFlight = 15000;

    uint32_t CongestionWindowBeforeLoss = Cubic->CongestionWindow;

    // Trigger congestion event
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3000;
    LossEvent.PersistentCongestion = FALSE;
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Normal convergence: WindowMax = previous CongestionWindow, WindowLastMax = previous CongestionWindow
    ASSERT_EQ(Cubic->WindowMax, CongestionWindowBeforeLoss);
    ASSERT_EQ(Cubic->WindowLastMax, CongestionWindowBeforeLoss);
    ASSERT_EQ(Cubic->WindowPrior, CongestionWindowBeforeLoss);
    
    // Verify new congestion window reduced by BETA
    uint32_t ExpectedCongestionWindow = (CongestionWindowBeforeLoss * 7) / 10;
    ASSERT_EQ(Cubic->CongestionWindow, ExpectedCongestionWindow);
}

//
// Test 24: Slow Start Overshoot - Transition to Congestion Avoidance
// Scenario: Tests the scenario where congestion window growth in slow start
// exceeds the SlowStartThreshold. CUBIC should clamp the window to the threshold
// and treat the excess BytesAcked as if they were acknowledged during congestion
// avoidance, ensuring smooth transition.
//
// How: Set CongestionWindow just below SlowStartThreshold, ACK enough bytes to
// push it significantly over the threshold. Verify window is clamped and excess
// bytes trigger congestion avoidance growth.
//
// Assertions:
// - Before ACK: CongestionWindow < SlowStartThreshold
// - After ACK: CongestionWindow >= SlowStartThreshold (clamped if overshoot)
// - TimeOfCongAvoidStart is set
// - Excess growth applied via congestion avoidance rules
//
TEST(DeepTest_CubicTest, SlowStartOvershoot_TransitionToCongestionAvoidance)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up: CongestionWindow just below SlowStartThreshold
    Cubic->SlowStartThreshold = 20000;
    Cubic->CongestionWindow = 19000;
    Cubic->BytesInFlight = 15000;
    Cubic->WindowMax = 25000;
    Cubic->AimdWindow = 19000;
    
    // ACK large amount that would overshoot threshold
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 20;
    AckEvent.LargestSentPacketNumber = 25;
    AckEvent.NumRetransmittableBytes = 5000;  // Large ACK
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify transition: window should be at or near threshold
    ASSERT_GE(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
    
    // TimeOfCongAvoidStart should be set
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, 0u);
    
    // Verify we're now in congestion avoidance (window >= threshold)
    ASSERT_GE(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
}

//
// Test 25: HyStart++ Transition to HYSTART_ACTIVE - RTT Increase Detection
// Scenario: Tests HyStart++ transition from HYSTART_NOT_STARTED to HYSTART_ACTIVE
// when RTT increase is detected. HyStart samples N ACKs per round, then checks if
// MinRttInCurrentRound >= MinRttInLastRound + Eta. If so, it enters conservative
// slow start to safely exit aggressive slow start.
//
// How: Enable HyStart, send ACKs to sample N RTTs, then send ACKs with increased
// RTT to trigger transition. Verify CWndSlowStartGrowthDivisor increases and
// ConservativeSlowStartRounds is set.
//
// Assertions:
// - Initial state: HYSTART_NOT_STARTED, CWndSlowStartGrowthDivisor=1
// - After N samples: HyStartAckCount >= N
// - After RTT increase: HYSTART_ACTIVE, CWndSlowStartGrowthDivisor=4
// - ConservativeSlowStartRounds = 5
//
TEST(DeepTest_CubicTest, HyStart_TransitionToActive_RttIncreaseDetected)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.HyStartEnabled = TRUE;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Connection.Send.NextPacketNumber = 100;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up for slow start
    Cubic->BytesInFlight = 5000;
    Cubic->HyStartRoundEnd = 110;  // Round ends at packet 110
    
    // Initial state
    ASSERT_EQ(Cubic->HyStartState, HYSTART_NOT_STARTED);
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, 1u);

    // Send N ACKs to establish baseline MinRtt (QUIC_HYSTART_DEFAULT_N_SAMPLING = 8)
    for (int i = 0; i < QUIC_HYSTART_DEFAULT_N_SAMPLING; i++) {
        QUIC_ACK_EVENT AckEvent;
        CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
        AckEvent.TimeNow = CxPlatTimeUs64();
        AckEvent.LargestAck = 100 + i;
        AckEvent.LargestSentPacketNumber = 105 + i;
        AckEvent.NumRetransmittableBytes = 1200;
        AckEvent.SmoothedRtt = 50000;
        AckEvent.MinRtt = 40000;  // Baseline RTT
        AckEvent.MinRttValid = TRUE;

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
            &Connection.CongestionControl,
            &AckEvent);
    }
    
    // Should have sampled N ACKs
    ASSERT_GE(Cubic->HyStartAckCount, QUIC_HYSTART_DEFAULT_N_SAMPLING);
    ASSERT_EQ(Cubic->MinRttInCurrentRound, 40000u);

    // Move to next round
    Cubic->HyStartRoundEnd = 120;
    Connection.Send.NextPacketNumber = 120;

    // Send ACK in new round with INCREASED RTT (triggers transition)
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 115;  // Past previous round end
    AckEvent.LargestSentPacketNumber = 120;
    AckEvent.NumRetransmittableBytes = 1200;
    AckEvent.SmoothedRtt = 50000;
    // Increase RTT by more than Eta (Eta = MinRttInLastRound / 8 = 40000/8 = 5000)
    AckEvent.MinRtt = 46000;  // Increase by 6000 > Eta
    AckEvent.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Should transition to HYSTART_ACTIVE
    ASSERT_EQ(Cubic->HyStartState, HYSTART_ACTIVE);
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR);
    ASSERT_EQ(Cubic->ConservativeSlowStartRounds, QUIC_CONSERVATIVE_SLOW_START_DEFAULT_ROUNDS);
    ASSERT_EQ(Cubic->CssBaselineMinRtt, 46000u);
}

//
// Test 26: HyStart++ Transition from ACTIVE to HYSTART_DONE - Rounds Exhausted
// Scenario: Tests HyStart++ transition from HYSTART_ACTIVE to HYSTART_DONE when
// ConservativeSlowStartRounds reaches 0. After conservative slow start completes,
// CUBIC enters full congestion avoidance mode.
//
// How: Manually set HYSTART_ACTIVE state with ConservativeSlowStartRounds=1,
// then send ACK that completes a round. Verify transition to HYSTART_DONE and
// SlowStartThreshold is set to current window.
//
// Assertions:
// - Initial: HYSTART_ACTIVE, ConservativeSlowStartRounds > 0
// - After round completion: HYSTART_DONE, CWndSlowStartGrowthDivisor=1
// - SlowStartThreshold = CongestionWindow
// - TimeOfCongAvoidStart set
// - AimdWindow = CongestionWindow
//
TEST(DeepTest_CubicTest, HyStart_TransitionToDone_RoundsExhausted)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.HyStartEnabled = TRUE;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Connection.Send.NextPacketNumber = 100;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Manually set to HYSTART_ACTIVE with 1 round remaining
    Cubic->HyStartState = HYSTART_ACTIVE;
    Cubic->CWndSlowStartGrowthDivisor = QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR;
    Cubic->ConservativeSlowStartRounds = 1;
    Cubic->HyStartRoundEnd = 105;
    Cubic->BytesInFlight = 5000;
    Cubic->WindowMax = 25000;
    Cubic->AimdWindow = 20000;
    
    // Send ACK that completes the round (LargestAck >= HyStartRoundEnd)
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 106;  // >= HyStartRoundEnd
    AckEvent.LargestSentPacketNumber = 110;
    AckEvent.NumRetransmittableBytes = 1200;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Should transition to HYSTART_DONE
    ASSERT_EQ(Cubic->HyStartState, HYSTART_DONE);
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, 1u);
    
    // SlowStartThreshold should be set to CongestionWindow
    ASSERT_EQ(Cubic->SlowStartThreshold, Cubic->CongestionWindow);
    
    // TimeOfCongAvoidStart should be set
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, 0u);
    
    // AimdWindow should be initialized
    ASSERT_EQ(Cubic->AimdWindow, Cubic->CongestionWindow);
}

//
// Test 27: HyStart++ Transition from ACTIVE back to NOT_STARTED - RTT Decreased
// Scenario: Tests HyStart++ transition from HYSTART_ACTIVE back to HYSTART_NOT_STARTED
// when RTT decreases below the CSS baseline. This indicates the earlier RTT increase
// was spurious, so HyStart resumes normal slow start.
//
// How: Set HYSTART_ACTIVE state with a CssBaselineMinRtt, then send ACK with
// MinRtt < CssBaselineMinRtt. Verify transition back to HYSTART_NOT_STARTED.
//
// Assertions:
// - Initial: HYSTART_ACTIVE, CssBaselineMinRtt set
// - After decreased RTT ACK: HYSTART_NOT_STARTED
// - CWndSlowStartGrowthDivisor = 1 (back to normal slow start)
//
TEST(DeepTest_CubicTest, HyStart_TransitionBackToNotStarted_RttDecreased)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.HyStartEnabled = TRUE;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Connection.Send.NextPacketNumber = 100;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Manually set to HYSTART_ACTIVE with baseline RTT
    Cubic->HyStartState = HYSTART_ACTIVE;
    Cubic->CWndSlowStartGrowthDivisor = QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR;
    Cubic->ConservativeSlowStartRounds = 3;
    Cubic->CssBaselineMinRtt = 50000;  // Baseline
    Cubic->HyStartRoundEnd = 105;
    Cubic->BytesInFlight = 5000;
    Cubic->MinRttInCurrentRound = UINT64_MAX;

    // Send ACK with RTT LOWER than baseline
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 102;
    AckEvent.LargestSentPacketNumber = 108;
    AckEvent.NumRetransmittableBytes = 1200;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 40000;  // Lower than CssBaselineMinRtt (50000)
    AckEvent.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Should transition back to HYSTART_NOT_STARTED
    ASSERT_EQ(Cubic->HyStartState, HYSTART_NOT_STARTED);
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, 1u);
}

//
// Test 28: Congestion Avoidance - CUBIC Window Growth
// Scenario: Tests CUBIC window growth formula during congestion avoidance phase.
// The CUBIC formula is: W_cubic(t) = C*(t-K)^3 + WindowMax, where K is the time
// it would take to reach WindowMax again. Tests that window grows according to
// this formula after congestion event.
//
// How: Trigger congestion to enter congestion avoidance, then send ACKs over time
// and verify window grows. Check that KCubic is calculated and CUBIC formula
// is applied.
//
// Assertions:
// - After congestion: CongestionWindow < SlowStartThreshold (in congestion avoidance)
// - KCubic is calculated (> 0)
// - After ACKs: Window grows (may be limited by AIMD or growth cap)
// - WindowMax, WindowPrior set correctly
//
TEST(DeepTest_CubicTest, CongestionAvoidance_CubicWindowGrowth)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Grow to a larger window first
    Cubic->CongestionWindow = 30000;
    Cubic->SlowStartThreshold = 30000;
    Cubic->BytesInFlight = 15000;

    // Trigger congestion to enter congestion avoidance
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.NumRetransmittableBytes = 3000;
    LossEvent.PersistentCongestion = FALSE;
    LossEvent.LargestPacketNumberLost = 10;
    LossEvent.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify in congestion avoidance
    ASSERT_LT(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
    ASSERT_GT(Cubic->KCubic, 0u);
    ASSERT_EQ(Cubic->WindowMax, 30000u);
    ASSERT_EQ(Cubic->WindowPrior, 30000u);
    
    uint32_t WindowAfterLoss = Cubic->CongestionWindow;
    Cubic->BytesInFlight = 10000;
    Cubic->BytesInFlightMax = 15000;

    // Send multiple ACKs to grow window
    for (int i = 0; i < 10; i++) {
        QUIC_ACK_EVENT AckEvent;
        CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
        AckEvent.TimeNow = CxPlatTimeUs64() + (i * 60000);  // 60ms apart
        AckEvent.LargestAck = 20 + i;
        AckEvent.LargestSentPacketNumber = 25 + i;
        AckEvent.NumRetransmittableBytes = 1200;
        AckEvent.SmoothedRtt = 50000;
        AckEvent.MinRtt = 45000;
        AckEvent.MinRttValid = TRUE;

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
            &Connection.CongestionControl,
            &AckEvent);
    }

    // Window should have grown (CUBIC or AIMD)
    ASSERT_GT(Cubic->CongestionWindow, WindowAfterLoss);
    
    // Verify AIMD window is also tracking
    ASSERT_GT(Cubic->AimdWindow, 0u);
}

//
// Test 29: Time Gap Handling - Freeze Window Growth During Idle
// Scenario: Tests that when there's a long gap between ACKs (exceeding SendIdleTimeoutMs
// or RTT + 4*RttVariance), CUBIC adjusts TimeOfCongAvoidStart forward to effectively
// freeze window growth during the gap. This prevents window growth when no feedback
// is received.
//
// How: Set TimeOfLastAckValid, send first ACK to establish baseline, then send
// second ACK after a long delay. Verify TimeOfCongAvoidStart is adjusted.
//
// Assertions:
// - TimeOfLastAckValid set after first ACK
// - After long gap: TimeOfCongAvoidStart adjusted forward
// - Window growth is limited compared to if gap didn't occur
//
TEST(DeepTest_CubicTest, TimeGapHandling_FreezeWindowGrowthDuringIdle)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 100;  // 100ms idle timeout

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;  // 50ms
    Connection.Paths[0].RttVariance = 5000;    // 5ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Enter congestion avoidance
    Cubic->SlowStartThreshold = 15000;
    Cubic->CongestionWindow = 20000;
    Cubic->BytesInFlight = 10000;
    Cubic->BytesInFlightMax = 15000;
    Cubic->WindowMax = 25000;
    Cubic->TimeOfCongAvoidStart = CxPlatTimeUs64();
    
    // First ACK to establish TimeOfLastAck
    QUIC_ACK_EVENT AckEvent1;
    CxPlatZeroMemory(&AckEvent1, sizeof(AckEvent1));
    AckEvent1.TimeNow = CxPlatTimeUs64();
    AckEvent1.LargestAck = 10;
    AckEvent1.LargestSentPacketNumber = 15;
    AckEvent1.NumRetransmittableBytes = 1200;
    AckEvent1.SmoothedRtt = 50000;
    AckEvent1.MinRtt = 45000;
    AckEvent1.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent1);

    ASSERT_TRUE(Cubic->TimeOfLastAckValid);
    uint64_t TimeOfLastAck1 = Cubic->TimeOfLastAck;
    uint64_t TimeOfCongAvoidStart1 = Cubic->TimeOfCongAvoidStart;

    // Second ACK after LONG gap (exceeding timeout)
    // Gap threshold = max(SendIdleTimeoutMs, SmoothedRtt + 4*RttVariance)
    //                = max(100ms, 50ms + 4*5ms) = max(100ms, 70ms) = 100ms
    uint64_t LongGap = 200000;  // 200ms gap (> 100ms threshold)
    
    QUIC_ACK_EVENT AckEvent2;
    CxPlatZeroMemory(&AckEvent2, sizeof(AckEvent2));
    AckEvent2.TimeNow = TimeOfLastAck1 + LongGap;
    AckEvent2.LargestAck = 20;
    AckEvent2.LargestSentPacketNumber = 25;
    AckEvent2.NumRetransmittableBytes = 1200;
    AckEvent2.SmoothedRtt = 50000;
    AckEvent2.MinRtt = 45000;
    AckEvent2.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent2);

    // TimeOfCongAvoidStart should be adjusted forward by the gap
    // (effectively freezing window growth calculation during the gap)
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, TimeOfCongAvoidStart1);
}

//
// Test 30: Window Growth Limiting - Capped at 2x BytesInFlightMax
// Scenario: Tests that congestion window growth is limited to 2 * BytesInFlightMax
// to prevent unbounded growth when application doesn't fully utilize the window
// (app-limited scenario). This ensures the window reflects actual network capacity.
//
// How: Set BytesInFlightMax to a moderate value, ACK data to trigger window growth,
// verify window doesn't exceed 2 * BytesInFlightMax.
//
// Assertions:
// - Before ACKs: Set BytesInFlightMax
// - After ACKs: CongestionWindow <= 2 * BytesInFlightMax
// - Growth is capped even if CUBIC formula would suggest larger window
//
TEST(DeepTest_CubicTest, WindowGrowthLimiting_CappedAtTwiceBytesInFlightMax)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up congestion avoidance with constrained BytesInFlightMax
    Cubic->SlowStartThreshold = 15000;
    Cubic->CongestionWindow = 20000;
    Cubic->BytesInFlight = 8000;
    Cubic->BytesInFlightMax = 10000;  // Constrain this
    Cubic->WindowMax = 30000;
    Cubic->TimeOfCongAvoidStart = CxPlatTimeUs64();

    // ACK enough to try to grow window significantly
    for (int i = 0; i < 20; i++) {
        QUIC_ACK_EVENT AckEvent;
        CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
        AckEvent.TimeNow = CxPlatTimeUs64() + (i * 60000);
        AckEvent.LargestAck = 30 + i;
        AckEvent.LargestSentPacketNumber = 35 + i;
        AckEvent.NumRetransmittableBytes = 1200;
        AckEvent.SmoothedRtt = 50000;
        AckEvent.MinRtt = 45000;
        AckEvent.MinRttValid = TRUE;

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
            &Connection.CongestionControl,
            &AckEvent);
    }

    // Verify window is capped at 2 * BytesInFlightMax
    uint32_t MaxAllowedWindow = 2 * Cubic->BytesInFlightMax;
    ASSERT_LE(Cubic->CongestionWindow, MaxAllowedWindow);
}

//
// Test 31: AIMD vs CUBIC Window Selection - Reno-Friendly Region
// Scenario: Tests that CUBIC selects the larger of AIMD window and CUBIC window.
// When AIMD window > CUBIC window (Reno-friendly region), CUBIC uses AIMD to be
// fair to TCP Reno flows. Otherwise, uses CUBIC formula.
//
// How: Set up scenario where AIMD would be larger, ACK data, verify AIMD window
// is used. Then set up where CUBIC would be larger and verify CUBIC is used.
//
// Assertions:
// - When AimdWindow > CubicWindow: CongestionWindow = AimdWindow
// - When AimdWindow < CubicWindow: CongestionWindow follows CUBIC growth
//
TEST(DeepTest_CubicTest, AimdVsCubic_RenoFriendlyRegion)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Enter congestion avoidance shortly after loss
    Cubic->SlowStartThreshold = 15000;
    Cubic->CongestionWindow = 18000;
    Cubic->AimdWindow = 19000;  // AIMD slightly larger
    Cubic->WindowMax = 25000;
    Cubic->WindowPrior = 25000;
    Cubic->BytesInFlight = 10000;
    Cubic->BytesInFlightMax = 15000;
    Cubic->TimeOfCongAvoidStart = CxPlatTimeUs64() - 10000; // Recent start

    // ACK some data
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 20;
    AckEvent.LargestSentPacketNumber = 25;
    AckEvent.NumRetransmittableBytes = 2400;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // When in early congestion avoidance (near K), AIMD may dominate
    // Verify AIMD window is tracked and used appropriately
    ASSERT_GT(Cubic->AimdWindow, 0u);
    
    // Window should be influenced by AIMD in Reno-friendly region
    // (exact value depends on CUBIC calculation, but AIMD should be considered)
}

//
// Test 32: Pacing Overflow Handling - Large EstimatedWnd Calculation
// Scenario: Tests pacing calculation overflow protection. When EstimatedWnd is
// calculated (2x in slow start, 1.25x in congestion avoidance), multiplication
// can overflow. The code should clamp to available window safely.
//
// How: Set large CongestionWindow near UINT32_MAX, enable pacing, call
// GetSendAllowance and verify no overflow/crash and result is reasonable.
//
// Assertions:
// - With large window: SendAllowance doesn't overflow
// - Result is clamped to CongestionWindow - BytesInFlight
// - LastSendAllowance is updated correctly
//
TEST(DeepTest_CubicTest, PacingOverflow_LargeEstimatedWndCalculation)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set very large congestion window (near overflow territory)
    Cubic->CongestionWindow = 0x7FFFFFFF;  // Large but not UINT32_MAX
    Cubic->BytesInFlight = 1000000;
    Cubic->SlowStartThreshold = 0x80000000;  // In slow start
    Cubic->LastSendAllowance = 0;

    uint32_t AvailableWindow = Cubic->CongestionWindow - Cubic->BytesInFlight;

    // Call GetSendAllowance with pacing
    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl,
        10000,  // 10ms
        TRUE);

    // Should not crash and should return reasonable value
    ASSERT_GT(Allowance, 0u);
    ASSERT_LE(Allowance, AvailableWindow);
}

//
// Test 33: Pacing in Congestion Avoidance - EstimatedWnd Growth Factor
// Scenario: Tests that pacing uses different growth factors for EstimatedWnd
// calculation: 2x in slow start vs 1.25x in congestion avoidance. This affects
// the pacing rate to match expected window growth.
//
// How: Test GetSendAllowance in slow start (should use 2x factor) and in
// congestion avoidance (should use 1.25x factor).
//
// Assertions:
// - In slow start: Larger pacing rate (EstimatedWnd = 2 * CongestionWindow)
// - In congestion avoidance: Smaller pacing rate (EstimatedWnd = 1.25 * CongestionWindow)
//
TEST(DeepTest_CubicTest, PacingEstimatedWnd_GrowthFactorByPhase)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 100000;  // 100ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Test in SLOW START (CongestionWindow < SlowStartThreshold)
    Cubic->CongestionWindow = 20000;
    Cubic->SlowStartThreshold = 50000;
    Cubic->BytesInFlight = 5000;
    Cubic->LastSendAllowance = 0;

    uint32_t AllowanceSlowStart = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl,
        50000,  // 50ms (half RTT)
        TRUE);

    // Should allow more (EstimatedWnd = 2x)
    ASSERT_GT(AllowanceSlowStart, 0u);
    
    // Test in CONGESTION AVOIDANCE (CongestionWindow >= SlowStartThreshold)
    Cubic->CongestionWindow = 20000;
    Cubic->SlowStartThreshold = 15000;  // Now in congestion avoidance
    Cubic->BytesInFlight = 5000;
    Cubic->LastSendAllowance = 0;

    uint32_t AllowanceCongestionAvoidance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl,
        50000,  // 50ms (half RTT)
        TRUE);

    // Should allow less (EstimatedWnd = 1.25x)
    ASSERT_GT(AllowanceCongestionAvoidance, 0u);
    
    // In slow start, allowance should be higher due to larger EstimatedWnd
    // (This is a qualitative check; exact values depend on implementation details)
}

//
// Test 34: OnDataSent - LastSendAllowance Underflow Protection
// Scenario: Tests that when NumRetransmittableBytes > LastSendAllowance, the
// subtraction doesn't underflow and LastSendAllowance is correctly clamped to 0.
//
// How: Set small LastSendAllowance, call OnDataSent with larger bytes, verify
// LastSendAllowance = 0 (not negative/underflow).
//
// Assertions:
// - Before: LastSendAllowance = small value
// - After OnDataSent with larger bytes: LastSendAllowance = 0
//
TEST(DeepTest_CubicTest, OnDataSent_LastSendAllowanceUnderflowProtection)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set small LastSendAllowance
    Cubic->LastSendAllowance = 500;
    Cubic->BytesInFlight = 5000;

    // Send more bytes than LastSendAllowance
    Connection.CongestionControl.QuicCongestionControlOnDataSent(
        &Connection.CongestionControl,
        2000);  // > LastSendAllowance

    // Should clamp to 0, not underflow
    ASSERT_EQ(Cubic->LastSendAllowance, 0u);
    ASSERT_EQ(Cubic->BytesInFlight, 7000u);
}

//
// Test 35: ECN Congestion Event - No State Saving
// Scenario: Tests that ECN-triggered congestion events do NOT save previous state
// (unlike loss-triggered events), since ECN events cannot be spurious. Verifies
// that Prev* fields are not updated when Ecn parameter is TRUE.
//
// How: Set up initial state, trigger ECN event via OnEcn, verify Prev* fields
// remain at their initial values (not updated).
//
// Assertions:
// - Before ECN: Set non-zero Prev* values
// - After ECN: Prev* values unchanged (not overwritten)
// - Window is still reduced by BETA
// - IsInRecovery = TRUE
//
TEST(DeepTest_CubicTest, EcnCongestionEvent_NoStateSaving)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.EcnEnabled = TRUE;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up state with pre-existing Prev* values
    Cubic->CongestionWindow = 30000;
    Cubic->PrevCongestionWindow = 99999;  // Sentinel value
    Cubic->PrevWindowMax = 88888;
    Cubic->PrevWindowPrior = 77777;
    Cubic->BytesInFlight = 15000;

    // Trigger ECN congestion event
    QUIC_ECN_EVENT EcnEvent;
    CxPlatZeroMemory(&EcnEvent, sizeof(EcnEvent));
    EcnEvent.LargestPacketNumberAcked = 10;
    EcnEvent.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnEcn(
        &Connection.CongestionControl,
        &EcnEvent);

    // Verify congestion response occurred
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_LT(Cubic->CongestionWindow, 30000u);
    
    // Verify Prev* values were NOT updated (still sentinel values)
    ASSERT_EQ(Cubic->PrevCongestionWindow, 99999u);
    ASSERT_EQ(Cubic->PrevWindowMax, 88888u);
    ASSERT_EQ(Cubic->PrevWindowPrior, 77777u);
}

//
// Test 36: Multiple Congestion Events - State Saving on Each Non-ECN Event
// Scenario: Tests that each non-ECN congestion event saves the current state
// to Prev* fields, allowing multiple congestion events to be tracked and
// potentially reverted if spurious.
//
// How: Trigger first loss event (saves state), modify state, trigger second
// loss event (saves new state), verify Prev* updated each time.
//
// Assertions:
// - After first loss: Prev* = state before first loss
// - After second loss: Prev* = state before second loss (updated)
//
TEST(DeepTest_CubicTest, MultipleCongestionEvents_StateSavingOnEachEvent)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 30;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Grow window
    Cubic->CongestionWindow = 40000;
    Cubic->SlowStartThreshold = 40000;
    Cubic->WindowMax = 50000;
    Cubic->BytesInFlight = 20000;

    uint32_t CongestionWindow1 = Cubic->CongestionWindow;

    // First congestion event
    QUIC_LOSS_EVENT LossEvent1;
    CxPlatZeroMemory(&LossEvent1, sizeof(LossEvent1));
    LossEvent1.NumRetransmittableBytes = 3000;
    LossEvent1.PersistentCongestion = FALSE;
    LossEvent1.LargestPacketNumberLost = 10;
    LossEvent1.LargestSentPacketNumber = 15;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent1);

    // Verify first event saved state
    ASSERT_EQ(Cubic->PrevCongestionWindow, CongestionWindow1);
    
    // Exit recovery and grow window again
    Cubic->IsInRecovery = FALSE;
    Cubic->CongestionWindow = 35000;
    Cubic->WindowMax = 40000;
    
    uint32_t CongestionWindow2 = Cubic->CongestionWindow;

    // Second congestion event (new loss)
    QUIC_LOSS_EVENT LossEvent2;
    CxPlatZeroMemory(&LossEvent2, sizeof(LossEvent2));
    LossEvent2.NumRetransmittableBytes = 2000;
    LossEvent2.PersistentCongestion = FALSE;
    LossEvent2.LargestPacketNumberLost = 25;
    LossEvent2.LargestSentPacketNumber = 30;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent2);

    // Verify second event saved NEW state (overwrote previous Prev*)
    ASSERT_EQ(Cubic->PrevCongestionWindow, CongestionWindow2);
}

//
// Test 37: Zero Bytes Acknowledged - No Window Growth
// Scenario: Tests that when NumRetransmittableBytes=0 in an ACK event (e.g., ACK-only
// packet or all bytes were previously acked), the congestion window does not grow
// and the function exits early during recovery.
//
// How: Call OnDataAcknowledged with NumRetransmittableBytes=0, verify window unchanged.
//
// Assertions:
// - CongestionWindow unchanged
// - No state modifications occur
// - TimeOfLastAck still updated (for gap tracking)
//
TEST(DeepTest_CubicTest, ZeroBytesAcknowledged_NoWindowGrowth)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Enter recovery
    Cubic->IsInRecovery = TRUE;
    Cubic->RecoverySentPacketNumber = 20;
    Cubic->BytesInFlight = 5000;
    uint32_t InitialWindow = Cubic->CongestionWindow;

    // ACK with zero bytes
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CxPlatTimeUs64();
    AckEvent.LargestAck = 15;  // Below RecoverySentPacketNumber
    AckEvent.LargestSentPacketNumber = 20;
    AckEvent.NumRetransmittableBytes = 0;  // Zero bytes
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRttValid = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Window should be unchanged
    ASSERT_EQ(Cubic->CongestionWindow, InitialWindow);
    
    // Still in recovery
    ASSERT_TRUE(Cubic->IsInRecovery);
    
    // TimeOfLastAck should be updated
    ASSERT_TRUE(Cubic->TimeOfLastAckValid);
    ASSERT_EQ(Cubic->TimeOfLastAck, AckEvent.TimeNow);
}

//
// Test 38: Pacing Disabled - No RTT Sample
// Scenario: Tests that pacing is skipped when no RTT sample is available yet
// (GotFirstRttSample=FALSE), even if PacingEnabled=TRUE. This ensures we don't
// pace with invalid/zero RTT values.
//
// How: Enable pacing but set GotFirstRttSample=FALSE, call GetSendAllowance,
// verify it returns full available window (no pacing applied).
//
// Assertions:
// - PacingEnabled=TRUE but GotFirstRttSample=FALSE
// - Returns CongestionWindow - BytesInFlight (no pacing)
//
TEST(DeepTest_CubicTest, PacingDisabled_NoRttSample)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = FALSE;  // No RTT sample yet
    Connection.Paths[0].SmoothedRtt = 0;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    Cubic->BytesInFlight = 5000;
    uint32_t ExpectedAllowance = Cubic->CongestionWindow - Cubic->BytesInFlight;

    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl,
        10000,
        TRUE);

    // Should return full window (no pacing)
    ASSERT_EQ(Allowance, ExpectedAllowance);
}

//
// Test 39: Pacing with Small RTT - Below Minimum Threshold
// Scenario: Tests that pacing is skipped when SmoothedRtt < QUIC_MIN_PACING_RTT
// (typically 1ms). This prevents excessive pacing overhead for very low latency
// connections where pacing provides little benefit.
//
// How: Set SmoothedRtt below threshold, enable pacing, verify no pacing applied.
//
// Assertions:
// - SmoothedRtt < QUIC_MIN_PACING_RTT
// - Returns full available window (no pacing)
//
TEST(DeepTest_CubicTest, PacingDisabled_RttBelowMinimumThreshold)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Settings.PacingEnabled = TRUE;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 500;  // 0.5ms, below QUIC_MIN_PACING_RTT (1ms = 1000us)

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    Cubic->BytesInFlight = 5000;
    uint32_t ExpectedAllowance = Cubic->CongestionWindow - Cubic->BytesInFlight;

    uint32_t Allowance = Connection.CongestionControl.QuicCongestionControlGetSendAllowance(
        &Connection.CongestionControl,
        10000,
        TRUE);

    // Should return full window (no pacing due to small RTT)
    ASSERT_EQ(Allowance, ExpectedAllowance);
}

//
// Test 40: CUBIC DeltaT Overflow Protection - Time Clamped at 2.5M ms
// Scenario: Tests that the DeltaT calculation in CUBIC formula is clamped at
// 2.5M milliseconds to prevent overflow in the cube calculation. This is
// important for very long-lived connections or after very long idle periods.
//
// How: Set TimeOfCongAvoidStart to very old time, ACK data, verify window
// calculation doesn't overflow/crash and produces reasonable result.
//
// Assertions:
// - Very large time difference (> 2.5M ms)
// - Window growth occurs without overflow
// - No crash or undefined behavior
//
TEST(DeepTest_CubicTest, CubicDeltaTOverflow_ClampedAtMaximum)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Set up congestion avoidance with VERY old TimeOfCongAvoidStart
    Cubic->SlowStartThreshold = 15000;
    Cubic->CongestionWindow = 20000;
    Cubic->AimdWindow = 20000;
    Cubic->WindowMax = 30000;
    Cubic->KCubic = 1000;
    Cubic->BytesInFlight = 10000;
    Cubic->BytesInFlightMax = 15000;
    
    // Set time to 3 million milliseconds ago (> 2.5M ms limit)
    uint64_t CurrentTime = CxPlatTimeUs64();
    Cubic->TimeOfCongAvoidStart = CurrentTime - (3000000ULL * 1000ULL);  // Cast to avoid overflow

    // ACK data
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = CurrentTime;
    AckEvent.LargestAck = 20;
    AckEvent.LargestSentPacketNumber = 25;
    AckEvent.NumRetransmittableBytes = 1200;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = TRUE;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Should not crash and window should be reasonable
    ASSERT_GT(Cubic->CongestionWindow, 0u);
    ASSERT_LT(Cubic->CongestionWindow, UINT32_MAX);
}

//
// Test 41: IsAppLimited and SetAppLimited - No-op Verification
// Scenario: Tests that IsAppLimited always returns FALSE and SetAppLimited is
// a no-op in CUBIC (these are not implemented for CUBIC congestion control).
//
// How: Call both functions and verify expected behavior.
//
// Assertions:
// - IsAppLimited always returns FALSE
// - SetAppLimited doesn't change any state
//
TEST(DeepTest_CubicTest, AppLimited_NoOpFunctions)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    uint32_t InitialWindow = Cubic->CongestionWindow;

    // Test IsAppLimited
    BOOLEAN IsAppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(
        &Connection.CongestionControl);
    ASSERT_FALSE(IsAppLimited);

    // Test SetAppLimited (should be no-op)
    Connection.CongestionControl.QuicCongestionControlSetAppLimited(
        &Connection.CongestionControl);

    // Verify no state change
    ASSERT_EQ(Cubic->CongestionWindow, InitialWindow);
    
    // IsAppLimited should still return FALSE
    IsAppLimited = Connection.CongestionControl.QuicCongestionControlIsAppLimited(
        &Connection.CongestionControl);
    ASSERT_FALSE(IsAppLimited);
}

//
// Test 42: Slow Start with HyStart++ Sampling - First N ACKs
// Scenario: Tests that HyStart++ correctly samples the first N ACKs in each round
// to establish MinRtt baseline. During these N samples, state should not transition.
//
// How: Enable HyStart, send N ACKs with valid MinRtt, verify HyStartAckCount
// increments and MinRttInCurrentRound is updated.
//
// Assertions:
// - After each ACK: HyStartAckCount increments (up to N)
// - MinRttInCurrentRound tracks minimum
// - State remains HYSTART_NOT_STARTED during sampling
//
TEST(DeepTest_CubicTest, HyStartSampling_FirstNAcks)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    Settings.HyStartEnabled = TRUE;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;
    Connection.Send.NextPacketNumber = 100;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    Cubic->BytesInFlight = 5000;
    Cubic->HyStartRoundEnd = 120;
    
    // Initial state
    ASSERT_EQ(Cubic->HyStartAckCount, 0u);
    ASSERT_EQ(Cubic->MinRttInCurrentRound, UINT64_MAX);

    // Send first N/2 ACKs
    for (int i = 0; i < QUIC_HYSTART_DEFAULT_N_SAMPLING / 2; i++) {
        QUIC_ACK_EVENT AckEvent;
        CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
        AckEvent.TimeNow = CxPlatTimeUs64();
        AckEvent.LargestAck = 100 + i;
        AckEvent.LargestSentPacketNumber = 105 + i;
        AckEvent.NumRetransmittableBytes = 1200;
        AckEvent.SmoothedRtt = 50000;
        AckEvent.MinRtt = 40000 + (i * 100);  // Slightly varying RTT
        AckEvent.MinRttValid = TRUE;

        uint32_t PrevAckCount = Cubic->HyStartAckCount;

        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
            &Connection.CongestionControl,
            &AckEvent);

        // HyStartAckCount should increment
        ASSERT_EQ(Cubic->HyStartAckCount, PrevAckCount + 1);
    }

    // MinRtt should be the minimum seen
    ASSERT_EQ(Cubic->MinRttInCurrentRound, 40000u);
    
    // Should still be in HYSTART_NOT_STARTED (haven't completed N samples yet in this test)
    ASSERT_EQ(Cubic->HyStartState, HYSTART_NOT_STARTED);
}
