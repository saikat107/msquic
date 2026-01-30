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

## New Tests Added

### Test 18: SlowStartToCongestionAvoidanceTransition

**Scenario Summary**: Tests the transition from slow start to congestion avoidance phase when the congestion window grows to meet the SlowStartThreshold. This exercises the critical transition point where CUBIC switches from exponential to sub-linear growth.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: Cc initialized, valid AckEvent with non-zero BytesAcked
- Invariants: CongestionWindow should not exceed SlowStartThreshold during transition
- The test sets up SlowStartThreshold below CongestionWindow to trigger transition

**Expected Coverage Impact**:
- Lines 541-560 in cubic.c (slow start to congestion avoidance transition)
- Lines 549-560 (BytesAcked spillover handling)

**Non-redundancy**: No existing test exercises the SlowStartThreshold transition boundary.

---

### Test 19: CongestionAvoidanceWindowGrowth

**Scenario Summary**: Tests CUBIC congestion avoidance phase where window grows according to the CUBIC function W_cubic(t). This exercises the core CUBIC algorithm implementation including time-based calculations.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: CongestionWindow >= SlowStartThreshold (in congestion avoidance)
- Invariants: Window growth should be bounded by 2 * BytesInFlightMax

**Expected Coverage Impact**:
- Lines 563-670 (congestion avoidance phase)
- Lines 610-623 (CUBIC window calculation with DeltaT)

**Non-redundancy**: Existing OnDataAcknowledged test only covers basic ACK, not congestion avoidance growth.

---

### Test 20: RecoveryExitViaAck

**Scenario Summary**: Tests that recovery mode exits correctly when an ACK is received for a packet sent after the congestion event. This is a key state transition for CUBIC.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: IsInRecovery=TRUE, LargestAck > RecoverySentPacketNumber
- Postconditions: IsInRecovery=FALSE, IsInPersistentCongestion=FALSE

**Expected Coverage Impact**:
- Lines 453-468 (recovery exit handling)

**Non-redundancy**: No existing test exercises recovery exit path.

---

### Test 21: PersistentCongestionHandling

**Scenario Summary**: Tests CUBIC's handling of persistent congestion, which triggers a more aggressive window reduction to the minimum congestion window.

**Primary Public API Target(s)**: OnDataLost

**Contract Reasoning**:
- Preconditions: LossEvent.PersistentCongestion=TRUE
- Postconditions: CongestionWindow reduced to minimum (QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS * MTU)

**Expected Coverage Impact**:
- Lines 307-328 (persistent congestion handling)

**Non-redundancy**: Existing OnDataLost test only covers regular loss, not persistent congestion.

---

### Test 22: FastConvergenceCongestionEvent

**Scenario Summary**: Tests CUBIC's fast convergence feature where WindowMax is adjusted when WindowLastMax > WindowMax to enable faster convergence to fair share.

**Primary Public API Target(s)**: OnDataLost

**Contract Reasoning**:
- Preconditions: WindowLastMax > WindowMax before congestion event
- Postconditions: WindowLastMax and WindowMax adjusted per fast convergence

**Expected Coverage Impact**:
- Lines 335-343 (fast convergence logic)

**Non-redundancy**: Existing loss test doesn't set up conditions for fast convergence.

---

### Test 23: SpuriousCongestionEventRecovery

**Scenario Summary**: Tests that CUBIC correctly reverts state when a congestion event is determined to be spurious (false loss detection).

**Primary Public API Target(s)**: OnSpuriousCongestionEvent

**Contract Reasoning**:
- Preconditions: IsInRecovery=TRUE, Prev* state values saved
- Postconditions: All state reverted to Prev* values

**Expected Coverage Impact**:
- Lines 786-823 (spurious congestion event handling)

**Non-redundancy**: Existing MiscFunctions test calls OnSpuriousCongestionEvent but doesn't verify state reversion.

---

### Test 24: HyStartDelayIncreaseDetection

**Scenario Summary**: Tests HyStart delay increase detection which triggers transition from HYSTART_NOT_STARTED to HYSTART_ACTIVE.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: HyStartEnabled=TRUE, sufficient RTT samples
- Postconditions: HyStartState transitions, CWndSlowStartGrowthDivisor updated

**Expected Coverage Impact**:
- Lines 476-510 (HyStart RTT sampling and delay detection)

**Non-redundancy**: Existing HyStart test doesn't exercise the delay increase detection path.

---

### Test 25: HyStartRoundReset

**Scenario Summary**: Tests that HyStart correctly resets its per-round counters when a new RTT round begins.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: AckEvent.LargestAck >= HyStartRoundEnd
- Postconditions: HyStartRoundEnd updated, counters reset

**Expected Coverage Impact**:
- Lines 524-538 (HyStart round reset)

**Non-redundancy**: Existing tests don't verify round reset behavior.

---

### Test 26: TimeGapHandlingInCongestionAvoidance

**Scenario Summary**: Tests that CUBIC correctly handles large time gaps between ACKs by adjusting TimeOfCongAvoidStart to prevent unfair window growth.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: TimeOfLastAckValid=TRUE, large time gap since last ACK
- Postconditions: TimeOfCongAvoidStart adjusted

**Expected Coverage Impact**:
- Lines 580-588 (time gap handling)

**Non-redundancy**: No existing test exercises the time gap adjustment path.

---

### Test 27: AimdWindowCalculation

**Scenario Summary**: Tests the AIMD window calculation in congestion avoidance, which ensures CUBIC is TCP-friendly.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: In congestion avoidance, AimdWindow < WindowPrior
- Postconditions: AimdAccumulator and AimdWindow updated correctly

**Expected Coverage Impact**:
- Lines 633-657 (AIMD window calculation)

**Non-redundancy**: No existing test specifically exercises AIMD window growth path.

---

### Test 28: WindowLimitingByBytesInFlightMax

**Scenario Summary**: Tests that CUBIC correctly limits window growth based on BytesInFlightMax to prevent window explosion without loss feedback.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: CongestionWindow would exceed 2 * BytesInFlightMax
- Postconditions: CongestionWindow capped at 2 * BytesInFlightMax

**Expected Coverage Impact**:
- Lines 683-685 (window limiting)

**Non-redundancy**: No existing test exercises the window limiting path.

---

### Test 29: PacingWithEstimatedWindowInSlowStart

**Scenario Summary**: Tests pacing calculation with estimated window doubling in slow start phase.

**Primary Public API Target(s)**: GetSendAllowance

**Contract Reasoning**:
- Preconditions: PacingEnabled, CongestionWindow < SlowStartThreshold
- Postconditions: EstimatedWnd = CongestionWindow * 2 (up to threshold)

**Expected Coverage Impact**:
- Lines 220-229 (estimated window calculation in slow start)

**Non-redundancy**: Existing pacing test doesn't specifically test slow start estimated window.

---

### Test 30: PacingWithEstimatedWindowInCongestionAvoidance

**Scenario Summary**: Tests pacing calculation with 25% growth estimation in congestion avoidance.

**Primary Public API Target(s)**: GetSendAllowance

**Contract Reasoning**:
- Preconditions: PacingEnabled, CongestionWindow >= SlowStartThreshold
- Postconditions: EstimatedWnd = CongestionWindow * 1.25

**Expected Coverage Impact**:
- Lines 227-229 (estimated window in congestion avoidance)

**Non-redundancy**: Existing pacing test uses slow start state.

---

### Test 31: PacingOverflowHandling

**Scenario Summary**: Tests pacing calculation overflow protection when SendAllowance would overflow.

**Primary Public API Target(s)**: GetSendAllowance

**Contract Reasoning**:
- Preconditions: Large TimeSinceLastSend that could cause overflow
- Postconditions: SendAllowance capped at available window

**Expected Coverage Impact**:
- Lines 234-236 (overflow handling)

**Non-redundancy**: No existing test exercises overflow protection path.

---

### Test 32: DataSentLastSendAllowanceDecrement

**Scenario Summary**: Tests that OnDataSent correctly decrements LastSendAllowance based on bytes sent.

**Primary Public API Target(s)**: OnDataSent

**Contract Reasoning**:
- Preconditions: LastSendAllowance > 0
- Postconditions: LastSendAllowance decremented or zeroed

**Expected Coverage Impact**:
- Lines 387-391 (LastSendAllowance decrement)

**Non-redundancy**: Existing OnDataSent test doesn't verify LastSendAllowance handling.

---

### Test 33: HyStartConservativeSlowStartRounds

**Scenario Summary**: Tests that HyStart in ACTIVE state correctly decrements ConservativeSlowStartRounds and transitions to DONE when rounds reach 0.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: HyStartState=HYSTART_ACTIVE, ConservativeSlowStartRounds > 0
- Postconditions: ConservativeSlowStartRounds decremented, eventual transition to HYSTART_DONE

**Expected Coverage Impact**:
- Lines 526-535 (Conservative slow start round handling)

**Non-redundancy**: No existing test exercises the full CSS rounds countdown.

---

### Test 34: HyStartResumeFromSpuriousExit

**Scenario Summary**: Tests that HyStart correctly resumes slow start when RTT decreases, indicating the exit was spurious.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: HyStartState=HYSTART_ACTIVE, MinRttInCurrentRound < CssBaselineMinRtt
- Postconditions: HyStartState transitions back to HYSTART_NOT_STARTED

**Expected Coverage Impact**:
- Lines 515-517 (HyStart spurious exit detection)

**Non-redundancy**: No existing test exercises this HyStart rollback path.

---

### Test 35: OnDataLostMultipleLossEventsInRecovery

**Scenario Summary**: Tests that subsequent loss events during recovery don't trigger additional window reductions.

**Primary Public API Target(s)**: OnDataLost

**Contract Reasoning**:
- Preconditions: IsInRecovery=TRUE, loss packet number <= RecoverySentPacketNumber
- Postconditions: No additional window reduction, only BytesInFlight updated

**Expected Coverage Impact**:
- Lines 735-736 (conditional check for new congestion event)

**Non-redundancy**: Existing test only exercises single loss event.

---

### Test 36: OnEcnMultipleEventsInRecovery

**Scenario Summary**: Tests that subsequent ECN events during recovery don't trigger additional window reductions.

**Primary Public API Target(s)**: OnEcn

**Contract Reasoning**:
- Preconditions: IsInRecovery=TRUE, packet number <= RecoverySentPacketNumber
- Postconditions: No additional window reduction

**Expected Coverage Impact**:
- Lines 770-771 (conditional check for new ECN event)

**Non-redundancy**: Existing test only exercises single ECN event.

---

### Test 37: CubicWindowNegativeOverflowProtection

**Scenario Summary**: Tests that CUBIC window calculation handles negative overflow correctly when DeltaT is very large.

**Primary Public API Target(s)**: OnDataAcknowledged

**Contract Reasoning**:
- Preconditions: Very large DeltaT value
- Postconditions: Window capped appropriately

**Expected Coverage Impact**:
- Lines 616-618 (DeltaT clamping) and lines 625-631 (negative overflow check)

**Non-redundancy**: No existing test exercises overflow protection in CUBIC calculation.
