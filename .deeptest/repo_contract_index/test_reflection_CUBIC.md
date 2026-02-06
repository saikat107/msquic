# Test Reflection Document: CUBIC Component

This document tracks all tests generated for the CUBIC congestion control component, their scenarios, contract reasoning, and non-redundancy justification.

## Existing Tests (from CubicTest.cpp)

### Test 1: DeepTest_CubicTest.InitializeComprehensive
**Scenario**: Comprehensive initialization verification with all function pointers, state flags, HyStart fields, and zero-initialized fields.

**Primary APIs**: CubicCongestionControlInitialize

**Contract Reasoning**:
- Ensures all 17 function pointers are properly set
- Verifies initial state flags are FALSE
- Confirms HyStart state initialized to HYSTART_NOT_STARTED
- Checks CongestionWindow calculation based on MTU * InitialWindowPackets

**Expected Coverage**: Lines 915-940 (initialization function)

**Non-redundancy**: First comprehensive test of initialization

---

### Test 2: DeepTest_CubicTest.InitializeBoundaries
**Scenario**: Tests initialization with boundary parameter values (min/max MTU, min/max window packets, min/max timeout).

**Primary APIs**: CubicCongestionControlInitialize

**Contract Reasoning**:
- Tests extreme but valid parameter combinations
- Ensures robustness across configuration space

**Expected Coverage**: Lines 915-940 (with various parameter paths)

**Non-redundancy**: Tests boundary conditions not covered by basic initialization

---

### Test 3: DeepTest_CubicTest.MultipleSequentialInitializations
**Scenario**: Re-initialization with different settings to verify state reset.

**Primary APIs**: CubicCongestionControlInitialize

**Contract Reasoning**:
- Ensures re-initialization properly resets state
- Verifies settings changes are reflected in window size

**Expected Coverage**: Lines 915-940 (multiple times)

**Non-redundancy**: Tests re-initialization scenario

---

### Test 4: DeepTest_CubicTest.CanSendScenarios
**Scenario**: Tests CanSend logic with available window, blocked state, and exemptions.

**Primary APIs**: CubicCongestionControlCanSend

**Contract Reasoning**:
- BytesInFlight < CongestionWindow → can send
- BytesInFlight >= CongestionWindow and Exemptions == 0 → blocked
- Exemptions > 0 → can send regardless of window

**Expected Coverage**: Lines 129-135

**Non-redundancy**: First test of CanSend API

---

### Test 5: DeepTest_CubicTest.SetExemption
**Scenario**: Tests SetExemption and its interaction with CanSend.

**Primary APIs**: CubicCongestionControlSetExemption, CubicCongestionControlCanSend

**Contract Reasoning**:
- Exemptions allow sending beyond congestion window
- Postcondition: Exemptions field updated

**Expected Coverage**: Lines 139-145, 129-135

**Non-redundancy**: First test of SetExemption API

---

### Test 6: DeepTest_CubicTest.GetSendAllowanceScenarios
**Scenario**: Tests GetSendAllowance without pacing (disabled or no RTT sample).

**Primary APIs**: CubicCongestionControlGetSendAllowance

**Contract Reasoning**:
- Returns 0 if CC blocked
- Returns (CongestionWindow - BytesInFlight) if pacing disabled

**Expected Coverage**: Lines 179-204

**Non-redundancy**: First test of GetSendAllowance without pacing

---

### Test 7: DeepTest_CubicTest.GetSendAllowanceWithActivePacing
**Scenario**: Tests GetSendAllowance with pacing enabled and RTT sample available.

**Primary APIs**: CubicCongestionControlGetSendAllowance

**Contract Reasoning**:
- Pacing enabled requires: PacingEnabled, GotFirstRttSample, SmoothedRtt >= QUIC_MIN_PACING_RTT
- Returns paced allowance based on estimated window and time since last send

**Expected Coverage**: Lines 179-242 (pacing path)

**Non-redundancy**: Tests pacing logic not covered by previous test

---

### Test 8: DeepTest_CubicTest.GetterFunctions
**Scenario**: Tests all simple getter functions.

**Primary APIs**: GetExemptions, GetBytesInFlightMax, GetCongestionWindow, IsAppLimited, SetAppLimited

**Contract Reasoning**:
- Simple field accessors
- IsAppLimited always returns FALSE
- SetAppLimited is no-op

**Expected Coverage**: Lines 849-890

**Non-redundancy**: First test of getter APIs

---

### Test 9: DeepTest_CubicTest.ResetScenarios
**Scenario**: Tests Reset with full and partial reset scenarios.

**Primary APIs**: CubicCongestionControlReset

**Contract Reasoning**:
- FullReset=TRUE resets BytesInFlight to 0
- FullReset=FALSE preserves BytesInFlight
- Always resets window, threshold, recovery state

**Expected Coverage**: Lines 149-175

**Non-redundancy**: First test of Reset API

---

### Test 10: DeepTest_CubicTest.OnDataSent_IncrementsBytesInFlight
**Scenario**: Tests OnDataSent increments BytesInFlight and decrements exemptions.

**Primary APIs**: CubicCongestionControlOnDataSent

**Contract Reasoning**:
- BytesInFlight += NumRetransmittableBytes
- BytesInFlightMax updated if exceeded
- LastSendAllowance reduced
- Exemptions decremented if > 0

**Expected Coverage**: Lines 372-398

**Non-redundancy**: First test of OnDataSent API

---

### Test 11: DeepTest_CubicTest.OnDataInvalidated_DecrementsBytesInFlight
**Scenario**: Tests OnDataInvalidated decrements BytesInFlight and may unblock.

**Primary APIs**: CubicCongestionControlOnDataInvalidated

**Contract Reasoning**:
- BytesInFlight -= NumRetransmittableBytes
- Returns TRUE if transitioned from blocked to unblocked

**Expected Coverage**: Lines 402-415

**Non-redundancy**: First test of OnDataInvalidated API

---

### Test 12: DeepTest_CubicTest.OnDataAcknowledged_BasicAck
**Scenario**: Tests OnDataAcknowledged basic ACK handling in slow start.

**Primary APIs**: CubicCongestionControlOnDataAcknowledged

**Contract Reasoning**:
- BytesInFlight decremented
- In slow start: window grows exponentially
- TimeOfLastAck updated

**Expected Coverage**: Lines 438-548 (slow start path)

**Non-redundancy**: First test of OnDataAcknowledged in slow start

---

### Test 13: DeepTest_CubicTest.OnDataLost_WindowReduction
**Scenario**: Tests OnDataLost triggers congestion event and window reduction.

**Primary APIs**: CubicCongestionControlOnDataLost

**Contract Reasoning**:
- Loss triggers OnCongestionEvent
- Window reduced by BETA (0.7)
- BytesInFlight decremented

**Expected Coverage**: Lines 721-752, 272-368 (congestion event)

**Non-redundancy**: First test of OnDataLost and congestion event

---

### Test 14: DeepTest_CubicTest.OnEcn_CongestionSignal
**Scenario**: Tests OnEcn triggers congestion event.

**Primary APIs**: CubicCongestionControlOnEcn

**Contract Reasoning**:
- ECN signal triggers OnCongestionEvent with Ecn=TRUE
- EcnCongestionCount incremented

**Expected Coverage**: Lines 756-784

**Non-redundancy**: First test of OnEcn API

---

### Test 15: DeepTest_CubicTest.GetNetworkStatistics_RetrieveStats
**Scenario**: Tests GetNetworkStatistics populates output structure.

**Primary APIs**: CubicCongestionControlGetNetworkStatistics

**Contract Reasoning**:
- Reads BytesInFlight, CongestionWindow, etc.
- Populates NetworkStatistics output

**Expected Coverage**: Lines 419-434

**Non-redundancy**: First test of GetNetworkStatistics API

---

### Test 16: DeepTest_CubicTest.MiscFunctions_APICompleteness
**Scenario**: Tests LogOutFlowStatus and spurious congestion handling.

**Primary APIs**: CubicCongestionControlLogOutFlowStatus, CubicCongestionControlOnSpuriousCongestionEvent

**Contract Reasoning**:
- LogOutFlowStatus emits trace event
- OnSpuriousCongestionEvent reverts to previous state

**Expected Coverage**: Lines 826-846, 788-823

**Non-redundancy**: First test of these APIs

---

### Test 17: DeepTest_CubicTest.HyStart_StateTransitions
**Scenario**: Tests HyStart++ state transitions.

**Primary APIs**: CubicCongestionControlOnDataAcknowledged (with HyStart logic)

**Contract Reasoning**:
- HyStart enabled via Settings.HyStartEnabled
- NOT_STARTED → ACTIVE on RTT increase
- ACTIVE → DONE after conservative rounds

**Expected Coverage**: Lines 476-538 (HyStart logic in OnDataAcknowledged)

**Non-redundancy**: First test of HyStart state machine

---

## Coverage Analysis

Based on existing tests, we have good coverage of:
- ✅ Initialization (comprehensive)
- ✅ CanSend basic scenarios
- ✅ Exemptions
- ✅ GetSendAllowance (with and without pacing)
- ✅ Reset
- ✅ OnDataSent
- ✅ OnDataInvalidated
- ✅ OnDataAcknowledged (basic slow start)
- ✅ OnDataLost
- ✅ OnEcn
- ✅ GetNetworkStatistics
- ✅ Getter functions
- ✅ SpuriousCongestionEvent
- ✅ HyStart state transitions

**Coverage Gaps Identified**:

1. **CongestionAvoidance deep scenarios**:
   - CUBIC window calculation (lines 610-623)
   - AIMD window calculation (lines 648-657)
   - Reno-friendly vs CUBIC regions (lines 659-670)
   - Window capping at 2*BytesInFlightMax (lines 683-685)
   - Time gap handling in congestion avoidance (lines 580-588)

2. **Recovery scenarios**:
   - Exiting recovery when LargestAck > RecoverySentPacketNumber (lines 454-467)
   - Multiple losses during recovery

3. **Persistent congestion**:
   - Deep testing of persistent congestion path (lines 307-328)
   - Route state changes

4. **HyStart++ edge cases**:
   - ACTIVE → NOT_STARTED (spurious exit) (lines 512-517)
   - MinRtt sampling logic (lines 477-486)
   - Conservative slow start window growth

5. **Edge cases**:
   - Overflow protection in pacing (lines 234-237)
   - Overflow in CUBIC window calculation (lines 625-631)
   - Zero bytes acked (line 469-471)
   - DeltaT clamping (lines 616-618)

6. **Internal helper coverage**:
   - CubeRoot function (lines 45-62) - not directly testable but exercised via congestion avoidance
   - UpdateBlockedState transitions

---

## New Tests to Generate

To reach 100% coverage, we need tests for:

1. **Congestion Avoidance CUBIC window**: Test that exercises CUBIC window calculation with various time deltas
2. **Congestion Avoidance AIMD window**: Test AIMD window growth
3. **Reno-friendly region**: Test where AIMD > CUBIC
4. **CUBIC region**: Test where CUBIC > AIMD
5. **Window capping**: Test 2*BytesInFlightMax limit
6. **Recovery exit**: Test exiting recovery
7. **Multiple losses in recovery**: Test that subsequent losses don't trigger new congestion events
8. **Persistent congestion deep**: Test persistent congestion path thoroughly
9. **HyStart spurious exit**: Test ACTIVE → NOT_STARTED transition
10. **Overflow scenarios**: Test overflow protection in pacing and CUBIC calculations
11. **Time gap handling**: Test window growth freezing during ACK gaps
12. **Zero bytes acked**: Test no-op path
13. **CubeRoot edge cases**: Test via congestion avoidance with specific WindowMax values

---

