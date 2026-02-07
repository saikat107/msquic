# Repository Contract Index for QUIC_CUBIC Component

## Component Overview
- **Component**: QUIC_CUBIC (CUBIC Congestion Control Algorithm per RFC8312bis)
- **Source**: `src/core/cubic.c`
- **Header**: `src/core/cubic.h`, `src/core/congestion_control.h`
- **Purpose**: Implements CUBIC congestion control algorithm for QUIC connections

## A) Public API Inventory

The CUBIC component implements the standard congestion control interface defined in `QUIC_CONGESTION_CONTROL`. All functions are accessed via function pointers set during initialization.

### 1. `CubicCongestionControlInitialize`
- **Signature**: `void CubicCongestionControlInitialize(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_SETTINGS_INTERNAL* Settings)`
- **Declaration**: `src/core/cubic.h:121`
- **Purpose**: Initializes CUBIC congestion control state and sets up function pointers
- **Preconditions**:
  - `Cc` must not be NULL
  - `Settings` must not be NULL
  - `Cc` must be part of a valid `QUIC_CONNECTION` structure (pointer arithmetic via `CXPLAT_CONTAINING_RECORD`)
  - Connection must have valid `Paths[0]` with MTU set
- **Postconditions**:
  - All 17 function pointers in `Cc` are set to CUBIC implementations
  - `Cubic.CongestionWindow` initialized based on `InitialWindowPackets` and MTU
  - `Cubic.SlowStartThreshold` set to `UINT32_MAX`
  - All state flags (`HasHadCongestionEvent`, `IsInRecovery`, etc.) set to FALSE
  - `BytesInFlight` and `Exemptions` zeroed
  - HyStart state initialized to `HYSTART_NOT_STARTED`
- **Side Effects**: None (pure initialization)
- **Thread Safety**: Not thread-safe; caller must ensure exclusive access
- **Error Contract**: No error returns; crashes on NULL inputs (precondition violation)

### 2. `CubicCongestionControlCanSend` (via function pointer)
- **Signature**: `BOOLEAN (*QuicCongestionControlCanSend)(_In_ QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Determines if more data can be sent based on congestion window
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: Returns TRUE if `BytesInFlight < CongestionWindow` OR `Exemptions > 0`
- **Side Effects**: None (read-only query)
- **Thread Safety**: Read-only but caller must ensure no concurrent writes
- **Error Contract**: No errors; always returns valid BOOLEAN

### 3. `CubicCongestionControlSetExemption` (via function pointer)
- **Signature**: `void (*QuicCongestionControlSetExemption)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint8_t NumPackets)`
- **Purpose**: Sets exemption count for probe packets that bypass congestion window
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: `Cubic.Exemptions` = `NumPackets`
- **Side Effects**: Modifies state; may unblock sending
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 4. `CubicCongestionControlReset` (via function pointer)
- **Signature**: `void (*QuicCongestionControlReset)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN FullReset)`
- **Purpose**: Resets congestion control state (e.g., after idle timeout or connection migration)
- **Preconditions**: `Cc` must be initialized
- **Postconditions**:
  - `CongestionWindow` reset to initial window
  - `SlowStartThreshold` reset to `UINT32_MAX`
  - Recovery state cleared (`IsInRecovery` = FALSE)
  - If `FullReset` is TRUE, `BytesInFlight` also zeroed
  - HyStart state reset to `HYSTART_NOT_STARTED`
- **Side Effects**: Logs state changes
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 5. `CubicCongestionControlGetSendAllowance` (via function pointer)
- **Signature**: `uint32_t (*QuicCongestionControlGetSendAllowance)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid)`
- **Purpose**: Returns number of bytes that can be sent immediately, considering pacing
- **Preconditions**: `Cc` must be initialized
- **Postconditions**:
  - Returns 0 if `BytesInFlight >= CongestionWindow`
  - Returns `CongestionWindow - BytesInFlight` if pacing disabled
  - Returns paced allowance if pacing enabled (proportional to time since last send)
- **Side Effects**: Updates `LastSendAllowance` if pacing active
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors; always returns valid uint32_t

### 6. `CubicCongestionControlOnDataSent` (via function pointer)
- **Signature**: `void (*QuicCongestionControlOnDataSent)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`
- **Purpose**: Called when retransmittable data is sent; increments bytes in flight
- **Preconditions**: `Cc` must be initialized
- **Postconditions**:
  - `BytesInFlight` += `NumRetransmittableBytes`
  - `BytesInFlightMax` updated to max seen value
  - If `Exemptions > 0`, decremented by 1
- **Side Effects**: Modifies state, may block future sends
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 7. `CubicCongestionControlOnDataInvalidated` (via function pointer)
- **Signature**: `BOOLEAN (*QuicCongestionControlOnDataInvalidated)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`
- **Purpose**: Removes data from in-flight tracking without treating as loss or ACK
- **Preconditions**: `Cc` must be initialized, `NumRetransmittableBytes <= BytesInFlight`
- **Postconditions**:
  - `BytesInFlight` -= `NumRetransmittableBytes`
  - Returns TRUE if state changed from blocked to unblocked
- **Side Effects**: May unblock sending, logs state changes
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 8. `CubicCongestionControlOnDataAcknowledged` (via function pointer)
- **Signature**: `BOOLEAN (*QuicCongestionControlOnDataAcknowledged)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ACK_EVENT* AckEvent)`
- **Purpose**: Processes ACKed data; grows congestion window, exits recovery
- **Preconditions**:
  - `Cc` and `AckEvent` must be initialized
  - `AckEvent->AckedPackets` must point to valid metadata chain
- **Postconditions**:
  - `BytesInFlight` decremented by ACKed bytes
  - `CongestionWindow` may grow (slow start or congestion avoidance)
  - May exit recovery if ACKed packet > `RecoverySentPacketNumber`
  - Returns TRUE if state changed from blocked to unblocked
- **Side Effects**: Updates window, HyStart state, logs events
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors; gracefully handles edge cases

### 9. `CubicCongestionControlOnDataLost` (via function pointer)
- **Signature**: `void (*QuicCongestionControlOnDataLost)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_LOSS_EVENT* LossEvent)`
- **Purpose**: Handles loss detection; reduces congestion window, enters recovery
- **Preconditions**: `Cc` and `LossEvent` must be initialized
- **Postconditions**:
  - `BytesInFlight` decremented by lost bytes
  - If not in recovery: `CongestionWindow` reduced by BETA factor (70%)
  - `IsInRecovery` set to TRUE
  - `RecoverySentPacketNumber` updated to largest sent packet
  - If persistent congestion: window reset to minimum
- **Side Effects**: Triggers congestion event, logs state
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 10. `CubicCongestionControlOnEcn` (via function pointer)
- **Signature**: `void (*QuicCongestionControlOnEcn)(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent)`
- **Purpose**: Handles ECN congestion signals (ECT marks)
- **Preconditions**: `Cc` and `EcnEvent` must be initialized
- **Postconditions**: Triggers congestion response similar to loss but without decrementing BytesInFlight
- **Side Effects**: Enters recovery, reduces window
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 11. `CubicCongestionControlOnSpuriousCongestionEvent` (via function pointer)
- **Signature**: `BOOLEAN (*QuicCongestionControlOnSpuriousCongestionEvent)(_In_ QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Reverts congestion response if loss was spurious (false alarm)
- **Preconditions**: `Cc` must be initialized, must have had prior congestion event
- **Postconditions**:
  - Restores previous window state (`PrevCongestionWindow`, `PrevWindowMax`, etc.)
  - Returns TRUE if state changed
- **Side Effects**: Restores state, logs event
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 12. `CubicCongestionControlGetExemptions` (via function pointer)
- **Signature**: `uint8_t (*QuicCongestionControlGetExemptions)(_In_ const QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Returns current exemption count
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: Returns `Cubic.Exemptions` value
- **Side Effects**: None (read-only)
- **Thread Safety**: Read-only
- **Error Contract**: No errors

### 13. `CubicCongestionControlGetBytesInFlightMax` (via function pointer)
- **Signature**: `uint32_t (*QuicCongestionControlGetBytesInFlightMax)(_In_ const QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Returns maximum bytes in flight seen
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: Returns `Cubic.BytesInFlightMax`
- **Side Effects**: None (read-only)
- **Thread Safety**: Read-only
- **Error Contract**: No errors

### 14. `CubicCongestionControlGetCongestionWindow` (via function pointer)
- **Signature**: `uint32_t (*QuicCongestionControlGetCongestionWindow)(_In_ const QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Returns current congestion window size
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: Returns `Cubic.CongestionWindow`
- **Side Effects**: None (read-only)
- **Thread Safety**: Read-only
- **Error Contract**: No errors

### 15. `CubicCongestionControlIsAppLimited` (via function pointer)
- **Signature**: `BOOLEAN (*QuicCongestionControlIsAppLimited)(_In_ const QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Checks if connection is application-limited (not sending despite available window)
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: Returns TRUE if `BytesInFlight <= BytesInFlightMax / 2`
- **Side Effects**: None (read-only)
- **Thread Safety**: Read-only
- **Error Contract**: No errors

### 16. `CubicCongestionControlSetAppLimited` (via function pointer)
- **Signature**: `void (*QuicCongestionControlSetAppLimited)(_In_ QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Marks connection as application-limited; prevents spurious window growth
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: `BytesInFlightMax` updated if currently app-limited
- **Side Effects**: Updates tracking state
- **Thread Safety**: Not thread-safe
- **Error Contract**: No errors

### 17. `CubicCongestionControlGetNetworkStatistics` (via function pointer)
- **Signature**: `void (*QuicCongestionControlGetNetworkStatistics)(_In_ const QUIC_CONNECTION* Connection, _In_ const QUIC_CONGESTION_CONTROL* Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics)`
- **Purpose**: Populates network statistics structure for telemetry
- **Preconditions**: All parameters must be valid, `NetworkStatistics` points to writable memory
- **Postconditions**: `NetworkStatistics` filled with current congestion control state
- **Side Effects**: None (read-only)
- **Thread Safety**: Read-only
- **Error Contract**: No errors

### 18. `CubicCongestionControlLogOutFlowStatus` (via function pointer)
- **Signature**: `void (*QuicCongestionControlLogOutFlowStatus)(_In_ const QUIC_CONGESTION_CONTROL* Cc)`
- **Purpose**: Logs current congestion control state for diagnostics
- **Preconditions**: `Cc` must be initialized
- **Postconditions**: State logged to telemetry
- **Side Effects**: Emits log events
- **Thread Safety**: Read-only
- **Error Contract**: No errors

## B) Type/Object Invariants

### `QUIC_CONGESTION_CONTROL_CUBIC` Structure

**Object Invariants** (must hold at all API boundaries):
1. `CongestionWindow >= (MTU * InitialWindowPackets)` (minimum window size)
2. `BytesInFlight <= CongestionWindow + MTU + Exemptions * MTU` (may exceed by packet + exemptions)
3. `BytesInFlightMax >= BytesInFlight` (max is cumulative)
4. If `IsInRecovery == TRUE`, then `HasHadCongestionEvent == TRUE`
5. `SlowStartThreshold <= UINT32_MAX`
6. `HyStartState` in {`HYSTART_NOT_STARTED`, `HYSTART_ACTIVE`, `HYSTART_DONE`}
7. If `HyStartState == HYSTART_NOT_STARTED`, then `HyStartAckCount == 0`
8. `Exemptions <= 255` (uint8_t)
9. All window values (`CongestionWindow`, `WindowMax`, etc.) are in bytes

**State Machine**:

```
┌─────────────────┐
│   Initialized   │
│ (No Congestion) │
└────────┬────────┘
         │
         │ OnDataSent()
         ▼
┌─────────────────┐    Loss/ECN Event    ┌──────────────┐
│  Normal Send    │───────────────────────>│  In Recovery │
│  (Slow Start/   │                        │  (Window     │
│   Cong Avoid)   │<───────────────────────│   Reduced)   │
└─────────────────┘    ACK > Recovery     └──────────────┘
         │             Packet Number
         │
         │ Idle Timeout / Reset()
         ▼
┌─────────────────┐
│     Reset       │
│ (Window Reset)  │
└─────────────────┘
```

**State Invariants**:

1. **Initialized**: 
   - `CongestionWindow == MTU * InitialWindowPackets`
   - `SlowStartThreshold == UINT32_MAX`
   - `BytesInFlight == 0`, `HasHadCongestionEvent == FALSE`

2. **Normal Send (Slow Start)**:
   - `CongestionWindow < SlowStartThreshold`
   - Window grows by ACKed bytes (exponential)
   - `IsInRecovery == FALSE`

3. **Normal Send (Congestion Avoidance)**:
   - `CongestionWindow >= SlowStartThreshold`
   - Window grows via CUBIC function (sub-linear)
   - `IsInRecovery == FALSE`

4. **In Recovery**:
   - `IsInRecovery == TRUE`
   - `HasHadCongestionEvent == TRUE`
   - `RecoverySentPacketNumber` is valid (largest packet at congestion event)
   - Window does NOT grow on ACKs until exit recovery
   - Exit when ACK received for packet > `RecoverySentPacketNumber`

5. **Persistent Congestion**:
   - `IsInPersistentCongestion == TRUE`
   - `CongestionWindow` reset to minimum (MTU * InitialWindowPackets)
   - Triggered when loss spans significant time/RTT window

**HyStart State Machine** (when enabled):
```
┌──────────────────┐
│ HYSTART_NOT_STARTED │
└────────┬─────────┘
         │ In Slow Start && ACK received
         ▼
┌──────────────────┐    RTT increase detected OR
│  HYSTART_ACTIVE  │─────────────────────────────┐
└──────────────────┘    CSS rounds exceeded      │
                                                  ▼
                                         ┌──────────────┐
                                         │ HYSTART_DONE │
                                         └──────────────┘
```

## C) Environment Invariants

1. **Connection Context**: CUBIC must be embedded in a `QUIC_CONNECTION` structure where `CongestionControl` field is at a known offset. The code uses `QuicCongestionControlGetConnection(Cc)` which performs `CXPLAT_CONTAINING_RECORD` pointer arithmetic.

2. **Initialization Ordering**: 
   - `CubicCongestionControlInitialize` must be called before any other CUBIC function
   - Connection must have `Paths[0].Mtu` set before initialization

3. **No Concurrent Access**: CUBIC state is not thread-safe. Caller (connection layer) must ensure serialized access.

4. **Logging Infrastructure**: Uses `QuicTraceEvent` macros for telemetry; requires QUIC_CLOG to be properly configured.

5. **No Memory Allocation**: CUBIC does not allocate memory; all state is in-place within the connection structure.

6. **Integer Arithmetic**: All calculations use 32-bit or 64-bit integers; relies on overflow wrapping behavior in some cases (explicitly checked).

## D) Dependency Map

### Key Dependencies

**Platform Layer** (`src/platform/`):
- `CxPlatZeroMemory`: Memory initialization
- `CxPlatTimeUs64`: Timestamp retrieval for pacing
- Logging macros: `QuicTraceEvent`

**Connection Layer** (`src/core/connection.h`):
- `QuicCongestionControlGetConnection`: Retrieves connection from `QUIC_CONGESTION_CONTROL*`
- `QuicPathGetDatagramPayloadSize`: Gets MTU from path
- `QuicConnLogOutFlowStats`: Logs connection flow statistics
- `QuicConnAddOutFlowBlockedReason` / `QuicConnRemoveOutFlowBlockedReason`: Updates blocked state

**Internal CUBIC Functions** (not public, used internally):
- `CubeRoot`: Computes cubic root for window calculation
- `QuicConnLogCubic`: Logs CUBIC-specific state
- `CubicCongestionHyStartChangeState`: Manages HyStart state transitions
- `CubicCongestionHyStartResetPerRttRound`: Resets HyStart per-RTT counters
- `CubicCongestionControlUpdateBlockedState`: Updates congestion blocking status
- `CubicCongestionControlOnCongestionEvent`: Internal handler for congestion events

### Internal Call Relationships

1. **OnDataAcknowledged** calls:
   - `CubicCongestionControlUpdateBlockedState`
   - `CubicCongestionHyStartResetPerRttRound` (if HyStart active)
   - `CubicCongestionHyStartChangeState` (on HyStart state change)
   - `CubeRoot` (for CUBIC function calculation)

2. **OnDataLost** calls:
   - `CubicCongestionControlOnCongestionEvent`
   - `CubicCongestionControlUpdateBlockedState`

3. **OnEcn** calls:
   - `CubicCongestionControlOnCongestionEvent`

4. **Reset** calls:
   - `CubicCongestionHyStartResetPerRttRound`
   - `CubicCongestionHyStartChangeState`
   - `QuicConnLogCubic`

### Callback Registration Points
- None. CUBIC does not register callbacks; it provides callbacks to the connection layer via function pointers.

## Test Strategy Notes

**Coverage Considerations**:
1. All 17 public API functions must have scenario-based tests
2. State machine transitions (Normal → Recovery → Normal, HyStart states)
3. Boundary conditions: window at minimum, window at maximum, zero bytes in flight
4. Pacing enabled vs disabled paths
5. HyStart enabled vs disabled paths
6. Spurious congestion event reversal
7. Persistent congestion detection
8. ECN vs loss-based congestion signals
9. Integer overflow protection (e.g., EstimatedWnd calculation)
10. App-limited detection and handling

**Anti-Patterns to Avoid**:
- Accessing internal state directly (must use public APIs)
- Calling internal static functions (not part of public API)
- Testing with invalid pointers (violates preconditions)
- Testing without proper connection context (pointer arithmetic will fail)
