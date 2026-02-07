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
// Test 18: Congestion Avoidance - CUBIC Formula Application
// Scenario: Tests CUBIC window growth in congestion avoidance mode using the CUBIC formula:
// W_cubic(t) = C*(t-K)^3 + WindowMax. This test transitions from slow start to congestion
// avoidance by setting CWND >= SST, then sends ACKs to trigger CUBIC formula calculations.
// Verifies that the window grows according to CUBIC algorithm (either following CUBIC formula
// or AIMD depending on which is more aggressive) and validates the convex/concave region behavior.
//
TEST(DeepTest_CubicTest, CongestionAvoidance_CubicFormula)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Force into congestion avoidance by setting CWND = SST
    uint32_t TargetWindow = Cubic->CongestionWindow;
    Cubic->SlowStartThreshold = TargetWindow;
    Cubic->CongestionWindow = TargetWindow;
    Cubic->AimdWindow = TargetWindow;
    Cubic->WindowMax = TargetWindow;
    Cubic->TimeOfCongAvoidStart = 100000; // 100ms
    Cubic->TimeOfLastAck = 100000;
    Cubic->TimeOfLastAckValid = TRUE;

    // Simulate ACK event in congestion avoidance
    QUIC_ACK_EVENT AckEvent{};
    AckEvent.TimeNow = 200000; // 200ms (100ms after congestion avoidance start)
    AckEvent.LargestAck = 10;
    AckEvent.LargestSentPacketNumber = 10;
    AckEvent.NumRetransmittableBytes = 1280; // 1 packet
    AckEvent.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent.SmoothedRtt = 50000; // 50ms
    AckEvent.MinRtt = 50000;
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    // Set bytes in flight
    Cubic->BytesInFlight = 1280;
    uint32_t InitialCongestionWindow = Cubic->CongestionWindow;

    // Process ACK - should trigger CUBIC formula calculations
    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent);

    // Verify window increased (CUBIC or AIMD formula applied)
    ASSERT_GE(Cubic->CongestionWindow, InitialCongestionWindow);
    
    // Verify AIMD window was updated
    ASSERT_GE(Cubic->AimdWindow, TargetWindow);
    
    // Verify we're still in congestion avoidance
    ASSERT_GE(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
    
    // Verify TimeOfLastAck updated
    ASSERT_EQ(Cubic->TimeOfLastAck, AckEvent.TimeNow);
    ASSERT_TRUE(Cubic->TimeOfLastAckValid);
}

//
// Test 19: Spurious Congestion Event Recovery
// Scenario: Tests the spurious congestion detection and recovery mechanism. When a loss is
// later determined to be spurious (e.g., due to packet reordering), CUBIC should revert to
// its previous state before the congestion event. This test simulates: 1) trigger congestion
// event (saves previous state), 2) call OnSpuriousCongestionEvent to restore state. Verifies
// that window, thresholds, and K values are restored, and recovery flag is cleared.
//
TEST(DeepTest_CubicTest, SpuriousCongestionEvent_StateRestoration)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up initial state in congestion avoidance
    Cubic->CongestionWindow = 50000;
    Cubic->SlowStartThreshold = 40000;
    Cubic->WindowMax = 48000;
    Cubic->WindowLastMax = 45000;
    Cubic->WindowPrior = 47000;
    Cubic->KCubic = 150;
    Cubic->AimdWindow = 49000;

    uint32_t PrevCongestionWindow = Cubic->CongestionWindow;
    uint32_t PrevSlowStartThreshold = Cubic->SlowStartThreshold;
    uint32_t PrevWindowMax = Cubic->WindowMax;
    uint32_t PrevWindowLastMax = Cubic->WindowLastMax;
    uint32_t PrevWindowPrior = Cubic->WindowPrior;
    uint32_t PrevKCubic = Cubic->KCubic;
    uint32_t PrevAimdWindow = Cubic->AimdWindow;

    // Simulate loss event (non-ECN) - should save previous state
    QUIC_LOSS_EVENT LossEvent{};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 10;
    LossEvent.NumRetransmittableBytes = 1280;
    LossEvent.PersistentCongestion = FALSE;

    Cubic->BytesInFlight = 10000;
    Cubic->HasHadCongestionEvent = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl, &LossEvent);

    // Verify we're in recovery and window was reduced
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    ASSERT_LT(Cubic->CongestionWindow, PrevCongestionWindow);

    // Now detect spurious congestion and restore state
    BOOLEAN BecameUnblocked = Connection.CongestionControl.QuicCongestionControlOnSpuriousCongestionEvent(
        &Connection.CongestionControl);

    // Verify state was restored to pre-congestion values
    ASSERT_EQ(Cubic->CongestionWindow, PrevCongestionWindow);
    ASSERT_EQ(Cubic->SlowStartThreshold, PrevSlowStartThreshold);
    ASSERT_EQ(Cubic->WindowMax, PrevWindowMax);
    ASSERT_EQ(Cubic->WindowLastMax, PrevWindowLastMax);
    ASSERT_EQ(Cubic->WindowPrior, PrevWindowPrior);
    ASSERT_EQ(Cubic->KCubic, PrevKCubic);
    ASSERT_EQ(Cubic->AimdWindow, PrevAimdWindow);

    // Verify recovery flags cleared
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->HasHadCongestionEvent);
}

//
// Test 20: Persistent Congestion Window Reduction
// Scenario: Tests the severe congestion handling when persistent congestion is detected
// (e.g., multiple consecutive RTOs). Persistent congestion forces CUBIC to drop the
// congestion window to the minimum (2 * MTU) and reset all CUBIC state variables.
// This test triggers a loss event with PersistentCongestion=TRUE and verifies that
// the window drops to QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS (2 packets), all
// window tracking variables are updated, K is reset to 0, and HyStart is set to DONE.
//
TEST(DeepTest_CubicTest, PersistentCongestion_MinimumWindow)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up large congestion window
    Cubic->CongestionWindow = 100000;
    Cubic->SlowStartThreshold = 80000;
    Cubic->WindowMax = 90000;
    Cubic->BytesInFlight = 50000;

    // Trigger persistent congestion
    QUIC_LOSS_EVENT LossEvent{};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 10;
    LossEvent.NumRetransmittableBytes = 5000;
    LossEvent.PersistentCongestion = TRUE;

    Cubic->HasHadCongestionEvent = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl, &LossEvent);

    // Verify window dropped to minimum (2 * MTU = 2560 bytes)
    uint32_t ExpectedMinWindow = 1280 * 2; // QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS = 2
    ASSERT_EQ(Cubic->CongestionWindow, ExpectedMinWindow);

    // Verify persistent congestion flag set
    ASSERT_TRUE(Cubic->IsInPersistentCongestion);
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);

    // Verify all window variables updated with BETA reduction
    ASSERT_GT(Cubic->SlowStartThreshold, 0u);
    ASSERT_GT(Cubic->WindowMax, 0u);
    ASSERT_GT(Cubic->WindowPrior, 0u);

    // Verify K reset to 0
    ASSERT_EQ(Cubic->KCubic, 0u);

    // Verify HyStart set to DONE
    ASSERT_EQ(Cubic->HyStartState, HYSTART_DONE);

    // Verify BytesInFlight decremented
    ASSERT_EQ(Cubic->BytesInFlight, 45000u);
}

//
// Test 21: Recovery Exit on Post-Recovery ACK
// Scenario: Tests the recovery exit mechanism where CUBIC exits recovery state when
// receiving an ACK for a packet sent after entering recovery. This test simulates:
// 1) Enter recovery via loss event (sets RecoverySentPacketNumber), 2) Send more packets
// (NextPacketNumber advances), 3) Receive ACK with LargestAck > RecoverySentPacketNumber.
// Verifies that IsInRecovery flag clears and IsInPersistentCongestion flag clears.
//
TEST(DeepTest_CubicTest, Recovery_ExitOnPostRecoveryAck)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Trigger loss to enter recovery
    Connection.Send.NextPacketNumber = 20;
    QUIC_LOSS_EVENT LossEvent{};
    LossEvent.LargestPacketNumberLost = 5;
    LossEvent.LargestSentPacketNumber = 20;
    LossEvent.NumRetransmittableBytes = 1280;
    LossEvent.PersistentCongestion = FALSE;

    Cubic->BytesInFlight = 5000;
    Cubic->HasHadCongestionEvent = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl, &LossEvent);

    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, 20u);

    // Simulate sending more packets after recovery starts
    Connection.Send.NextPacketNumber = 30;

    // Receive ACK for packet sent AFTER recovery started (packet 25 > RecoverySentPacketNumber 20)
    QUIC_ACK_EVENT AckEvent{};
    AckEvent.TimeNow = 200000;
    AckEvent.LargestAck = 25; // > RecoverySentPacketNumber
    AckEvent.LargestSentPacketNumber = 30;
    AckEvent.NumRetransmittableBytes = 1280;
    AckEvent.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 50000;
    AckEvent.MinRttValid = FALSE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent);

    // Verify recovery exit
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->IsInPersistentCongestion);
    
    // Verify TimeOfCongAvoidStart updated
    ASSERT_EQ(Cubic->TimeOfCongAvoidStart, AckEvent.TimeNow);
}

//
// Test 22: Window Growth Limited by BytesInFlightMax
// Scenario: Tests the window growth limitation mechanism that prevents the congestion window
// from growing beyond 2 * BytesInFlightMax. This prevents the window from growing unrealistically
// when the application or flow control limits actual bytes in flight. The test sets up a scenario
// where CUBIC would normally grow the window significantly, but BytesInFlightMax is low (app-limited
// or flow-control limited). Verifies that CongestionWindow is capped at 2 * BytesInFlightMax.
//
TEST(DeepTest_CubicTest, WindowGrowth_LimitedByBytesInFlightMax)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up congestion avoidance with low BytesInFlightMax (app-limited scenario)
    Cubic->CongestionWindow = 20000;
    Cubic->SlowStartThreshold = 20000;
    Cubic->AimdWindow = 20000;
    Cubic->WindowMax = 20000;
    Cubic->BytesInFlightMax = 5000; // Very low max (app-limited)
    Cubic->TimeOfCongAvoidStart = 100000;
    Cubic->TimeOfLastAck = 100000;
    Cubic->TimeOfLastAckValid = TRUE;

    // Simulate ACK event that would grow window
    QUIC_ACK_EVENT AckEvent{};
    AckEvent.TimeNow = 300000; // Large time delta
    AckEvent.LargestAck = 50;
    AckEvent.LargestSentPacketNumber = 50;
    AckEvent.NumRetransmittableBytes = 2560; // 2 packets
    AckEvent.NumTotalAckedRetransmittableBytes = 2560;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 50000;
    AckEvent.MinRttValid = FALSE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    Cubic->BytesInFlight = 2560;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent);

    // Verify window capped at 2 * BytesInFlightMax
    uint32_t MaxAllowedWindow = 2 * Cubic->BytesInFlightMax;
    ASSERT_LE(Cubic->CongestionWindow, MaxAllowedWindow);
    ASSERT_EQ(Cubic->CongestionWindow, MaxAllowedWindow);
}

//
// Test 23: ACK Time Gap Handling - Freeze Window Growth
// Scenario: Tests the mechanism that freezes CUBIC window growth when there's a long gap
// between ACKs (exceeding SendIdleTimeoutMs or 4*RttVariance). This prevents window growth
// when the connection is idle or when ACKs arrive sporadically. The test simulates a long
// time gap between ACKs and verifies that TimeOfCongAvoidStart is adjusted forward to
// effectively freeze window growth calculations during the gap.
//
TEST(DeepTest_CubicTest, AckTimeGap_FreezeWindowGrowth)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 100; // 100ms idle timeout

    InitializeMockConnection(Connection, 1280);
    Connection.Paths[0].SmoothedRtt = 50000; // 50ms
    Connection.Paths[0].RttVariance = 10000; // 10ms
    Connection.Paths[0].GotFirstRttSample = TRUE;

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up congestion avoidance
    Cubic->CongestionWindow = 30000;
    Cubic->SlowStartThreshold = 30000;
    Cubic->AimdWindow = 30000;
    Cubic->WindowMax = 30000;
    Cubic->TimeOfCongAvoidStart = 100000; // 100ms
    Cubic->TimeOfLastAck = 100000;
    Cubic->TimeOfLastAckValid = TRUE;
    Cubic->BytesInFlight = 1280;

    // First ACK - establishes TimeOfLastAck
    QUIC_ACK_EVENT AckEvent1{};
    AckEvent1.TimeNow = 150000; // 150ms (50ms after last)
    AckEvent1.LargestAck = 10;
    AckEvent1.LargestSentPacketNumber = 10;
    AckEvent1.NumRetransmittableBytes = 1280;
    AckEvent1.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent1.SmoothedRtt = 50000;
    AckEvent1.MinRttValid = FALSE;
    AckEvent1.IsImplicit = FALSE;
    AckEvent1.HasLoss = FALSE;
    AckEvent1.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent1.AdjustedAckTime = AckEvent1.TimeNow;
    AckEvent1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent1);

    uint64_t TimeOfCongAvoidAfterFirstAck = Cubic->TimeOfCongAvoidStart;

    // Second ACK with LONG gap (1000ms = 10x SendIdleTimeout)
    QUIC_ACK_EVENT AckEvent2{};
    AckEvent2.TimeNow = 1150000; // 1150ms (1000ms gap > 4*RttVariance and > SendIdleTimeoutMs)
    AckEvent2.LargestAck = 20;
    AckEvent2.LargestSentPacketNumber = 20;
    AckEvent2.NumRetransmittableBytes = 1280;
    AckEvent2.NumTotalAckedRetransmittableBytes = 2560;
    AckEvent2.SmoothedRtt = 50000;
    AckEvent2.MinRttValid = FALSE;
    AckEvent2.IsImplicit = FALSE;
    AckEvent2.HasLoss = FALSE;
    AckEvent2.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent2.AdjustedAckTime = AckEvent2.TimeNow;
    AckEvent2.AckedPackets = NULL;

    Cubic->BytesInFlight = 1280;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent2);

    // Verify TimeOfCongAvoidStart was adjusted forward to compensate for gap
    ASSERT_GT(Cubic->TimeOfCongAvoidStart, TimeOfCongAvoidAfterFirstAck);
    
    // The adjustment should be approximately the gap size (1000ms)
    uint64_t Adjustment = Cubic->TimeOfCongAvoidStart - TimeOfCongAvoidAfterFirstAck;
    ASSERT_GT(Adjustment, 900000u); // At least 900ms adjustment
}

//
// Test 24: HyStart++ RTT Decrease Resumes Slow Start
// Scenario: Tests the HyStart++ spurious detection mechanism where a decrease in RTT while
// in HYSTART_ACTIVE (Conservative Slow Start) causes a transition back to HYSTART_NOT_STARTED,
// resuming aggressive slow start. This handles the case where the initial RTT increase that
// triggered conservative slow start was spurious (e.g., due to transient network conditions).
// The test: 1) Forces HyStart into ACTIVE state with a baseline RTT, 2) Sends ACK with lower
// RTT, 3) Verifies transition back to NOT_STARTED and CWndSlowStartGrowthDivisor reset to 1.
//
TEST(DeepTest_CubicTest, HyStart_RttDecreaseResumesSlowStart)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;
    
    InitializeMockConnection(Connection, 1280);
    Connection.Settings.HyStartEnabled = TRUE; // Enable HyStart++

    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Set up HyStart in ACTIVE state (Conservative Slow Start)
    Cubic->HyStartState = HYSTART_ACTIVE;
    Cubic->CWndSlowStartGrowthDivisor = 2; // Conservative growth
    Cubic->ConservativeSlowStartRounds = 3; // Still in conservative rounds
    Cubic->CssBaselineMinRtt = 60000; // 60ms baseline
    Cubic->MinRttInCurrentRound = UINT64_MAX;
    Cubic->HyStartAckCount = 0;
    Cubic->HyStartRoundEnd = 50;
    Cubic->CongestionWindow = 20000;
    Cubic->SlowStartThreshold = UINT32_MAX; // Still in slow start
    Cubic->BytesInFlight = 1280;

    Connection.Send.NextPacketNumber = 60;

    // Send ACK with LOWER RTT than baseline (spurious detection)
    QUIC_ACK_EVENT AckEvent{};
    AckEvent.TimeNow = 200000;
    AckEvent.LargestAck = 55; // Triggers RTT round check
    AckEvent.LargestSentPacketNumber = 60;
    AckEvent.NumRetransmittableBytes = 1280;
    AckEvent.NumTotalAckedRetransmittableBytes = 1280;
    AckEvent.SmoothedRtt = 50000;
    AckEvent.MinRtt = 50000; // Lower than CssBaselineMinRtt (60ms)
    AckEvent.MinRttValid = TRUE;
    AckEvent.IsImplicit = FALSE;
    AckEvent.HasLoss = FALSE;
    AckEvent.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent.AdjustedAckTime = AckEvent.TimeNow;
    AckEvent.AckedPackets = NULL;

    // Update RTT in first few ACKs to set MinRttInCurrentRound
    Cubic->HyStartAckCount = 1; // Within N_SAMPLING range
    
    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent);

    // Verify RTT decrease caused transition back to NOT_STARTED
    ASSERT_EQ(Cubic->HyStartState, HYSTART_NOT_STARTED);
    
    // Verify growth divisor reset to aggressive (1)
    ASSERT_EQ(Cubic->CWndSlowStartGrowthDivisor, 1u);
    
    // Verify still in slow start
    ASSERT_LT(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
}

//
// Test 25: Complete Lifecycle - Slow Start to Recovery and Back
// Scenario: Tests a complete CUBIC lifecycle covering multiple states: initialization → slow start →
// congestion avoidance → loss → recovery → recovery exit → congestion avoidance. This integration
// test validates that all state transitions work correctly together and that window values, flags,
// and timestamps are properly maintained through the complete cycle. This test exercises multiple
// public APIs in sequence: Initialize → OnDataAcknowledged (slow start) → OnDataAcknowledged
// (trigger transition to CA) → OnDataLost (enter recovery) → OnDataAcknowledged (exit recovery).
//
TEST(DeepTest_CubicTest, CompleteLifecycle_SlowStartToRecoveryAndBack)
{
    QUIC_CONNECTION Connection;
    QUIC_SETTINGS_INTERNAL Settings{};

    Settings.InitialWindowPackets = 10;
    Settings.SendIdleTimeoutMs = 1000;

    InitializeMockConnection(Connection, 1280);
    CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

    QUIC_CONGESTION_CONTROL_CUBIC* Cubic = &Connection.CongestionControl.Cubic;

    // Phase 1: Slow Start - ACK packets to grow window
    Cubic->BytesInFlight = 5000;
    uint32_t InitialWindow = Cubic->CongestionWindow;

    QUIC_ACK_EVENT AckEvent1{};
    AckEvent1.TimeNow = 100000;
    AckEvent1.LargestAck = 5;
    AckEvent1.LargestSentPacketNumber = 10;
    AckEvent1.NumRetransmittableBytes = 5000; // Multiple packets
    AckEvent1.NumTotalAckedRetransmittableBytes = 5000;
    AckEvent1.SmoothedRtt = 50000;
    AckEvent1.MinRttValid = FALSE;
    AckEvent1.IsImplicit = FALSE;
    AckEvent1.HasLoss = FALSE;
    AckEvent1.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent1.AdjustedAckTime = AckEvent1.TimeNow;
    AckEvent1.AckedPackets = NULL;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent1);

    // Verify window grew (slow start)
    ASSERT_GT(Cubic->CongestionWindow, InitialWindow);
    ASSERT_LT(Cubic->CongestionWindow, Cubic->SlowStartThreshold);

    // Phase 2: Force transition to Congestion Avoidance
    Cubic->CongestionWindow = Cubic->SlowStartThreshold;
    Cubic->AimdWindow = Cubic->CongestionWindow;
    Cubic->WindowMax = Cubic->CongestionWindow;
    Cubic->TimeOfCongAvoidStart = 150000;
    uint32_t WindowBeforeLoss = Cubic->CongestionWindow;

    // Phase 3: Packet Loss - Enter Recovery
    Connection.Send.NextPacketNumber = 30;
    QUIC_LOSS_EVENT LossEvent{};
    LossEvent.LargestPacketNumberLost = 15;
    LossEvent.LargestSentPacketNumber = 30;
    LossEvent.NumRetransmittableBytes = 2560;
    LossEvent.PersistentCongestion = FALSE;

    Cubic->BytesInFlight = 10000;
    Cubic->HasHadCongestionEvent = FALSE;

    Connection.CongestionControl.QuicCongestionControlOnDataLost(
        &Connection.CongestionControl, &LossEvent);

    // Verify entered recovery
    ASSERT_TRUE(Cubic->IsInRecovery);
    ASSERT_TRUE(Cubic->HasHadCongestionEvent);
    ASSERT_LT(Cubic->CongestionWindow, WindowBeforeLoss); // Window reduced
    ASSERT_EQ(Cubic->RecoverySentPacketNumber, 30u);

    // Phase 4: More packets sent during recovery
    Connection.Send.NextPacketNumber = 50;

    // Phase 5: ACK for post-recovery packet - Exit Recovery
    QUIC_ACK_EVENT AckEvent2{};
    AckEvent2.TimeNow = 300000;
    AckEvent2.LargestAck = 35; // > RecoverySentPacketNumber
    AckEvent2.LargestSentPacketNumber = 50;
    AckEvent2.NumRetransmittableBytes = 1280;
    AckEvent2.NumTotalAckedRetransmittableBytes = 6280;
    AckEvent2.SmoothedRtt = 50000;
    AckEvent2.MinRttValid = FALSE;
    AckEvent2.IsImplicit = FALSE;
    AckEvent2.HasLoss = FALSE;
    AckEvent2.IsLargestAckedPacketAppLimited = FALSE;
    AckEvent2.AdjustedAckTime = AckEvent2.TimeNow;
    AckEvent2.AckedPackets = NULL;

    Cubic->BytesInFlight = 8000;

    Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
        &Connection.CongestionControl, &AckEvent2);

    // Verify exited recovery
    ASSERT_FALSE(Cubic->IsInRecovery);
    ASSERT_FALSE(Cubic->IsInPersistentCongestion);
    
    // Verify back in congestion avoidance
    ASSERT_GE(Cubic->CongestionWindow, Cubic->SlowStartThreshold);
    
    // Verify TimeOfCongAvoidStart updated
    ASSERT_EQ(Cubic->TimeOfCongAvoidStart, AckEvent2.TimeNow);
    
    // Verify TimeOfLastAck updated
    ASSERT_EQ(Cubic->TimeOfLastAck, AckEvent2.TimeNow);
    ASSERT_TRUE(Cubic->TimeOfLastAckValid);
}
