# Repository Contract Index (RCI) - CUBIC Congestion Control

## Component Overview
**Component**: QUIC_CUBIC  
**Source**: src/core/cubic.c  
**Header**: src/core/cubic.h  
**Purpose**: Implements CUBIC congestion control algorithm (RFC 8312) for QUIC protocol

## Public API Inventory

### 1. CubicCongestionControlInitialize
**Signature**: `void CubicCongestionControlInitialize(QUIC_CONGESTION_CONTROL* Cc, const QUIC_SETTINGS_INTERNAL* Settings)`

**Purpose**: Initializes CUBIC congestion control state and installs function pointers

**Preconditions**:
- `Cc` must be non-NULL
- `Settings` must be non-NULL
- Connection must be accessible via `QuicCongestionControlGetConnection(Cc)`
- Connection->Paths[0] must be initialized with valid MTU

**Postconditions**:
- All function pointers in `Cc` are set to CUBIC implementations
- Cubic state initialized with:
  - CongestionWindow = MTU * Settings->InitialWindowPackets
  - SlowStartThreshold = UINT32_MAX
  - BytesInFlight = 0 (implicitly)
  - HyStartState = HYSTART_NOT_STARTED
  - All flags (HasHadCongestionEvent, IsInRecovery, etc.) = FALSE

**Side Effects**: Calls QuicConnLogOutFlowStats and QuicConnLogCubic for logging

**Thread Safety**: Must be called at IRQL <= DISPATCH_LEVEL

---

### 2. CubicCongestionControlCanSend
**Signature**: `BOOLEAN CubicCongestionControlCanSend(QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Determines if data can be sent based on congestion window

**Preconditions**:
- `Cc` must be initialized via CubicCongestionControlInitialize
- `Cc` must be non-NULL

**Postconditions**: None (read-only)

**Return**: 
- TRUE if `BytesInFlight < CongestionWindow` OR `Exemptions > 0`
- FALSE otherwise

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 3. CubicCongestionControlSetExemption
**Signature**: `void CubicCongestionControlSetExemption(QUIC_CONGESTION_CONTROL* Cc, uint8_t NumPackets)`

**Purpose**: Sets number of packets that can bypass congestion control (probe packets)

**Preconditions**:
- `Cc` must be initialized
- `NumPackets` can be 0-255

**Postconditions**:
- `Cc->Cubic.Exemptions = NumPackets`

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 4. CubicCongestionControlReset
**Signature**: `void CubicCongestionControlReset(QUIC_CONGESTION_CONTROL* Cc, BOOLEAN FullReset)`

**Purpose**: Resets CUBIC state to initial conditions

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- SlowStartThreshold = UINT32_MAX
- IsInRecovery = FALSE
- HasHadCongestionEvent = FALSE
- CongestionWindow reset to initial value
- If FullReset == TRUE: BytesInFlight = 0
- If FullReset == FALSE: BytesInFlight preserved
- HyStart state reset to NOT_STARTED

**Side Effects**: Calls QuicConnLogOutFlowStats and QuicConnLogCubic

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 5. CubicCongestionControlGetSendAllowance
**Signature**: `uint32_t CubicCongestionControlGetSendAllowance(QUIC_CONGESTION_CONTROL* Cc, uint64_t TimeSinceLastSend, BOOLEAN TimeSinceLastSendValid)`

**Purpose**: Calculates how many bytes can be sent, implementing pacing if enabled

**Preconditions**:
- `Cc` must be initialized
- `TimeSinceLastSend` in microseconds if valid

**Postconditions**:
- Updates `LastSendAllowance` if pacing is active

**Return**:
- 0 if `BytesInFlight >= CongestionWindow`
- `CongestionWindow - BytesInFlight` if pacing disabled or not ready
- Paced allowance based on `(EstimatedWindow * TimeSinceLastSend) / RTT` if pacing active

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 6. CubicCongestionControlOnDataSent
**Signature**: `void CubicCongestionControlOnDataSent(QUIC_CONGESTION_CONTROL* Cc, uint32_t NumRetransmittableBytes)`

**Purpose**: Updates state when data is sent

**Preconditions**:
- `Cc` must be initialized
- `NumRetransmittableBytes >= 0`

**Postconditions**:
- `BytesInFlight += NumRetransmittableBytes`
- `BytesInFlightMax` updated if new max reached
- `Exemptions` decremented if > 0
- `LastSendAllowance` adjusted

**Side Effects**: May call QuicSendBufferConnectionAdjust

**Thread Safety**: Can be called at IRQL <= PASSIVE_LEVEL

---

### 7. CubicCongestionControlOnDataInvalidated
**Signature**: `BOOLEAN CubicCongestionControlOnDataInvalidated(QUIC_CONGESTION_CONTROL* Cc, uint32_t NumRetransmittableBytes)`

**Purpose**: Updates state when sent data is invalidated (e.g., key update)

**Preconditions**:
- `Cc` must be initialized
- `BytesInFlight >= NumRetransmittableBytes` (asserted)

**Postconditions**:
- `BytesInFlight -= NumRetransmittableBytes`

**Return**: TRUE if became unblocked (can now send when previously couldn't)

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 8. CubicCongestionControlOnDataAcknowledged
**Signature**: `BOOLEAN CubicCongestionControlOnDataAcknowledged(QUIC_CONGESTION_CONTROL* Cc, const QUIC_ACK_EVENT* AckEvent)`

**Purpose**: Core CUBIC algorithm - grows congestion window based on ACKs

**Preconditions**:
- `Cc` must be initialized
- `AckEvent` must be non-NULL with valid fields:
  - TimeNow, LargestAck, LargestSentPacketNumber
  - NumRetransmittableBytes <= BytesInFlight (asserted)
  - SmoothedRtt, MinRtt (if MinRttValid)

**Postconditions**:
- `BytesInFlight -= AckEvent->NumRetransmittableBytes`
- `CongestionWindow` may grow based on:
  - Slow start (< SlowStartThreshold): exponential growth
  - Congestion avoidance (>= SlowStartThreshold): CUBIC formula
- HyStart state may transition
- TimeOfLastAck updated

**Return**: TRUE if became unblocked

**Side Effects**: 
- May call CubeRoot() for CUBIC calculation
- May emit network statistics events if enabled

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 9. CubicCongestionControlOnDataLost
**Signature**: `void CubicCongestionControlOnDataLost(QUIC_CONGESTION_CONTROL* Cc, const QUIC_LOSS_EVENT* LossEvent)`

**Purpose**: Handles packet loss by reducing congestion window

**Preconditions**:
- `Cc` must be initialized
- `LossEvent` must be non-NULL with:
  - LargestPacketNumberLost, LargestSentPacketNumber
  - NumRetransmittableBytes <= BytesInFlight (asserted)
  - PersistentCongestion flag

**Postconditions**:
- If loss is after recovery start (or first loss):
  - Triggers CubicCongestionControlOnCongestionEvent
  - `CongestionWindow` reduced by BETA (0.7)
  - `IsInRecovery = TRUE`
  - `RecoverySentPacketNumber` updated
- `BytesInFlight -= LossEvent->NumRetransmittableBytes`
- If PersistentCongestion: window reset to minimum

**Side Effects**: Calls QuicConnLogCubic

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 10. CubicCongestionControlOnEcn
**Signature**: `void CubicCongestionControlOnEcn(QUIC_CONGESTION_CONTROL* Cc, const QUIC_ECN_EVENT* EcnEvent)`

**Purpose**: Handles ECN congestion signals

**Preconditions**:
- `Cc` must be initialized
- `EcnEvent` must be non-NULL with:
  - LargestPacketNumberAcked, LargestSentPacketNumber

**Postconditions**:
- If ECN after recovery start (or first):
  - Triggers CubicCongestionControlOnCongestionEvent with Ecn=TRUE
  - `CongestionWindow` reduced
  - `IsInRecovery = TRUE`
  - Previous state NOT saved (ECN congestion is not spurious)

**Side Effects**: Calls QuicConnLogCubic

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 11. CubicCongestionControlOnSpuriousCongestionEvent
**Signature**: `BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Reverts congestion event if determined to be spurious

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- If `IsInRecovery == TRUE`:
  - Restores previous state (WindowPrior, WindowMax, CongestionWindow, etc.)
  - `IsInRecovery = FALSE`
  - `HasHadCongestionEvent = FALSE`
- If not in recovery: No-op

**Return**: TRUE if became unblocked

**Side Effects**: Calls QuicConnLogCubic

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 12. CubicCongestionControlGetNetworkStatistics
**Signature**: `void CubicCongestionControlGetNetworkStatistics(const QUIC_CONNECTION* Connection, const QUIC_CONGESTION_CONTROL* Cc, QUIC_NETWORK_STATISTICS* NetworkStatistics)`

**Purpose**: Retrieves current network statistics

**Preconditions**:
- `Connection`, `Cc`, `NetworkStatistics` must be non-NULL
- `Cc` must be initialized

**Postconditions**:
- `NetworkStatistics` populated with current values

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 13. CubicCongestionControlLogOutFlowStatus
**Signature**: `void CubicCongestionControlLogOutFlowStatus(const QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: Logs current outflow status for diagnostics

**Side Effects**: Emits trace event

**Thread Safety**: Can be called at any IRQL

---

### 14. CubicCongestionControlGetExemptions
**Signature**: `uint8_t CubicCongestionControlGetExemptions(const QUIC_CONGESTION_CONTROL* Cc)`

**Return**: Current exemption count

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 15. CubicCongestionControlGetBytesInFlightMax
**Signature**: `uint32_t CubicCongestionControlGetBytesInFlightMax(const QUIC_CONGESTION_CONTROL* Cc)`

**Return**: Maximum BytesInFlight observed

**Thread Safety**: Can be called at any IRQL

---

### 16. CubicCongestionControlGetCongestionWindow
**Signature**: `uint32_t CubicCongestionControlGetCongestionWindow(const QUIC_CONGESTION_CONTROL* Cc)`

**Return**: Current congestion window size

**Thread Safety**: Can be called at any IRQL

---

### 17. CubicCongestionControlIsAppLimited
**Signature**: `BOOLEAN CubicCongestionControlIsAppLimited(const QUIC_CONGESTION_CONTROL* Cc)`

**Return**: Always FALSE (CUBIC doesn't track app-limited state)

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

### 18. CubicCongestionControlSetAppLimited
**Signature**: `void CubicCongestionControlSetAppLimited(QUIC_CONGESTION_CONTROL* Cc)`

**Purpose**: No-op for CUBIC (doesn't track app-limited state)

**Thread Safety**: Can be called at IRQL <= DISPATCH_LEVEL

---

## Internal Helper Functions (NOT PUBLIC - Cannot be tested directly)

### CubeRoot
**Purpose**: Calculates cube root for CUBIC formula  
**Note**: Called internally by OnDataAcknowledged during congestion avoidance

### QuicConnLogCubic
**Purpose**: Logs CUBIC state

### CubicCongestionControlOnCongestionEvent  
**Purpose**: Handles congestion event (loss or ECN) - called by OnDataLost/OnEcn

### CubicCongestionControlUpdateBlockedState
**Purpose**: Updates blocked state and notifies connection

### CubicCongestionHyStartChangeState
**Purpose**: Manages HyStart state transitions

### CubicCongestionHyStartResetPerRttRound
**Purpose**: Resets HyStart per-RTT tracking

---

## Type/Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC Invariants

**Always True**:
1. `CongestionWindow >= MinWindow` (at least PERSISTENT_CONGESTION_WINDOW_PACKETS * MTU)
2. `BytesInFlight >= 0`
3. `BytesInFlightMax >= BytesInFlight` (when updated)
4. `SlowStartThreshold <= UINT32_MAX`
5. `Exemptions >= 0` and `<= 255`
6. If `IsInRecovery == TRUE`, then `HasHadCongestionEvent == TRUE`
7. If `HasHadCongestionEvent == TRUE`, then `RecoverySentPacketNumber` is valid
8. `HyStartState` in {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}
9. If `TimeOfLastAckValid == TRUE`, then `TimeOfLastAck` contains valid timestamp

**State Machine**:
```
Recovery State:
  Initial: !HasHadCongestionEvent && !IsInRecovery
  → Loss/ECN: HasHadCongestionEvent && IsInRecovery
  → ACK after recovery: HasHadCongestionEvent && !IsInRecovery
  → Spurious: !HasHadCongestionEvent && !IsInRecovery (reverted)

HyStart State:
  HYSTART_NOT_STARTED (Initial/Slow Start)
  → Delay increase detected: HYSTART_ACTIVE (Conservative Slow Start)
  → Rounds complete or loss: HYSTART_DONE (Congestion Avoidance)
  → RTT decrease: Back to HYSTART_NOT_STARTED
```

---

## Environment Invariants

1. **Initialization Required**: Must call `CubicCongestionControlInitialize` before using any other API
2. **Connection Context**: All APIs assume connection is accessible via `QuicCongestionControlGetConnection(Cc)`
3. **MTU Validity**: Connection->Paths[0] must have valid MTU for calculations
4. **Byte Accounting**: Caller must ensure `BytesInFlight` accounting is consistent:
   - OnDataSent increments
   - OnDataInvalidated/OnDataAcknowledged/OnDataLost decrement
   - Never goes negative (asserted)
5. **Packet Number Ordering**: Packet numbers used in events must be monotonically increasing
6. **No Concurrent Modification**: APIs are not thread-safe; caller must serialize access

---

## Dependency Map

### Public API Call Graph

```
CubicCongestionControlInitialize
  └→ QuicPathGetDatagramPayloadSize
  └→ QuicConnLogOutFlowStats
  └→ QuicConnLogCubic
  └→ CubicCongestionHyStartResetPerRttRound (internal)
  └→ CubicCongestionHyStartChangeState (internal)

CubicCongestionControlOnDataAcknowledged
  └→ QuicCongestionControlGetConnection
  └→ CubicCongestionControlCanSend
  └→ CubicCongestionHyStartChangeState (internal, may call)
  └→ CubicCongestionHyStartResetPerRttRound (internal, may call)
  └→ CubeRoot (internal, may call in congestion avoidance)
  └→ QuicPathGetDatagramPayloadSize
  └→ QuicConnIndicateEvent (if NetStatsEventEnabled)
  └→ CubicCongestionControlUpdateBlockedState (internal)

CubicCongestionControlOnDataLost
  └→ CubicCongestionControlCanSend
  └→ CubicCongestionControlOnCongestionEvent (internal)
      └→ QuicPathGetDatagramPayloadSize
      └→ CubeRoot
      └→ CubicCongestionHyStartChangeState
  └→ QuicConnLogCubic
  └→ CubicCongestionControlUpdateBlockedState (internal)

CubicCongestionControlOnEcn
  └→ Similar to OnDataLost

CubicCongestionControlReset
  └→ QuicPathGetDatagramPayloadSize
  └→ CubicCongestionHyStartResetPerRttRound (internal)
  └→ CubicCongestionHyStartChangeState (internal)
  └→ QuicConnLogOutFlowStats
  └→ QuicConnLogCubic
```

### Key Internal Dependencies
- **CubeRoot**: Cube root calculation using shifting algorithm
- **CubicCongestionControlOnCongestionEvent**: Central handler for loss/ECN events
- **CubicCongestionControlUpdateBlockedState**: Flow control notification
- **CubicCongestionHyStart***: HyStart++ algorithm functions

---

## Contract-Safe Testing Strategy

### Reachable via Public API
- All 18 public functions can be called directly
- State transitions achieved by sequencing public API calls
- CubeRoot tested indirectly via OnDataAcknowledged in congestion avoidance

### Example Scenario Sequences

**Slow Start → Congestion Avoidance**:
1. Initialize
2. OnDataSent (multiple times to fill window)
3. OnDataAcknowledged (grows window exponentially)
4. OnDataLost (triggers congestion event, sets SSThresh)
5. OnDataAcknowledged (grows via CUBIC formula, calls CubeRoot)

**Recovery**:
1. Initialize
2. OnDataLost (enters recovery)
3. OnDataAcknowledged with old packet (stays in recovery)
4. OnDataAcknowledged with new packet (exits recovery)

**Spurious Congestion**:
1. Initialize
2. OnDataLost (enters recovery, saves state)
3. OnSpuriousCongestionEvent (reverts to saved state)

**HyStart**:
1. Initialize with HyStartEnabled
2. OnDataAcknowledged repeatedly with increasing MinRtt
3. State transitions NOT_STARTED → ACTIVE → DONE

### Unreachable/Private
- Cannot directly call: CubeRoot, CubicCongestionControlOnCongestionEvent, etc.
- But all are reachable via public API sequences
