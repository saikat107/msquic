# Test Reflection for CUBIC Congestion Control

This document tracks all tests for the CUBIC component, including reasoning about coverage, contract safety, and differentiation from other tests.

## Existing Tests Analysis

### Test 1: InitializeComprehensive
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Verifies comprehensive initialization of CUBIC state including settings, function pointers, state flags, HyStart fields, and zero-initialized fields.  
**Primary APIs**: CubicCongestionControlInitialize  
**Contract reasoning**: 
- Preconditions: Valid Connection, Settings with InitialWindowPackets=10, MTU=1280
- Object invariants maintained: All fields properly initialized
**Expected coverage**: Lines 915-940 (initialization function)  
**Quality**: Existing test, quality TBD

### Test 2: InitializeBoundaries
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests initialization with boundary values (min/max InitialWindowPackets, various MTU sizes)  
**Primary APIs**: CubicCongestionControlInitialize  
**Expected coverage**: Same initialization paths with different inputs  
**Quality**: Existing test, quality TBD

### Test 3: MultipleSequentialInitializations
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests that CUBIC can be re-initialized multiple times safely  
**Primary APIs**: CubicCongestionControlInitialize  
**Expected coverage**: Initialization repeated  
**Quality**: Existing test, quality TBD

### Test 4: CanSendScenarios
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests CanSend logic with various BytesInFlight and Exemptions combinations  
**Primary APIs**: CubicCongestionControlCanSend  
**Expected coverage**: Lines 129-135 (CanSend function)  
**Quality**: Existing test, quality TBD

### Test 5: SetExemption
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests setting exemptions  
**Primary APIs**: CubicCongestionControlSetExemption  
**Expected coverage**: Lines 139-145  
**Quality**: Existing test, quality TBD

### Test 6: GetSendAllowanceScenarios
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests send allowance calculation without pacing  
**Primary APIs**: CubicCongestionControlGetSendAllowance  
**Expected coverage**: Lines 179-242 (non-pacing paths)  
**Quality**: Existing test, quality TBD

### Test 7: GetSendAllowanceWithActivePacing
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests send allowance with pacing enabled  
**Primary APIs**: CubicCongestionControlGetSendAllowance  
**Expected coverage**: Lines 206-240 (pacing calculation paths)  
**Quality**: Existing test, quality TBD

### Test 8: GetterFunctions
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests simple getter functions  
**Primary APIs**: GetExemptions, GetBytesInFlightMax, GetCongestionWindow, IsAppLimited, SetAppLimited  
**Expected coverage**: Lines 858-890  
**Quality**: Existing test, quality TBD

### Test 9: ResetScenarios
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests Reset with FullReset=TRUE and FALSE  
**Primary APIs**: CubicCongestionControlReset  
**Expected coverage**: Lines 149-175  
**Quality**: Existing test, quality TBD

### Test 10: OnDataSent_IncrementsBytesInFlight
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests OnDataSent increments BytesInFlight and decrements exemptions  
**Primary APIs**: CubicCongestionControlOnDataSent  
**Expected coverage**: Lines 372-399  
**Quality**: Existing test, quality TBD

### Test 11: OnDataInvalidated_DecrementsBytesInFlight
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests OnDataInvalidated decrements BytesInFlight  
**Primary APIs**: CubicCongestionControlOnDataInvalidated  
**Expected coverage**: Lines 402-415  
**Quality**: Existing test, quality TBD

### Test 12: OnDataAcknowledged_BasicAck
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests basic ACK processing  
**Primary APIs**: CubicCongestionControlOnDataAcknowledged  
**Expected coverage**: Lines 438-718 (partial - basic path)  
**Quality**: Existing test, quality TBD

### Test 13: OnDataLost_WindowReduction
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests loss handling and window reduction  
**Primary APIs**: CubicCongestionControlOnDataLost  
**Expected coverage**: Lines 721-752  
**Quality**: Existing test, quality TBD

### Test 14: OnEcn_CongestionSignal
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests ECN congestion signal handling  
**Primary APIs**: CubicCongestionControlOnEcn  
**Expected coverage**: Lines 756-776  
**Quality**: Existing test, quality TBD

### Test 15: GetNetworkStatistics_RetrieveStats
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests GetNetworkStatistics  
**Primary APIs**: CubicCongestionControlGetNetworkStatistics  
**Expected coverage**: Lines 419-435  
**Quality**: Existing test, quality TBD

### Test 16: MiscFunctions_APICompleteness
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests LogOutFlowStatus and other misc functions  
**Primary APIs**: CubicCongestionControlLogOutFlowStatus  
**Expected coverage**: Lines 826-846  
**Quality**: Existing test, quality TBD

### Test 17: HyStart_StateTransitions
**Location**: src/core/unittest/CubicTest.cpp  
**Scenario**: Tests HyStart state transitions  
**Primary APIs**: Indirectly via OnDataAcknowledged  
**Expected coverage**: Lines 83-115 (HyStartChangeState), 118-125 (HyStartResetPerRttRound)  
**Quality**: Existing test, quality TBD

---

## Coverage Gaps Identified

Based on the RCI and existing tests, potential gaps include:

1. **CubeRoot function**: Not directly testable (static/internal), but indirectly via congestion avoidance
2. **Congestion avoidance CUBIC formula**: Complex logic in OnDataAcknowledged (lines ~520-650) may not be fully covered
3. **Spurious congestion event handling**: OnSpuriousCongestionEvent may need dedicated test
4. **Persistent congestion**: Full persistent congestion scenario
5. **HyStart detailed transitions**: More comprehensive HyStart scenarios
6. **Pacing edge cases**: Overflow, extreme RTT values
7. **Recovery exit scenarios**: Various ACK patterns during recovery
8. **Window growth in slow start vs congestion avoidance**: Boundary transition
9. **Complex ACK scenarios**: Multiple ACKs, out-of-order, etc.
10. **UpdateBlockedState transitions**: Flow control blocking/unblocking

---

## New Tests to Add

(Tests will be added below as generated, with full reflection entries)


## New Test 18: SpuriousCongestionEvent_CompleteReversion
**Test name**: DeepTest_CubicTest.SpuriousCongestionEvent_CompleteReversion  
**Scenario**: Tests complete state reversion when a congestion event is determined to be spurious. Simulates: Initialize → Trigger loss (window reduced) → Call OnSpuriousCongestionEvent → Verify full restoration of pre-congestion state.  
**Primary API target**: CubicCongestionControlOnSpuriousCongestionEvent  
**Contract reasoning**:
- Preconditions: CUBIC initialized, congestion event previously occurred (IsInRecovery==TRUE), Prev* values saved
- Object invariants: All window values, thresholds, and recovery flags correctly restored
- State machine: Recovery state transitions from "In Recovery" back to "Not In Recovery"
**Expected coverage**: Lines 788-823 (OnSpuriousCongestionEvent function body, including all Prev* restoration logic)  
**Non-redundancy**: Existing test MiscFunctions_APICompleteness calls the function but doesn't verify the complete reversion logic with specific pre/post congestion window values and all Prev* fields. This test creates a proper congestion scenario first, then validates complete state restoration.  
**Quality score**: TBD (will run test-quality-checker)

## New Test 19: SpuriousCongestionEvent_NotInRecovery
**Test name**: DeepTest_CubicTest.SpuriousCongestionEvent_NotInRecovery  
**Scenario**: Tests guard clause in OnSpuriousCongestionEvent that returns FALSE when not in recovery (IsInRecovery==FALSE). Verifies early-return path.  
**Primary API target**: CubicCongestionControlOnSpuriousCongestionEvent (early return path)  
**Contract reasoning**:
- Preconditions: CUBIC initialized, not in recovery state
- Expected: Function returns FALSE immediately, no state changes
**Expected coverage**: Lines 788-796 (function entry and guard clause)  
**Non-redundancy**: Tests the negative case (not in recovery) which is distinct from Test 18 that tests the positive case (in recovery, full reversion).  
**Quality score**: TBD


## New Test 20: PersistentCongestion_DrasticReduction
**Test name**: DeepTest_CubicTest.PersistentCongestion_DrasticReduction  
**Scenario**: Tests drastic window reduction when persistent congestion is detected (prolonged loss). Window reduced to minimum (2 packets), all other windows set to β * original.  
**Primary API target**: CubicCongestionControlOnDataLost → OnCongestionEvent (persistent path)  
**Contract reasoning**:
- Preconditions: CUBIC initialized, large CongestionWindow set, LossEvent with PersistentCongestion=TRUE
- Object invariants: CongestionWindow set to minimum safe value (2*MTU), all thresholds reduced by β
- State machine: Enters persistent congestion state, HyStart set to DONE
**Expected coverage**: Lines 307-328 (persistent congestion branch in OnCongestionEvent)  
**Non-redundancy**: Existing tests don't explicitly test persistent congestion flag. This test validates the special drastic reduction logic distinct from normal congestion handling.  
**Quality score**: TBD

## New Test 21: NonPersistentCongestion_NormalReduction
**Test name**: DeepTest_CubicTest.NonPersistentCongestion_NormalReduction  
**Scenario**: Tests normal CUBIC congestion window reduction (β = 0.7) for non-persistent loss events. Verifies K (time to recover to W_max) calculated via CubeRoot.  
**Primary API target**: CubicCongestionControlOnDataLost → OnCongestionEvent (normal path)  
**Contract reasoning**:
- Preconditions: CUBIC initialized, LossEvent with PersistentCongestion=FALSE
- Expected: Window reduced by multiplicative decrease factor, K calculated for CUBIC recovery
**Expected coverage**: Lines 330-368 (non-persistent congestion branch with K calculation)  
**Non-redundancy**: Complements Test 20 by testing the normal congestion path. Validates CubeRoot indirectly via K calculation. Tests fast convergence logic (WindowLastMax handling).  
**Quality score**: TBD

## New Test 22: SlowStartToCongestionAvoidance_Transition
**Test name**: DeepTest_CubicTest.SlowStartToCongestionAvoidance_Transition  
**Scenario**: Tests the critical transition from slow start (exponential growth) to congestion avoidance (CUBIC formula). Exercises complete congestion control lifecycle: init → slow start → loss → recovery → congestion avoidance.  
**Primary API target**: CubicCongestionControlOnDataAcknowledged (both slow start and congestion avoidance paths)  
**Contract reasoning**:
- Preconditions: CUBIC initialized, proper sequence of Send/ACK/Loss events
- State transitions: Slow Start → Recovery → Congestion Avoidance
- Invariants: Window growth rate changes from exponential to sublinear at SSThresh boundary
**Expected coverage**: Lines 438-718 (OnDataAcknowledged), including:
  - Slow start growth logic (~lines 480-550)
  - Congestion avoidance CUBIC formula (~lines 560-650)
  - Recovery exit logic (~lines 460-478)
  - K calculation and usage in CUBIC formula
**Non-redundancy**: Most comprehensive test covering full congestion control cycle. Existing tests check individual pieces (ACK, Loss, etc.) but don't validate the complete transition and interaction between slow start, recovery, and congestion avoidance modes. Indirectly validates CubeRoot via K usage in window calculations.  
**Quality score**: TBD

