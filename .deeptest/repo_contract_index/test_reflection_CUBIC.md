# Test Reflection - CUBIC Component

This document tracks all tests generated for the CUBIC component, their scenarios, contract reasoning, and non-redundancy justification.

**Component**: CUBIC Congestion Control  
**Source**: `src/core/cubic.c`  
**Test Harness**: `src/core/unittest/CubicTest.cpp`  
**Last Updated**: 2026-02-05

---

## Existing Tests (Baseline)

### Test: InitializeComprehensive
**Scenario**: Verifies comprehensive initialization of CUBIC state including settings, function pointers, flags, and HyStart fields.

**Primary APIs**: `CubicCongestionControlInitialize`

**Contract Reasoning**:
- Preconditions: Valid Connection structure with Paths[0].Mtu=1280, valid Settings
- Invariants maintained: All 18 function pointers set, window > 0, all flags FALSE, HyStart state = NOT_STARTED

**Expected Coverage**: Lines 915-940 (CubicCongestionControlInitialize full function)

**Non-redundancy**: This is the baseline initialization test, verifying all initialization postconditions.

---

## New Tests Generated

### Test 18: PersistentCongestion_WindowResetToMinimum
**Scenario**: When persistent congestion occurs after multiple RTOs, window must reset to absolute minimum (2*MTU).

**Primary APIs**: `CubicCongestionControlOnDataLost`

**Contract Reasoning**:
- Preconditions: Initialized Cc, LossEvent with PersistentCongestion=TRUE
- Invariants: Window reset to 2*DatagramPayloadLength, IsInPersistentCongestion=TRUE
- Persistent congestion is terminal - requires explicit recovery

**Expected Coverage**: Lines 307-328 (persistent congestion branch in OnCongestionEvent)

**Non-redundancy**: Existing loss test doesn't cover persistent congestion path (most severe reduction).

**Quality Score**: Pending test-quality-checker validation

---

### Test 19: SpuriousCongestion_WindowRestoration
**Scenario**: RFC 9293 spurious loss recovery - if loss was spurious (packets later ACKed), revert window reduction.

**Primary APIs**: `CubicCongestionControlOnDataLost`, `CubicCongestionControlOnSpuriousCongestionEvent`

**Contract Reasoning**:
- Preconditions: In recovery from non-ECN loss, Prev* state saved
- Postconditions: All Prev* state restored exactly, IsInRecovery=FALSE, HasHadCongestionEvent=FALSE
- Only non-ECN losses can be spurious (contract: ECN doesn't save Prev*)

**Expected Coverage**: Lines 788-823 (OnSpuriousCongestionEvent full function, Prev* restoration)

**Non-redundancy**: No existing test validates spurious recovery restoration logic.

**Quality Score**: Pending test-quality-checker validation

---

### Test 20: CongestionAvoidance_CubicFunctionGrowth
**Scenario**: Core CUBIC algorithm - window grows via cubic function W_cubic(t) = C*(t-K)^3 + WindowMax.

**Primary APIs**: `CubicCongestionControlOnDataAcknowledged`

**Contract Reasoning**:
- Preconditions: CongestionWindow >= SlowStartThreshold (CA mode), valid time tracking
- Postconditions: Window grows according to CUBIC formula, TimeOfLastAck updated
- Uses TimeOfCongAvoidStart to compute time in CA

**Expected Coverage**: Lines 563-671 (CA path with CUBIC calculation lines 610-631)

**Non-redundancy**: Existing OnDataAcknowledged test doesn't verify CA growth over time.

**Quality Score**: Pending test-quality-checker validation

---

### Test 21: CongestionAvoidance_AimdVsCubic_RenoFriendly
**Scenario**: CUBIC Reno-friendly mode - uses AIMD window when AIMD > CUBIC for fairness.

**Primary APIs**: `CubicCongestionControlOnDataAcknowledged`

**Contract Reasoning**:
- Preconditions: In CA, configured so AIMD calculation exceeds CUBIC window
- Postconditions: CongestionWindow == AimdWindow (not CUBIC value)
- Tests fairness mechanism from RFC 8312

**Expected Coverage**: Lines 648-663 (AIMD calculation and Reno-friendly branch at 659-663)

**Non-redundancy**: Tests specific branch where AIMD > CUBIC, not covered by basic CA test.

**Quality Score**: Pending test-quality-checker validation

---

### Test 22: SendIdleTimeout_FreezeCAGrowthDuringGaps
**Scenario**: Prevent unfair window growth during idle periods by adjusting TimeOfCongAvoidStart forward.

**Primary APIs**: `CubicCongestionControlOnDataAcknowledged`

**Contract Reasoning**:
- Preconditions: TimeOfLastAckValid=TRUE, ACK gap > SendIdleTimeoutMs AND > SmoothedRtt + 4*RttVariance
- Postconditions: TimeOfCongAvoidStart adjusted forward by gap duration
- Ensures window growth requires steady ACK feedback

**Expected Coverage**: Lines 580-589 (idle timeout check and TimeOfCongAvoidStart adjustment)

**Non-redundancy**: No existing test validates idle timeout freeze logic.

**Quality Score**: Pending test-quality-checker validation

---

### Test 23: WindowGrowthLimit_BytesInFlightMaxConstraint
**Scenario**: Prevent unbounded growth when flow control/app limits actual sending - cap at 2*BytesInFlightMax.

**Primary APIs**: `CubicCongestionControlOnDataAcknowledged`

**Contract Reasoning**:
- Preconditions: BytesInFlightMax << CongestionWindow (app/flow control limited)
- Postconditions: CongestionWindow <= 2*BytesInFlightMax after ACK
- Prudent growth based on actual utilization

**Expected Coverage**: Lines 683-685 (window cap check)

**Non-redundancy**: Tests specific constraint not validated in other CA tests.

**Quality Score**: Pending test-quality-checker validation

---

### Test 24: EcnSignal_CannotBeSpurious
**Scenario**: ECN-based congestion events do NOT save previous state and cannot be reverted (unlike loss).

**Primary APIs**: `CubicCongestionControlOnEcn`, `CubicCongestionControlOnSpuriousCongestionEvent`

**Contract Reasoning**:
- Preconditions: Trigger congestion via ECN (not loss)
- Postconditions: Spurious recovery doesn't restore window (Prev* not saved for ECN)
- Contract: OnCongestionEvent with Ecn=TRUE skips Prev* saves (lines 297-305)

**Expected Coverage**: Lines 756-784 (OnEcn function, ECN path in OnCongestionEvent)

**Non-redundancy**: Existing ECN test doesn't verify inability to revert.

**Quality Score**: Pending test-quality-checker validation

---

### Test 25: RecoveryExit_AckAfterRecoverySentPacketNumber
**Scenario**: Recovery ends when ACK covers packet sent AFTER entering recovery (QUIC's recovery definition).

**Primary APIs**: `CubicCongestionControlOnDataAcknowledged`

**Contract Reasoning**:
- Preconditions: IsInRecovery=TRUE, AckEvent.LargestAck > RecoverySentPacketNumber
- Postconditions: IsInRecovery=FALSE, IsInPersistentCongestion=FALSE, TimeOfCongAvoidStart updated
- Different from TCP (which uses flight size threshold)

**Expected Coverage**: Lines 453-468 (recovery exit check in OnDataAcknowledged)

**Non-redundancy**: Tests specific recovery exit condition, not validated in existing tests.

**Quality Score**: Pending test-quality-checker validation

---

### Test 26: FastConvergence_WindowLastMaxLogic
**Scenario**: When re-entering congestion before full recovery (WindowMax < WindowLastMax), apply fast convergence.

**Primary APIs**: `CubicCongestionControlOnDataLost`, `CubicCongestionControlOnCongestionEvent`

**Contract Reasoning**:
- Preconditions: Second loss with current window < previous WindowLastMax
- Postconditions: WindowMax = WindowMax * (10+BETA)/20 (extra reduction), WindowLastMax updated
- RFC 8312 fast convergence for stability

**Expected Coverage**: Lines 335-343 (fast convergence branch in OnCongestionEvent)

**Non-redundancy**: No existing test validates fast convergence logic (two losses scenario).

**Quality Score**: Pending test-quality-checker validation

---

## Coverage Summary

**New Tests Added**: 9 tests (Test 18-26)
**Primary Public APIs Covered**:
- CubicCongestionControlOnDataLost (with persistent congestion, fast convergence)
- CubicCongestionControlOnSpuriousCongestionEvent (full function)
- CubicCongestionControlOnDataAcknowledged (CA growth, AIMD, idle timeout, window limit, recovery exit)
- CubicCongestionControlOnEcn (spurious-proof ECN handling)

**Key Scenarios Added**:
1. Persistent congestion (most severe response)
2. Spurious loss recovery (Prev* restoration)
3. CUBIC function growth in CA
4. AIMD vs CUBIC (Reno-friendly region)
5. Idle timeout freeze
6. BytesInFlightMax constraint
7. ECN non-spurious guarantee
8. Recovery exit condition
9. Fast convergence

**Estimated Line Coverage Increase**: 
- Persistent congestion branch: ~20 lines
- Spurious recovery: ~35 lines  
- CA CUBIC/AIMD paths: ~100+ lines
- Idle timeout: ~10 lines
- Window limit: ~3 lines
- Fast convergence: ~15 lines
- Recovery exit: ~15 lines

**Total New Coverage**: ~200+ lines in critical paths
