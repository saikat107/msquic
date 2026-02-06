# Test Reflection Document - CUBIC Congestion Control Component

## Purpose
This document tracks all tests generated for the CUBIC component, ensuring comprehensive coverage while avoiding redundancy. Each test entry includes scenario description, API targets, contract reasoning, expected coverage, and differentiation from existing tests.

## Existing Tests Summary (from CubicTest.cpp)

### Test 1: InitializeComprehensive
- **Scenario**: Comprehensive initialization verification
- **APIs**: CubicCongestionControlInitialize
- **Coverage**: Initialization path, function pointer setup, state flags, HyStart initialization

### Test 2: InitializeBoundaries
- **Scenario**: Initialization with boundary parameter values
- **APIs**: CubicCongestionControlInitialize
- **Coverage**: Boundary MTU and settings values

### Test 3: MultipleSequentialInitializations
- **Scenario**: Re-initialization behavior
- **APIs**: CubicCongestionControlInitialize
- **Coverage**: Re-initialization path

### Test 4: CanSendScenarios
- **Scenario**: CanSend logic with various BytesInFlight and Exemptions
- **APIs**: CubicCongestionControlCanSend
- **Coverage**: CanSend decision branches

### Test 5: SetExemption
- **Scenario**: Setting exemption counts
- **APIs**: CubicCongestionControlSetExemption
- **Coverage**: Exemption setter

### Test 6: GetSendAllowanceScenarios
- **Scenario**: GetSendAllowance under different conditions
- **APIs**: CubicCongestionControlGetSendAllowance
- **Coverage**: Send allowance calculation without pacing

### Test 7: GetSendAllowanceWithActivePacing
- **Scenario**: Send allowance with pacing enabled
- **APIs**: CubicCongestionControlGetSendAllowance
- **Coverage**: Pacing logic branch

### Test 8: GetterFunctions
- **Scenario**: All simple getter functions
- **APIs**: GetExemptions, GetBytesInFlightMax, GetCongestionWindow
- **Coverage**: Getter functions

### Test 9: ResetScenarios
- **Scenario**: Reset with FullReset TRUE and FALSE
- **APIs**: CubicCongestionControlReset
- **Coverage**: Reset branches

### Test 10: OnDataSent_IncrementsBytesInFlight
- **Scenario**: Data sent increases BytesInFlight
- **APIs**: CubicCongestionControlOnDataSent
- **Coverage**: OnDataSent basic path

### Test 11: OnDataInvalidated_DecrementsBytesInFlight
- **Scenario**: Data invalidation decreases BytesInFlight
- **APIs**: CubicCongestionControlOnDataInvalidated
- **Coverage**: OnDataInvalidated path

### Test 12: OnDataAcknowledged_BasicAck
- **Scenario**: Basic ACK processing
- **APIs**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: OnDataAcknowledged basic path

### Test 13: OnDataLost_WindowReduction
- **Scenario**: Packet loss handling
- **APIs**: CubicCongestionControlOnDataLost
- **Coverage**: Loss handling path

### Test 14: OnEcn_CongestionSignal
- **Scenario**: ECN congestion notification
- **APIs**: CubicCongestionControlOnEcn
- **Coverage**: ECN handling path

### Test 15: GetNetworkStatistics_RetrieveStats
- **Scenario**: Statistics retrieval
- **APIs**: CubicCongestionControlGetNetworkStatistics
- **Coverage**: GetNetworkStatistics

### Test 16: MiscFunctions_APICompleteness
- **Scenario**: Miscellaneous small functions
- **APIs**: SetExemption, GetExemptions, OnDataInvalidated, GetCongestionWindow, LogOutFlowStatus, OnSpuriousCongestionEvent
- **Coverage**: Various misc functions

### Test 17: HyStart_StateTransitions
- **Scenario**: HyStart state transitions
- **APIs**: CubicCongestionControlOnDataAcknowledged (triggers HyStart logic)
- **Coverage**: HyStart basic behavior

## Coverage Gaps Identified

Based on RCI analysis and existing tests, the following coverage gaps exist:

### 1. CubeRoot Function
- **Gap**: No direct test of CubeRoot with various inputs
- **Needed**: Boundary values (0, 1, 8, 27, 64, 125, 1000, UINT32_MAX)

### 2. Congestion Avoidance (OnDataAcknowledged)
- **Gap**: No test specifically covering congestion avoidance phase
- **Needed**: Test with CongestionWindow >= SlowStartThreshold

### 3. HyStart Detailed Transitions
- **Gap**: HyStart state transitions not fully covered
- **Needed**: HYSTART_NOT_STARTED -> ACTIVE, ACTIVE -> DONE, ACTIVE -> NOT_STARTED

### 4. Persistent Congestion
- **Gap**: No test for persistent congestion scenario
- **Needed**: OnCongestionEvent with IsPersistentCongestion=TRUE

### 5. Spurious Congestion Event (Detailed)
- **Gap**: OnSpuriousCongestionEvent only briefly tested
- **Needed**: Full state revert verification

### 6. ECN During Recovery
- **Gap**: No test for ECN event during recovery period
- **Needed**: ECN event when IsInRecovery=TRUE

### 7. Loss During Recovery
- **Gap**: No test for loss during recovery period
- **Needed**: OnDataLost when IsInRecovery=TRUE

### 8. Pacing Edge Cases
- **Gap**: Pacing overflow, very small RTT
- **Needed**: Pacing with SmoothedRtt < QUIC_MIN_PACING_RTT, overflow scenarios

### 9. CUBIC Window Growth (Detailed)
- **Gap**: CUBIC formula calculation not directly tested
- **Needed**: Congestion avoidance with time progression, CUBIC vs AIMD window selection

### 10. Fast Convergence
- **Gap**: Fast convergence path in OnCongestionEvent
- **Needed**: Congestion event with WindowLastMax > WindowMax

### 11. Recovery Exit
- **Gap**: Recovery exit on ACK > RecoverySentPacketNumber
- **Needed**: OnDataAcknowledged exiting recovery

### 12. HyStart RTT Sample Collection
- **Gap**: HyStart N-sampling phase
- **Needed**: Multiple ACKs with MinRtt updates

### 13. Conservative Slow Start Rounds
- **Gap**: Multiple RTT rounds in HYSTART_ACTIVE
- **Needed**: CSS round countdown to HYSTART_DONE

### 14. Time-Based Growth Freeze
- **Gap**: Long ACK gap freezing window growth
- **Needed**: OnDataAcknowledged with large TimeSinceLastAck

### 15. Window Capping by BytesInFlightMax
- **Gap**: Window growth limited by BytesInFlightMax
- **Needed**: CongestionWindow capped at 2 * BytesInFlightMax

### 16. Slow Start Overflow to Congestion Avoidance
- **Gap**: CongestionWindow exceeding SlowStartThreshold
- **Needed**: BytesAcked carries over to congestion avoidance

### 17. ECN State Saving
- **Gap**: ECN events don't save previous state
- **Needed**: OnCongestionEvent with Ecn=TRUE vs Ecn=FALSE

### 18. IsAppLimited and SetAppLimited
- **Gap**: These APIs are tested but not thoroughly
- **Needed**: Verify they are proper no-ops for CUBIC

### 19. Pacing in Slow Start vs Congestion Avoidance
- **Gap**: Pacing rate differs in slow start (2x window estimate) vs CA (1.25x)
- **Needed**: GetSendAllowance in both phases

### 20. Multiple Sequential Data Operations
- **Gap**: Realistic sequences of Send/Ack/Loss
- **Needed**: Integration-style scenarios

---

## New Tests to be Added

Tests will be added iteratively below with full reflections.

---

## Test 18: CubeRoot_BoundaryValues

**Test Name**: DeepTest_CubicTest.CubeRoot_BoundaryValues

**Scenario Summary**: Tests the CubeRoot function indirectly by triggering congestion events which compute KCubic using CubeRoot. Exercises the function with small (minimum window) and large (1000 packets) values to verify the shifting nth root algorithm produces correct results across input ranges.

**Primary Public API Target(s)**: 
- CubicCongestionControlOnDataLost (triggers CubeRoot via KCubic calculation in OnCongestionEvent)
- CubeRoot (internal, tested indirectly)

**Contract Reasoning**:
- Preconditions: Valid connection, loss event with proper packet numbers
- Maintains: All object invariants preserved
- KCubic calculation follows CUBIC formula: CubeRoot((WindowMax * (1-BETA) / C))

**Expected Coverage Impact**:
- Lines 44-62 (CubeRoot function)
- Lines 353-358 (KCubic calculation in OnCongestionEvent)

**Non-Redundancy**: First test to specifically verify CubeRoot correctness with boundary values. Existing tests use CubeRoot but don't validate across input ranges.

---

## Test 19: CongestionAvoidance_CubicWindowGrowth

**Test Name**: DeepTest_CubicTest.CongestionAvoidance_CubicWindowGrowth

**Scenario Summary**: Tests CUBIC congestion avoidance phase by manually setting CongestionWindow >= SlowStartThreshold, triggering a congestion event, then sending ACKs to grow the window according to CUBIC formula W_cubic(t) = C*(t-K)^3 + WindowMax.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged (congestion avoidance growth path)
- CubicCongestionControlOnDataLost

**Contract Reasoning**:
- Preconditions: CongestionWindow >= SlowStartThreshold (in CA phase)
- Maintains: Window grows per CUBIC formula, slower than slow start
- Postconditions: Window increases but respects BytesInFlightMax cap

**Expected Coverage Impact**:
- Lines 563-670 (congestion avoidance logic in OnDataAcknowledged)
- Lines 610-623 (CUBIC window calculation)
- Lines 634-657 (AIMD window calculation)

**Non-Redundancy**: First test specifically targeting congestion avoidance growth. Existing tests mostly cover slow start or basic ACK handling without detailed CA logic.

---

## Test 20: PersistentCongestion_SevereWindowReduction

**Test Name**: DeepTest_CubicTest.PersistentCongestion_SevereWindowReduction

**Scenario Summary**: Tests persistent congestion scenario by calling OnDataLost with PersistentCongestion=TRUE. Verifies window reduces to minimum (2 packets), IsInPersistentCongestion flag is set, KCubic=0, and Route.State becomes RouteSuspected.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataLost
- CubicCongestionControlOnCongestionEvent (persistent congestion path)

**Contract Reasoning**:
- Preconditions: Valid loss event with PersistentCongestion=TRUE
- Maintains: Enters persistent congestion state correctly
- Postconditions: Window = 2 * MTU, IsInPersistentCongestion=TRUE, KCubic=0

**Expected Coverage Impact**:
- Lines 307-328 (persistent congestion path in OnCongestionEvent)

**Non-Redundancy**: First test for persistent congestion. This critical path was completely uncovered.

---

## Test 21-22: Spurious Congestion Tests

**Test Name**: DeepTest_CubicTest.SpuriousCongestion_CompleteStateRevert, DeepTest_CubicTest.SpuriousCongestion_NotInRecovery_NoOp

**Scenario Summary**: Tests spurious congestion event handling. Test 21 verifies complete state restoration (WindowPrior, WindowMax, etc.) when OnSpuriousCongestionEvent is called after a non-ECN congestion event. Test 22 verifies no-op behavior when not in recovery.

**Primary Public API Target(s)**:
- CubicCongestionControlOnSpuriousCongestionEvent
- CubicCongestionControlOnDataLost

**Contract Reasoning**:
- Preconditions: Test 21: IsInRecovery=TRUE; Test 22: IsInRecovery=FALSE
- Maintains: Complete state restoration (Test 21) or no changes (Test 22)
- Postconditions: Test 21 restores all Prev* values; Test 22 returns FALSE

**Expected Coverage Impact**:
- Lines 787-823 (OnSpuriousCongestionEvent, both branches)

**Non-Redundancy**: Existing Test 16 briefly calls OnSpuriousCongestionEvent but doesn't verify complete state restoration or the FALSE return path.

---

## Tests 23-24: ECN Congestion Handling

**Test Name**: DeepTest_CubicTest.EcnCongestion_FirstEcnSignal, DeepTest_CubicTest.EcnDuringRecovery_NoAdditionalReduction

**Scenario Summary**: Test 23 verifies first ECN event triggers window reduction and enters recovery. Test 24 verifies subsequent ECN events during recovery don't trigger additional reductions.

**Primary Public API Target(s)**:
- CubicCongestionControlOnEcn

**Contract Reasoning**:
- Preconditions: Test 23: No prior congestion; Test 24: Already in recovery
- Maintains: Recovery state transitions correctly
- Postconditions: Test 23 enters recovery; Test 24 window unchanged

**Expected Coverage Impact**:
- Lines 755-784 (OnEcn, both new event and during-recovery paths)

**Non-Redundancy**: Existing Test 14 only does basic ECN testing. These tests cover the conditional logic for new vs repeated ECN events.

---

## Tests 25-26: Loss and Recovery State Management

**Test Name**: DeepTest_CubicTest.LossDuringRecovery_NoAdditionalReduction, DeepTest_CubicTest.RecoveryExit_OnAckAfterRecoveryStart

**Scenario Summary**: Test 25 verifies loss events during recovery don't cause additional window reduction. Test 26 verifies recovery exit when ACK > RecoverySentPacketNumber.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataLost (Test 25)
- CubicCongestionControlOnDataAcknowledged (Test 26, recovery exit path)

**Contract Reasoning**:
- Preconditions: Both require IsInRecovery=TRUE
- Maintains: Recovery state machine transitions correctly
- Postconditions: Test 25 window unchanged; Test 26 exits recovery

**Expected Coverage Impact**:
- Lines 735-745 (repeated loss during recovery check)
- Lines 453-468 (recovery exit logic in OnDataAcknowledged)

**Non-Redundancy**: Covers recovery state machine edge cases not tested by existing tests.

---

## Tests 27-29: Pacing Edge Cases

**Test Name**: DeepTest_CubicTest.Pacing_VerySmallRtt_SkipsPacing, DeepTest_CubicTest.Pacing_OverflowProtection, DeepTest_CubicTest.Pacing_SlowStartVsCongestionAvoidance

**Scenario Summary**: Test 27 verifies pacing is skipped when RTT < QUIC_MIN_PACING_RTT. Test 28 verifies overflow protection in pacing calculation. Test 29 verifies different EstimatedWnd calculations in slow start (2x) vs CA (1.25x).

**Primary Public API Target(s)**:
- CubicCongestionControlGetSendAllowance

**Contract Reasoning**:
- Preconditions: Various pacing conditions (small RTT, extreme time values, different CC phases)
- Maintains: Pacing logic handles edge cases safely
- Postconditions: Returns clamped/appropriate allowances

**Expected Coverage Impact**:
- Lines 195-242 (GetSendAllowance pacing branches)
- Lines 222-229 (EstimatedWnd calculation for slow start vs CA)
- Lines 234-237 (overflow protection)

**Non-Redundancy**: Existing pacing tests (Tests 6-7) don't cover edge cases like overflow, small RTT, or phase-specific estimates.

---

## Tests 30-33: HyStart Detailed State Machine

**Test Name**: DeepTest_CubicTest.HyStart_NSamplingPhase, DeepTest_CubicTest.HyStart_DelayIncreaseDetection, DeepTest_CubicTest.HyStart_ActiveToDone_CssRoundsExhausted, DeepTest_CubicTest.HyStart_ActiveToNotStarted_SpuriousDetection

**Scenario Summary**: Comprehensive HyStart state machine testing. Test 30 verifies N-sampling phase. Test 31 verifies delay increase detection (NOT_STARTED->ACTIVE). Test 32 verifies CSS rounds countdown (ACTIVE->DONE). Test 33 verifies spurious detection (ACTIVE->NOT_STARTED).

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged (HyStart logic)

**Contract Reasoning**:
- Preconditions: HyStartEnabled=TRUE, various HyStart states
- Maintains: HyStart state machine transitions correctly
- Postconditions: Correct state transitions with proper CWndSlowStartGrowthDivisor updates

**Expected Coverage Impact**:
- Lines 476-538 (HyStart logic in OnDataAcknowledged)
- Lines 481-486 (N-sampling)
- Lines 487-510 (delay increase detection)
- Lines 511-518 (spurious detection)
- Lines 524-537 (round-end and CSS countdown)

**Non-Redundancy**: Existing Test 17 only touches HyStart basics. These tests comprehensively cover all state transitions and edge cases.

---

## Test 34: FastConvergence_PreviousWindowHigher

**Test Name**: DeepTest_CubicTest.FastConvergence_PreviousWindowHigher

**Scenario Summary**: Tests CUBIC fast convergence when WindowLastMax > WindowMax. Verifies WindowMax is reduced by additional factor (0.85x) to accelerate convergence.

**Primary Public API Target(s)**:
- CubicCongestionControlOnCongestionEvent (fast convergence path)

**Contract Reasoning**:
- Preconditions: WindowLastMax > current CongestionWindow
- Maintains: Fast convergence formula applied
- Postconditions: WindowMax = WindowMax * (10+BETA)/20

**Expected Coverage Impact**:
- Lines 335-343 (fast convergence logic in OnCongestionEvent)

**Non-Redundancy**: First test targeting fast convergence path, previously uncovered.

---

## Test 35: SlowStartOverflow_CarryoverToCongestionAvoidance

**Test Name**: DeepTest_CubicTest.SlowStartOverflow_CarryoverToCongestionAvoidance

**Scenario Summary**: Tests case where slow start growth exceeds SlowStartThreshold. Verifies window is clamped to threshold and excess BytesAcked carries over to CA mode.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged

**Contract Reasoning**:
- Preconditions: CongestionWindow close to SlowStartThreshold, large BytesAcked
- Maintains: Smooth transition from slow start to CA
- Postconditions: Window = SlowStartThreshold + CA growth from overflow

**Expected Coverage Impact**:
- Lines 549-560 (slow start overflow handling)

**Non-Redundancy**: First test for slow start overflow scenario, covering edge case at phase boundary.

---

## Test 36: WindowGrowth_CappedByBytesInFlightMax

**Test Name**: DeepTest_CubicTest.WindowGrowth_CappedByBytesInFlightMax

**Scenario Summary**: Tests window growth capping at 2 * BytesInFlightMax. Verifies unbounded growth is prevented when connection is flow control or app-limited.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged

**Contract Reasoning**:
- Preconditions: Large ACK, small BytesInFlightMax
- Maintains: Window capping invariant
- Postconditions: CongestionWindow <= 2 * BytesInFlightMax

**Expected Coverage Impact**:
- Lines 683-685 (window capping logic)

**Non-Redundancy**: First test explicitly verifying BytesInFlightMax capping.

---

## Test 37: TimeBasedGrowthFreeze_LongAckGap

**Test Name**: DeepTest_CubicTest.TimeBasedGrowthFreeze_LongAckGap

**Scenario Summary**: Tests growth freeze when ACK gap exceeds idle timeout. Verifies TimeOfCongAvoidStart is adjusted forward to prevent artificial window inflation.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged

**Contract Reasoning**:
- Preconditions: TimeOfLastAckValid=TRUE, large TimeSinceLastAck
- Maintains: Growth freeze during idle periods
- Postconditions: TimeOfCongAvoidStart adjusted forward

**Expected Coverage Impact**:
- Lines 580-589 (time-based growth freeze logic)

**Non-Redundancy**: First test for idle period growth freeze, important edge case.

---

## Test 38: EcnEvent_StateNotSaved

**Test Name**: DeepTest_CubicTest.EcnEvent_StateNotSaved

**Scenario Summary**: Tests that ECN events don't save previous state (unlike loss events). Verifies OnSpuriousCongestionEvent cannot revert ECN-triggered reductions.

**Primary Public API Target(s)**:
- CubicCongestionControlOnEcn
- CubicCongestionControlOnSpuriousCongestionEvent

**Contract Reasoning**:
- Preconditions: Trigger ECN event (Ecn=TRUE)
- Maintains: State saving branch is skipped
- Postconditions: Spurious revert is unreliable (no saved state)

**Expected Coverage Impact**:
- Lines 297-305 (ECN path that skips state saving)

**Non-Redundancy**: Tests ECN vs non-ECN state saving difference, important contract detail.

---

## Test 39: ZeroBytesAcked_NoWindowGrowth

**Test Name**: DeepTest_CubicTest.ZeroBytesAcked_NoWindowGrowth

**Scenario Summary**: Tests OnDataAcknowledged with NumRetransmittableBytes=0. Verifies no window growth occurs.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged

**Contract Reasoning**:
- Preconditions: AckEvent.NumRetransmittableBytes = 0
- Maintains: Window unchanged
- Postconditions: CongestionWindow same as before

**Expected Coverage Impact**:
- Lines 469-471 (early exit when BytesAcked=0)

**Non-Redundancy**: First test for zero-byte ACK edge case.

---

## Test 40: AckDuringRecovery_NoWindowGrowth

**Test Name**: DeepTest_CubicTest.AckDuringRecovery_NoWindowGrowth

**Scenario Summary**: Tests ACKs during recovery don't grow window. Only after exiting recovery does growth resume.

**Primary Public API Target(s)**:
- CubicCongestionControlOnDataAcknowledged

**Contract Reasoning**:
- Preconditions: IsInRecovery=TRUE, LargestAck <= RecoverySentPacketNumber
- Maintains: Window frozen during recovery
- Postconditions: Window unchanged

**Expected Coverage Impact**:
- Lines 453-468 (recovery check, early exit path)

**Non-Redundancy**: Tests recovery window freeze, complementing Test 26 (recovery exit).

---

## Summary of New Tests

**Total New Tests Added**: 23 tests (Tests 18-40)

**Coverage Goals Achieved**:
- ✅ CubeRoot function boundary testing
- ✅ Congestion avoidance CUBIC formula
- ✅ Persistent congestion
- ✅ Spurious congestion state revert
- ✅ ECN handling (first event, during recovery, state not saved)
- ✅ Loss during recovery
- ✅ Recovery exit on ACK
- ✅ Pacing edge cases (small RTT, overflow, slow start vs CA)
- ✅ HyStart complete state machine (N-sampling, delay detection, CSS rounds, spurious)
- ✅ Fast convergence
- ✅ Slow start overflow to CA
- ✅ Window capping by BytesInFlightMax
- ✅ Time-based growth freeze
- ✅ Zero bytes acknowledged
- ✅ ACK during recovery

**Expected Coverage Improvement**: These tests target 15+ previously uncovered code paths, significantly improving coverage toward 100% goal.

---
