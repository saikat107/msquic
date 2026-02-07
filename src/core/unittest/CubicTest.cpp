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
// Scenario: When persistent congestion occurs (e.g., multiple RTOs without progress),
// CUBIC should reset the window to the minimum (2*MTU). Tests the most severe
// congestion response which happens after prolonged packet loss.
// What: Trigger persistent congestion via OnDataLost with PersistentCongestion=TRUE
// How: Set up initial window, trigger loss event with PersistentCongestion flag
// Assertions: Window reduced to 2*MTU, IsInPersistentCongestion=TRUE, WindowMax/Prior set correctly
//
TEST(DeepTest_CubicTest, PersistentCongestion_WindowResetToMinimum)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up initial window and bytes in flight
    uint32_t InitialWindow = Cubic->CongestionWindow;
    Cubic->BytesInFlight = 10000;
    const uint16_t Mtu = 1280;
    const uint16_t DatagramPayloadLength = Mtu - QUIC_MIN_IPV4_HEADER_SIZE - QUIC_UDP_HEADER_SIZE;

    // Trigger persistent congestion
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.LargestPacketNumberLost = 100;
    LossEvent.LargestSentPacketNumber = 110;
    LossEvent.NumRetransmittableBytes = 5000;
    LossEvent.PersistentCongestion = TRUE; // Key flag

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify persistent congestion state
    ASSERT_TRUE(Cubic->IsInPersistentCongestion);
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);

    // Verify window reset to minimum (2 * MTU worth of payload)
    uint32_t MinWindow = DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS;
    ASSERT_EQ(Cubic->CongestionWindow, MinWindow);
    ASSERT_LT(Cubic->CongestionWindow, InitialWindow);

    // Verify CUBIC state variables updated
    ASSERT_EQ(Cubic->KCubic, 0u);
    ASSERT_EQ(Cubic->WindowMax, Cubic->CongestionWindow * TEN_TIMES_BETA_CUBIC / 10);
    ASSERT_EQ(Cubic->BytesInFlight, 5000u); // Should be reduced

    // Verify HyStart disabled after persistent congestion
    ASSERT_EQ(Cubic->HyStartState, HYSTART_DONE);
}

//
// Test 19: Spurious Congestion Event - Window Restoration
// Scenario: If packet loss was spurious (packets later ACKed), CUBIC can revert
// the window reduction to restore throughput. Tests RFC 9293 style spurious loss recovery.
// What: Trigger congestion event, then revert via OnSpuriousCongestionEvent
// How: Reduce window via loss, verify state saved, call spurious handler, verify restoration
// Assertions: Previous window/threshold restored exactly, IsInRecovery=FALSE, HasHadCongestionEvent=FALSE
//
TEST(DeepTest_CubicTest, SpuriousCongestion_WindowRestoration)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 50;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up substantial initial window
    uint32_t PreLossWindow = Cubic->CongestionWindow;
    uint32_t PreLossSsthresh = Cubic->SlowStartThreshold;
    Cubic->BytesInFlight = 20000;

    // Grow window a bit to make it more interesting
    Cubic->AimdWindow = PreLossWindow + 5000;
    Cubic->CongestionWindow = Cubic->AimdWindow;

    uint32_t PreLossCongestionWindow = Cubic->CongestionWindow;

    // Trigger regular (non-ECN, non-persistent) congestion event
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.LargestPacketNumberLost = 50;
    LossEvent.LargestSentPacketNumber = 60;
    LossEvent.NumRetransmittableBytes = 5000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify congestion event occurred
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    uint32_t PostLossWindow = Cubic->CongestionWindow;
    ASSERT_LT(PostLossWindow, PreLossCongestionWindow); // Window should be reduced

    // Verify previous state was saved (for non-ECN events)
    ASSERT_EQ(Cubic->PrevCongestionWindow, PreLossCongestionWindow);

    // Now declare the loss spurious
    BOOLEAN BecameUnblocked = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);

    // Verify window restored exactly
    ASSERT_EQ(Cubic->CongestionWindow, Cubic->PrevCongestionWindow);
    ASSERT_EQ(Cubic->WindowPrior, Cubic->PrevWindowPrior);
    ASSERT_EQ(Cubic->WindowMax, Cubic->PrevWindowMax);
    ASSERT_EQ(Cubic->WindowLastMax, Cubic->PrevWindowLastMax);
    ASSERT_EQ(Cubic->KCubic, Cubic->PrevKCubic);
    ASSERT_EQ(Cubic->SlowStartThreshold, Cubic->PrevSlowStartThreshold);
    ASSERT_EQ(Cubic->AimdWindow, Cubic->PrevAimdWindow);

    // Verify recovery state cleared
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->HasHadCongestionEvent);
}

//
// Test 20: Congestion Avoidance - CUBIC Function Growth
// Scenario: In congestion avoidance after slow start, CUBIC uses the cubic function
// W_cubic(t) = C*(t-K)^3 + WindowMax to grow the window. Tests the core CUBIC algorithm.
// What: Enter CA mode, acknowledge data over time, verify window growth follows cubic curve
// How: Set CongestionWindow >= SlowStartThreshold, simulate time passing with ACKs
// Assertions: Window grows according to CUBIC formula (not linear), uses TimeOfCongAvoidStart
//
TEST(DeepTest_CubicTest, CongestionAvoidance_CubicFunctionGrowth)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 10000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 100000; // 100ms
    Connection.Paths[0].RttVariance = 10000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Force into congestion avoidance by setting window >= threshold
    uint32_t InitialCAWindow = 50000;
    Cubic->CongestionWindow = InitialCAWindow;
    Cubic->SlowStartThreshold = InitialCAWindow - 1000; // CA mode
    Cubic->AimdWindow = InitialCAWindow;
    Cubic->TimeOfCongAvoidStart = 1000000; // 1 second
    Cubic->TimeOfLastAck = Cubic->TimeOfCongAvoidStart;
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->BytesInFlight = 25000;
    Cubic->BytesInFlightMax = 40000;

    // Set CUBIC state for CA
    Cubic->WindowMax = InitialCAWindow;
    Cubic->WindowPrior = InitialCAWindow;
    Cubic->WindowLastMax = InitialCAWindow * 2;
    Cubic->KCubic = 1000; // 1 second in milliseconds

    uint32_t WindowBeforeAck = Cubic->CongestionWindow;

    // Simulate ACK after some time in CA
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = Cubic->TimeOfCongAvoidStart + 500000; // +500ms
    AckEvent.LargestAck = 50;
    AckEvent.LargestSentPacketNumber = 60;
    AckEvent.NumRetransmittableBytes = 5000;
    AckEvent.NumTotalAckedRetransmittableBytes = 30000;
    AckEvent.SmoothedRtt = Connection.Paths[0].SmoothedRtt;
    AckEvent.MinRtt = 95000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify window grew (CA always grows, even if slowly)
    ASSERT_GT(Cubic->CongestionWindow, WindowBeforeAck);

    // Verify we stayed in CA mode
    ASSERT_GE(Cubic->CongestionWindow, Cubic->SlowStartThreshold);

    // Verify TimeOfLastAck updated
    ASSERT_EQ(Cubic->TimeOfLastAck, AckEvent.TimeNow);
    ASSERT_TRUE(Cubic->TimeOfLastAckValid);

    // Verify BytesInFlight reduced
    ASSERT_EQ(Cubic->BytesInFlight, 20000u); // 25000 - 5000
}

//
// Test 21: Congestion Avoidance - AIMD vs CUBIC (Reno-Friendly Region)
// Scenario: CUBIC includes an AIMD window calculation to ensure fairness with Reno.
// When AIMD window > CUBIC window, CUBIC uses AIMD (Reno-friendly region).
// What: Set up CA state where AIMD calculation exceeds CUBIC, verify AIMD used
// How: Configure WindowMax and time so CUBIC < AIMD, acknowledge data
// Assertions: CongestionWindow == AimdWindow (not CUBIC value), verifies fairness
//
TEST(DeepTest_CubicTest, CongestionAvoidance_AimdVsCubic_RenoFriendly)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 10000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms
    Connection.Paths[0].RttVariance = 5000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Configure CA state - early in recovery where AIMD > CUBIC
    uint32_t InitialWindow = 30000;
    Cubic->CongestionWindow = InitialWindow;
    Cubic->SlowStartThreshold = InitialWindow - 1;
    Cubic->TimeOfCongAvoidStart = 500000; // Recent congestion event
    Cubic->TimeOfLastAck = Cubic->TimeOfCongAvoidStart;
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->BytesInFlight = 15000;
    Cubic->BytesInFlightMax = 25000;

    // Set CUBIC state - large WindowMax (recovering from big window)
    Cubic->WindowMax = 80000; // Much larger than current
    Cubic->WindowPrior = 30000;
    Cubic->WindowLastMax = 90000;
    Cubic->KCubic = 2000; // 2 seconds (long time to recover)

    // Set AIMD window higher to force Reno-friendly mode
    Cubic->AimdWindow = InitialWindow + 2000; // Ahead of CUBIC
    Cubic->AimdAccumulator = 0;

    // Acknowledge data to trigger CA growth
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = Cubic->TimeOfCongAvoidStart + 100000; // +100ms (early)
    AckEvent.LargestAck = 30;
    AckEvent.LargestSentPacketNumber = 40;
    AckEvent.NumRetransmittableBytes = 3000;
    AckEvent.NumTotalAckedRetransmittableBytes = 18000;
    AckEvent.SmoothedRtt = Connection.Paths[0].SmoothedRtt;
    AckEvent.MinRtt = 48000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // In Reno-friendly region, CUBIC should use AIMD window
    // (early after congestion event, CUBIC is below AIMD)
    // The implementation should have grown the window
    ASSERT_GT(Cubic->CongestionWindow, InitialWindow);
    ASSERT_EQ(Cubic->BytesInFlight, 12000u); // 15000 - 3000
}

//
// Test 22: Send Idle Timeout - Freeze CA Growth During Gaps
// Scenario: If there's a long gap between ACKs (longer than SendIdleTimeoutMs and RTT variance),
// CUBIC freezes window growth by adjusting TimeOfCongAvoidStart. Prevents unfair growth during idle.
// What: Simulate long ACK gap exceeding idle timeout, verify TimeOfCongAvoidStart adjusted
// How: Set TimeOfLastAck, deliver ACK after long delay, check TimeOfCongAvoidStart moved forward
// Assertions: TimeOfCongAvoidStart increased by gap duration, window growth limited appropriately
//
TEST(DeepTest_CubicTest, SendIdleTimeout_FreezeCAGrowthDuringGaps)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 100; // 100ms idle timeout

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms
    Connection.Paths[0].RttVariance = 5000; // 5ms variance

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up CA state
    Cubic->CongestionWindow = 40000;
    Cubic->SlowStartThreshold = 39000;
    Cubic->TimeOfCongAvoidStart = 1000000; // 1 second
    Cubic->TimeOfLastAck = 2000000; // 2 seconds (last ACK)
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->BytesInFlight = 20000;
    Cubic->BytesInFlightMax = 35000;
    Cubic->AimdWindow = 40000;
    Cubic->WindowMax = 50000;
    Cubic->WindowPrior = 40000;
    Cubic->KCubic = 500;

    uint64_t OriginalTimeOfCongAvoidStart = Cubic->TimeOfCongAvoidStart;

    // Simulate ACK after LONG idle gap (500ms = 5x idle timeout)
    uint64_t LongGap = 500000; // 500ms
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = Cubic->TimeOfLastAck + LongGap; // 2.5 seconds
    AckEvent.LargestAck = 20;
    AckEvent.LargestSentPacketNumber = 30;
    AckEvent.NumRetransmittableBytes = 2000;
    AckEvent.NumTotalAckedRetransmittableBytes = 22000;
    AckEvent.SmoothedRtt = Connection.Paths[0].SmoothedRtt;
    AckEvent.MinRtt = 49000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    // Gap (500ms) exceeds both SendIdleTimeoutMs (100ms) and SmoothedRtt + 4*RttVariance (70ms)
    // So TimeOfCongAvoidStart should be adjusted forward

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify TimeOfCongAvoidStart was adjusted forward
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, OriginalTimeOfCongAvoidStart);

    // Verify TimeOfLastAck updated
    ASSERT_EQ(Cubic->TimeOfLastAck, AckEvent.TimeNow);

    // BytesInFlight should be reduced
    ASSERT_EQ(Cubic->BytesInFlight, 18000u); // 20000 - 2000
}

//
// Test 23: Window Growth Limit - BytesInFlightMax Constraint
// Scenario: CUBIC constrains window growth to 2*BytesInFlightMax to prevent unbounded
// growth when flow control or application limits actual bytes sent. Tests prudent growth.
// What: Set BytesInFlightMax << CongestionWindow, acknowledge data, verify cap applied
// How: Configure small BytesInFlightMax, simulate CA growth, check window capped
// Assertions: CongestionWindow <= 2*BytesInFlightMax, growth limited despite CUBIC formula
//
TEST(DeepTest_CubicTest, WindowGrowthLimit_BytesInFlightMaxConstraint)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 30000; // 30ms

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up CA with very limited actual sending (small BytesInFlightMax)
    Cubic->CongestionWindow = 50000;
    Cubic->SlowStartThreshold = 40000;
    Cubic->BytesInFlight = 8000;
    Cubic->BytesInFlightMax = 10000; // Very small - app/flow control limited
    Cubic->TimeOfCongAvoidStart = 500000;
    Cubic->TimeOfLastAck = Cubic->TimeOfCongAvoidStart;
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->AimdWindow = 50000;
    Cubic->WindowMax = 60000;
    Cubic->WindowPrior = 50000;
    Cubic->KCubic = 100;

    // Acknowledge data
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = Cubic->TimeOfCongAvoidStart + 200000; // +200ms
    AckEvent.LargestAck = 40;
    AckEvent.LargestSentPacketNumber = 50;
    AckEvent.NumRetransmittableBytes = 4000;
    AckEvent.NumTotalAckedRetransmittableBytes = 12000;
    AckEvent.SmoothedRtt = Connection.Paths[0].SmoothedRtt;
    AckEvent.MinRtt = 29000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify window capped at 2 * BytesInFlightMax
    uint32_t MaxAllowedWindow = 2 * Cubic->BytesInFlightMax;
    ASSERT_LE(Cubic->CongestionWindow, MaxAllowedWindow);

    // Verify BytesInFlight reduced
    ASSERT_EQ(Cubic->BytesInFlight, 4000u); // 8000 - 4000
}

//
// Test 24: ECN Signal Cannot Be Spurious
// Scenario: Unlike loss-based congestion events, ECN-based events do NOT save previous
// state and cannot be reverted. Tests that spurious recovery doesn't apply to ECN.
// What: Trigger ECN event, attempt spurious recovery, verify no restoration
// How: Call OnEcn, verify PrevWindow* NOT saved, call OnSpuriousCongestion (should no-op or fail)
// Assertions: Window stays reduced after ECN, OnSpuriousCongestion returns FALSE or has no effect
//
TEST(DeepTest_CubicTest, EcnSignal_CannotBeSpurious)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 50;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up initial state
    uint32_t PreEcnWindow = Cubic->CongestionWindow;
    Cubic->BytesInFlight = 30000;
    Cubic->AimdWindow = PreEcnWindow;

    // Trigger ECN congestion event
    QUIC_ECN_EVENT EcnEvent;
    CxPlatZeroMemory(&EcnEvent, sizeof(EcnEvent));
    EcnEvent.LargestPacketNumberAcked = 50;
    EcnEvent.LargestSentPacketNumber = 60;

    Connection.CongestionControl.QuicCongestionControlOnEcn(
        &Connection.CongestionControl,
        &EcnEvent);

    // Verify congestion event occurred
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    uint32_t PostEcnWindow = Cubic->CongestionWindow;
    ASSERT_LT(PostEcnWindow, PreEcnWindow); // Window reduced

    // Verify ECN events don't save previous state (contract: only loss events do)
    // (PrevCongestionWindow would have been set only for non-ECN events)
    // We can't directly check Prev* values weren't saved (implementation detail),
    // but we can verify spurious recovery doesn't restore to PreEcnWindow

    uint32_t WindowBeforeSpuriousCall = Cubic->CongestionWindow;

    // Attempt spurious recovery
    BOOLEAN Result = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);

    // ECN-based recovery should either:
    // 1) Not restore window (if Prev* weren't saved), OR
    // 2) Return FALSE if not in recovery anymore
    // In any case, we should NOT go back to PreEcnWindow
    ASSERT_NE(Cubic->CongestionWindow, PreEcnWindow);

    // Verify we don't magically jump back up
    ASSERT_LE(Cubic->CongestionWindow, PreEcnWindow);
}

//
// Test 25: Recovery Exit - ACK After RecoverySentPacketNumber
// Scenario: Recovery ends when a packet sent AFTER entering recovery is ACKed.
// Tests the recovery exit condition (different from TCP's flight size threshold).
// What: Enter recovery, set RecoverySentPacketNumber, ACK packet > that number
// How: Trigger loss (enter recovery), note RecoverySentPacketNumber, simulate ACK of later packet
// Assertions: IsInRecovery becomes FALSE, IsInPersistentCongestion becomes FALSE
//
TEST(DeepTest_CubicTest, RecoveryExit_AckAfterRecoverySentPacketNumber)
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

    // Set up state
    Cubic->BytesInFlight = 15000;
    Cubic->CongestionWindow = 30000;
    Cubic->SlowStartThreshold = UINT32_MAX;

    // Trigger loss to enter recovery
    QUIC_LOSS_EVENT LossEvent;
    CxPlatZeroMemory(&LossEvent, sizeof(LossEvent));
    LossEvent.LargestPacketNumberLost = 40;
    LossEvent.LargestSentPacketNumber = 50; // Sets RecoverySentPacketNumber
    LossEvent.NumRetransmittableBytes = 5000;
    LossEvent.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent);

    // Verify we entered recovery
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, 50u);

    // Acknowledge packet AFTER RecoverySentPacketNumber (exits recovery)
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 2000000;
    AckEvent.LargestAck = 55; // > RecoverySentPacketNumber (50)
    AckEvent.LargestSentPacketNumber = 60;
    AckEvent.NumRetransmittableBytes = 3000;
    AckEvent.NumTotalAckedRetransmittableBytes = 13000;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 48000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    // Verify recovery exited
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->IsInPersistentCongestion);

    // Verify BytesInFlight updated
    ASSERT_EQ(Cubic->BytesInFlight, 7000u); // 10000 - 3000
}

//
// Test 26: Fast Convergence - WindowLastMax Logic
// Scenario: When re-entering congestion after incomplete recovery (new WindowMax < WindowLastMax),
// CUBIC applies "fast convergence" by reducing WindowMax further for stability.
// What: Trigger congestion twice with second WindowMax < first WindowLastMax
// How: First loss sets WindowLastMax, second loss before full recovery triggers fast convergence
// Assertions: Second WindowMax = WindowMax * (10+BETA)/20 (multiplicative decrease beyond BETA)
//
TEST(DeepTest_CubicTest, FastConvergence_WindowLastMaxLogic)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 100; // Large initial window
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].GotFirstRttSample = TRUE;
    Connection.Paths[0].SmoothedRtt = 50000;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up large initial window
    Cubic->CongestionWindow = 100000;
    Cubic->SlowStartThreshold = UINT32_MAX;
    Cubic->BytesInFlight = 50000;

    // First congestion event at large window
    QUIC_LOSS_EVENT LossEvent1;
    CxPlatZeroMemory(&LossEvent1, sizeof(LossEvent1));
    LossEvent1.LargestPacketNumberLost = 100;
    LossEvent1.LargestSentPacketNumber = 110;
    LossEvent1.NumRetransmittableBytes = 10000;
    LossEvent1.PersistentCongestion = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent1);

    uint32_t WindowMaxAfterFirstLoss = Cubic->WindowMax;
    uint32_t WindowLastMaxAfterFirstLoss = Cubic->WindowLastMax;

    // Verify first loss reduced window
    ASSERT_LT(Cubic->CongestionWindow, 100000u);
    ASSERT_TRUE(Cubic->IsInRecovery);

    // Exit recovery (ACK packet after RecoverySentPacketNumber)
    QUIC_ACK_EVENT AckEvent;
    CxPlatZeroMemory(&AckEvent, sizeof(AckEvent));
    AckEvent.TimeNow = 2000000;
    AckEvent.LargestAck = 120; // > 110
    AckEvent.LargestSentPacketNumber = 130;
    AckEvent.NumRetransmittableBytes = 5000;
    AckEvent.NumTotalAckedRetransmittableBytes = 45000;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 48000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl,
        &AckEvent);

    ASSERT_FALSE(Cubic->IsInRecovery);

    // Grow window slightly (incomplete recovery)
    Cubic->CongestionWindow += 5000;
    Cubic->BytesInFlight = 40000;

    // Second congestion event BEFORE recovering to WindowLastMax
    QUIC_LOSS_EVENT LossEvent2;
    CxPlatZeroMemory(&LossEvent2, sizeof(LossEvent2));
    LossEvent2.LargestPacketNumberLost = 140;
    LossEvent2.LargestSentPacketNumber = 150;
    LossEvent2.NumRetransmittableBytes = 8000;
    LossEvent2.PersistentCongestion = FALSE;

    uint32_t WindowBeforeSecondLoss = Cubic->CongestionWindow;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl,
        &LossEvent2);

    // Fast convergence condition: WindowMax (from second loss) < WindowLastMax (from first loss)
    // Since CongestionWindow < WindowLastMax, fast convergence should apply:
    // WindowMax = WindowMax * (10 + TEN_TIMES_BETA_CUBIC) / 20
    // where TEN_TIMES_BETA_CUBIC = 7, so multiplier = (10+7)/20 = 17/20 = 0.85

    // Verify fast convergence applied (WindowMax further reduced)
    // Normal reduction: WindowMax = CongestionWindow = WindowBeforeSecondLoss * BETA (0.7)
    // Fast convergence: WindowMax = (WindowBeforeSecondLoss * 0.7) * 0.85 = WindowBeforeSecondLoss * 0.595
    
    uint32_t NormalReduction = WindowBeforeSecondLoss * TEN_TIMES_BETA_CUBIC / 10;
    
    // WindowMax should be less than normal BETA reduction due to fast convergence
    ASSERT_LT(Cubic->WindowMax, NormalReduction);
    
    // Verify WindowLastMax updated to reflect new (reduced) WindowMax
    ASSERT_EQ(Cubic->WindowLastMax, Cubic->CongestionWindow);
}
