# Test Reflection Document: CUBIC Congestion Control

## Existing Test Coverage Analysis

### Current Tests (17 tests):
1. **InitializeComprehensive** - Covers initialization with settings, function pointers, state flags, HyStart fields
2. **InitializeBoundaries** - Boundary values for MTU and settings
3. **MultipleSequentialInitializations** - Re-initialization behavior
4. **CanSendScenarios** - CanSend logic (available, blocked, exemptions)
5. **SetExemption** - SetExemption function
6. **GetSendAllowanceScenarios** - GetSendAllowance without pacing
7. **GetSendAllowanceWithActivePacing** - GetSendAllowance with pacing
8. **GetterFunctions** - GetExemptions, GetBytesInFlightMax, GetCongestionWindow
9. **ResetScenarios** - Reset with FullReset TRUE/FALSE
10. **OnDataSent_IncrementsBytesInFlight** - OnDataSent increments BytesInFlight and exemptions
11. **OnDataInvalidated_DecrementsBytesInFlight** - OnDataInvalidated decrements BytesInFlight
12. **OnDataAcknowledged_BasicAck** - Basic ACK processing
13. **OnDataLost_WindowReduction** - Loss handling and window reduction
14. **OnEcn_CongestionSignal** - ECN handling
15. **GetNetworkStatistics_RetrieveStats** - Statistics retrieval
16. **MiscFunctions_APICompleteness** - Various API calls
17. **HyStart_StateTransitions** - Basic HyStart state transitions

## Identified Coverage Gaps

### Critical Gaps (Must Cover):

1. **CubeRoot Function Edge Cases**
   - Not directly tested
   - Need tests for: 0, 1, 8, 27, powers of 2, large values, max uint32
   - Lines 45-62 in cubic.c

2. **HyStart++ State Machine Detailed Transitions**
   - Missing HYSTART_ACTIVE → HYSTART_NOT_STARTED (RTT decreased)
   - Missing HYSTART_ACTIVE → HYSTART_DONE (ConservativeSlowStartRounds exhausted)
   - Missing detailed RTT sampling logic (N samples, Eta calculation)
   - Lines 476-539 in OnDataAcknowledged

3. **Congestion Avoidance CUBIC Formula**
   - Missing detailed testing of CUBIC window calculation
   - Missing DeltaT edge cases (negative, overflow at 2.5M ms)
   - Missing AIMD window vs CUBIC window comparison
   - Missing WindowPrior transition logic
   - Lines 591-670 in OnDataAcknowledged

4. **Persistent Congestion Handling**
   - Not tested: PersistentCongestion=TRUE scenario
   - Window reset to minimum window
   - Route state change
   - Lines 307-328 in OnCongestionEvent

5. **Recovery Entry and Exit**
   - Basic coverage exists, but missing:
   - Recovery exit when LargestAck > RecoverySentPacketNumber
   - ACKs during recovery (no window growth)
   - Lines 453-468 in OnDataAcknowledged

6. **Spurious Congestion Event Detailed Testing**
   - Only basic call tested
   - Need verification of state restoration (WindowPrior, WindowMax, KCubic, etc.)
   - Need test when not in recovery (should return FALSE)
   - Lines 788-823

7. **Fast Convergence Logic**
   - Not tested: WindowLastMax > WindowMax scenario
   - WindowMax adjustment calculation
   - Lines 335-343 in OnCongestionEvent

8. **Slow Start to Congestion Avoidance Transition**
   - Missing: CongestionWindow overshooting SlowStartThreshold
   - BytesAcked spillover handling
   - Lines 549-561 in OnDataAcknowledged

9. **Time Gap Handling in Congestion Avoidance**
   - Not tested: Long gaps between ACKs
   - TimeOfCongAvoidStart adjustment
   - SendIdleTimeoutMs threshold
   - Lines 580-589 in OnDataAcknowledged

10. **Pacing Edge Cases**
    - Missing: Overflow handling in pacing calculation
    - EstimatedWnd calculation in slow start vs congestion avoidance
    - LastSendAllowance tracking
    - Lines 206-240 in GetSendAllowance

11. **Window Growth Limiting**
    - Not tested: Growth limited by 2 * BytesInFlightMax
    - Lines 680-685 in OnDataAcknowledged

12. **IsAppLimited and SetAppLimited**
    - Only called, not verified (both are no-ops in CUBIC)
    - Lines 875-890

### Secondary Gaps (Should Cover):

13. **OnDataSent with LastSendAllowance underflow**
    - Lines 387-391

14. **Multiple congestion events**
    - Saving/restoring previous state (Prev* fields)
    - Lines 297-305

15. **KCubic calculation**
    - CubeRoot with scaling
    - Lines 353-358

16. **ECN vs non-ECN congestion events**
    - ECN doesn't save previous state
    - Lines 297-305

17. **BytesInFlight underflow assertions**
    - Verify preconditions hold

## Coverage Goals

Target: **100% line coverage** of cubic.c

Priority order for new tests:
1. CubeRoot edge cases
2. Persistent congestion
3. Spurious congestion event (detailed)
4. HyStart++ detailed state machine
5. CUBIC formula and congestion avoidance
6. Fast convergence
7. Time gap handling
8. Slow start overshoot
9. Window growth limiting
10. Pacing edge cases
11. Recovery detailed scenarios
12. All other gaps

## Notes
- All tests must use public API only
- All tests must be contract-safe (no precondition violations)
- All tests must follow existing test pattern (TEST macro, InitializeMockConnection, via function pointers)
- Each test must target one specific scenario

## New Tests Added (Test 18-42)

### Test 18: PersistentCongestion_WindowResetToMinimum
- **Scenario**: Persistent congestion detection resets window to minimum (2 packets)
- **Primary API**: CubicCongestionControlOnDataLost
- **Coverage**: Lines 307-328 (persistent congestion handling)
- **Non-redundancy**: Existing Test 13 only tests normal loss; this tests persistent congestion flag

### Test 19: SpuriousCongestion_StateRestoration
- **Scenario**: Complete state restoration when congestion event is spurious
- **Primary API**: CubicCongestionControlOnSpuriousCongestionEvent
- **Coverage**: Lines 788-823 (spurious congestion handling with state restoration)
- **Non-redundancy**: Existing Test 16 only calls function; this verifies full state restoration

### Test 20: SpuriousCongestion_NotInRecovery
- **Scenario**: No-op when not in recovery
- **Primary API**: CubicCongestionControlOnSpuriousCongestionEvent
- **Coverage**: Lines 794-796 (early return when not in recovery)
- **Non-redundancy**: Tests different code path (early exit)

### Test 21: Recovery_ExitOnAckBeyondRecoveryPoint
- **Scenario**: Exit recovery when ACK > RecoverySentPacketNumber
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 453-468 (recovery exit logic)
- **Non-redundancy**: Tests recovery exit; existing tests don't specifically test this transition

### Test 22: FastConvergence_WindowLastMaxReduction
- **Scenario**: Fast convergence when WindowLastMax > CongestionWindow
- **Primary API**: CubicCongestionControlOnDataLost -> OnCongestionEvent
- **Coverage**: Lines 335-343 (fast convergence logic)
- **Non-redundancy**: Tests fast convergence path not covered by existing tests

### Test 23: NormalConvergence_NoFastConvergenceReduction
- **Scenario**: Normal convergence when WindowLastMax <= CongestionWindow
- **Primary API**: CubicCongestionControlOnDataLost -> OnCongestionEvent
- **Coverage**: Lines 332-343 (normal convergence path - else branch)
- **Non-redundancy**: Tests alternative convergence path

### Test 24: SlowStartOvershoot_TransitionToCongestionAvoidance
- **Scenario**: Window overshoots threshold, excess applied to congestion avoidance
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 549-561 (slow start overshoot handling)
- **Non-redundancy**: Tests specific transition scenario not in existing tests

### Test 25: HyStart_TransitionToActive_RttIncreaseDetected
- **Scenario**: HYSTART_NOT_STARTED → HYSTART_ACTIVE on RTT increase
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 481-510 (HyStart RTT sampling and transition to ACTIVE)
- **Non-redundancy**: Existing Test 17 only samples; this tests full transition

### Test 26: HyStart_TransitionToDone_RoundsExhausted
- **Scenario**: HYSTART_ACTIVE → HYSTART_DONE when rounds exhausted
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 526-537 (ConservativeSlowStartRounds countdown and transition)
- **Non-redundancy**: Tests specific HyStart transition not covered

### Test 27: HyStart_TransitionBackToNotStarted_RttDecreased
- **Scenario**: HYSTART_ACTIVE → HYSTART_NOT_STARTED on RTT decrease
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 512-518 (RTT decreased spurious exit detection)
- **Non-redundancy**: Tests HyStart revert path not covered

### Test 28: CongestionAvoidance_CubicWindowGrowth
- **Scenario**: CUBIC formula window growth in congestion avoidance
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 591-670 (CUBIC window calculation, AIMD window, growth)
- **Non-redundancy**: Existing Test 12 is basic; this tests detailed growth logic

### Test 29: TimeGapHandling_FreezeWindowGrowthDuringIdle
- **Scenario**: TimeOfCongAvoidStart adjustment during long ACK gaps
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 580-589 (time gap detection and adjustment)
- **Non-redundancy**: Tests time gap logic not covered by existing tests

### Test 30: WindowGrowthLimiting_CappedAtTwiceBytesInFlightMax
- **Scenario**: Window growth capped at 2 * BytesInFlightMax
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 680-685 (window growth limiting)
- **Non-redundancy**: Tests growth cap not explicitly tested before

### Test 31: AimdVsCubic_RenoFriendlyRegion
- **Scenario**: AIMD vs CUBIC window selection (Reno-friendly)
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 649-670 (AIMD window calculation and selection logic)
- **Non-redundancy**: Tests AIMD path and window comparison

### Test 32: PacingOverflow_LargeEstimatedWndCalculation
- **Scenario**: Overflow protection in pacing with large window
- **Primary API**: CubicCongestionControlGetSendAllowance
- **Coverage**: Lines 221-240 (pacing calculation with large values)
- **Non-redundancy**: Tests overflow edge case in pacing

### Test 33: PacingEstimatedWnd_GrowthFactorByPhase
- **Scenario**: Different EstimatedWnd factors in slow start vs congestion avoidance
- **Primary API**: CubicCongestionControlGetSendAllowance
- **Coverage**: Lines 222-229 (EstimatedWnd calculation branches)
- **Non-redundancy**: Tests both branches of EstimatedWnd calculation

### Test 34: OnDataSent_LastSendAllowanceUnderflowProtection
- **Scenario**: LastSendAllowance underflow protection
- **Primary API**: CubicCongestionControlOnDataSent
- **Coverage**: Lines 387-391 (LastSendAllowance underflow handling)
- **Non-redundancy**: Tests specific edge case in OnDataSent

### Test 35: EcnCongestionEvent_NoStateSaving
- **Scenario**: ECN events don't save Prev* state
- **Primary API**: CubicCongestionControlOnEcn
- **Coverage**: Lines 297-305 (state saving logic, ECN path)
- **Non-redundancy**: Existing Test 14 tests ECN; this verifies no state saving

### Test 36: MultipleCongestionEvents_StateSavingOnEachEvent
- **Scenario**: Each non-ECN event overwrites Prev* state
- **Primary API**: CubicCongestionControlOnDataLost
- **Coverage**: Lines 297-305 (state saving on multiple events)
- **Non-redundancy**: Tests state saving across multiple events

### Test 37: ZeroBytesAcknowledged_NoWindowGrowth
- **Scenario**: No window growth when NumRetransmittableBytes=0
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 469-471 (zero bytes early exit)
- **Non-redundancy**: Tests edge case of zero-byte ACK

### Test 38: PacingDisabled_NoRttSample
- **Scenario**: Pacing skipped when GotFirstRttSample=FALSE
- **Primary API**: CubicCongestionControlGetSendAllowance
- **Coverage**: Lines 195-203 (pacing conditions check - no RTT sample)
- **Non-redundancy**: Tests pacing precondition

### Test 39: PacingDisabled_RttBelowMinimumThreshold
- **Scenario**: Pacing skipped when RTT < QUIC_MIN_PACING_RTT
- **Primary API**: CubicCongestionControlGetSendAllowance
- **Coverage**: Lines 195-203 (pacing conditions check - RTT threshold)
- **Non-redundancy**: Tests another pacing precondition

### Test 40: CubicDeltaTOverflow_ClampedAtMaximum
- **Scenario**: DeltaT clamped at 2.5M ms to prevent overflow
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 610-618 (DeltaT clamping)
- **Non-redundancy**: Tests overflow protection in CUBIC formula

### Test 41: AppLimited_NoOpFunctions
- **Scenario**: IsAppLimited and SetAppLimited are no-ops
- **Primary API**: CubicCongestionControlIsAppLimited, CubicCongestionControlSetAppLimited
- **Coverage**: Lines 875-890 (app-limited functions)
- **Non-redundancy**: Explicitly tests these functions

### Test 42: HyStartSampling_FirstNAcks
- **Scenario**: HyStart samples first N ACKs per round
- **Primary API**: CubicCongestionControlOnDataAcknowledged
- **Coverage**: Lines 481-487 (HyStart ACK sampling logic)
- **Non-redundancy**: Tests detailed sampling behavior

## Summary
- Added 25 new tests (Test 18-42)
- Total tests: 42
- Focus areas: Persistent congestion, spurious congestion, recovery, fast convergence, HyStart++ state machine, CUBIC formula, time gap handling, pacing edge cases, ECN details
- All tests are contract-safe, use public API only, follow existing patterns
