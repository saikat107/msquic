# Repository Contract Index - BBR Congestion Control Component

## Component Overview
The BBR (Bottleneck Bandwidth and RTT) congestion control implementation in MsQuic, located in `src/core/bbr.c` and `src/core/bbr.h`.

## BBR State Machine

```
                    ┌─────────────┐
                    │   STARTUP   │ (Find bandwidth)
                    └──────┬──────┘
                           │ BtlbwFound
                           ▼
                    ┌─────────────┐
                    │    DRAIN    │ (Drain queue)
                    └──────┬──────┘
                           │ BytesInFlight <= target
                           ▼
                    ┌─────────────┐
              ┌────►│  PROBE_BW   │◄────┐ (Maintain bandwidth)
              │     └──────┬──────┘     │
              │            │            │
              │            │ RTT expired│
              │            ▼            │
              │     ┌─────────────┐    │
              └─────┤  PROBE_RTT  │────┘ (Measure RTT)
                    └─────────────┘
```

### State Invariants:

**BBR_STATE_STARTUP:**
- CwndGain = kHighGain (~2.89)
- PacingGain = kHighGain (~2.89)
- Objective: Aggressively grow congestion window to find bottleneck bandwidth
- Exit condition: BtlbwFound = TRUE (when bandwidth stops growing at expected rate)

**BBR_STATE_DRAIN:**
- PacingGain = kDrainGain (~1/2.89)
- CwndGain = kHighGain
- Objective: Drain excess packets queued during STARTUP
- Exit condition: BytesInFlight <= target congestion window

**BBR_STATE_PROBE_BW:**
- CwndGain = kCwndGain (2.0)
- PacingGain cycles through kPacingGain[8] array
- Objective: Maintain bandwidth estimate and probe for more
- Continuously cycles through 8 pacing gains to probe for bandwidth
- Exit condition: MinRtt expires AND NOT exiting quiescence

**BBR_STATE_PROBE_RTT:**
- CongestionWindow = MinCwndInMss * MTU (4 packets)
- PacingGain = GAIN_UNIT (1.0)
- Objective: Force low inflight to get fresh MinRtt sample
- Must stay for at least kProbeRttTimeInUs (200ms) with low inflight
- Exit condition: ProbeRttEndTime reached AND ProbeRttRound completed

## Recovery State Machine

```
┌────────────────────┐
│ NOT_RECOVERY (0)   │
└──────┬─────────────┘
       │ OnDataLost
       ▼
┌────────────────────┐
│ CONSERVATIVE (1)   │ (First round)
└──────┬─────────────┘
       │ NewRoundTrip
       ▼
┌────────────────────┐
│ GROWTH (2)         │ (Grow recovery window)
└──────┬─────────────┘
       │ EndOfRecovery acked
       ▼
┌────────────────────┐
│ NOT_RECOVERY (0)   │
└────────────────────┘
```

### Recovery State Invariants:

**RECOVERY_STATE_NOT_RECOVERY:**
- Normal operation, no packet loss being handled
- RecoveryWindow may be stale
- EndOfRecoveryValid = FALSE

**RECOVERY_STATE_CONSERVATIVE:**
- First round trip after loss event
- RecoveryWindow frozen at BytesInFlight at time of loss
- RecoveryWindow decreases by lost bytes
- EndOfRecoveryValid = TRUE
- Exit: NewRoundTrip

**RECOVERY_STATE_GROWTH:**
- Subsequent rounds after loss
- RecoveryWindow grows by AckedBytes
- Continues until EndOfRecovery packet is acked
- Exit: LargestAck > EndOfRecovery

## Public API Functions

1. **BbrCongestionControlInitialize** - Initialize BBR state
2. **BbrCongestionControlCanSend** - Check if can send more data
3. **BbrCongestionControlSetExemption** - Set exemption count
4. **BbrCongestionControlReset** - Reset BBR state
5. **BbrCongestionControlGetSendAllowance** - Calculate send allowance with pacing
6. **BbrCongestionControlGetCongestionWindow** - Get effective congestion window
7. **BbrCongestionControlOnDataSent** - Handle data sent event
8. **BbrCongestionControlOnDataInvalidated** - Handle data invalidation
9. **BbrCongestionControlOnDataAcknowledged** - Main BBR logic on ACK
10. **BbrCongestionControlOnDataLost** - Handle packet loss
11. **BbrCongestionControlOnSpuriousCongestionEvent** - Handle spurious congestion
12. **BbrCongestionControlLogOutFlowStatus** - Log outflow statistics
13. **BbrCongestionControlGetExemptions** - Get current exemptions
14. **BbrCongestionControlGetBytesInFlightMax** - Get max bytes in flight
15. **BbrCongestionControlIsAppLimited** - Check if app-limited
16. **BbrCongestionControlSetAppLimited** - Set app-limited state
17. **BbrCongestionControlGetNetworkStatistics** - Get network statistics

See full RCI document for detailed API contracts, preconditions, and postconditions.
