# Repository Contract Index (RCI) - CUBIC Component

**Component**: CUBIC Congestion Control  
**Source File**: `src/core/cubic.c`  
**Header File**: `src/core/cubic.h`, `src/core/congestion_control.h`  
**Last Updated**: 2026-02-05

---

## Executive Summary

The CUBIC component implements RFC 8312bis cubic congestion control algorithm for QUIC connections. It manages congestion window growth/reduction, packet pacing, slow start, congestion avoidance, and HyStart++ for detecting optimal exit from slow start.

**Public Interface**: 18 function pointers in `QUIC_CONGESTION_CONTROL` vtable + 1 initialization function  
**State Machine**: Yes - Recovery state machine and HyStart++ state machine  
**Thread Safety**: Must be called at DISPATCH_LEVEL or lower (PASSIVE_LEVEL for OnDataSent)  
**Resource Management**: Embedded in `QUIC_CONGESTION_CONTROL` union, no dynamic allocation

---

## A) Public API Inventory

### 1. `CubicCongestionControlInitialize`
**Signature**: `void CubicCongestionControlInitialize(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_SETTINGS_INTERNAL* Settings)`

**What it does**: Initializes the CUBIC congestion control state, sets up function pointers, configures initial window based on settings.

**Preconditions**:
- `Cc` must be non-NULL and point to a valid `QUIC_CONGESTION_CONTROL` structure
- `Cc` must be embedded in a `QUIC_CONNECTION` structure (required for `QuicCongestionControlGetConnection()` pointer arithmetic)
- `Settings` must be non-NULL with valid `InitialWindowPackets` and `SendIdleTimeoutMs`
- The containing `QUIC_CONNECTION` must have `Paths[0]` initialized with valid `Mtu`

**Postconditions**:
- All 18 function pointers are set to CUBIC implementations
- `CongestionWindow = DatagramPayloadLength * InitialWindowPackets`
- `SlowStartThreshold = UINT32_MAX` (no threshold initially)
- `BytesInFlightMax = CongestionWindow / 2`
- `HyStartState = HYSTART_NOT_STARTED`
- `CWndSlowStartGrowthDivisor = 1`
- All boolean flags (`IsInRecovery`, `HasHadCongestionEvent`, etc.) are FALSE
- `MinRttInCurrentRound = UINT64_MAX`

**Side Effects**: Calls logging functions (`QuicConnLogOutFlowStats`, `QuicConnLogCubic`)

**Error Handling**: None (void return). Invalid inputs lead to undefined behavior.

**Thread Safety**: IRQL <= DISPATCH_LEVEL. Not thread-safe - caller must serialize.

**Ownership**: `Cc` remains owned by caller. No internal allocations.

---

### 2. `CubicCongestionControlCanSend`
**Signature**: `BOOLEAN CubicCongestionControlCanSend(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Determines whether more data can be sent based on congestion window and exemptions.

**Preconditions**:
- `Cc` must be initialized via `CubicCongestionControlInitialize`
- `BytesInFlight` and `CongestionWindow` must be consistent with actual network state

**Postconditions**:
- Returns TRUE if `BytesInFlight < CongestionWindow` OR `Exemptions > 0`
- Returns FALSE otherwise
- No state modification

**Side Effects**: None (pure query)

**Error Handling**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL. Read-only, safe for concurrent reads if caller ensures no concurrent writes.

---

### 3. `CubicCongestionControlSetExemption`
**Signature**: `void CubicCongestionControlSetExemption(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint8_t NumPackets)`

**What it does**: Allows `NumPackets` to be sent ignoring congestion window (for probe packets).

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- `Exemptions = NumPackets`
- Packets sent will decrement `Exemptions` via `OnDataSent`

**Side Effects**: None

**Error Handling**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL. Caller must serialize.

---

### 4. `CubicCongestionControlReset`
**Signature**: `void CubicCongestionControlReset(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN FullReset)`

**What it does**: Resets congestion control to initial state (used after idle timeout or path change).

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- `SlowStartThreshold = UINT32_MAX`
- `CongestionWindow = DatagramPayloadLength * InitialWindowPackets`
- `IsInRecovery = FALSE`
- `HasHadCongestionEvent = FALSE`
- `HyStartState = HYSTART_NOT_STARTED`
- If `FullReset == TRUE`: `BytesInFlight = 0`
- If `FullReset == FALSE`: `BytesInFlight` unchanged

**Side Effects**: Calls HyStart state change, logging functions

**Error Handling**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL

---

### 5. `CubicCongestionControlGetSendAllowance`
**Signature**: `uint32_t CubicCongestionControlGetSendAllowance(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid)`

**What it does**: Calculates how many bytes can be sent immediately, considering pacing.

**Preconditions**:
- `Cc` must be initialized
- `TimeSinceLastSend` in microseconds, valid only if `TimeSinceLastSendValid == TRUE`

**Postconditions**:
- Returns 0 if `BytesInFlight >= CongestionWindow`
- Returns `CongestionWindow - BytesInFlight` if pacing disabled
- Returns paced allowance if pacing enabled (based on RTT and estimated window)
- Updates `LastSendAllowance` if pacing active

**Side Effects**: Modifies `LastSendAllowance` when pacing

**Error Handling**: Handles overflow by clamping to available window

**Thread Safety**: IRQL <= DISPATCH_LEVEL

---

### 6. `CubicCongestionControlOnDataSent`
**Signature**: `void CubicCongestionControlOnDataSent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`

**What it does**: Updates state when retransmittable data is sent.

**Preconditions**:
- `Cc` must be initialized
- `NumRetransmittableBytes` must represent actual retransmittable bytes sent

**Postconditions**:
- `BytesInFlight += NumRetransmittableBytes`
- `BytesInFlightMax = max(BytesInFlightMax, BytesInFlight)`
- `LastSendAllowance` decremented (for pacing)
- `Exemptions` decremented if > 0
- May update blocked state (logs and connection flow control state)

**Side Effects**: May call `QuicSendBufferConnectionAdjust`, `CubicCongestionControlUpdateBlockedState` (which logs)

**Error Handling**: None

**Thread Safety**: IRQL <= PASSIVE_LEVEL (note: more restrictive than other functions)

---

### 7. `CubicCongestionControlOnDataInvalidated`
**Signature**: `BOOLEAN CubicCongestionControlOnDataInvalidated(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`

**What it does**: Removes bytes from in-flight that are neither lost nor acknowledged (e.g., 0-RTT rejected).

**Preconditions**:
- `Cc` must be initialized
- `BytesInFlight >= NumRetransmittableBytes` (contract: caller ensures this)

**Postconditions**:
- `BytesInFlight -= NumRetransmittableBytes`
- Returns TRUE if state changed from blocked to unblocked

**Side Effects**: May update blocked state (logs)

**Error Handling**: Debug assertion if `BytesInFlight < NumRetransmittableBytes`

**Thread Safety**: IRQL <= DISPATCH_LEVEL

---

### 8. `CubicCongestionControlOnDataAcknowledged`
**Signature**: `BOOLEAN CubicCongestionControlOnDataAcknowledged(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ACK_EVENT* AckEvent)`

**What it does**: Handles acknowledgment of data, grows congestion window according to CUBIC algorithm.

**Preconditions**:
- `Cc` must be initialized
- `AckEvent` must be non-NULL with valid fields
- `BytesInFlight >= AckEvent->NumRetransmittableBytes`

**Postconditions**:
- `BytesInFlight -= NumRetransmittableBytes`
- In Slow Start: `CongestionWindow += BytesAcked / CWndSlowStartGrowthDivisor`
- In Congestion Avoidance: Grows window according to CUBIC formula or AIMD (whichever is larger)
- May exit recovery if `LargestAck > RecoverySentPacketNumber`
- May trigger HyStart++ state transitions
- Returns TRUE if became unblocked

**Side Effects**: Complex - updates many internal state variables, may trigger HyStart++ state changes, logs

**Error Handling**: Debug assertion if `BytesInFlight < NumRetransmittableBytes`

**Thread Safety**: IRQL <= DISPATCH_LEVEL

**Algorithm Details**: 
- Slow Start: Linear growth by `BytesAcked / CWndSlowStartGrowthDivisor`
- HyStart++: Monitors RTT increases to exit slow start early
- Congestion Avoidance: CUBIC function `W_cubic(t)` vs. AIMD, takes max
- Recovery: No window growth until recovery complete

---

### 9. `CubicCongestionControlOnDataLost`
**Signature**: `void CubicCongestionControlOnDataLost(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_LOSS_EVENT* LossEvent)`

**What it does**: Handles packet loss, triggers congestion event if loss is after last recovery.

**Preconditions**:
- `Cc` must be initialized
- `LossEvent` must be non-NULL
- `BytesInFlight >= LossEvent->NumRetransmittableBytes`

**Postconditions**:
- `BytesInFlight -= NumRetransmittableBytes`
- If loss is new (after last recovery): Enters recovery, reduces window (multiplicative decrease)
- `RecoverySentPacketNumber = LargestSentPacketNumber`
- `HyStartState = HYSTART_DONE`

**Side Effects**: May call `CubicCongestionControlOnCongestionEvent` (complex side effects), updates blocked state, logs

**Error Handling**: Debug assertion if `BytesInFlight < NumRetransmittableBytes`

**Thread Safety**: IRQL <= DISPATCH_LEVEL

---

### 10. `CubicCongestionControlOnEcn`
**Signature**: `void CubicCongestionControlOnEcn(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent)`

**What it does**: Handles ECN congestion signal (similar to loss but triggered by ECN marks).

**Preconditions**:
- `Cc` must be initialized
- `EcnEvent` must be non-NULL

**Postconditions**:
- If ECN signal is new: Enters recovery, reduces window, does NOT save previous state (cannot be spurious)
- `RecoverySentPacketNumber = LargestSentPacketNumber`
- `HyStartState = HYSTART_DONE`

**Side Effects**: Calls `CubicCongestionControlOnCongestionEvent`, increments `EcnCongestionCount`, updates blocked state, logs

**Error Handling**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL

---

### 11. `CubicCongestionControlOnSpuriousCongestionEvent`
**Signature**: `BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Reverts window reduction if loss was spurious (packets later acknowledged).

**Preconditions**:
- `Cc` must be initialized
- Only valid if last congestion event was due to loss (not ECN)

**Postconditions**:
- If `IsInRecovery == TRUE`: Restores previous window state (WindowPrior, WindowMax, WindowLastMax, KCubic, SlowStartThreshold, CongestionWindow, AimdWindow)
- `IsInRecovery = FALSE`
- `HasHadCongestionEvent = FALSE`
- Returns TRUE if became unblocked

**Side Effects**: Updates blocked state, logs

**Error Handling**: Returns FALSE if not in recovery

**Thread Safety**: IRQL <= DISPATCH_LEVEL

---

### 12. `CubicCongestionControlGetNetworkStatistics`
**Signature**: `void CubicCongestionControlGetNetworkStatistics(_In_ const QUIC_CONNECTION* const Connection, _In_ const QUIC_CONGESTION_CONTROL* const Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics)`

**What it does**: Populates network statistics structure with current CC state.

**Preconditions**:
- `Connection` must be non-NULL and valid
- `Cc` must be initialized
- `NetworkStatistics` must be non-NULL

**Postconditions**:
- `NetworkStatistics` populated with: BytesInFlight, PostedBytes, IdealBytes, SmoothedRTT, CongestionWindow, Bandwidth

**Side Effects**: None

**Error Handling**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL. Read-only snapshot.

---

### 13. `CubicCongestionControlLogOutFlowStatus`
**Signature**: `void CubicCongestionControlLogOutFlowStatus(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Logs detailed outflow statistics for diagnostics.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**: None

**Side Effects**: Emits telemetry event (ETW/LTTng)

**Error Handling**: None

**Thread Safety**: Read-only

---

### 14. `CubicCongestionControlGetExemptions`
**Signature**: `uint8_t CubicCongestionControlGetExemptions(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Returns current exemption count.

**Preconditions**: Cc initialized

**Postconditions**: Returns `Exemptions` value

**Side Effects**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL. Read-only.

---

### 15. `CubicCongestionControlGetBytesInFlightMax`
**Signature**: `uint32_t CubicCongestionControlGetBytesInFlightMax(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Returns max bytes in flight observed.

**Preconditions**: Cc initialized

**Postconditions**: Returns `BytesInFlightMax` value

**Side Effects**: None

**Thread Safety**: Read-only

---

### 16. `CubicCongestionControlGetCongestionWindow`
**Signature**: `uint32_t CubicCongestionControlGetCongestionWindow(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Returns current congestion window size.

**Preconditions**: Cc initialized

**Postconditions**: Returns `CongestionWindow` value

**Side Effects**: None

**Thread Safety**: Read-only

---

### 17. `CubicCongestionControlIsAppLimited`
**Signature**: `BOOLEAN CubicCongestionControlIsAppLimited(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Checks if sender is application-limited (not implemented for CUBIC).

**Preconditions**: Cc initialized

**Postconditions**: Always returns FALSE

**Side Effects**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL

**Note**: Stub implementation - CUBIC doesn't track app-limited state.

---

### 18. `CubicCongestionControlSetAppLimited`
**Signature**: `void CubicCongestionControlSetAppLimited(_In_ struct QUIC_CONGESTION_CONTROL* Cc)`

**What it does**: Sets app-limited state (not implemented for CUBIC).

**Preconditions**: Cc initialized

**Postconditions**: No-op

**Side Effects**: None

**Thread Safety**: IRQL <= DISPATCH_LEVEL

**Note**: Stub implementation - CUBIC doesn't track app-limited state.

---

## B) Type/Object Invariants

### `QUIC_CONGESTION_CONTROL_CUBIC` Structure Invariants

**Always valid**:
1. `0 <= BytesInFlight <= BytesInFlightMax`
2. `CongestionWindow > 0` (after initialization)
3. `SlowStartThreshold >= CongestionWindow` OR `SlowStartThreshold == UINT32_MAX`
4. `InitialWindowPackets > 0`
5. `SendIdleTimeoutMs >= 0`
6. `Exemptions <= 255` (uint8_t)
7. If `TimeOfLastAckValid == TRUE`, then `TimeOfLastAck` contains valid timestamp
8. If `HasHadCongestionEvent == TRUE`, then `RecoverySentPacketNumber` is valid

**State Machine: Recovery**
- **States**: `IsInRecovery = {FALSE, TRUE}`
- **Transitions**:
  - FALSE → TRUE: On loss or ECN event after last recovery
  - TRUE → FALSE: On ACK of packet sent after `RecoverySentPacketNumber`
- **Invariant**: If `IsInRecovery == TRUE`, then `HasHadCongestionEvent == TRUE`

**State Machine: HyStart++**
- **States**: `HyStartState ∈ {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}`
- **Transitions**:
  ```
  HYSTART_NOT_STARTED 
    → HYSTART_ACTIVE: RTT increase detected (MinRtt >= LastMinRtt + Eta)
    → HYSTART_DONE: On loss/ECN event
  
  HYSTART_ACTIVE
    → HYSTART_NOT_STARTED: RTT decreased (spurious detection)
    → HYSTART_DONE: After ConservativeSlowStartRounds complete OR loss/ECN
  
  HYSTART_DONE
    → (terminal state - never exits)
  ```

**State Invariants by HyStart State**:
- `HYSTART_NOT_STARTED`: `CWndSlowStartGrowthDivisor == 1`, `ConservativeSlowStartRounds` undefined
- `HYSTART_ACTIVE`: `CWndSlowStartGrowthDivisor == 2`, `ConservativeSlowStartRounds > 0`
- `HYSTART_DONE`: `CWndSlowStartGrowthDivisor == 1`, in congestion avoidance (may still be growing)

**Persistent Congestion**:
- `IsInPersistentCongestion` can only be TRUE if `IsInRecovery == TRUE`
- When TRUE: Window reset to minimum (2 * MTU)

**Window Relationships**:
- In Slow Start: `CongestionWindow < SlowStartThreshold`
- In Congestion Avoidance: `CongestionWindow >= SlowStartThreshold` OR `SlowStartThreshold == UINT32_MAX`

**HyStart RTT Tracking**:
- `MinRttInCurrentRound <= MinRttInLastRound` OR `== UINT64_MAX` (initial)
- `HyStartAckCount` reset each RTT round
- `CssBaselineMinRtt` valid only when `HyStartState == HYSTART_ACTIVE`

---

## C) Environment Invariants

1. **Initialization Required**: `CubicCongestionControlInitialize` must be called before any other function
2. **Embedding Requirement**: `QUIC_CONGESTION_CONTROL` must be embedded in a `QUIC_CONNECTION` structure at the correct offset for `QuicCongestionControlGetConnection()` macro to work
3. **Connection State Dependencies**:
   - `Connection->Paths[0]` must be initialized and valid
   - `Connection->Paths[0].Mtu` must be set before initialization
   - `Connection->Send.NextPacketNumber` used for HyStart round tracking
4. **No Memory Leaks**: CUBIC has no dynamic allocations, no explicit cleanup needed
5. **Logging Invariant**: Logging functions (`QuicTraceEvent`) must be callable (may be no-ops in release builds)
6. **Settings Constraints**:
   - `Settings.InitialWindowPackets` typically 10 (RFC default)
   - `Settings.HyStartEnabled` controls whether HyStart++ is active
   - `Settings.PacingEnabled` controls whether pacing logic is used

---

## D) Dependency Map & State Transition Diagram

### Call Relationships

**Initialization Chain**:
```
CubicCongestionControlInitialize
  └─> CubicCongestionHyStartResetPerRttRound
  └─> QuicConnLogOutFlowStats
  └─> QuicConnLogCubic
```

**Data Flow (typical packet lifecycle)**:
```
OnDataSent
  └─> CubicCongestionControlUpdateBlockedState
        └─> CubicCongestionControlCanSend

OnDataAcknowledged
  ├─> [Recovery check]
  ├─> [HyStart++ processing]
  │     └─> CubicCongestionHyStartChangeState
  │     └─> CubicCongestionHyStartResetPerRttRound
  ├─> [Slow Start growth]
  ├─> [Congestion Avoidance growth]
  │     └─> CubeRoot (math helper)
  └─> CubicCongestionControlUpdateBlockedState

OnDataLost
  ├─> CubicCongestionControlOnCongestionEvent
  │     ├─> [Window reduction: CWND = CWND * BETA]
  │     └─> QuicTraceEvent (logging)
  ├─> CubicCongestionHyStartChangeState(HYSTART_DONE)
  └─> CubicCongestionControlUpdateBlockedState

OnEcn
  └─> (similar to OnDataLost, but Ecn=TRUE)
```

### State Transition Diagram

#### Recovery State Machine
```
┌─────────────────┐     Loss/ECN after last recovery
│ Not In Recovery │────────────────────────────────────┐
│ IsInRecovery=F  │                                    │
└─────────────────┘                                    ▼
       ▲                                      ┌──────────────────┐
       │                                      │   In Recovery    │
       │   ACK of packet > RecoverySentPN     │  IsInRecovery=T  │
       └──────────────────────────────────────│                  │
                                              └──────────────────┘
```

#### HyStart++ State Machine
```
┌──────────────────┐  RTT increase ≥ Eta    ┌──────────────────┐
│ HYSTART_NOT_     │───────────────────────▶│ HYSTART_ACTIVE   │
│    STARTED       │                        │ (Conservative SS)│
│ (Normal SS)      │◀───────────────────────│                  │
└──────────────────┘  RTT decreased         └──────────────────┘
        │                                            │
        │ Loss/ECN                                   │ ConservativeRounds=0
        │                                            │ OR Loss/ECN
        ▼                                            ▼
┌──────────────────┐                        ┌──────────────────┐
│  HYSTART_DONE    │◀───────────────────────│  (Transition)    │
│ (CA or slow SS)  │                        └──────────────────┘
└──────────────────┘
```

### Indirect Calls / Callbacks
- **None**: CUBIC does not register callbacks or use function pointers internally
- **Assumptions**: 
  - `QuicCongestionControlGetConnection(Cc)` macro correctly computes connection pointer (relies on struct layout)
  - All `QuicTraceEvent`, `QuicConn*`, `QuicPath*`, `QuicSendBuffer*` functions are available and safe to call

---

## Scenario Catalog (for Test Generation)

### Initialization Scenarios
1. Initialize with default settings (10 initial window packets)
2. Initialize with custom initial window (vary 1-100 packets)
3. Initialize with different MTU sizes (1200, 1280, 1500, 9000)
4. Initialize then reset (partial and full reset)

### Send Control Scenarios
5. CanSend returns TRUE when BytesInFlight < CongestionWindow
6. CanSend returns FALSE when BytesInFlight >= CongestionWindow
7. CanSend returns TRUE when Exemptions > 0 even if CC blocked
8. GetSendAllowance with pacing disabled (returns full window)
9. GetSendAllowance with pacing enabled (gradual allowance)
10. GetSendAllowance when CC blocked (returns 0)

### Slow Start Scenarios
11. Window grows linearly during slow start (OnDataAcknowledged)
12. Slow start reaches threshold and enters congestion avoidance
13. HyStart++ exits slow start early on RTT increase
14. HyStart++ conservative slow start (reduced growth rate)
15. HyStart++ spurious exit (RTT decreases, resumes normal SS)

### Congestion Avoidance Scenarios
16. Window grows via CUBIC function in CA
17. Window grows via AIMD if AIMD > CUBIC (friendly mode)
18. Long-term CA growth towards WindowMax
19. CA with very long time since congestion event (large K)

### Loss and Recovery Scenarios
20. First packet loss triggers congestion event (window reduction)
21. Loss during recovery does not trigger new event
22. Loss after recovery triggers new congestion event
23. Persistent congestion resets window to minimum
24. Spurious loss reverts window to previous state

### ECN Scenarios
25. ECN signal triggers congestion event
26. ECN during recovery does not trigger new event
27. ECN after recovery triggers new congestion event

### State Tracking Scenarios
28. BytesInFlight tracked across Sent/Acked/Lost/Invalidated
29. BytesInFlightMax updated when BytesInFlight increases
30. Exemptions decremented on each send
31. LastSendAllowance updated with pacing

### Query Scenarios
32. GetCongestionWindow returns current window
33. GetExemptions returns current exemptions
34. GetBytesInFlightMax returns max observed
35. GetNetworkStatistics populates all fields
36. IsAppLimited/SetAppLimited (stubs - no effect)

### Edge Cases
37. OnDataAcknowledged with 0 bytes (no-op)
38. OnDataAcknowledged in recovery (no window growth)
39. Multiple consecutive losses
40. ACK event with implicit ACK flag
41. GetSendAllowance overflow handling

---

## Notes for Test Generation

- **Contract-Safe Testing**: All tests must provide initialized `QUIC_CONNECTION` with valid `Paths[0]`
- **No Private Function Access**: Tests cannot call `CubeRoot`, `CubicCongestionControlUpdateBlockedState`, `CubicCongestionControlOnCongestionEvent`, `CubicCongestionHyStartChangeState`, `CubicCongestionHyStartResetPerRttRound` - these are private helpers
- **Public Interface Only**: Tests must use the 18 public functions via `QUIC_CONGESTION_CONTROL` function pointers or direct calls to public APIs
- **State Observation**: State must be observed via public getters or by observing side effects (return values, window growth)
- **Logging**: Many functions emit logs - tests should not depend on log content but logs are side effects
- **Mock Connection**: Tests use minimal valid `QUIC_CONNECTION` structure with only required fields initialized
