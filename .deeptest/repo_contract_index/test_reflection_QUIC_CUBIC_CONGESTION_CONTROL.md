# Test Reflection: QUIC_CUBIC_CONGESTION_CONTROL

This document tracks test additions and their expected coverage impact.

## Existing Tests Analysis (17 tests)

| Test Name | Coverage Target | APIs Tested |
|-----------|----------------|-------------|
| InitializeComprehensive | Initialize, function pointers | CubicCongestionControlInitialize |
| InitializeBoundaries | Boundary values for MTU/window | CubicCongestionControlInitialize |
| MultipleSequentialInitializations | Re-initialization | CubicCongestionControlInitialize |
| CanSendScenarios | Can send logic | CanSend |
| SetExemption | Exemption setting | SetExemption |
| GetSendAllowanceScenarios | Send allowance (non-pacing) | GetSendAllowance |
| GetSendAllowanceWithActivePacing | Pacing calculation | GetSendAllowance |
| GetterFunctions | Getters | GetExemptions, GetBytesInFlightMax, GetCongestionWindow |
| ResetScenarios | Reset behavior | Reset |
| OnDataSent_IncrementsBytesInFlight | Data sent tracking | OnDataSent |
| OnDataInvalidated_DecrementsBytesInFlight | Data invalidation | OnDataInvalidated |
| OnDataAcknowledged_BasicAck | Basic ACK | OnDataAcknowledged |
| OnDataLost_WindowReduction | Loss handling | OnDataLost |
| OnEcn_CongestionSignal | ECN handling | OnEcn |
| GetNetworkStatistics_RetrieveStats | Network stats | GetNetworkStatistics |
| MiscFunctions_APICompleteness | Misc functions | LogOutFlowStatus, OnSpuriousCongestionEvent |
| HyStart_StateTransitions | HyStart states | OnDataAcknowledged (with HyStart) |

---

## New Tests Added (18 tests: Tests 18-35)

### Test 18: SlowStartToCongestionAvoidanceTransition

**Scenario Summary**: Tests slow start window growth and the transition mechanism to congestion avoidance when a loss event triggers SlowStartThreshold to be set.

**Primary Public API Target(s)**: OnDataAcknowledged, OnDataLost

**Contract Reasoning**:
- Preconditions: Cc initialized, valid AckEvent with non-zero BytesAcked
- Invariants: SlowStartThreshold is UINT32_MAX initially, set after loss event

**Expected Coverage Impact**: Lines 541-560 (slow start growth), loss-triggered threshold setting

---

### Test 19: CongestionAvoidanceWindowGrowth

**Scenario Summary**: Tests CUBIC congestion avoidance phase where window grows according to the CUBIC function.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**: In congestion avoidance, window growth is bounded by 2 * BytesInFlightMax.

**Expected Coverage Impact**: Lines 563-670 (congestion avoidance phase)

---

### Test 20: RecoveryExitViaAck

**Scenario Summary**: Tests recovery mode exits when ACK received for packet sent after congestion event.

**Primary Public API Target(s)**: OnDataAcknowledged

**Expected Coverage Impact**: Lines 453-468 (recovery exit handling)

---

### Test 21: PersistentCongestionHandling

**Scenario Summary**: Tests CUBIC's handling of persistent congestion.

**Primary Public API Target(s)**: OnDataLost

**Expected Coverage Impact**: Lines 307-328 (persistent congestion handling)

---

### Test 22: FastConvergenceCongestionEvent

**Scenario Summary**: Tests CUBIC's fast convergence feature.

**Primary Public API Target(s)**: OnDataLost

**Expected Coverage Impact**: Lines 335-343 (fast convergence logic)

---

### Test 23: SpuriousCongestionEventRecovery

**Scenario Summary**: Tests state reversion when congestion event is spurious.

**Primary Public API Target(s)**: OnSpuriousCongestionEvent

**Expected Coverage Impact**: Lines 786-823

---

### Test 24: OnDataLostMultipleLossEventsInRecovery

**Scenario Summary**: Tests that subsequent loss events during recovery don't trigger additional window reductions.

**Primary Public API Target(s)**: OnDataLost

**Expected Coverage Impact**: Lines 735-736 (conditional check for new congestion event)

---

### Test 25: OnEcnMultipleEventsInRecovery

**Scenario Summary**: Tests that subsequent ECN events during recovery don't trigger additional window reductions.

**Primary Public API Target(s)**: OnEcn

**Expected Coverage Impact**: Lines 770-771

---

### Test 26: PacingWithEstimatedWindowInSlowStart

**Scenario Summary**: Tests pacing calculation with doubled estimated window during slow start.

**Primary Public API Target(s)**: GetSendAllowance

**Expected Coverage Impact**: Lines 220-229

---

### Test 27: PacingWithEstimatedWindowInCongestionAvoidance

**Scenario Summary**: Tests pacing calculation with 1.25x estimated window in congestion avoidance.

**Primary Public API Target(s)**: GetSendAllowance

**Expected Coverage Impact**: Lines 227-229

---

### Test 28: PacingOverflowHandling

**Scenario Summary**: Tests pacing calculation overflow protection.

**Primary Public API Target(s)**: GetSendAllowance

**Expected Coverage Impact**: Lines 234-236

---

### Test 29: OnDataSentLastSendAllowanceDecrement

**Scenario Summary**: Tests LastSendAllowance decrement when data is sent.

**Primary Public API Target(s)**: OnDataSent

**Expected Coverage Impact**: Lines 387-391

---

### Test 30: WindowLimitingByBytesInFlightMax

**Scenario Summary**: Tests window growth limiting by 2 * BytesInFlightMax.

**Primary Public API Target(s)**: OnDataAcknowledged

**Expected Coverage Impact**: Lines 683-685

---

### Test 31: TimeGapHandlingInCongestionAvoidance

**Scenario Summary**: Tests TimeOfCongAvoidStart adjustment for large ACK gaps.

**Primary Public API Target(s)**: OnDataAcknowledged

**Expected Coverage Impact**: Lines 580-588

---

### Test 32: ZeroBytesAckedDuringRecovery

**Scenario Summary**: Tests zero-byte ACK processing during recovery.

**Primary Public API Target(s)**: OnDataAcknowledged

**Expected Coverage Impact**: Lines 469-470 (early exit for zero bytes)

---

### Test 33: AppLimitedFunctions

**Scenario Summary**: Tests IsAppLimited (always FALSE) and SetAppLimited (no-op) for CUBIC.

**Primary Public API Target(s)**: IsAppLimited, SetAppLimited

**Expected Coverage Impact**: Lines 873-890

---

### Test 34: DataInvalidationUnblockedStatus

**Scenario Summary**: Tests OnDataInvalidated return value for blocked state transition.

**Primary Public API Target(s)**: OnDataInvalidated

**Expected Coverage Impact**: Lines 400-415

---

### Test 35: HyStartDisabledBehavior

**Scenario Summary**: Tests that HyStart state transitions are skipped when disabled.

**Primary Public API Target(s)**: OnDataAcknowledged

**Expected Coverage Impact**: Lines 88-91 (HyStartEnabled check)
