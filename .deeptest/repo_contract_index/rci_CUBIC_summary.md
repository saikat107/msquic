# Repository Contract Index (RCI) for CUBIC Congestion Control

## Component Overview
- **Component**: CUBIC Congestion Control
- **Source File**: src/core/cubic.c
- **Header File**: src/core/cubic.h
- **Test Harness**: src/core/unittest/CubicTest.cpp
- **Description**: Implementation of CUBIC congestion control algorithm (RFC8312bis) for QUIC protocol

## A) Public API Inventory

### 1. CubicCongestionControlInitialize
**Signature:**
```c
void CubicCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings
);
```

**Declaration**: src/core/cubic.h:121-124  
**Definition**: src/core/cubic.c:915-940

**What it does**: Initializes a CUBIC congestion control instance by copying function pointers from the static vtable and setting up initial congestion window, slow start threshold, and HyStart state.

**Preconditions**:
- `Cc` must not be NULL (dereferenced immediately)
- `Cc` must be part of a valid QUIC_CONNECTION structure (CxPlatContainingRecord used)
- `Settings` must not be NULL and must contain valid:
  - `InitialWindowPackets` > 0
  - `SendIdleTimeoutMs` >= 0
- Connection's `Paths[0]` must be initialized with valid MTU

**Postconditions**:
- All 17 function pointers in `Cc` are set to CUBIC implementations
- `Cc->Name` == "Cubic"
- `Cubic->CongestionWindow` == DatagramPayloadLength * InitialWindowPackets
- `Cubic->BytesInFlightMax` == CongestionWindow / 2
- `Cubic->SlowStartThreshold` == UINT32_MAX
- `Cubic->HyStartState` == HYSTART_NOT_STARTED
- `Cubic->MinRttInCurrentRound` == UINT64_MAX
- `Cubic->CWndSlowStartGrowthDivisor` == 1
- All other fields zero-initialized (via vtable copy)

**Side effects**:
- Logs initialization via `QuicConnLogOutFlowStats` and `QuicConnLogCubic`
- Calls `CubicCongestionHyStartResetPerRttRound` which sets HyStartAckCount=0

**Error contract**: None (void return, no error paths)

**Thread safety**: Can be called at IRQL <= DISPATCH_LEVEL. Not thread-safe if same `Cc` accessed concurrently.

**Resource/ownership**: Does not allocate memory. Caller owns `Cc` structure.

---

### Function Pointer APIs (accessed via Cc->QuicCongestionControl*)

The following APIs are accessed through the function pointer table after initialization:

### 2. CubicCongestionControlCanSend
**Signature:**
```c
BOOLEAN CubicCongestionControlCanSend(_In_ QUIC_CONGESTION_CONTROL* Cc);
```

**What it does**: Returns TRUE if more data can be sent based on congestion window or exemptions.

**Preconditions**: `Cc` must be initialized via CubicCongestionControlInitialize

**Postconditions**: 
- Returns TRUE if `BytesInFlight < CongestionWindow || Exemptions > 0`
- Returns FALSE otherwise

**Side effects**: None (pure function)

**Thread safety**: IRQL <= DISPATCH_LEVEL, read-only operation

---

### 3. CubicCongestionControlSetExemption
**Signature:**
```c
void CubicCongestionControlSetExemption(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint8_t NumPackets);
```

**What it does**: Sets the number of packets that can be sent ignoring congestion window (for loss recovery probes).

**Preconditions**: `Cc` initialized, NumPackets can be any uint8_t value

**Postconditions**: `Cubic->Exemptions` == NumPackets

**Side effects**: Modifies Exemptions field only

---

### 4. CubicCongestionControlReset
**Signature:**
```c
void CubicCongestionControlReset(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN FullReset);
```

**What it does**: Resets congestion control state to initial conditions.

**Preconditions**:
- `Cc` initialized
- Connection fields (Paths[0], Send.NextPacketNumber) must be valid

**Postconditions**:
- `SlowStartThreshold` == UINT32_MAX
- `IsInRecovery` == FALSE
- `HasHadCongestionEvent` == FALSE
- `CongestionWindow` == DatagramPayloadLength * InitialWindowPackets
- `HyStartState` == HYSTART_NOT_STARTED
- If FullReset==TRUE: `BytesInFlight` == 0
- If FullReset==FALSE: BytesInFlight unchanged

**Side effects**: Logs state via QuicConnLogOutFlowStats and QuicConnLogCubic

---

### 5. CubicCongestionControlGetSendAllowance
**Signature:**
```c
uint32_t CubicCongestionControlGetSendAllowance(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint64_t TimeSinceLastSend,
    _In_ BOOLEAN TimeSinceLastSendValid
);
```

**What it does**: Calculates how many bytes can be sent now, implementing pacing if enabled.

**Preconditions**:
- `Cc` initialized
- Connection settings and Path[0] must be valid
- If pacing enabled: SmoothedRtt must be > 0

**Postconditions**:
- Returns 0 if BytesInFlight >= CongestionWindow
- Returns full allowance if pacing disabled or conditions not met
- Returns paced allowance based on EstimatedWindow * TimeSinceLastSend / SmoothedRtt if pacing active
- Updates `LastSendAllowance` to the returned value

**Side effects**: Modifies LastSendAllowance

**Thread safety**: IRQL <= DISPATCH_LEVEL

---

### 6. CubicCongestionControlOnDataSent
**Signature:**
```c
void CubicCongestionControlOnDataSent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
);
```

**What it does**: Updates state when data is sent.

**Preconditions**:
- `Cc` initialized
- NumRetransmittableBytes >= 0

**Postconditions**:
- `BytesInFlight` += NumRetransmittableBytes
- `BytesInFlightMax` = max(BytesInFlightMax, BytesInFlight)
- Exemptions decremented if > 0
- Connection might be marked as blocked if BytesInFlight >= CongestionWindow

**Side effects**: 
- May trigger flow blocked state via QuicConnAddOutFlowBlockedReason
- Logs via QuicConnLogOutFlowStats

---

### 7. CubicCongestionControlOnDataInvalidated
**Signature:**
```c
void CubicCongestionControlOnDataInvalidated(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
);
```

**What it does**: Decrements BytesInFlight when sent data is discarded (e.g., lost keys).

**Preconditions**:
- `Cc` initialized
- NumRetransmittableBytes <= BytesInFlight

**Postconditions**:
- `BytesInFlight` -= NumRetransmittableBytes
- May unblock sending if previously blocked

**Side effects**: May trigger unblock via QuicConnRemoveOutFlowBlockedReason

---

### 8. CubicCongestionControlOnDataAcknowledged
**Signature:**
```c
void CubicCongestionControlOnDataAcknowledged(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ACK_EVENT* AckEvent
);
```

**What it does**: Processes ACK events and grows congestion window according to CUBIC algorithm.

**Preconditions**:
- `Cc` initialized
- `AckEvent` must not be NULL
- `AckEvent->NumRetransmittableBytes` >= 0
- `AckEvent->LargestAck` >= last LargestAck seen

**Postconditions**:
- `BytesInFlight` decremented by NumRetransmittableBytes
- `CongestionWindow` may increase (slow start: exponential, congestion avoidance: CUBIC formula)
- `IsInRecovery` may transition to FALSE if LargestAck > RecoverySentPacketNumber
- `TimeOfLastAck` updated
- `TimeOfLastAckValid` set to TRUE
- May update HyStart state machine

**Side effects**: 
- Extensive logging
- May unblock sending
- Updates multiple internal state variables

**Thread safety**: IRQL <= DISPATCH_LEVEL

---

### 9. CubicCongestionControlOnDataLost
**Signature:**
```c
void CubicCongestionControlOnDataLost(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_LOSS_EVENT* LossEvent
);
```

**What it does**: Handles packet loss by reducing congestion window.

**Preconditions**:
- `Cc` initialized
- `LossEvent` not NULL
- `LossEvent->NumRetransmittableBytes` <= BytesInFlight

**Postconditions**:
- `BytesInFlight` -= NumRetransmittableBytes
- If new congestion event: calls OnCongestionEvent (reduces window)
- `RecoverySentPacketNumber` updated to LargestSentPacketNumber
- May set `IsInPersistentCongestion` if LossEvent->PersistentCongestion==TRUE

**Side effects**:
- May significantly reduce CongestionWindow
- Logs congestion event
- May unblock if window increased enough

---

### 10. CubicCongestionControlOnEcn
**Signature:**
```c
void CubicCongestionControlOnEcn(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ECN_EVENT* EcnEvent
);
```

**What it does**: Handles ECN (Explicit Congestion Notification) signals.

**Preconditions**:
- `Cc` initialized
- `EcnEvent` not NULL

**Postconditions**:
- If not in recovery: triggers congestion event with Ecn=TRUE
- `RecoverySentPacketNumber` updated

**Side effects**: Similar to loss event but triggered by ECN marks

---

### 11. CubicCongestionControlOnSpuriousCongestionEvent
**Signature:**
```c
BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(
    _In_ QUIC_CONGESTION_CONTROL* Cc
);
```

**What it does**: Reverts congestion window reduction when loss was spurious.

**Preconditions**:
- `Cc` initialized
- Must have had previous congestion event (PrevWindowMax, etc. saved)

**Postconditions**:
- Restores Prev* values to current values
- `IsInRecovery` = FALSE
- `HasHadCongestionEvent` = FALSE
- Returns TRUE if state changed from blocked to unblocked

**Side effects**: Logs spurious event, may unblock sending

---

### 12. CubicCongestionControlGetNetworkStatistics
**Signature:**
```c
void CubicCongestionControlGetNetworkStatistics(
    _In_ const QUIC_CONGESTION_CONTROL* Cc,
    _Out_ QUIC_NETWORK_STATISTICS* Stats
);
```

**What it does**: Populates statistics structure with current congestion control state.

**Preconditions**:
- `Cc` initialized
- `Stats` not NULL

**Postconditions**:
- Stats->CongestionWindow, BytesInFlight, BytesInFlightMax set from Cubic state

**Side effects**: None (read-only)

---

### 13-17. Getter and Utility Functions

**CubicCongestionControlGetExemptions**: Returns Cubic->Exemptions  
**CubicCongestionControlGetBytesInFlightMax**: Returns Cubic->BytesInFlightMax  
**CubicCongestionControlGetCongestionWindow**: Returns Cubic->CongestionWindow  
**CubicCongestionControlIsAppLimited**: Always returns FALSE (not implemented)  
**CubicCongestionControlSetAppLimited**: No-op (not implemented)  
**CubicCongestionControlLogOutFlowStatus**: Logs current flow statistics  

All are simple, read-only or minimal side-effect functions.

---

## B) Type/Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC Structure Invariants

**Always true for a valid, initialized CUBIC instance:**

1. `InitialWindowPackets` > 0
2. `CongestionWindow` >= DatagramPayloadLength (minimum 1 packet)
3. `BytesInFlight` <= BytesInFlightMax (except during transient updates)
4. `SlowStartThreshold` > 0 (typically UINT32_MAX or reduced value)
5. If `HasHadCongestionEvent` == TRUE, then `RecoverySentPacketNumber` is valid
6. `Exemptions` <= small value (typically 0-2, used for probe packets)
7. If `TimeOfLastAckValid` == TRUE, then `TimeOfLastAck` contains valid timestamp
8. `HyStartState` ∈ {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}
9. `MinRttInCurrentRound` either UINT64_MAX or valid RTT value
10. `CWndSlowStartGrowthDivisor` >= 1

**State Machine: Recovery State**
- States: {Not in Recovery, In Recovery}
- Transitions:
  - Not in Recovery → In Recovery: OnCongestionEvent or OnEcn called
  - In Recovery → Not in Recovery: ACK received with LargestAck > RecoverySentPacketNumber

**State Machine: HyStart State**
- States: {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}
- Transitions (only if Settings.HyStartEnabled):
  - NOT_STARTED → ACTIVE: During slow start, certain conditions met
  - ACTIVE → DONE: RTT increase detected, exit slow start early
  - Any → NOT_STARTED: Reset called
- State Invariants:
  - HYSTART_NOT_STARTED: CWndSlowStartGrowthDivisor == 1
  - HYSTART_ACTIVE: CWndSlowStartGrowthDivisor may be > 1
  - HYSTART_DONE: CWndSlowStartGrowthDivisor == 1

**State Machine: Slow Start vs Congestion Avoidance**
- States: {Slow Start, Congestion Avoidance}
- Determination: CongestionWindow < SlowStartThreshold → Slow Start, else Congestion Avoidance
- Window growth:
  - Slow Start: Exponential (doubles per RTT)
  - Congestion Avoidance: CUBIC formula (slower growth)

---

## C) Environment Invariants

1. **Connection Initialization**: QUIC_CONNECTION must be properly initialized before calling CubicCongestionControlInitialize
2. **Path Initialization**: Connection->Paths[0] must have valid MTU set
3. **No Concurrent Access**: CUBIC state must not be accessed by multiple threads without external synchronization
4. **IRQL Constraints**: Most functions require IRQL <= DISPATCH_LEVEL
5. **Logging Infrastructure**: QuicTrace* functions must be available (may be no-ops in non-logging builds)
6. **No Memory Allocation**: CUBIC does not allocate memory; all state in pre-allocated structure
7. **No Resource Leaks**: CUBIC has no cleanup function; no resources to leak

---

## D) Dependency Map and State Transition Diagram

### Internal Helper Functions (Not Public)
- `CubeRoot(uint32_t)`: Computes cube root for CUBIC formula
- `QuicConnLogCubic(Connection)`: Logging helper
- `CubicCongestionHyStartChangeState(Cc, NewState)`: HyStart state transitions
- `CubicCongestionHyStartResetPerRttRound(Cubic)`: Resets per-RTT HyStart counters
- `CubicCongestionControlUpdateBlockedState(Cc, PrevState)`: Updates connection flow control

### Key Dependencies from Connection
- `QuicCongestionControlGetConnection(Cc)`: Extracts containing QUIC_CONNECTION
- `QuicPathGetDatagramPayloadSize(&Path)`: Gets MTU
- Connection fields: Send.NextPacketNumber, Settings, Paths[0].SmoothedRtt, etc.

### Call Relationships

**Initialization Flow:**
```
CubicCongestionControlInitialize
  ├─> Copies QuicCongestionControlCubic vtable
  ├─> Sets CongestionWindow, SlowStartThreshold
  ├─> CubicCongestionHyStartResetPerRttRound
  ├─> QuicConnLogOutFlowStats
  └─> QuicConnLogCubic
```

**Data Sending Flow:**
```
CanSend → TRUE if allowed
GetSendAllowance → Calculate bytes to send
OnDataSent → Increment BytesInFlight
  └─> May block via UpdateBlockedState
```

**ACK Reception Flow:**
```
OnDataAcknowledged
  ├─> Decrement BytesInFlight
  ├─> Check recovery exit
  ├─> Grow CongestionWindow (slow start or CUBIC)
  ├─> Process HyStart (if enabled)
  └─> UpdateBlockedState (may unblock)
```

**Loss Detection Flow:**
```
OnDataLost
  ├─> Decrement BytesInFlight
  ├─> Check if new congestion event
  └─> OnCongestionEvent
        ├─> Reduce CongestionWindow (CUBIC: βW)
        ├─> Update WindowMax, KCubic
        ├─> Set IsInRecovery
        ├─> Save Prev* state (for spurious revert)
        └─> HyStart → DONE
```

**Spurious Loss Recovery:**
```
OnSpuriousCongestionEvent
  ├─> Restore Prev* values
  ├─> Clear IsInRecovery
  └─> UpdateBlockedState
```

### State Transition Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        CUBIC State Machine                       │
└─────────────────────────────────────────────────────────────────┘

Recovery State:
┌──────────────────┐                                ┌──────────────┐
│  Not In Recovery │─────OnCongestionEvent────────>│  In Recovery │
│                  │<────ACK(pkt > Recovery)───────│              │
└──────────────────┘                                └──────────────┘

HyStart State (if enabled):
┌─────────────────┐        Conditions      ┌─────────────┐
│ HYSTART_NOT_    │───────Met────────────>│ HYSTART_    │
│ STARTED         │                        │ ACTIVE      │
└─────────────────┘                        └─────────────┘
       ^                                          │
       │                                   RTT Increase
       │                                   Detected
       │                                          │
       │                                          v
       │                                   ┌─────────────┐
       └───────────Reset()─────────────────│ HYSTART_    │
                                           │ DONE        │
                                           └─────────────┘

Congestion Window Growth Mode (implicit state):
┌──────────────────┐                                ┌──────────────────┐
│  Slow Start      │───CWnd >= SSThresh──────────>│  Congestion      │
│  (Exponential)   │                               │  Avoidance       │
│                  │<──SSThresh = UINT32_MAX──────│  (CUBIC Formula) │
└──────────────────┘       Reset()                 └──────────────────┘
```

### Typical Usage Scenario

1. **Initialization**: `CubicCongestionControlInitialize(Cc, Settings)`
2. **Before Sending**: `CanSend()` → TRUE?, `GetSendAllowance()` → bytes
3. **On Send**: `OnDataSent(bytes)`
4. **On ACK**: `OnDataAcknowledged(AckEvent)` → grows window
5. **On Loss**: `OnDataLost(LossEvent)` → shrinks window, enters recovery
6. **Optional**: `OnSpuriousCongestionEvent()` → reverts shrink
7. **Query State**: `GetCongestionWindow()`, `GetNetworkStatistics()`, etc.

---

## E) Coverage Considerations

**Critical paths to cover:**
1. Initialization with various InitialWindowPackets values
2. Slow start growth (CongestionWindow < SlowStartThreshold)
3. Congestion avoidance growth (CUBIC formula with CubeRoot)
4. Congestion event handling (window reduction)
5. Recovery exit (ACK after recovery)
6. Spurious loss recovery
7. HyStart state transitions (if enabled)
8. Pacing calculations (when enabled)
9. Exemptions handling
10. Persistent congestion
11. ECN handling
12. Edge cases: overflow prevention, zero values, boundary conditions

**Unreachable code (contract-safe):**
- Internal static functions (CubeRoot, helpers) are indirectly covered via public API usage
- Error paths that violate preconditions (e.g., null Cc) are undefined behavior, not testable

---

## Summary

The CUBIC component implements RFC8312bis congestion control for QUIC. It has **one public API** (`CubicCongestionControlInitialize`) and **17 function pointer APIs** accessed via the vtable. Tests must be strictly interface-driven, using only the initialized function pointers, and must maintain all object and state machine invariants. The component has no error returns (all void or simple getters), so tests focus on state transitions, window growth/reduction correctness, and boundary conditions.
