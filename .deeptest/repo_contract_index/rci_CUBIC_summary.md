# Repository Contract Index: CUBIC Congestion Control

## Component Overview
**Component**: QUIC_CONGESTION_CONTROL_CUBIC  
**Source**: src/core/cubic.c  
**Header**: src/core/cubic.h  
**Purpose**: Implements the CUBIC congestion control algorithm (RFC8312bis) for QUIC

## Public API Inventory

### 1. CubicCongestionControlInitialize
**Signature**: `void CubicCongestionControlInitialize(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_SETTINGS_INTERNAL* Settings)`

**Purpose**: Initializes the CUBIC congestion control structure with settings and function pointers.

**Preconditions**:
- `Cc` must not be NULL
- `Settings` must not be NULL
- The containing QUIC_CONNECTION must be properly initialized with valid Paths[0]
- Connection->Paths[0].Mtu must be set

**Postconditions**:
- All function pointers in `Cc` are set to CUBIC implementations
- `Cubic->CongestionWindow` = DatagramPayloadLength * InitialWindowPackets
- `Cubic->SlowStartThreshold` = UINT32_MAX
- `Cubic->BytesInFlightMax` = CongestionWindow / 2
- `Cubic->HyStartState` = HYSTART_NOT_STARTED
- Boolean flags (IsInRecovery, HasHadCongestionEvent, etc.) = FALSE
- `Cubic->MinRttInCurrentRound` = UINT64_MAX
- `Cubic->CWndSlowStartGrowthDivisor` = 1

**Side effects**: Logs connection and CUBIC state

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 2. CubicCongestionControlCanSend
**Signature**: `BOOLEAN CubicCongestionControlCanSend(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Determines if more data can be sent based on congestion window.

**Preconditions**:
- `Cc` must be initialized via CubicCongestionControlInitialize

**Postconditions**:
- Returns TRUE if: `BytesInFlight < CongestionWindow` OR `Exemptions > 0`
- Returns FALSE otherwise
- No state modification

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 3. CubicCongestionControlSetExemption
**Signature**: `void CubicCongestionControlSetExemption(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint8_t NumPackets)`

**Purpose**: Sets the number of packets that can bypass congestion control.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- `Cubic->Exemptions` = `NumPackets`

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 4. CubicCongestionControlReset
**Signature**: `void CubicCongestionControlReset(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN FullReset)`

**Purpose**: Resets congestion control state, optionally clearing BytesInFlight.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- `SlowStartThreshold` = UINT32_MAX
- `CongestionWindow` = DatagramPayloadLength * InitialWindowPackets
- `IsInRecovery` = FALSE
- `HasHadCongestionEvent` = FALSE
- `HyStartState` = HYSTART_NOT_STARTED
- If `FullReset` is TRUE: `BytesInFlight` = 0
- Otherwise: `BytesInFlight` unchanged

**Side effects**: Logs connection state

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 5. CubicCongestionControlGetSendAllowance
**Signature**: `uint32_t CubicCongestionControlGetSendAllowance(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid)`

**Purpose**: Calculates how many bytes can be sent, considering pacing if enabled.

**Preconditions**:
- `Cc` must be initialized
- If pacing enabled: `Connection->Paths[0].GotFirstRttSample` should be TRUE for accurate pacing

**Postconditions**:
- Returns 0 if `BytesInFlight >= CongestionWindow`
- Returns `CongestionWindow - BytesInFlight` if pacing is disabled or not ready
- Returns paced send allowance if pacing is active
- Updates `Cubic->LastSendAllowance`

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 6. CubicCongestionControlOnDataSent
**Signature**: `void CubicCongestionControlOnDataSent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`

**Purpose**: Updates state when data is sent on the wire.

**Preconditions**:
- `Cc` must be initialized
- `NumRetransmittableBytes` should be valid (not cause overflow)

**Postconditions**:
- `BytesInFlight` += `NumRetransmittableBytes`
- `BytesInFlightMax` = max(BytesInFlightMax, BytesInFlight)
- `LastSendAllowance` adjusted by subtracting `NumRetransmittableBytes` (clamped to 0)
- `Exemptions` decremented if > 0
- May trigger flow blocked/unblocked events

**Side effects**: May log flow status changes

**Thread-safety**: Must be called at IRQL <= PASSIVE_LEVEL

### 7. CubicCongestionControlOnDataInvalidated
**Signature**: `BOOLEAN CubicCongestionControlOnDataInvalidated(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`

**Purpose**: Updates state when previously sent data is invalidated (e.g., cancelled).

**Preconditions**:
- `Cc` must be initialized
- `BytesInFlight >= NumRetransmittableBytes` (asserted)

**Postconditions**:
- `BytesInFlight` -= `NumRetransmittableBytes`
- Returns TRUE if state changed from blocked to unblocked
- Returns FALSE otherwise

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 8. CubicCongestionControlOnDataAcknowledged
**Signature**: `BOOLEAN CubicCongestionControlOnDataAcknowledged(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ACK_EVENT* AckEvent)`

**Purpose**: Core window adjustment logic when data is ACKed.

**Preconditions**:
- `Cc` must be initialized
- `AckEvent` must not be NULL
- `AckEvent->NumRetransmittableBytes <= Cubic->BytesInFlight` (asserted)

**Postconditions**:
- `BytesInFlight` decremented by acked bytes
- If in recovery and LargestAck > RecoverySentPacketNumber:
  - Exits recovery (`IsInRecovery` = FALSE, `IsInPersistentCongestion` = FALSE)
- If in slow start: `CongestionWindow` grows by acked bytes (divided by growth divisor)
- If in congestion avoidance: Window grows according to CUBIC formula
- HyStart state machine may transition
- `TimeOfLastAck` and `TimeOfLastAckValid` updated
- Returns TRUE if state changed from blocked to unblocked

**Side effects**: May log state changes, may trigger network statistics events

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 9. CubicCongestionControlOnDataLost
**Signature**: `void CubicCongestionControlOnDataLost(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_LOSS_EVENT* LossEvent)`

**Purpose**: Handles packet loss events and triggers congestion response.

**Preconditions**:
- `Cc` must be initialized
- `LossEvent` must not be NULL
- `LossEvent->NumRetransmittableBytes <= Cubic->BytesInFlight` (asserted)

**Postconditions**:
- `BytesInFlight` decremented by lost bytes
- If loss is after most recent congestion event:
  - `RecoverySentPacketNumber` = `LossEvent->LargestSentPacketNumber`
  - Calls CubicCongestionControlOnCongestionEvent
  - Sets `HyStartState` = HYSTART_DONE
- May trigger flow blocked/unblocked events

**Side effects**: Logs CUBIC state

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 10. CubicCongestionControlOnEcn
**Signature**: `void CubicCongestionControlOnEcn(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent)`

**Purpose**: Handles ECN (Explicit Congestion Notification) signals.

**Preconditions**:
- `Cc` must be initialized
- `EcnEvent` must not be NULL

**Postconditions**:
- If ECN signal is after most recent congestion event:
  - `RecoverySentPacketNumber` = `EcnEvent->LargestSentPacketNumber`
  - Calls CubicCongestionControlOnCongestionEvent with ECN flag
  - Sets `HyStartState` = HYSTART_DONE
  - Increments ECN congestion count
- May trigger flow blocked/unblocked events

**Side effects**: Logs CUBIC state

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 11. CubicCongestionControlOnSpuriousCongestionEvent
**Signature**: `BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Reverts congestion control state when a congestion event is determined to be spurious.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- If not in recovery: returns FALSE, no state change
- If in recovery:
  - Restores previous CUBIC state (WindowPrior, WindowMax, WindowLastMax, KCubic, SlowStartThreshold, CongestionWindow, AimdWindow)
  - `IsInRecovery` = FALSE
  - `HasHadCongestionEvent` = FALSE
  - Returns TRUE if unblocked

**Side effects**: Logs CUBIC state

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 12. CubicCongestionControlGetNetworkStatistics
**Signature**: `void CubicCongestionControlGetNetworkStatistics(_In_ const QUIC_CONNECTION* Connection, _In_ const QUIC_CONGESTION_CONTROL* Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics)`

**Purpose**: Retrieves current network statistics.

**Preconditions**:
- `Connection`, `Cc`, `NetworkStatistics` must not be NULL
- `Cc` must be initialized

**Postconditions**:
- `NetworkStatistics` populated with current values:
  - BytesInFlight, PostedBytes, IdealBytes, SmoothedRTT, CongestionWindow, Bandwidth

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 13. CubicCongestionControlGetBytesInFlightMax
**Signature**: `uint32_t CubicCongestionControlGetBytesInFlightMax(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Returns the maximum bytes in flight observed.

**Preconditions**: `Cc` must be initialized

**Postconditions**: Returns `Cubic->BytesInFlightMax`

### 14. CubicCongestionControlGetExemptions
**Signature**: `uint8_t CubicCongestionControlGetExemptions(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Returns the current exemption count.

**Preconditions**: `Cc` must be initialized

**Postconditions**: Returns `Cubic->Exemptions`

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 15. CubicCongestionControlGetCongestionWindow
**Signature**: `uint32_t CubicCongestionControlGetCongestionWindow(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Returns the current congestion window.

**Preconditions**: `Cc` must be initialized

**Postconditions**: Returns `Cubic->CongestionWindow`

### 16. CubicCongestionControlIsAppLimited
**Signature**: `BOOLEAN CubicCongestionControlIsAppLimited(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Returns whether the connection is application-limited.

**Preconditions**: `Cc` must be initialized

**Postconditions**: Always returns FALSE (not implemented for CUBIC)

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 17. CubicCongestionControlSetAppLimited
**Signature**: `void CubicCongestionControlSetAppLimited(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Sets the connection as application-limited.

**Preconditions**: `Cc` must be initialized

**Postconditions**: No-op (not implemented for CUBIC)

**Thread-safety**: Must be called at IRQL <= DISPATCH_LEVEL

### 18. CubicCongestionControlLogOutFlowStatus
**Signature**: `void CubicCongestionControlLogOutFlowStatus(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Logs current outflow status for debugging.

**Preconditions**: `Cc` must be initialized

**Postconditions**: No state change

**Side effects**: Logs trace event

### 19. CubeRoot (Internal but potentially testable via public API effects)
**Signature**: `uint32_t CubeRoot(uint32_t Radicand)`

**Purpose**: Computes integer cube root using shifting algorithm.

**Preconditions**: None (all uint32_t values valid)

**Postconditions**: Returns y such that y^3 <= Radicand < (y+1)^3

**Thread-safety**: Pure function, thread-safe

---

## Private/Internal Functions (NOT TO BE TESTED DIRECTLY)

### CubicCongestionHyStartChangeState
- Changes HyStart state machine state
- Updates CWndSlowStartGrowthDivisor based on state

### CubicCongestionHyStartResetPerRttRound
- Resets per-RTT round HyStart tracking

### CubicCongestionControlUpdateBlockedState
- Updates flow control blocked state and triggers events

### CubicCongestionControlOnCongestionEvent
- Core congestion event handler (called by OnDataLost and OnEcn)

### QuicConnLogCubic
- Logging helper

---

## Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC State Invariants:

1. **Congestion Window**: `CongestionWindow > 0` always
2. **Bytes In Flight**: `BytesInFlight <= BytesInFlightMax` (enforced on increment)
3. **Slow Start Threshold**: `SlowStartThreshold >= CongestionWindow` OR `SlowStartThreshold == UINT32_MAX`
4. **Recovery State**: 
   - If `HasHadCongestionEvent == TRUE`, then `RecoverySentPacketNumber` is valid
   - `IsInPersistentCongestion == TRUE` implies `IsInRecovery == TRUE`
5. **HyStart State Machine**:
   - Valid states: HYSTART_NOT_STARTED (0), HYSTART_ACTIVE (1), HYSTART_DONE (2)
   - `CWndSlowStartGrowthDivisor == 1` when NOT in HYSTART_ACTIVE
   - `MinRttInLastRound` and `MinRttInCurrentRound` track RTT samples
6. **Time Tracking**:
   - If `TimeOfLastAckValid == TRUE`, then `TimeOfLastAck` contains a valid timestamp

---

## HyStart++ State Machine

```
     ┌─────────────────┐
     │ HYSTART_NOT_    │
     │   STARTED (0)   │
     └────────┬────────┘
              │
              │ RTT increase detected
              │ (MinRttInCurrentRound >= MinRttInLastRound + Eta)
              ↓
     ┌─────────────────┐
     │   HYSTART_      │
     │   ACTIVE (1)    │────┐ RTT decreased
     └────────┬────────┘    │ (Resume slow start)
              │              ↓
              │ ConservativeSlowStartRounds == 0
              ↓
     ┌─────────────────┐
     │   HYSTART_      │←───────────────┐
     │    DONE (2)     │                │
     └─────────────────┘                │
              ↑                         │
              └─────────────────────────┘
                Congestion event or loss
```

**State Transitions**:
- HYSTART_NOT_STARTED → HYSTART_ACTIVE: When delay increase detected (MinRttInCurrentRound >= MinRttInLastRound + Eta)
- HYSTART_ACTIVE → HYSTART_NOT_STARTED: When RTT decreased below baseline
- HYSTART_ACTIVE → HYSTART_DONE: When ConservativeSlowStartRounds reaches 0
- Any state → HYSTART_DONE: On congestion event (loss or ECN)

**State-specific Invariants**:
- **HYSTART_NOT_STARTED**: `CWndSlowStartGrowthDivisor == 1`, sampling RTTs
- **HYSTART_ACTIVE**: `CWndSlowStartGrowthDivisor == QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR`, counting down `ConservativeSlowStartRounds`
- **HYSTART_DONE**: `CWndSlowStartGrowthDivisor == 1`, in congestion avoidance or post-congestion

---

## Environment Invariants

1. **Initialization Order**: Must call `CubicCongestionControlInitialize` before any other CUBIC functions
2. **Connection Context**: All functions require a valid `QUIC_CONNECTION` context accessible via `QuicCongestionControlGetConnection(Cc)`
3. **Path Validity**: `Connection->Paths[0]` must be valid and contain:
   - Valid MTU (for calculating DatagramPayloadSize)
   - RTT samples (for pacing and window calculations)
4. **No Leaks**: All memory is managed by the containing `QUIC_CONNECTION`; no dynamic allocation in CUBIC
5. **IRQL Constraints**: Most functions require IRQL <= DISPATCH_LEVEL

---

## Dependency Map

### Internal Dependencies:
- **QuicCongestionControlGetConnection**: Macro to get containing QUIC_CONNECTION from Cc pointer
- **QuicPathGetDatagramPayloadSize**: Gets MTU-based payload size from path
- **QuicConnLogOutFlowStats, QuicConnLogCubic**: Logging functions
- **QuicSendBufferConnectionAdjust**: Adjusts send buffer when BytesInFlightMax increases
- **QuicConnAddOutFlowBlockedReason / QuicConnRemoveOutFlowBlockedReason**: Flow control signaling
- **CxPlatTimeUs64, CxPlatTimeDiff64, CxPlatTimeAtOrBefore64**: Time utilities

### Public API Call Relationships:
- **Initialize** → (standalone, entry point)
- **CanSend** → (query, no side effects)
- **Reset** → CubicCongestionHyStartChangeState (internal)
- **OnDataSent** → CanSend, UpdateBlockedState (internal)
- **OnDataInvalidated** → CanSend, UpdateBlockedState (internal)
- **OnDataAcknowledged** → CanSend, UpdateBlockedState, HyStartChangeState (internal), may call logging
- **OnDataLost** → OnCongestionEvent (internal), UpdateBlockedState, HyStartChangeState
- **OnEcn** → OnCongestionEvent (internal), UpdateBlockedState, HyStartChangeState
- **OnSpuriousCongestionEvent** → CanSend, UpdateBlockedState (internal)

### Callbacks/Events:
- None (CUBIC does not register callbacks; it responds to events via public API calls)

---

## Key Constants (from code)

- `TEN_TIMES_BETA_CUBIC = 7` (BETA = 0.7)
- `TEN_TIMES_C_CUBIC = 4` (C = 0.4)
- `QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS` (min window after persistent congestion)
- `QUIC_MIN_PACING_RTT` (minimum RTT for pacing)
- `QUIC_HYSTART_DEFAULT_N_SAMPLING` (number of ACKs to sample for HyStart)
- `QUIC_HYSTART_DEFAULT_MIN_ETA`, `QUIC_HYSTART_DEFAULT_MAX_ETA` (RTT increase thresholds)
- `QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR` (growth rate during conservative slow start)
- `QUIC_CONSERVATIVE_SLOW_START_DEFAULT_ROUNDS` (number of RTT rounds in conservative slow start)

---

## Test Strategy Notes

### Scenarios to Cover:
1. **Initialization**: Various settings, MTU values, re-initialization
2. **Slow Start**: Window growth, transition to congestion avoidance
3. **Congestion Avoidance**: CUBIC formula, AIMD window, Reno-friendly region
4. **HyStart++**: State transitions, RTT sampling, conservative slow start
5. **Recovery**: Entry, exit conditions, spurious congestion handling
6. **Persistent Congestion**: Window reset to minimum
7. **ECN**: Congestion signaling without loss
8. **Pacing**: Send allowance calculation, overflow handling
9. **Exemptions**: Bypass congestion control
10. **Flow Control**: Blocked/unblocked state transitions
11. **Edge Cases**: Zero bytes acked, overflow in calculations, boundary values

### Coverage-Critical Regions:
- CubeRoot function (lines 45-62)
- HyStart state machine (lines 476-539 in OnDataAcknowledged)
- CUBIC window calculation (lines 610-670 in OnDataAcknowledged)
- Congestion event handling (lines 272-368)
- Pacing logic (lines 206-240 in GetSendAllowance)
- Recovery entry/exit (lines 453-468, 735-745)
- Spurious congestion revert (lines 788-823)

