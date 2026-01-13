# BBR Test Categorization Report

## Overview
This document categorizes all BBR tests (tests 1-44) by their testing focus and purpose.

---

## Category 1: Initialization & Configuration Tests (Tests 1-2)

These tests verify correct initialization of BBR congestion control with various configurations.

### Test 1: `InitializeComprehensive`
- **Purpose**: Verify complete BBR initialization with default settings
- **Scope**: Initial state values, window sizes, timestamps, filters
- **Public APIs**: `BbrCongestionControlInitialize`

### Test 2: `InitializeBoundaries`
- **Purpose**: Test initialization with boundary values (zero, very large windows)
- **Scope**: Edge cases in initial window configuration
- **Public APIs**: `BbrCongestionControlInitialize`

---

## Category 2: Data Transmission & Flow Control Tests (Tests 3-6)

These tests verify basic data transmission tracking and flow control mechanisms.

### Test 3: `OnDataSentIncreasesInflight`
- **Purpose**: Verify bytes-in-flight tracking on packet send
- **Scope**: BytesInFlight increment
- **Public APIs**: `QuicCongestionControlOnDataSent`

### Test 4: `OnDataInvalidatedDecreasesInflight`
- **Purpose**: Verify bytes-in-flight decrement when data is invalidated
- **Scope**: BytesInFlight decrement without ACK
- **Public APIs**: `QuicCongestionControlOnDataInvalidated`

### Test 5: `CanSendRespectsWindow`
- **Purpose**: Test congestion window enforcement
- **Scope**: Window-based send blocking
- **Public APIs**: `QuicCongestionControlCanSend`

### Test 6: `ExemptionsBypassControl`
- **Purpose**: Verify exempted packets bypass congestion control
- **Scope**: Exemption flags override window checks
- **Public APIs**: `QuicCongestionControlCanSend`

---

## Category 3: Loss Handling & Recovery Tests (Tests 7-8, 17, 23, 35, 42)

These tests verify BBR's response to packet loss and recovery mechanisms.

### Test 7: `OnDataLostEntersRecovery`
- **Purpose**: Verify BBR enters recovery state on loss
- **Scope**: Recovery state transition, window reduction
- **Public APIs**: `QuicCongestionControlOnDataLost`

### Test 8: `ResetReturnsToStartup`
- **Purpose**: Test complete reset returns to STARTUP state
- **Scope**: Full state reset
- **Public APIs**: `BbrCongestionControlReset`

### Test 17: `RecoveryWindowUpdateOnAck`
- **Purpose**: Verify recovery window updates during recovery
- **Scope**: Recovery window management
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged` (during recovery)

### Test 23: `PersistentCongestionResetsWindow`
- **Purpose**: Test persistent congestion detection and window reset
- **Scope**: Persistent congestion handling
- **Public APIs**: `QuicCongestionControlOnDataLost` (with persistent flag)

### Test 35: `RecoveryWindowManagement`
- **Purpose**: Verify recovery window is managed during recovery state
- **Scope**: Recovery window non-zero and growth
- **Public APIs**: `QuicCongestionControlOnDataLost`, `OnDataAcknowledged`

### Test 42: `RecoveryGrowthState_WindowExpansion`
- **Purpose**: Test recovery window expansion during GROWTH recovery state
- **Scope**: Recovery window growth on ACKs
- **Public APIs**: `QuicCongestionControlOnDataLost`, `OnDataAcknowledged`

---

## Category 4: Reset & Partial Reset Tests (Tests 9)

### Test 9: `ResetPartialPreservesInflight`
- **Purpose**: Verify partial reset preserves bytes-in-flight
- **Scope**: Partial reset semantics
- **Public APIs**: `BbrCongestionControlReset` (partial flag)

---

## Category 5: Spurious Loss Detection Tests (Test 10)

### Test 10: `SpuriousCongestionEventNoRevert`
- **Purpose**: Verify BBR does not revert window on spurious loss
- **Scope**: Spurious congestion event handling
- **Public APIs**: `QuicCongestionControlOnSpuriousCongestionEvent`

---

## Category 6: ACK Processing Tests (Tests 11, 16, 21, 27, 40, 41)

These tests verify correct ACK event processing and bandwidth estimation.

### Test 11: `OnDataAcknowledgedBasic`
- **Purpose**: Verify basic ACK processing updates state correctly
- **Scope**: BytesInFlight reduction, RTT updates, bandwidth tracking
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

### Test 16: `GetNetworkStatisticsViaCongestionEvent`
- **Purpose**: Verify network statistics query via congestion event
- **Scope**: Statistics reporting API
- **Public APIs**: `QuicCongestionControlGetCongestionEvent`

### Test 21: `NetStatsEventTriggersStatsFunctions`
- **Purpose**: Test network statistics event triggers stats calculation
- **Scope**: Statistics functions execution
- **Public APIs**: `QuicCongestionControlGetCongestionEvent`

### Test 27: `ImplicitAckTriggersNetStats`
- **Purpose**: Verify implicit ACK triggers network statistics
- **Scope**: Implicit ACK handling
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged` (implicit)

### Test 40: `AckTimingEdgeCase_FallbackToRawAckTime`
- **Purpose**: Test ACK timing edge case with backward-adjusted time
- **Scope**: ACK time fallback path (lines 153-154 in bbr.c)
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

### Test 41: `InvalidDeliveryRates_BothMaxSkip`
- **Purpose**: Verify invalid delivery rates (both UINT64_MAX) are skipped
- **Scope**: Delivery rate validation (continue statement coverage)
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

---

## Category 7: Send Allowance & Pacing Tests (Tests 12-15, 19, 25, 36-38, 43)

These tests verify send allowance calculations and pacing behavior.

### Test 12: `GetSendAllowanceNoPacing`
- **Purpose**: Test send allowance without pacing enabled
- **Scope**: Window-based allowance only
- **Public APIs**: `QuicCongestionControlGetSendAllowance`

### Test 13: `GetSendAllowanceWithPacing`
- **Purpose**: Test send allowance with pacing enabled
- **Scope**: Pacing-based allowance calculation
- **Public APIs**: `QuicCongestionControlGetSendAllowance`

### Test 14: `CanSendFlowControlUnblocking`
- **Purpose**: Verify flow control unblocking allows send
- **Scope**: Unblocking mechanism
- **Public APIs**: `QuicCongestionControlCanSend`

### Test 15: `ExitingQuiescenceOnSendAfterIdle`
- **Purpose**: Test exiting quiescence on send after idle period
- **Scope**: Idle state exit
- **Public APIs**: `QuicCongestionControlOnDataSent` (after idle)

### Test 19: `PacingSendQuantumTiers`
- **Purpose**: Verify send quantum calculation based on pacing rate tiers
- **Scope**: Three-tier send quantum logic
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged` (triggers SetSendQuantum)

### Test 25: `SendAllowanceWithPacingDisabled`
- **Purpose**: Test send allowance when pacing is explicitly disabled
- **Scope**: Pacing disabled path
- **Public APIs**: `QuicCongestionControlGetSendAllowance`

### Test 36: `SendAllowanceFullyBlocked`
- **Purpose**: Verify send allowance is 0 when fully CC blocked
- **Scope**: BytesInFlight >= CongestionWindow blocking
- **Public APIs**: `QuicCongestionControlGetSendAllowance`

### Test 37: `SendAllowanceBandwidthBased`
- **Purpose**: Test bandwidth-based send allowance (non-STARTUP)
- **Scope**: Bandwidth calculation path in GetSendAllowance
- **Public APIs**: `QuicCongestionControlGetSendAllowance`

### Test 38: `SendQuantumHighPacingRate`
- **Purpose**: Verify high pacing rate triggers rate-based send quantum
- **Scope**: High pacing rate threshold (>= 1.2 Mbps)
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

### Test 43: `SendAllowanceBandwidthOnly_NonStartupPath`
- **Purpose**: Test pure bandwidth-based send allowance in PROBE_RTT
- **Scope**: Non-STARTUP bandwidth-only path
- **Public APIs**: `QuicCongestionControlGetSendAllowance`

---

## Category 8: Application-Limited Detection Tests (Tests 18, 20, 24)

These tests verify application-limited state tracking.

### Test 18: `SetAppLimitedSuccess`
- **Purpose**: Verify app-limited state is set correctly
- **Scope**: Application-limited flag setting
- **Public APIs**: `QuicCongestionControlSetAppLimited`

### Test 20: `ZeroLengthPacketSkippedInBandwidthUpdate`
- **Purpose**: Test zero-length packets don't affect bandwidth estimation
- **Scope**: Zero-byte packet filtering
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

### Test 24: `SetAppLimitedWhenCongestionLimited`
- **Purpose**: Verify app-limited not set when congestion limited
- **Scope**: Congestion-limited condition check
- **Public APIs**: `QuicCongestionControlSetAppLimited`

---

## Category 9: Recovery Exit Tests (Test 26)

### Test 26: `RecoveryExitOnEndOfRecoveryAck`
- **Purpose**: Test recovery exit when ACK exceeds recovery end marker
- **Scope**: Recovery state exit condition
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

---

## Category 10: Public API Query Tests (Test 22)

### Test 22: `GetBytesInFlightMaxPublicAPI`
- **Purpose**: Verify GetBytesInFlightMax returns correct value
- **Scope**: Public query API
- **Public APIs**: `QuicCongestionControlGetBytesInFlightMax`

---

## Category 11: Edge Case & Boundary Tests (Test 28, 39, 44)

### Test 28: `BandwidthEstimationEdgeCaseTimestamps`
- **Purpose**: Test bandwidth estimation with edge-case timestamps
- **Scope**: Timestamp boundary handling in bandwidth update
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

### Test 39: `TargetCwndWithAckAggregation`
- **Purpose**: Verify target CWND includes ACK aggregation
- **Scope**: ACK aggregation in CWND calculation
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

### Test 44: `HighPacingRateSendQuantum_LargeBurstSize`
- **Purpose**: Test very high pacing rate triggers large send quantum
- **Scope**: High pacing rate edge case (10 Mbps)
- **Public APIs**: `QuicCongestionControlOnDataAcknowledged`

---

## Category 12: State Transition Tests (Tests 29-34)

These tests verify BBR state machine transitions through realistic scenarios.

### Test 29: `StateTransition_StartupToProbeRtt_RttExpired`
- **Purpose**: Test STARTUP → PROBE_RTT on RTT measurement expiry
- **Scope**: RTT expiry detection and state transition
- **Public APIs**: `BbrCongestionControlInitialize`, `OnDataSent`, `OnDataAcknowledged`
- **Transition Condition**: MinRtt timestamp expired (> 10 seconds old)

### Test 30: `StateTransition_ProbeBwToProbeRtt_RttExpired`
- **Purpose**: Test PROBE_BW → PROBE_RTT on RTT measurement expiry
- **Scope**: RTT expiry from PROBE_BW state
- **Public APIs**: `OnDataSent`, `OnDataAcknowledged`
- **Transition Condition**: MinRtt timestamp expired in PROBE_BW

### Test 31: `StateTransition_ProbeRttToProbeBw_ProbeComplete`
- **Purpose**: Test PROBE_RTT → PROBE_BW after probe duration complete
- **Scope**: PROBE_RTT duration timeout (200ms)
- **Public APIs**: `OnDataAcknowledged`
- **Transition Condition**: ProbeRttDoneTimestamp reached

### Test 32: `StateTransition_ProbeRttToStartup_NoBottleneckFound`
- **Purpose**: Test PROBE_RTT → STARTUP when bottleneck not found
- **Scope**: Bottleneck not found condition
- **Public APIs**: `OnDataAcknowledged`
- **Transition Condition**: BtlbwFound = FALSE after PROBE_RTT

### Test 33: `StateTransition_DrainToProbeRtt_RttExpired`
- **Purpose**: Test DRAIN → PROBE_RTT on RTT measurement expiry
- **Scope**: RTT expiry from DRAIN state
- **Public APIs**: `OnDataAcknowledged`
- **Transition Condition**: MinRtt timestamp expired in DRAIN

### Test 34: `ProbeBwGainCycleDrain_TargetReached`
- **Purpose**: Test PROBE_BW gain cycle advancement when drain target reached
- **Scope**: PROBE_BW cycle management (not a state transition, but cycle phase transition)
- **Public APIs**: `OnDataAcknowledged`
- **Transition Condition**: BytesInFlight < TargetCwnd during drain phase

---

## Summary Statistics

- **Total Tests**: 44
- **Initialization & Configuration**: 2 tests (Tests 1-2)
- **Data Transmission & Flow Control**: 4 tests (Tests 3-6)
- **Loss Handling & Recovery**: 6 tests (Tests 7-8, 17, 23, 35, 42)
- **Reset & Partial Reset**: 1 test (Test 9)
- **Spurious Loss Detection**: 1 test (Test 10)
- **ACK Processing**: 6 tests (Tests 11, 16, 21, 27, 40, 41)
- **Send Allowance & Pacing**: 10 tests (Tests 12-15, 19, 25, 36-38, 43)
- **Application-Limited Detection**: 3 tests (Tests 18, 20, 24)
- **Recovery Exit**: 1 test (Test 26)
- **Public API Query**: 1 test (Test 22)
- **Edge Case & Boundary**: 3 tests (Tests 28, 39, 44)
- **State Transition**: 6 tests (Tests 29-34)

### Tests by Public API:
- `BbrCongestionControlInitialize`: 2 tests
- `QuicCongestionControlOnDataSent`: 3 direct + many composite
- `QuicCongestionControlOnDataInvalidated`: 1 test
- `QuicCongestionControlOnDataLost`: 5 tests
- `QuicCongestionControlOnDataAcknowledged`: 22 tests (covers most complex logic)
- `QuicCongestionControlCanSend`: 3 tests
- `QuicCongestionControlGetSendAllowance`: 6 tests
- `BbrCongestionControlReset`: 2 tests
- `QuicCongestionControlSetAppLimited`: 2 tests
- `QuicCongestionControlOnSpuriousCongestionEvent`: 1 test
- `QuicCongestionControlGetCongestionEvent`: 2 tests
- `QuicCongestionControlGetBytesInFlightMax`: 1 test

### State Machine Coverage:
- **5 states covered**: STARTUP, DRAIN, PROBE_BW, PROBE_RTT (all 4 states defined)
- **6 transition scenarios tested**: All major RTT-expiry and duration-based transitions
- **2 transitions NOT testable** via unit tests (require bandwidth convergence):
  - STARTUP → DRAIN (requires bottleneck bandwidth detection)
  - DRAIN → PROBE_BW (requires queue draining)

---

## Tests Still Setting BBR State Directly (Needs Investigation)

The following tests manually set `Bbr->BbrState` directly, which may violate object invariants:

- **Test 30**: Sets `BBR_STATE_PROBE_BW` directly
- **Test 31**: Sets `BBR_STATE_PROBE_RTT` directly
- **Test 32**: Sets `BBR_STATE_PROBE_RTT` directly
- **Test 33**: Sets `BBR_STATE_DRAIN` directly
- **Test 34**: Sets `BBR_STATE_PROBE_BW` directly
- **Test 37**: Sets `BBR_STATE_PROBE_RTT` directly
- **Test 43**: Sets `BBR_STATE_PROBE_RTT` directly

**Recommendation**: These tests should either:
1. Evolve BBR state from STARTUP using only public APIs, OR
2. Be accompanied by an object invariant validation function to ensure BBR state is consistent

---

## Coverage Gaps Addressed by Tests 35-44

Tests 35-44 were specifically added to cover previously uncovered lines in `bbr.c`:

- **Lines 153-154**: Covered by Test 40 (ACK time fallback)
- **Lines 172-174**: Covered by Test 41 (invalid delivery rates)
- **Lines 292-294**: Covered by Test 42 (recovery growth state)
- **Lines 313-314**: Covered by Test 35 (recovery window management)
- **Lines 441-444**: Covered by Test 43 (bandwidth-only send allowance)
- **Lines 447-448**: Covered by Test 36 (fully blocked send allowance)
- **Lines 610-612**: Covered by Test 44 (high pacing rate quantum)
- **Lines 615-617**: Covered by Test 38 (send quantum tiers)

---

## Conclusion

The test suite provides comprehensive coverage of:
- All public BBR APIs
- All BBR states and most testable state transitions
- Core congestion control behaviors (send, loss, ack, recovery)
- Pacing and send allowance calculations
- Edge cases and boundary conditions
- Application-limited detection
- Network statistics reporting

The state transition tests (29-34) specifically target the BBR state machine, while tests 1-28 and 35-44 provide thorough functional coverage of all public APIs.
