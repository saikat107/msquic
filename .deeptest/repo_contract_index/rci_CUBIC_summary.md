# Repository Contract Index (RCI) - CUBIC Congestion Control Component

## Component Overview
**Component**: QUIC_CONGESTION_CONTROL_CUBIC  
**Source**: `src/core/cubic.c`  
**Header**: `src/core/cubic.h`  
**Purpose**: Implementation of CUBIC congestion control algorithm (RFC 8312bis) for QUIC protocol

## A) Public API Inventory

### 1. CubicCongestionControlInitialize
**Signature**: 
```c
void CubicCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings
)
```
**Purpose**: Initializes the CUBIC congestion control state and assigns all function pointers
**Preconditions**: 
- `Cc` must not be NULL
- `Settings` must not be NULL
- `Cc` must be part of a valid QUIC_CONNECTION structure (for CXPLAT_CONTAINING_RECORD arithmetic)
**Postconditions**:
- All function pointers in `Cc` are set
- `Cubic->CongestionWindow` initialized based on Settings->InitialWindowPackets and MTU
- `Cubic->SlowStartThreshold` set to UINT32_MAX
- All boolean flags set to FALSE/0
- HyStart state initialized to HYSTART_NOT_STARTED
**Side Effects**: May trigger logging via QuicConnLogCubic
**Thread Safety**: Not thread-safe, caller must ensure exclusive access

### 2. CubicCongestionControlCanSend
**Signature**:
```c
BOOLEAN CubicCongestionControlCanSend(_In_ QUIC_CONGESTION_CONTROL* Cc)
```
**Purpose**: Determines if more data can be sent based on congestion window
**Preconditions**: `Cc` initialized via CubicCongestionControlInitialize
**Postconditions**: Returns TRUE if `BytesInFlight < CongestionWindow OR Exemptions > 0`
**Thread Safety**: Read-only, safe for concurrent reads if no writes

### 3. CubicCongestionControlSetExemption
**Signature**:
```c
void CubicCongestionControlSetExemption(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint8_t NumPackets
)
```
**Purpose**: Sets exemption count for packets that bypass congestion control
**Preconditions**: `Cc` initialized
**Postconditions**: `Cubic->Exemptions = NumPackets`
**Use Case**: Probe packets, PTO packets

### 4. CubicCongestionControlReset
**Signature**:
```c
void CubicCongestionControlReset(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN FullReset
)
```
**Purpose**: Resets congestion control state
**Preconditions**: `Cc` initialized
**Postconditions**:
- If `FullReset=TRUE`: `BytesInFlight` zeroed
- If `FullReset=FALSE`: `BytesInFlight` preserved
- Recovery flags cleared
- Congestion window reset to initial value
**Use Cases**: Connection migration, idle timeout recovery

### 5. CubicCongestionControlGetSendAllowance
**Signature**:
```c
uint32_t CubicCongestionControlGetSendAllowance(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint64_t TimeSinceLastSend,
    _In_ BOOLEAN TimeSinceLastSendValid
)
```
**Purpose**: Calculates how many bytes can be sent, considering pacing
**Preconditions**: `Cc` initialized
**Postconditions**: Returns available send allowance in bytes (0 if congestion blocked)
**Algorithm**: 
- If congestion blocked: return 0
- If pacing disabled: return full window
- If pacing enabled: calculate pacing rate = (CongestionWindow * TimeSinceLastSend) / RTT
**Thread Safety**: Read-heavy, may read connection state

### 6. CubicCongestionControlOnDataSent
**Signature**:
```c
void CubicCongestionControlOnDataSent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
)
```
**Purpose**: Updates BytesInFlight when data is transmitted
**Preconditions**: `Cc` initialized, `NumRetransmittableBytes` is valid packet size
**Postconditions**: 
- `BytesInFlight += NumRetransmittableBytes`
- `BytesInFlightMax` updated if new max reached
- `Exemptions` decremented if > 0
**Invariant Maintained**: BytesInFlight reflects actual bytes outstanding in network

### 7. CubicCongestionControlOnDataInvalidated
**Signature**:
```c
void CubicCongestionControlOnDataInvalidated(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
)
```
**Purpose**: Decrements BytesInFlight when sent data is invalidated (e.g., key update discards)
**Preconditions**: `Cc` initialized, `NumRetransmittableBytes <= BytesInFlight`
**Postconditions**: `BytesInFlight -= NumRetransmittableBytes`
**Contract**: Caller must ensure bytes were previously counted in BytesInFlight

### 8. CubicCongestionControlOnDataAcknowledged
**Signature**:
```c
void CubicCongestionControlOnDataAcknowledged(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ACK_EVENT* AckEvent
)
```
**Purpose**: Handles ACK events, updates congestion window growth
**Preconditions**: 
- `Cc` initialized
- `AckEvent` must be valid with `NumRetransmittableBytes <= BytesInFlight`
**Postconditions**:
- `BytesInFlight` decremented by acked bytes
- Recovery state updated if LargestAck > RecoverySentPacketNumber
- In Slow Start: window grows by acked bytes (additive)
- In Congestion Avoidance: CUBIC/AIMD window growth
**Side Effects**: May exit recovery, update HyStart state, trigger logging
**Algorithm**: Implements CUBIC/HyStart++ window growth based on current phase

### 9. CubicCongestionControlOnDataLost
**Signature**:
```c
void CubicCongestionControlOnDataLost(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_LOSS_EVENT* LossEvent
)
```
**Purpose**: Handles packet loss detection, triggers congestion event
**Preconditions**: 
- `Cc` initialized
- `LossEvent->NumRetransmittableBytes <= BytesInFlight`
**Postconditions**:
- `BytesInFlight` decremented by lost bytes
- If loss is after last recovery: trigger OnCongestionEvent
- Window reduced per CUBIC algorithm (BETA factor)
**Side Effects**: Enters recovery, updates stats, may trigger HyStart exit

### 10. CubicCongestionControlOnEcn
**Signature**:
```c
void CubicCongestionControlOnEcn(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ECN_EVENT* EcnEvent
)
```
**Purpose**: Handles ECN (Explicit Congestion Notification) signals
**Preconditions**: `Cc` initialized, `EcnEvent` valid
**Postconditions**: If ECN after last recovery: trigger OnCongestionEvent with Ecn=TRUE
**Difference from Loss**: ECN congestion event does NOT save previous state (no spurious undo)

### 11. CubicCongestionControlOnSpuriousCongestionEvent
**Signature**:
```c
BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc)
```
**Purpose**: Attempts to undo a false congestion event
**Preconditions**: `Cc` initialized
**Postconditions**: 
- If undo possible: restores previous CUBIC state, returns TRUE
- If not possible (ECN event or no saved state): returns FALSE
**Use Case**: Spurious retransmission timeout detected

### 12. CubicCongestionControlOnCongestionEvent (Internal, called by OnDataLost/OnEcn)
**Purpose**: Core congestion event handler
**Algorithm**:
- If PersistentCongestion: reset window to minimum (2 packets)
- Else: apply CUBIC multiplicative decrease (BETA = 0.7)
- Calculate K (time to reach W_max) using cube root
- Update W_max, W_last_max for fast convergence

### 13. CubicCongestionControlGetNetworkStatistics
**Signature**:
```c
void CubicCongestionControlGetNetworkStatistics(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _Out_ QUIC_NETWORK_STATISTICS* Stats
)
```
**Purpose**: Retrieves current network statistics
**Preconditions**: `Cc` and `Stats` not NULL
**Postconditions**: `Stats` populated with BytesInFlight, PostedBytes, CongestionWindow

### 14-17. Getter Functions
- **GetExemptions**: Returns `Cubic->Exemptions`
- **GetBytesInFlightMax**: Returns `Cubic->BytesInFlightMax`
- **GetCongestionWindow**: Returns `Cubic->CongestionWindow`
- **IsAppLimited**: Returns TRUE if `BytesInFlight * 2 < CongestionWindow`

### 18. CubicCongestionControlSetAppLimited
**Purpose**: Marks connection as application-limited
**Preconditions**: `Cc` initialized
**Postconditions**: Updates BytesInFlightMax if currently app-limited

### 19. CubicCongestionControlLogOutFlowStatus
**Purpose**: Logs outflow statistics for diagnostics
**Side Effects**: ETW/LTTng logging only

### 20. CubicCongestionControlUpdateBlockedState (Internal)
**Purpose**: Updates blocked state and triggers send callbacks if unblocked

---

## B) Type/Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC Structure Invariants

**Always Valid (Post-Initialization)**:
1. `CongestionWindow > 0`
2. `BytesInFlightMax >= 0`
3. `SlowStartThreshold >= MinWindow OR == UINT32_MAX`
4. `InitialWindowPackets >= 1`
5. `Exemptions >= 0 AND <= 255`

**State Machine Invariants**:
```
State: Recovery (IsInRecovery=TRUE)
- RecoverySentPacketNumber is valid
- HasHadCongestionEvent=TRUE
- Window growth suspended until ACK > RecoverySentPacketNumber

State: Slow Start (CongestionWindow < SlowStartThreshold)
- Window grows additively per ACK
- HyStart may be active to detect congestion

State: Congestion Avoidance (CongestionWindow >= SlowStartThreshold)
- Window grows per CUBIC formula
- AimdWindow tracks AIMD-based growth
- Chooses max(CUBIC, AIMD) for friendliness
```

**HyStart State Machine**:
```
HYSTART_NOT_STARTED → HYSTART_ACTIVE → HYSTART_DONE
     ↑___________________|
        (RTT decrease detected)

HYSTART_NOT_STARTED:
- Sampling RTT, checking for delay increase
- CWndSlowStartGrowthDivisor = 1 (normal growth)

HYSTART_ACTIVE:
- Conservative Slow Start active
- CWndSlowStartGrowthDivisor = 2 (slower growth)
- Runs for ConservativeSlowStartRounds RTT rounds

HYSTART_DONE:
- Exited slow start, in congestion avoidance
- CWndSlowStartGrowthDivisor = 1 (unused)
```

**Resource Lifetime**:
- CUBIC state is embedded in QUIC_CONNECTION
- No dynamic allocations
- Lifetime tied to connection lifetime

---

## C) Environment Invariants

1. **Initialization Order**: 
   - QUIC_CONNECTION must be initialized before calling CubicCongestionControlInitialize
   - Connection->Paths[0] must be active and have valid MTU

2. **Threading**: 
   - All CUBIC functions assume single-threaded access per connection
   - Caller (connection layer) must serialize access

3. **Callback Context**:
   - All public functions use CXPLAT_CONTAINING_RECORD to get QUIC_CONNECTION from Cc pointer
   - Cc must always be the CongestionControl field of a valid QUIC_CONNECTION

4. **Logging**:
   - Functions may call logging macros (QuicTraceEvent)
   - Logging requires CLOG initialization

---

## D) Dependency Map and State Transitions

### Call Graph (Public → Internal)
```
CubicCongestionControlOnDataLost
  → CubicCongestionControlOnCongestionEvent
    → CubeRoot (math helper)
    → CubicCongestionHyStartChangeState
    → QuicConnLogCubic (logging)
  → CubicCongestionControlUpdateBlockedState

CubicCongestionControlOnDataAcknowledged
  → CubicCongestionHyStartResetPerRttRound
  → CubicCongestionHyStartChangeState
  → CubicCongestionControlUpdateBlockedState
```

### State Transition Diagram

```
┌─────────────┐
│   Initial   │ (After CubicCongestionControlInitialize)
│             │ CongestionWindow = InitialWindowPackets * MTU
│ IsInRecovery=FALSE │ SlowStartThreshold = UINT32_MAX
└──────┬──────┘
       │ OnDataAcknowledged (ACKs accumulate)
       v
┌──────────────────┐
│   Slow Start     │ (CongestionWindow < SlowStartThreshold)
│                  │ Window doubles per RTT
│ HyStart may      │ CongestionWindow += BytesAcked
│ detect exit      │
└─────┬────────────┘
      │
      ├─ HyStart delay increase detected
      │  → HYSTART_ACTIVE (Conservative Slow Start)
      │  → CWndSlowStartGrowthDivisor = 2
      │
      ├─ OnDataLost / OnEcn
      │  → Congestion Event
      v
┌──────────────────┐
│   Recovery       │ (IsInRecovery=TRUE)
│                  │ RecoverySentPacketNumber set
│ Window frozen    │ Window = Window * BETA (0.7)
│ until ACK >      │ SlowStartThreshold = new window
│ recovery PN      │
└─────┬────────────┘
      │ OnDataAcknowledged with LargestAck > RecoverySentPacketNumber
      v
┌─────────────────────┐
│ Congestion Avoidance│ (CongestionWindow >= SlowStartThreshold)
│                     │ CUBIC formula: W(t) = C*(t-K)^3 + W_max
│ Window grows slowly │ Choose max(CUBIC, AIMD) for TCP friendliness
└─────┬───────────────┘
      │
      ├─ OnDataLost / OnEcn
      │  → Congestion Event (back to Recovery)
      │
      └─ Reset / IdleTimeout
         → back to Initial

┌──────────────────────┐
│ Persistent Congestion│ (Special case in Recovery)
│                      │ Window reset to 2*MTU
│ Severe loss          │ All CUBIC state reset
└──────────────────────┘
```

### Key Internal Dependencies

1. **CubeRoot(uint32_t)**: Mathematical helper for CUBIC K calculation
   - Pure function, no side effects
   - Implements shifting nth-root algorithm

2. **QuicCongestionControlGetConnection(Cc)**: Pointer arithmetic helper
   - Returns QUIC_CONNECTION* from QUIC_CONGESTION_CONTROL*
   - Uses CXPLAT_CONTAINING_RECORD macro

3. **CubicCongestionHyStartChangeState**: Internal state transition
   - Updates HyStart state machine
   - Adjusts CWndSlowStartGrowthDivisor

---

## Test Coverage Strategy

### Scenarios Requiring Tests

**Initialization & Reset**:
- ✅ Basic initialization with default settings
- ✅ Boundary values (min/max MTU, window sizes)
- ✅ Re-initialization
- ✅ Reset with FullReset=TRUE/FALSE

**Steady State Operations**:
- ✅ CanSend with various BytesInFlight levels
- ✅ SetExemption
- ✅ GetSendAllowance without pacing
- ✅ GetSendAllowance with pacing
- ✅ OnDataSent incrementing BytesInFlight
- ✅ OnDataInvalidated decrementing BytesInFlight

**Window Growth (Slow Start)**:
- ✅ OnDataAcknowledged in slow start (window doubling)
- 🔄 Slow start with small incremental ACKs
- 🔄 Slow start until reaching SlowStartThreshold

**Window Growth (Congestion Avoidance)**:
- 🔄 OnDataAcknowledged in congestion avoidance (CUBIC growth)
- 🔄 AIMD vs CUBIC comparison (TCP friendliness)
- 🔄 Long-duration congestion avoidance (K calculation accuracy)

**Congestion Events**:
- ✅ OnDataLost triggering congestion event
- 🔄 OnDataLost with persistent congestion
- ✅ OnEcn triggering congestion event
- 🔄 Congestion event during recovery (ignored)
- 🔄 Fast convergence (W_last_max > W_max)

**Recovery**:
- 🔄 Recovery entry and exit
- 🔄 Multiple losses during recovery
- 🔄 ACK advancing past RecoverySentPacketNumber
- ✅ OnSpuriousCongestionEvent (undo)

**HyStart++**:
- ✅ HyStart state transitions (NOT_STARTED → ACTIVE → DONE)
- 🔄 RTT sampling during slow start
- 🔄 Delay increase detection
- 🔄 Conservative Slow Start rounds
- 🔄 RTT decrease causing HyStart resume

**Getters & Misc**:
- ✅ All getter functions
- ✅ GetNetworkStatistics
- 🔄 IsAppLimited / SetAppLimited

**Legend**: ✅ Covered by existing tests | 🔄 Needs new test

---

## Contracts & Testing Constraints

### STRICT Contract Requirements
1. **Public Surface Only**: Tests MUST NOT call internal functions:
   - ❌ CubeRoot (internal math helper)
   - ❌ CubicCongestionHyStartChangeState (internal state transition)
   - ❌ CubicCongestionHyStartResetPerRttRound (internal reset)
   - ❌ CubicCongestionControlUpdateBlockedState (internal callback)
   - ❌ QuicConnLogCubic (internal logging)

2. **Contract-Safe Only**: Tests MUST NOT violate preconditions:
   - ❌ NULL pointers where disallowed
   - ❌ Invalid BytesInFlight values (> actual bytes sent)
   - ❌ Invalid AckEvent/LossEvent structures

3. **Public APIs to Test** (17 function pointers + 1 initializer):
   - CubicCongestionControlInitialize (direct call)
   - All 17 function pointers assigned during initialization

### Coverage Goals
- **Line Coverage**: 100% of cubic.c
- **Branch Coverage**: All conditional paths
- **Scenario Coverage**: All public API combinations in realistic usage sequences
