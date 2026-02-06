# Repository Contract Index: CUBIC Congestion Control

## Component Overview
**Component**: CUBIC Congestion Control  
**Source**: `/home/runner/work/msquic/msquic/src/core/cubic.c`  
**Header**: `/home/runner/work/msquic/msquic/src/core/cubic.h`  
**Purpose**: Implements the CUBIC congestion control algorithm (RFC 8312bis) for QUIC connections

## Public API Inventory

### 1. CubicCongestionControlInitialize
**Signature**:
```c
void CubicCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings
)
```

**Declaration**: `cubic.h` (line 119-124)

**Purpose**: Initializes the CUBIC congestion control state and function pointers

**Preconditions**:
- `Cc` must not be NULL (enforced by caller context)
- `Settings` must not be NULL (enforced by caller context)
- The Connection must be properly initialized with valid Paths[0]
- Connection->Paths[0].Mtu must be set

**Postconditions**:
- All CUBIC function pointers are set to their implementations
- CongestionWindow is initialized to DatagramPayloadLength * InitialWindowPackets
- SlowStartThreshold is set to UINT32_MAX
- BytesInFlightMax is set to CongestionWindow / 2
- All boolean flags (HasHadCongestionEvent, IsInRecovery, IsInPersistentCongestion, TimeOfLastAckValid) are FALSE
- HyStart state initialized to HYSTART_NOT_STARTED
- MinRttInCurrentRound and MinRttInLastRound initialized to UINT64_MAX
- CWndSlowStartGrowthDivisor set to 1

**Side Effects**:
- Logs outflow stats and CUBIC state via QuicConnLogOutFlowStats and QuicConnLogCubic

**Error Handling**: None (void return)

**Thread Safety**: Caller must ensure single-threaded access during initialization

**Resource/Ownership**: Does not allocate resources; initializes in-place state

---

### 2. Cube Root (CubeRoot)
**Signature**:
```c
uint32_t CubeRoot(uint32_t Radicand)
```

**Declaration**: Internal to cubic.c but algorithmically critical

**Purpose**: Computes the integer cube root of a 32-bit unsigned integer using a shifting nth root algorithm

**Preconditions**:
- None (accepts any uint32_t)

**Postconditions**:
- Returns y such that y^3 <= Radicand < (y+1)^3

**Side Effects**: None (pure function)

**Error Handling**: None (always succeeds)

**Thread Safety**: Thread-safe (pure function)

---

### 3. QuicConnLogCubic
**Signature**:
```c
void QuicConnLogCubic(_In_ const QUIC_CONNECTION* const Connection)
```

**Declaration**: Internal to cubic.c

**Purpose**: Logs CUBIC state for diagnostics/tracing

**Preconditions**:
- Connection must not be NULL

**Postconditions**:
- Emits trace event with SlowStartThreshold, KCubic, WindowMax, WindowLastMax

**Side Effects**: Emits ETW/LTTng trace event

**Error Handling**: None

**Thread Safety**: Safe if Connection is valid

---

### 4. CubicCongestionHyStartChangeState
**Signature**:
```c
void CubicCongestionHyStartChangeState(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ QUIC_CUBIC_HYSTART_STATE NewHyStartState
)
```

**Declaration**: Internal to cubic.c

**Purpose**: Transitions HyStart state machine

**Preconditions**:
- Cc must not be NULL
- NewHyStartState must be a valid enum value (HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE)

**Postconditions**:
- HyStartState updated to NewHyStartState
- If transitioning to HYSTART_DONE or HYSTART_NOT_STARTED, CWndSlowStartGrowthDivisor set to 1
- Trace event logged if state changes

**Side Effects**:
- May emit trace event
- Modifies Cubic state

**Error Handling**: Asserts on invalid NewHyStartState (CXPLAT_FRE_ASSERT)

**Thread Safety**: Caller must serialize

---

### 5. CubicCongestionHyStartResetPerRttRound
**Signature**:
```c
void CubicCongestionHyStartResetPerRttRound(
    _In_ QUIC_CONGESTION_CONTROL_CUBIC* Cubic
)
```

**Declaration**: Internal to cubic.c

**Purpose**: Resets HyStart per-RTT round state

**Preconditions**:
- Cubic must not be NULL

**Postconditions**:
- HyStartAckCount set to 0
- MinRttInLastRound set to MinRttInCurrentRound
- MinRttInCurrentRound set to UINT64_MAX

**Side Effects**: None (pure state mutation)

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 6. CubicCongestionControlCanSend (Public via function pointer)
**Signature**:
```c
BOOLEAN CubicCongestionControlCanSend(_In_ QUIC_CONGESTION_CONTROL* Cc)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Determines if more data can be sent based on congestion control

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Returns TRUE if BytesInFlight < CongestionWindow OR Exemptions > 0
- Returns FALSE otherwise

**Side Effects**: None (read-only)

**Error Handling**: None

**Thread Safety**: Safe for concurrent reads

---

### 7. CubicCongestionControlSetExemption (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlSetExemption(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint8_t NumPackets
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Sets the number of packets that can be sent ignoring congestion window

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Exemptions set to NumPackets

**Side Effects**: None

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 8. CubicCongestionControlReset (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlReset(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN FullReset
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Resets CUBIC state to initial conditions

**Preconditions**:
- Cc must not be NULL
- Connection->Paths[0] must be valid

**Postconditions**:
- SlowStartThreshold reset to UINT32_MAX
- HyStart state reset to HYSTART_NOT_STARTED
- IsInRecovery and HasHadCongestionEvent set to FALSE
- CongestionWindow reset to DatagramPayloadLength * InitialWindowPackets
- If FullReset is TRUE, BytesInFlight set to 0

**Side Effects**:
- Logs outflow stats and CUBIC state

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 9. CubicCongestionControlGetSendAllowance (Public via function pointer)
**Signature**:
```c
uint32_t CubicCongestionControlGetSendAllowance(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint64_t TimeSinceLastSend,
    _In_ BOOLEAN TimeSinceLastSendValid
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Calculates how many bytes can be sent, accounting for pacing

**Preconditions**:
- Cc must not be NULL
- Connection->Paths[0] must be valid

**Postconditions**:
- Returns 0 if BytesInFlight >= CongestionWindow (CC blocked)
- Returns (CongestionWindow - BytesInFlight) if pacing disabled
- Returns paced allowance if pacing enabled and RTT sample available
- Updates LastSendAllowance for pacing

**Side Effects**:
- May update LastSendAllowance

**Error Handling**: Handles overflow in pacing calculation

**Thread Safety**: Caller must serialize

---

### 10. CubicCongestionControlUpdateBlockedState (Internal helper)
**Signature**:
```c
BOOLEAN CubicCongestionControlUpdateBlockedState(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN PreviousCanSendState
)
```

**Declaration**: Internal to cubic.c

**Purpose**: Updates connection's blocked state and returns if we became unblocked

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Returns TRUE if transitioned from blocked to unblocked
- Returns FALSE otherwise
- Adds/removes QUIC_FLOW_BLOCKED_CONGESTION_CONTROL reason

**Side Effects**:
- Logs outflow stats
- May reset Connection->Send.LastFlushTime

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 11. CubicCongestionControlOnCongestionEvent (Internal helper)
**Signature**:
```c
void CubicCongestionControlOnCongestionEvent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN IsPersistentCongestion,
    _In_ BOOLEAN Ecn
)
```

**Declaration**: Internal to cubic.c

**Purpose**: Handles a congestion event (loss or ECN signal)

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- IsInRecovery set to TRUE
- HasHadCongestionEvent set to TRUE
- If persistent congestion:
  - IsInPersistentCongestion set to TRUE
  - CongestionWindow reduced to QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS
  - WindowMax, WindowPrior, WindowLastMax, SlowStartThreshold all reduced
  - Route.State set to RouteSuspected
- Otherwise:
  - CUBIC window reduction applied (multiply by BETA = 0.7)
  - KCubic computed from WindowMax
  - Fast convergence logic applied
- HyStart transitioned to HYSTART_DONE
- Previous state saved if not ECN-triggered (for spurious detection)

**Side Effects**:
- Emits trace events
- Increments Connection->Stats.Send.CongestionCount
- Increments Connection->Stats.Send.PersistentCongestionCount if persistent

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 12. CubicCongestionControlOnDataSent (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlOnDataSent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Notifies CC that data has been sent

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- BytesInFlight increased by NumRetransmittableBytes
- BytesInFlightMax updated if BytesInFlight exceeds it
- LastSendAllowance reduced by NumRetransmittableBytes (clamped to 0)
- Exemptions decremented if > 0

**Side Effects**:
- May call QuicSendBufferConnectionAdjust
- May update blocked state and log

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 13. CubicCongestionControlOnDataInvalidated (Public via function pointer)
**Signature**:
```c
BOOLEAN CubicCongestionControlOnDataInvalidated(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Notifies CC that previously sent data is no longer in flight

**Preconditions**:
- Cc must not be NULL
- BytesInFlight >= NumRetransmittableBytes (asserted)

**Postconditions**:
- BytesInFlight decreased by NumRetransmittableBytes
- Returns TRUE if transitioned from blocked to unblocked

**Side Effects**:
- May update blocked state and log

**Error Handling**: Asserts BytesInFlight >= NumRetransmittableBytes

**Thread Safety**: Caller must serialize

---

### 14. CubicCongestionControlGetNetworkStatistics (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlGetNetworkStatistics(
    _In_ const QUIC_CONNECTION* const Connection,
    _In_ const QUIC_CONGESTION_CONTROL* const Cc,
    _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Populates network statistics structure

**Preconditions**:
- Connection, Cc, NetworkStatistics must not be NULL

**Postconditions**:
- NetworkStatistics populated with BytesInFlight, PostedBytes, IdealBytes, SmoothedRTT, CongestionWindow, Bandwidth

**Side Effects**: None (read-only)

**Error Handling**: None

**Thread Safety**: Safe for concurrent reads

---

### 15. CubicCongestionControlOnDataAcknowledged (Public via function pointer)
**Signature**:
```c
BOOLEAN CubicCongestionControlOnDataAcknowledged(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ACK_EVENT* AckEvent
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Handles ACK event, updates congestion window

**Preconditions**:
- Cc and AckEvent must not be NULL
- BytesInFlight >= AckEvent->NumRetransmittableBytes (asserted)

**Postconditions**:
- BytesInFlight decreased by NumRetransmittableBytes
- If in recovery and LargestAck > RecoverySentPacketNumber: exits recovery
- If not in recovery:
  - HyStart++ logic applied (RTT sampling, state transitions)
  - If in slow start: CongestionWindow grows exponentially
  - If in congestion avoidance: CUBIC window calculation and AIMD window calculation performed
  - CongestionWindow updated based on max(CUBIC, AIMD)
  - Window growth capped at 2 * BytesInFlightMax
- TimeOfLastAck and TimeOfLastAckValid updated
- Returns TRUE if transitioned from blocked to unblocked

**Side Effects**:
- May emit trace events
- May emit network statistics event if enabled
- Updates blocked state

**Error Handling**: Asserts BytesInFlight >= NumRetransmittableBytes

**Thread Safety**: Caller must serialize

---

### 16. CubicCongestionControlOnDataLost (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlOnDataLost(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_LOSS_EVENT* LossEvent
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Handles loss event

**Preconditions**:
- Cc and LossEvent must not be NULL
- BytesInFlight >= LossEvent->NumRetransmittableBytes (asserted)

**Postconditions**:
- If loss is after most recent congestion event (or no previous event):
  - RecoverySentPacketNumber updated to LargestSentPacketNumber
  - Triggers CubicCongestionControlOnCongestionEvent
  - HyStart transitioned to HYSTART_DONE
- BytesInFlight decreased by NumRetransmittableBytes
- Blocked state updated

**Side Effects**:
- May trigger congestion event with all its side effects
- Logs CUBIC state

**Error Handling**: Asserts BytesInFlight >= NumRetransmittableBytes

**Thread Safety**: Caller must serialize

---

### 17. CubicCongestionControlOnEcn (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlOnEcn(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ECN_EVENT* EcnEvent
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Handles ECN (Explicit Congestion Notification) signal

**Preconditions**:
- Cc and EcnEvent must not be NULL

**Postconditions**:
- If ECN after most recent congestion event (or no previous event):
  - RecoverySentPacketNumber updated to LargestSentPacketNumber
  - Connection->Stats.Send.EcnCongestionCount incremented
  - Triggers CubicCongestionControlOnCongestionEvent with Ecn=TRUE
  - HyStart transitioned to HYSTART_DONE
- Blocked state updated

**Side Effects**:
- May trigger congestion event
- Logs CUBIC state

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 18. CubicCongestionControlOnSpuriousCongestionEvent (Public via function pointer)
**Signature**:
```c
BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(
    _In_ QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Reverts CUBIC state after detecting spurious congestion event

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- If not in recovery, returns FALSE (no-op)
- Otherwise:
  - Reverts to previous state saved during congestion event
  - WindowPrior, WindowMax, WindowLastMax, KCubic, SlowStartThreshold, CongestionWindow, AimdWindow restored
  - IsInRecovery and HasHadCongestionEvent set to FALSE
  - Returns TRUE if transitioned from blocked to unblocked

**Side Effects**:
- Emits spurious congestion trace event
- Logs CUBIC state
- Updates blocked state

**Error Handling**: None

**Thread Safety**: Caller must serialize

---

### 19. CubicCongestionControlLogOutFlowStatus (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlLogOutFlowStatus(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Logs outflow status for diagnostics

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Emits trace event with BytesSent, BytesInFlight, CongestionWindow, connection FC, ISB, PostedBytes, SmoothedRtt, OneWayDelay

**Side Effects**: Emits trace event

**Error Handling**: None

**Thread Safety**: Safe for concurrent reads

---

### 20. CubicCongestionControlGetBytesInFlightMax (Public via function pointer)
**Signature**:
```c
uint32_t CubicCongestionControlGetBytesInFlightMax(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Returns maximum bytes in flight observed

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Returns BytesInFlightMax

**Side Effects**: None (read-only)

**Error Handling**: None

**Thread Safety**: Safe for concurrent reads

---

### 21. CubicCongestionControlGetExemptions (Public via function pointer)
**Signature**:
```c
uint8_t CubicCongestionControlGetExemptions(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Returns current exemption count

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Returns Exemptions

**Side Effects**: None (read-only)

**Error Handling**: None

**Thread Safety**: Safe for concurrent reads

---

### 22. CubicCongestionControlGetCongestionWindow (Public via function pointer)
**Signature**:
```c
uint32_t CubicCongestionControlGetCongestionWindow(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Returns current congestion window

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Returns CongestionWindow

**Side Effects**: None (read-only)

**Error Handling**: None

**Thread Safety**: Safe for concurrent reads

---

### 23. CubicCongestionControlIsAppLimited (Public via function pointer)
**Signature**:
```c
BOOLEAN CubicCongestionControlIsAppLimited(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Checks if congestion control is app-limited

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- Always returns FALSE (CUBIC doesn't track app-limited state)

**Side Effects**: None

**Error Handling**: None

**Thread Safety**: Safe

---

### 24. CubicCongestionControlSetAppLimited (Public via function pointer)
**Signature**:
```c
void CubicCongestionControlSetAppLimited(
    _In_ struct QUIC_CONGESTION_CONTROL* Cc
)
```

**Declaration**: Registered in QuicCongestionControlCubic function pointer table

**Purpose**: Sets app-limited state

**Preconditions**:
- Cc must not be NULL

**Postconditions**:
- No-op (CUBIC doesn't track app-limited state)

**Side Effects**: None

**Error Handling**: None

**Thread Safety**: Safe

---

## Type/Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC

**Object Invariants (must always hold)**:
1. `CongestionWindow > 0` (always at least DatagramPayloadLength)
2. `BytesInFlight <= CongestionWindow + (max packet size) + Exemptions * (max packet size)`
3. `BytesInFlightMax >= BytesInFlight / 2` (typically)
4. `SlowStartThreshold >= 0` or `UINT32_MAX` (never negative)
5. `Exemptions <= 255` (uint8_t)
6. If `HasHadCongestionEvent == TRUE`, then `RecoverySentPacketNumber` is valid
7. If `IsInRecovery == TRUE`, then `HasHadCongestionEvent == TRUE`
8. If `IsInPersistentCongestion == TRUE`, then `IsInRecovery == TRUE`
9. If `TimeOfLastAckValid == TRUE`, then `TimeOfLastAck` contains valid timestamp
10. `HyStartState` is one of {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}
11. `CWndSlowStartGrowthDivisor >= 1` (at least 1)
12. `MinRttInLastRound <= UINT64_MAX` and `MinRttInCurrentRound <= UINT64_MAX`

### State Machine: CUBIC Congestion Control

**States**:
1. **Initialization**: Fresh state, no ACKs received
2. **Slow Start**: CongestionWindow < SlowStartThreshold, exponential growth
3. **Congestion Avoidance**: CongestionWindow >= SlowStartThreshold, CUBIC/AIMD growth
4. **Recovery**: IsInRecovery == TRUE, waiting for ACK > RecoverySentPacketNumber
5. **Persistent Congestion**: IsInPersistentCongestion == TRUE, severe window reduction

**HyStart++ Sub-State Machine**:
1. **HYSTART_NOT_STARTED**: Not yet exited slow start via HyStart
2. **HYSTART_ACTIVE**: Conservative slow start phase (reduced growth)
3. **HYSTART_DONE**: Exited slow start (in congestion avoidance or after congestion)

**State Transitions**:

1. **Initialization → Slow Start**:
   - Trigger: First ACK received
   - Condition: CongestionWindow < SlowStartThreshold
   - API: CubicCongestionControlOnDataAcknowledged

2. **Slow Start → Congestion Avoidance**:
   - Trigger: CongestionWindow >= SlowStartThreshold
   - API: CubicCongestionControlOnDataAcknowledged
   - Invariant: TimeOfCongAvoidStart set, AimdWindow initialized

3. **Slow Start → Recovery** (or **Congestion Avoidance → Recovery**):
   - Trigger: Loss detected or ECN signal
   - API: CubicCongestionControlOnDataLost or CubicCongestionControlOnEcn
   - Invariant: IsInRecovery = TRUE, RecoverySentPacketNumber set, window reduced

4. **Recovery → Slow Start** or **Recovery → Congestion Avoidance**:
   - Trigger: ACK received with LargestAck > RecoverySentPacketNumber
   - API: CubicCongestionControlOnDataAcknowledged
   - Invariant: IsInRecovery = FALSE, IsInPersistentCongestion = FALSE

5. **Any State → Persistent Congestion**:
   - Trigger: Loss event with PersistentCongestion == TRUE
   - API: CubicCongestionControlOnDataLost
   - Invariant: IsInPersistentCongestion = TRUE, window set to minimum

6. **Recovery → Slow Start/Congestion Avoidance** (spurious):
   - Trigger: Spurious congestion detected
   - API: CubicCongestionControlOnSpuriousCongestionEvent
   - Invariant: Previous state restored

7. **HYSTART_NOT_STARTED → HYSTART_ACTIVE**:
   - Trigger: RTT increase detected (MinRttInCurrentRound >= MinRttInLastRound + Eta)
   - API: CubicCongestionControlOnDataAcknowledged
   - Invariant: CWndSlowStartGrowthDivisor increased, ConservativeSlowStartRounds set

8. **HYSTART_ACTIVE → HYSTART_NOT_STARTED**:
   - Trigger: RTT decrease (MinRttInCurrentRound < CssBaselineMinRtt)
   - API: CubicCongestionControlOnDataAcknowledged
   - Invariant: CWndSlowStartGrowthDivisor reset to 1

9. **HYSTART_ACTIVE → HYSTART_DONE**:
   - Trigger: ConservativeSlowStartRounds reaches 0
   - API: CubicCongestionControlOnDataAcknowledged
   - Invariant: SlowStartThreshold = CongestionWindow, enters congestion avoidance

10. **Any HyStart State → HYSTART_DONE**:
    - Trigger: Congestion event
    - API: CubicCongestionControlOnCongestionEvent
    - Invariant: HyStart disabled after congestion

**State Invariants**:

- **In Slow Start**:
  - `CongestionWindow < SlowStartThreshold`
  - Window grows by `BytesAcked / CWndSlowStartGrowthDivisor` per ACK

- **In Congestion Avoidance**:
  - `CongestionWindow >= SlowStartThreshold`
  - Window grows based on CUBIC function or AIMD
  - `TimeOfCongAvoidStart` is valid
  - `AimdWindow` tracks AIMD window

- **In Recovery**:
  - `IsInRecovery == TRUE`
  - `RecoverySentPacketNumber` is valid
  - Window growth paused until exiting recovery

- **In Persistent Congestion**:
  - `IsInPersistentCongestion == TRUE`
  - `CongestionWindow == DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS`
  - Severe reduction applied

### Ownership/Lifetime Invariants

- CUBIC state is embedded in `QUIC_CONGESTION_CONTROL` structure
- No dynamic allocations (all state is inline)
- Lifetime tied to connection lifetime
- No explicit cleanup required (no resources to free)

---

## Environment Invariants

1. **Connection Context**: All CUBIC functions assume they are called with a valid `QUIC_CONNECTION` context accessible via `QuicCongestionControlGetConnection(Cc)`

2. **Path Validity**: Connection->Paths[0] must be valid and active for all operations

3. **Single-Threaded Access**: CUBIC assumes single-threaded access per connection (enforced by QUIC connection lock)

4. **Initialization Order**: `CubicCongestionControlInitialize` must be called before any other CUBIC function

5. **MTU Availability**: Connection->Paths[0].Mtu must be set before initialization

6. **Logging**: CUBIC emits ETW/LTTng events; logging subsystem must be initialized

---

## Dependency Map

### Call Relationships

```
CubicCongestionControlInitialize
├─> QuicCongestionControlGetConnection (to get Connection)
├─> QuicPathGetDatagramPayloadSize (to get MTU)
├─> CubicCongestionHyStartResetPerRttRound
├─> QuicConnLogOutFlowStats (logging)
└─> QuicConnLogCubic (logging)

CubicCongestionControlOnDataAcknowledged (main ACK handler)
├─> QuicCongestionControlGetConnection
├─> CubicCongestionControlCanSend
├─> CubicCongestionHyStartChangeState
│   └─> QuicCongestionControlGetConnection
│       └─> QuicTraceEvent
├─> CubicCongestionHyStartResetPerRttRound
├─> QuicPathGetDatagramPayloadSize
├─> CubicCongestionControlUpdateBlockedState
│   ├─> QuicCongestionControlGetConnection
│   ├─> CubicCongestionControlCanSend
│   ├─> QuicConnAddOutFlowBlockedReason
│   ├─> QuicConnRemoveOutFlowBlockedReason
│   ├─> CxPlatTimeUs64
│   └─> QuicConnLogOutFlowStats
├─> QuicTraceEvent (multiple)
└─> QuicConnIndicateEvent (if NetStatsEventEnabled)

CubicCongestionControlOnDataLost
├─> CubicCongestionControlCanSend
├─> CubicCongestionControlOnCongestionEvent
│   ├─> QuicCongestionControlGetConnection
│   ├─> QuicPathGetDatagramPayloadSize
│   ├─> CubeRoot (for KCubic calculation)
│   ├─> CubicCongestionHyStartChangeState
│   └─> QuicTraceEvent
├─> CubicCongestionHyStartChangeState
├─> CubicCongestionControlUpdateBlockedState
└─> QuicConnLogCubic

CubicCongestionControlOnEcn
├─> CubicCongestionControlCanSend
├─> QuicCongestionControlGetConnection
├─> CubicCongestionControlOnCongestionEvent
├─> CubicCongestionHyStartChangeState
├─> CubicCongestionControlUpdateBlockedState
└─> QuicConnLogCubic

CubicCongestionControlReset
├─> QuicCongestionControlGetConnection
├─> QuicPathGetDatagramPayloadSize
├─> CubicCongestionHyStartResetPerRttRound
├─> CubicCongestionHyStartChangeState
├─> QuicConnLogOutFlowStats
└─> QuicConnLogCubic

CubicCongestionControlGetSendAllowance
├─> QuicCongestionControlGetConnection
└─> (pacing calculations)

CubicCongestionControlOnDataSent
├─> QuicCongestionControlCanSend
├─> QuicCongestionControlGetConnection
├─> QuicSendBufferConnectionAdjust
└─> CubicCongestionControlUpdateBlockedState
```

### Key External Dependencies

1. **Platform Abstractions**:
   - `CxPlatTimeUs64()`: Get current time in microseconds
   - `CxPlatTimeDiff64()`: Calculate time difference

2. **Connection Functions**:
   - `QuicCongestionControlGetConnection()`: Extract connection from CC structure
   - `QuicPathGetDatagramPayloadSize()`: Get MTU
   - `QuicConnLogOutFlowStats()`: Log outflow statistics
   - `QuicConnAddOutFlowBlockedReason()`: Add blocked reason
   - `QuicConnRemoveOutFlowBlockedReason()`: Remove blocked reason
   - `QuicSendBufferConnectionAdjust()`: Adjust send buffer
   - `QuicConnIndicateEvent()`: Indicate event to app

3. **Tracing**:
   - `QuicTraceEvent()`: Emit ETW/LTTng events
   - `QuicTraceLogConnVerbose()`: Emit verbose connection log

### Callback/Indirect Calls

- No callbacks or function pointers called by CUBIC
- CUBIC is called via function pointers registered in `QuicCongestionControlCubic` structure

---

## Constants and Magic Numbers

- `TEN_TIMES_BETA_CUBIC = 7`: Beta * 10 = 0.7 * 10 (multiplicative decrease factor)
- `TEN_TIMES_C_CUBIC = 4`: C * 10 = 0.4 * 10 (CUBIC scaling constant)
- `QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS`: Minimum window during persistent congestion
- `QUIC_MIN_PACING_RTT`: Minimum RTT for pacing
- `QUIC_HYSTART_DEFAULT_N_SAMPLING`: Number of ACKs for HyStart sampling
- `QUIC_HYSTART_DEFAULT_MIN_ETA`: Minimum Eta for HyStart
- `QUIC_HYSTART_DEFAULT_MAX_ETA`: Maximum Eta for HyStart
- `QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR`: Growth divisor during conservative slow start
- `QUIC_CONSERVATIVE_SLOW_START_DEFAULT_ROUNDS`: Number of conservative slow start rounds

---

## Testing Strategy

### Public API Coverage

All public APIs (function pointers registered in `QuicCongestionControlCubic`) must be tested:
1. ✅ CubicCongestionControlInitialize
2. ✅ CubicCongestionControlCanSend
3. ✅ CubicCongestionControlSetExemption
4. ✅ CubicCongestionControlReset
5. ✅ CubicCongestionControlGetSendAllowance
6. ✅ CubicCongestionControlOnDataSent
7. ✅ CubicCongestionControlOnDataInvalidated
8. ✅ CubicCongestionControlOnDataAcknowledged
9. ✅ CubicCongestionControlOnDataLost
10. ✅ CubicCongestionControlOnEcn
11. ✅ CubicCongestionControlOnSpuriousCongestionEvent
12. ✅ CubicCongestionControlLogOutFlowStatus
13. ✅ CubicCongestionControlGetExemptions
14. ✅ CubicCongestionControlGetBytesInFlightMax
15. ✅ CubicCongestionControlIsAppLimited
16. ✅ CubicCongestionControlSetAppLimited
17. ✅ CubicCongestionControlGetCongestionWindow
18. ✅ CubicCongestionControlGetNetworkStatistics

### Scenario Coverage

1. **Initialization Scenarios**:
   - Normal initialization
   - Boundary MTU values
   - Re-initialization

2. **Slow Start Scenarios**:
   - Window growth
   - Transition to congestion avoidance
   - With/without HyStart

3. **Congestion Avoidance Scenarios**:
   - CUBIC window calculation
   - AIMD window calculation
   - Reno-friendly region
   - Concave/convex regions

4. **Congestion Event Scenarios**:
   - Loss-triggered congestion
   - ECN-triggered congestion
   - Persistent congestion
   - Spurious congestion (revert)

5. **HyStart++ Scenarios**:
   - NOT_STARTED → ACTIVE transition
   - ACTIVE → DONE transition
   - ACTIVE → NOT_STARTED (spurious exit)

6. **Recovery Scenarios**:
   - Enter recovery
   - Exit recovery
   - Multiple losses during recovery

7. **Pacing Scenarios**:
   - Pacing enabled
   - Pacing disabled
   - Overflow handling

8. **Edge Cases**:
   - Exemptions
   - Zero bytes acked
   - Large time gaps
   - Overflow protection

---

## Notes

- CUBIC is RFC 8312bis compliant
- HyStart++ is an extension for safer slow start exit
- All arithmetic uses integer math with appropriate scaling
- Overflow protection is critical in pacing and window calculations
- State machine transitions are well-defined and testable
