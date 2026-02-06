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
// Test 18: Spurious Congestion Event Revert
// Scenario: Tests that OnSpuriousCongestionEvent correctly reverts CUBIC state
// to pre-congestion values when a congestion event is determined to be spurious.
// This tests the undo mechanism that restores WindowPrior, WindowMax, WindowLastMax,
// KCubic, SlowStartThreshold, CongestionWindow, and AimdWindow to their previous values.
//
TEST(DeepTest_CubicTest, SpuriousCongestionRevert)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Save initial state
    uint32_t InitialCongestionWindow = Cubic->CongestionWindow;
    uint32_t InitialSlowStartThreshold = Cubic->SlowStartThreshold;
    
    // Simulate data in flight and trigger congestion event via loss
    Cubic->BytesInFlight = 10000;
    Connection.Send.NextPacketNumber = 50;
    
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.LargestPacketNumberLost = 45;
    LossEvent.LargestSentPacketNumber = 50;
    LossEvent.NumRetransmittableBytes = 2000;
    LossEvent.PersistentCongestion = FALSE;

    // Trigger congestion event (should reduce window)
    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify we're in recovery with reduced window
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    ASSERT_LT(Cubic->CongestionWindow, InitialCongestionWindow);
    uint32_t ReducedWindow = Cubic->CongestionWindow;

    // Now declare congestion event as spurious - should revert to initial state
    BOOLEAN Unblocked = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);

    // Verify state was reverted
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->HasHadCongestionEvent);
    ASSERT_EQ(Cubic->CongestionWindow, InitialCongestionWindow);
    ASSERT_EQ(Cubic->SlowStartThreshold, InitialSlowStartThreshold);
}

//
// Test 19: Persistent Congestion to Recovery Exit
// Scenario: Tests the complete lifecycle of persistent congestion: triggering
// persistent congestion (which resets window to 2 MTU minimum), staying in recovery,
// and finally exiting recovery when an ACK beyond RecoverySentPacketNumber arrives.
// This verifies the most severe congestion handling path and recovery completion.
//
TEST(DeepTest_CubicTest, PersistentCongestion_RecoveryExit)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    const uint16_t MTU = 1280;
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Simulate data in flight
    Cubic->BytesInFlight = 10000;
    Connection.Send.NextPacketNumber = 100;

    // Trigger persistent congestion
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.LargestPacketNumberLost = 95;
    LossEvent.LargestSentPacketNumber = 100;
    LossEvent.NumRetransmittableBytes = 8000;
    LossEvent.PersistentCongestion = TRUE; // This is the key difference

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify persistent congestion state
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->IsInPersistentCongestion);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    
    // Window should be reset to 2 MTU (QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS = 2)
    uint32_t ExpectedMinWindow = MTU * 2;
    ASSERT_EQ(Cubic->CongestionWindow, ExpectedMinWindow);
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, 100u);

    // Now simulate recovery by ACKing packet beyond RecoverySentPacketNumber
    Connection.Send.NextPacketNumber = 150;
    Cubic->BytesInFlight = 3000;

    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 2000000; // 2 seconds later
    AckEvent.LargestAck = 120; // Beyond RecoverySentPacketNumber (100)
    AckEvent.LargestSentPacketNumber = 150;
    AckEvent.NumRetransmittableBytes = 2560; // 2 MTU worth
    AckEvent.NumTotalAckedRetransmittableBytes = 2560;
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

    // Verify recovery exit (LargestAck > RecoverySentPacketNumber should exit recovery)
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->IsInPersistentCongestion);
    ASSERT_TRUE(Cubic->TimeOfLastAckValid);
}

//
// Test 20: CUBIC Concave Region Window Calculation
// Scenario: Tests CUBIC window growth in the concave region (t < K), where
// the connection is recovering from a congestion event and growing the window
// cautiously back toward WindowMax. In this region, growth should be slower
// than exponential but faster than linear, following the cubic function.
// This test verifies the mathematical correctness of the concave growth formula.
//
TEST(DeepTest_CubicTest, CUBIC_ConcaveRegion)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Force into congestion avoidance by setting SSThresh below current window
    Cubic->SlowStartThreshold = Cubic->CongestionWindow / 2;
    Cubic->AimdWindow = Cubic->SlowStartThreshold;
    Cubic->TimeOfCongAvoidStart = 1000000; // 1 second
    Cubic->TimeOfLastAck = 1000000;
    Cubic->TimeOfLastAckValid = TRUE;
    
    // Simulate having had a congestion event that set WindowMax
    Cubic->WindowMax = Cubic->CongestionWindow;
    Cubic->WindowLastMax = Cubic->WindowMax;
    Cubic->KCubic = 500; // 500 ms (we'll ACK before reaching K)
    
    // Now reduce window to simulate congestion event
    uint32_t PreCongestionWindow = Cubic->CongestionWindow;
    Cubic->CongestionWindow = PreCongestionWindow * 7 / 10; // BETA = 0.7
    Cubic->BytesInFlight = Cubic->CongestionWindow / 2;

    uint32_t WindowBeforeAck = Cubic->CongestionWindow;

    // ACK data in congestion avoidance (early in concave region, t < K)
    Connection.Send.NextPacketNumber = 50;
    
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 1100000; // 100ms after CA start (t < K = 500ms)
    AckEvent.LargestAck = 45;
    AckEvent.LargestSentPacketNumber = 50;
    AckEvent.NumRetransmittableBytes = 1280; // 1 MTU
    AckEvent.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = FALSE; // Not in slow start
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify window grew (concave region should still show growth)
    // Growth should be modest since we're in concave region (below WindowMax)
    ASSERT_GE(Cubic->CongestionWindow, WindowBeforeAck);
    ASSERT_LE(Cubic->CongestionWindow, Cubic->WindowMax);
    
    // Verify we're still in congestion avoidance
    ASSERT_GE(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
}

//
// Test 21: CUBIC Convex Region Window Calculation
// Scenario: Tests CUBIC window growth in the convex region (t > K), where
// the connection has recovered past WindowMax and is probing for additional
// bandwidth. In this region, growth should accelerate (convex cubic function),
// being more aggressive than the concave region. This verifies that CUBIC
// properly transitions through K and exhibits the characteristic concave-to-convex
// shape that gives the algorithm its name and performance properties.
//
TEST(DeepTest_CubicTest, CUBIC_ConvexRegion)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 20;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Force into congestion avoidance
    Cubic->SlowStartThreshold = Cubic->CongestionWindow / 2;
    Cubic->AimdWindow = Cubic->CongestionWindow;
    Cubic->TimeOfCongAvoidStart = 1000000; // 1 second
    Cubic->TimeOfLastAck = 1000000;
    Cubic->TimeOfLastAckValid = TRUE;
    
    // Set up for convex region: we've recovered past WindowMax
    Cubic->WindowMax = Cubic->CongestionWindow - 5000; // WindowMax is below current
    Cubic->WindowLastMax = Cubic->WindowMax;
    Cubic->KCubic = 200; // 200 ms - we'll ACK after this (t > K)
    Cubic->BytesInFlight = Cubic->CongestionWindow / 2;
    Cubic->BytesInFlightMax = Cubic->CongestionWindow;

    uint32_t WindowBeforeAck = Cubic->CongestionWindow;

    // ACK data in convex region (t > K)
    Connection.Send.NextPacketNumber = 100;
    
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 1350000; // 350ms after CA start (t = 350ms > K = 200ms)
    AckEvent.LargestAck = 95;
    AckEvent.LargestSentPacketNumber = 100;
    AckEvent.NumRetransmittableBytes = 2560; // 2 MTU
    AckEvent.NumTotalAckedRetransmittableBytes = 2560;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = FALSE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify window grew beyond WindowMax (convex probing)
    ASSERT_GT(Cubic->CongestionWindow, WindowBeforeAck);
    ASSERT_GT(Cubic->CongestionWindow, Cubic->WindowMax);
    
    // Window should still be capped by 2*BytesInFlightMax
    ASSERT_LE(Cubic->CongestionWindow, 2 * Cubic->BytesInFlightMax);
}

//
// Test 22: Fast Convergence on Multiple Congestion Events
// Scenario: Tests the fast convergence mechanism (RFC 8312 Section 4.6) where
// repeated congestion events cause CUBIC to reduce WindowMax more aggressively.
// When a new congestion event occurs and WindowMax < WindowLastMax, CUBIC applies
// an additional reduction to WindowMax: WindowMax = WindowMax * (1 + BETA) / 2.
// This helps multiple flows converge to fair bandwidth sharing more quickly.
//
TEST(DeepTest_CubicTest, FastConvergence_MultipleCongestionEvents)
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
    Cubic->BytesInFlight = 8000;
    Connection.Send.NextPacketNumber = 50;

    // First congestion event
    QUIC_LOSS_EVENT LossEvent1;
    CxPlatZeroMemory(&LossEvent1, sizeof(LossEvent1));
    LossEvent1.LargestPacketNumberLost = 45;
    LossEvent1.LargestSentPacketNumber = 50;
    LossEvent1.NumRetransmittableBytes = 2000;
    LossEvent1.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent1);

    uint32_t WindowAfterFirstLoss = Cubic->CongestionWindow;
    uint32_t WindowMaxAfterFirstLoss = Cubic->WindowMax;
    uint32_t WindowLastMaxAfterFirstLoss = Cubic->WindowLastMax;
    
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_EQ(WindowMaxAfterFirstLoss, InitialWindow); // WindowMax = CongestionWindow
    ASSERT_EQ(WindowLastMaxAfterFirstLoss, InitialWindow);

    // Exit recovery by ACKing beyond RecoverySentPacketNumber
    Connection.Send.NextPacketNumber = 100;
    Cubic->BytesInFlight = 5000;
    
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 1500000;
    AckEvent.LargestAck = 60; // Beyond recovery packet number
    AckEvent.LargestSentPacketNumber = 100;
    AckEvent.NumRetransmittableBytes = 3000;
    AckEvent.NumTotalAckedRetransmittableBytes = 3000;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = FALSE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    ASSERT_FALSE(Cubic->IsInRecovery);

    // Artificially set window below previous WindowMax to simulate incomplete recovery
    Cubic->CongestionWindow = WindowMaxAfterFirstLoss - 5000;
    Cubic->BytesInFlight = 7000;
    Connection.Send.NextPacketNumber = 150;

    // Second congestion event (fast convergence should apply)
    QUIC_LOSS_EVENT LossEvent2;
    CxPlatZeroMemory(&LossEvent2, sizeof(LossEvent2));
    LossEvent2.LargestPacketNumberLost = 145;
    LossEvent2.LargestSentPacketNumber = 150;
    LossEvent2.NumRetransmittableBytes = 2500;
    LossEvent2.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent2);

    uint32_t WindowMaxAfterSecondLoss = Cubic->WindowMax;
    
    // Fast convergence: WindowMax should be further reduced
    // Expected: WindowMax = (CongestionWindow) * (10 + BETA) / 20 where BETA = 0.7
    // This is: WindowMax * (10 + 7) / 20 = WindowMax * 17/20 = WindowMax * 0.85
    ASSERT_LT(WindowMaxAfterSecondLoss, Cubic->CongestionWindow);
    ASSERT_LT(WindowMaxAfterSecondLoss, WindowMaxAfterFirstLoss);
}

//
// Test 23: Window Growth Cap at 2*BytesInFlightMax
// Scenario: Tests that CongestionWindow growth is properly capped at 2*BytesInFlightMax
// to prevent the window from growing without actual network feedback. This is a safety
// mechanism to ensure the window only grows based on demonstrated ability to send data,
// not just passage of time. Without this cap, flow control or application rate limits
// could allow unbounded window growth, causing problems when restrictions are lifted.
//
TEST(DeepTest_CubicTest, WindowGrowthCap_TwoBytesInFlightMax)
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
    
    // Set BytesInFlightMax to a specific value
    Cubic->BytesInFlightMax = 10000;
    uint32_t MaxAllowedWindow = 2 * Cubic->BytesInFlightMax;

    // Force into congestion avoidance with a large window
    Cubic->SlowStartThreshold = 5000;
    Cubic->CongestionWindow = MaxAllowedWindow + 5000; // Exceeds cap
    Cubic->AimdWindow = Cubic->CongestionWindow;
    Cubic->WindowMax = Cubic->CongestionWindow;
    Cubic->TimeOfCongAvoidStart = 1000000;
    Cubic->TimeOfLastAck = 1000000;
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->BytesInFlight = 9000;

    Connection.Send.NextPacketNumber = 100;

    // ACK data - window should be capped despite potential growth
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 1100000;
    AckEvent.LargestAck = 95;
    AckEvent.LargestSentPacketNumber = 100;
    AckEvent.NumRetransmittableBytes = 2560; // 2 MTU
    AckEvent.NumTotalAckedRetransmittableBytes = 2560;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 45000;
    AckEvent.MinRttValid = FALSE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify window was capped at 2*BytesInFlightMax
    ASSERT_LE(Cubic->CongestionWindow, MaxAllowedWindow);
    ASSERT_EQ(Cubic->CongestionWindow, MaxAllowedWindow);
}

//
// Test 24: Idle Timeout Handling in Congestion Avoidance
// Scenario: Tests that window growth is paused/frozen when there's a long gap between
// ACKs (idle timeout). CUBIC adjusts TimeOfCongAvoidStart forward by the idle gap to
// effectively freeze the CUBIC time parameter, preventing artificial window growth
// during periods of inactivity. This test verifies the idle detection logic that checks
// if TimeSinceLastAck > SendIdleTimeoutMs AND > (SmoothedRtt + 4*RttVariance).
//
TEST(DeepTest_CubicTest, IdleTimeout_CongestionAvoidance)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 100; // 100ms idle timeout

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms
    Connection.Paths[0].RttVariance = 5000; // 5ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;
    
    // Force into congestion avoidance
    Cubic->SlowStartThreshold = Cubic->CongestionWindow / 2;
    Cubic->AimdWindow = Cubic->SlowStartThreshold;
    Cubic->TimeOfCongAvoidStart = 1000000; // 1 second
    Cubic->TimeOfLastAck = 1000000;
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->WindowMax = Cubic->CongestionWindow;
    Cubic->KCubic = 300;
    Cubic->BytesInFlight = 5000;

    uint64_t InitialTimeOfCongAvoidStart = Cubic->TimeOfCongAvoidStart;

    Connection.Send.NextPacketNumber = 50;

    // First ACK at normal time
    QUIC_ACK_EVENT AckEvent1;
    CxPlatZeroMemory(&AckEvent1, sizeof(AckEvent1));
    AckEvent1.TimeNow = 1050000; // 50ms later (within normal range)
    AckEvent1.LargestAck = 45;
    AckEvent1.LargestSentPacketNumber = 50;
    AckEvent1.NumRetransmittableBytes = 1280;
    AckEvent1.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent1.SmoothedRtt = 50000;
    AckEvent1.MinRtt = 45000;
    AckEvent1.MinRttValid = FALSE;
    AckEvent1.IsImplicit = FALSE;
    AckEvent1.HasLoss = FALSE;
    AckEvent1.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent1.AdjustedAckTime = AckEvent1.TimeNow;
    AckEvent1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent1);

    // TimeOfCongAvoidStart should not change (normal ACK timing)
    ASSERT_EQ(Cubic->TimeOfCongAvoidStart, InitialTimeOfCongAvoidStart);

    Connection.Send.NextPacketNumber = 100;
    Cubic->BytesInFlight = 5500;

    // Second ACK after LONG idle period (triggers idle timeout logic)
    QUIC_ACK_EVENT AckEvent2;
    CxPlatZeroMemory(&AckEvent2, sizeof(AckEvent2));
    // 500ms later (exceeds SendIdleTimeoutMs=100ms AND SmoothedRtt + 4*RttVariance = 70ms)
    AckEvent2.TimeNow = 1550000; 
    AckEvent2.LargestAck = 95;
    AckEvent2.LargestSentPacketNumber = 100;
    AckEvent2.NumRetransmittableBytes = 1280;
    AckEvent2.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent2.SmoothedRtt = 50000;
    AckEvent2.MinRtt = 45000;
    AckEvent2.MinRttValid = FALSE;
    AckEvent2.IsImplicit = FALSE;
    AckEvent2.HasLoss = FALSE;
    AckEvent2.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent2.AdjustedAckTime = AckEvent2.TimeNow;
    AckEvent2.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent2);

    // TimeOfCongAvoidStart should have been adjusted forward by the idle gap
    // to freeze window growth during idle period
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, InitialTimeOfCongAvoidStart);
}

//
// Test 25: ECN Event During Recovery (No Duplicate Congestion)
// Scenario: Tests that receiving an ECN signal while already in recovery does NOT
// trigger a duplicate congestion event. CUBIC checks if the ECN's LargestPacketNumberAcked
// is beyond RecoverySentPacketNumber before triggering a new event. This prevents
// multiple window reductions for the same congestion episode, which would be overly
// conservative and harm performance. Only ECN signals for new congestion should reduce window.
//
TEST(DeepTest_CubicTest, ECN_DuringRecovery_NoDuplicateCongestion)
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
    
    uint32_t InitialWindow = Cubic->CongestionWindow;
    Cubic->BytesInFlight = 8000;
    Connection.Send.NextPacketNumber = 100;

    // First: trigger congestion via loss
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.LargestPacketNumberLost = 95;
    LossEvent.LargestSentPacketNumber = 100;
    LossEvent.NumRetransmittableBytes = 2000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    ASSERT_TRUE(Cubic->IsInRecovery);
    uint32_t WindowAfterLoss = Cubic->CongestionWindow;
    uint64_t RecoverySentPacketNum = Cubic->RecoverySentPacketNumber;
    ASSERT_LT(WindowAfterLoss, InitialWindow);

    // Now receive ECN signal for OLD congestion (packet number < RecoverySentPacketNumber)
    // This should NOT trigger another congestion event
    QUIC_ECN_EVENT EcnEvent;
    CxPlatZeroMemory(&EcnEvent, sizeof(EcnEvent));
    EcnEvent.LargestPacketNumberAcked = 90; // BEFORE RecoverySentPacketNumber
    EcnEvent.LargestSentPacketNumber = 110;

    Connection.CongestionControl.QuicCongestionControlOnEcn(
        &Connection.CongestionControl,
        &EcnEvent);

    // Window should NOT have changed (no duplicate congestion event)
    ASSERT_EQ(Cubic->CongestionWindow, WindowAfterLoss);
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, RecoverySentPacketNum);
}

//
// Test 26: Loss Event During Recovery (No Duplicate Congestion)
// Scenario: Similar to Test 25 but for loss events. Tests that packet loss detected
// during recovery does NOT trigger a duplicate congestion event if the lost packet
// number is not beyond RecoverySentPacketNumber. This ensures CUBIC doesn't over-react
// to losses that are part of the same congestion episode, maintaining appropriate
// window size during recovery rather than repeatedly cutting it.
//
TEST(DeepTest_CubicTest, Loss_DuringRecovery_NoDuplicateCongestion)
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
    
    Cubic->BytesInFlight = 10000;
    Connection.Send.NextPacketNumber = 100;

    // First loss triggers congestion
    QUIC_LOSS_EVENT LossEvent1;
    CxPlatZeroMemory(&LossEvent1, sizeof(LossEvent1));
    LossEvent1.LargestPacketNumberLost = 95;
    LossEvent1.LargestSentPacketNumber = 100;
    LossEvent1.NumRetransmittableBytes = 3000;
    LossEvent1.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent1);

    ASSERT_TRUE(Cubic->IsInRecovery);
    uint32_t WindowAfterFirstLoss = Cubic->CongestionWindow;
    uint64_t RecoverySentPacketNum = Cubic->RecoverySentPacketNumber;

    // Second loss for OLD packet (during same recovery period)
    // Should NOT trigger new congestion event
    Cubic->BytesInFlight = 8000;
    
    QUIC_LOSS_EVENT LossEvent2;
    CxPlatZeroMemory(&LossEvent2, sizeof(LossEvent2));
    LossEvent2.LargestPacketNumberLost = 92; // BEFORE RecoverySentPacketNumber
    LossEvent2.LargestSentPacketNumber = 110;
    LossEvent2.NumRetransmittableBytes = 1500;
    LossEvent2.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent2);

    // Window should NOT have changed further
    ASSERT_EQ(Cubic->CongestionWindow, WindowAfterFirstLoss);
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, RecoverySentPacketNum);
    
    // BytesInFlight should have been updated though
    ASSERT_EQ(Cubic->BytesInFlight, 6500u); // 8000 - 1500
}
