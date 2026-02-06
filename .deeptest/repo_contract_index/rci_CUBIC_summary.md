# Repository Contract Index (RCI) - QUIC_CONGESTION_CONTROL_CUBIC Component

## Component Overview
The CUBIC congestion control implementation for MsQuic, implementing RFC 8312bis. 
This is a high-performance TCP-friendly congestion control algorithm designed for 
high-bandwidth delay product networks.

## A. Public API Inventory

### 1. CubicCongestionControlInitialize
**Signature:** `void CubicCongestionControlInitialize(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_SETTINGS_INTERNAL* Settings)`

**Declaration:** cubic.h:121-124

**Summary:** Initializes the CUBIC congestion control state and installs function pointers.

**Preconditions:**
- Cc must not be NULL (contract expectation)
- Settings must not be NULL (contract expectation)
- Connection structure containing Cc must be properly initialized with valid Paths[0]
- Settings.InitialWindowPackets must be >= 1
- Settings.SendIdleTimeoutMs can be any uint32_t value

**Postconditions:**
- All 17 function pointers in Cc are set to CUBIC implementations
- Cubic.CongestionWindow = DatagramPayloadLength * Settings.InitialWindowPackets
- Cubic.BytesInFlightMax = Cubic.CongestionWindow / 2
- Cubic.SlowStartThreshold = UINT32_MAX
- Cubic.InitialWindowPackets = Settings.InitialWindowPackets
- Cubic.SendIdleTimeoutMs = Settings.SendIdleTimeoutMs
- Cubic.MinRttInCurrentRound = UINT64_MAX
- Cubic.HyStartState = HYSTART_NOT_STARTED
- Cubic.CWndSlowStartGrowthDivisor = 1
- Cubic.HyStartRoundEnd = Connection->Send.NextPacketNumber
- Boolean flags (HasHadCongestionEvent, IsInRecovery, IsInPersistentCongestion, TimeOfLastAckValid) are FALSE
- Cubic.BytesInFlight remains unchanged
- Cubic.HyStartAckCount = 0
- Cubic.MinRttInLastRound = UINT64_MAX

**Side Effects:**
- Calls QuicConnLogOutFlowStats and QuicConnLogCubic for telemetry

**Error Contract:** No error returns, function always succeeds

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

**Resource/Ownership:** Does not allocate or free resources

---

### 2. CubicCongestionControlCanSend
**Signature:** `BOOLEAN CubicCongestionControlCanSend(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:128-135 (static implementations), exposed via function pointer

**Summary:** Determines if more data can be sent based on congestion window and exemptions.

**Preconditions:**
- Cc must not be NULL

**Postconditions:**
- Returns TRUE if BytesInFlight < CongestionWindow OR Exemptions > 0
- Returns FALSE otherwise
- No state modifications

**Side Effects:** None

**Error Contract:** No errors, always returns a boolean

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 3. CubicCongestionControlSetExemption
**Signature:** `void CubicCongestionControlSetExemption(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint8_t NumPackets)`

**Declaration:** cubic.c:138-145

**Summary:** Sets the number of packets that can bypass congestion control (for probe packets).

**Preconditions:**
- Cc must not be NULL
- NumPackets can be any uint8_t value (0-255)

**Postconditions:**
- Cubic.Exemptions = NumPackets

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 4. CubicCongestionControlReset
**Signature:** `void CubicCongestionControlReset(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN FullReset)`

**Declaration:** cubic.c:148-175

**Summary:** Resets CUBIC state to initial conditions, optionally zeroing BytesInFlight.

**Preconditions:**
- Cc must not be NULL
- Connection and Paths[0] must be valid

**Postconditions:**
- Cubic.SlowStartThreshold = UINT32_MAX
- Cubic.MinRttInCurrentRound = UINT32_MAX
- Cubic.HyStartRoundEnd = Connection->Send.NextPacketNumber
- Cubic.HyStartAckCount = 0
- Cubic.MinRttInLastRound = UINT64_MAX
- Cubic.HyStartState = HYSTART_NOT_STARTED (if HyStartEnabled)
- Cubic.IsInRecovery = FALSE
- Cubic.HasHadCongestionEvent = FALSE
- Cubic.CongestionWindow = DatagramPayloadLength * InitialWindowPackets
- Cubic.BytesInFlightMax = CongestionWindow / 2
- Cubic.LastSendAllowance = 0
- If FullReset == TRUE: Cubic.BytesInFlight = 0
- If FullReset == FALSE: Cubic.BytesInFlight unchanged

**Side Effects:** Calls QuicConnLogOutFlowStats and QuicConnLogCubic

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 5. CubicCongestionControlGetSendAllowance
**Signature:** `uint32_t CubicCongestionControlGetSendAllowance(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint64_t TimeSinceLastSend, _In_ BOOLEAN TimeSinceLastSendValid)`

**Declaration:** cubic.c:178-242

**Summary:** Calculates how many bytes can be sent immediately, with pacing support.

**Preconditions:**
- Cc must not be NULL
- TimeSinceLastSend in microseconds (ignored if TimeSinceLastSendValid == FALSE)
- Connection and Paths[0] must be valid

**Postconditions:**
- Returns 0 if BytesInFlight >= CongestionWindow
- Returns (CongestionWindow - BytesInFlight) if:
  - !TimeSinceLastSendValid, OR
  - !PacingEnabled, OR  
  - !GotFirstRttSample, OR
  - SmoothedRtt < QUIC_MIN_PACING_RTT
- Otherwise calculates paced allowance: LastSendAllowance + (EstimatedWnd * TimeSinceLastSend) / SmoothedRtt
- Updates Cubic.LastSendAllowance if pacing is active
- EstimatedWnd in slow start = min(CongestionWindow * 2, SlowStartThreshold)
- EstimatedWnd in congestion avoidance = CongestionWindow * 1.25

**Side Effects:** Updates LastSendAllowance when pacing is active

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 6. CubicCongestionControlOnDataSent
**Signature:** `void CubicCongestionControlOnDataSent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`

**Declaration:** cubic.c:371-398

**Summary:** Called when retransmittable data is sent; updates BytesInFlight and consumes exemptions.

**Preconditions:**
- Cc must not be NULL
- NumRetransmittableBytes >= 0

**Postconditions:**
- Cubic.BytesInFlight += NumRetransmittableBytes
- If new BytesInFlight > BytesInFlightMax: BytesInFlightMax = BytesInFlight
- If NumRetransmittableBytes > LastSendAllowance: LastSendAllowance = 0, else: LastSendAllowance -= NumRetransmittableBytes
- If Exemptions > 0: Exemptions--

**Side Effects:** 
- May call QuicSendBufferConnectionAdjust if BytesInFlightMax increases
- Calls CubicCongestionControlUpdateBlockedState (may update flow control state and logging)

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= PASSIVE_LEVEL

---

### 7. CubicCongestionControlOnDataInvalidated
**Signature:** `BOOLEAN CubicCongestionControlOnDataInvalidated(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ uint32_t NumRetransmittableBytes)`

**Declaration:** cubic.c:401-415

**Summary:** Removes bytes from flight when data is discarded (not lost or acked).

**Preconditions:**
- Cc must not be NULL
- NumRetransmittableBytes <= BytesInFlight (ASSERT checked)

**Postconditions:**
- Cubic.BytesInFlight -= NumRetransmittableBytes
- Returns TRUE if became unblocked (transitioned from can't send to can send)
- Returns FALSE otherwise

**Side Effects:** Calls CubicCongestionControlUpdateBlockedState

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 8. CubicCongestionControlOnDataAcknowledged
**Signature:** `BOOLEAN CubicCongestionControlOnDataAcknowledged(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ACK_EVENT* AckEvent)`

**Declaration:** cubic.c:437-717

**Summary:** Core CUBIC algorithm implementation; handles ACKs and grows congestion window.

**Preconditions:**
- Cc must not be NULL
- AckEvent must not be NULL
- AckEvent.NumRetransmittableBytes <= BytesInFlight (ASSERT checked)
- AckEvent must have valid TimeNow, LargestAck, LargestSentPacketNumber
- If HyStartEnabled and MinRttValid == TRUE: MinRtt must be valid

**Postconditions:**
- Cubic.BytesInFlight -= AckEvent.NumRetransmittableBytes
- If IsInRecovery && LargestAck > RecoverySentPacketNumber:
  - IsInRecovery = FALSE
  - IsInPersistentCongestion = FALSE
  - TimeOfCongAvoidStart = TimeNow
- If !IsInRecovery && NumRetransmittableBytes > 0:
  - Updates HyStart state (if enabled)
  - In slow start: CongestionWindow grows by (BytesAcked / CWndSlowStartGrowthDivisor)
  - In congestion avoidance: CongestionWindow grows per CUBIC formula
  - CongestionWindow capped at 2 * BytesInFlightMax
- Cubic.TimeOfLastAck = TimeNow
- Cubic.TimeOfLastAckValid = TRUE
- Returns TRUE if became unblocked

**Side Effects:**
- May transition HyStart state
- May trigger QUIC_CONNECTION_EVENT_NETWORK_STATISTICS if Settings.NetStatsEventEnabled
- Calls QuicConnIndicateEvent, CubicCongestionControlUpdateBlockedState

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

**HyStart State Transitions:**
- HYSTART_NOT_STARTED -> HYSTART_ACTIVE: when MinRtt increase >= Eta detected
- HYSTART_ACTIVE -> HYSTART_DONE: after ConservativeSlowStartRounds RTT rounds
- HYSTART_ACTIVE -> HYSTART_NOT_STARTED: if MinRtt decreases (spurious exit)
- Any state -> HYSTART_DONE: on congestion event

**Algorithm Details:**
- Slow Start: exponential growth (CongestionWindow += BytesAcked)
- Conservative Slow Start (HyStart ACTIVE): slower growth (BytesAcked / 4)
- Congestion Avoidance: CUBIC window growth formula W_cubic(t) = C*(t-K)^3 + WindowMax
- AIMD window for TCP-friendliness
- Final window = max(CUBIC window, AIMD window)

---

### 9. CubicCongestionControlOnDataLost
**Signature:** `void CubicCongestionControlOnDataLost(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_LOSS_EVENT* LossEvent)`

**Declaration:** cubic.c:720-752

**Summary:** Handles packet loss events; reduces congestion window and enters recovery.

**Preconditions:**
- Cc must not be NULL
- LossEvent must not be NULL
- LossEvent.NumRetransmittableBytes <= BytesInFlight (ASSERT checked)

**Postconditions:**
- If !HasHadCongestionEvent OR LargestPacketNumberLost > RecoverySentPacketNumber:
  - Calls CubicCongestionControlOnCongestionEvent
  - RecoverySentPacketNumber = LargestSentPacketNumber
  - HyStartState = HYSTART_DONE
- Cubic.BytesInFlight -= LossEvent.NumRetransmittableBytes

**Side Effects:**
- Calls CubicCongestionControlOnCongestionEvent (reduces window)
- Calls CubicCongestionControlUpdateBlockedState
- Calls QuicConnLogCubic

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 10. CubicCongestionControlOnCongestionEvent
**Signature:** `void CubicCongestionControlOnCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ BOOLEAN IsPersistentCongestion, _In_ BOOLEAN Ecn)`

**Declaration:** cubic.c:271-368

**Summary:** Handles congestion events (loss or ECN); applies multiplicative decrease.

**Preconditions:**
- Cc must not be NULL
- Connection and Paths[0] must be valid

**Postconditions:**
- Cubic.IsInRecovery = TRUE
- Cubic.HasHadCongestionEvent = TRUE
- If Ecn == FALSE: saves previous state (PrevWindowPrior, PrevWindowMax, etc.)
- If IsPersistentCongestion && !IsInPersistentCongestion:
  - IsInPersistentCongestion = TRUE
  - CongestionWindow = DatagramPayloadLength * QUIC_PERSISTENT_CONGESTION_WINDOW_PACKETS (2)
  - SlowStartThreshold = AimdWindow = WindowPrior = WindowMax = WindowLastMax = old CongestionWindow * BETA
  - KCubic = 0
  - Route.State = RouteSuspected
- Else (normal congestion):
  - WindowPrior = WindowMax = CongestionWindow
  - If WindowLastMax > WindowMax: fast convergence (WindowMax *= (10+BETA)/20, WindowLastMax = WindowMax)
  - Else: WindowLastMax = WindowMax
  - Computes KCubic = CubeRoot((WindowMax * (1-BETA) / C) in milliseconds
  - CongestionWindow = SlowStartThreshold = AimdWindow = max(2*DatagramPayloadLength, CongestionWindow * BETA)
- HyStartState = HYSTART_DONE

**Side Effects:**
- Increments Stats.Send.CongestionCount
- If persistent congestion: increments Stats.Send.PersistentCongestionCount
- Calls QuicTraceEvent for logging

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

**CUBIC Constants:**
- BETA = 0.7 (TEN_TIMES_BETA_CUBIC = 7)
- C = 0.4 (TEN_TIMES_C_CUBIC = 4)

---

### 11. CubicCongestionControlOnEcn
**Signature:** `void CubicCongestionControlOnEcn(_In_ QUIC_CONGESTION_CONTROL* Cc, _In_ const QUIC_ECN_EVENT* EcnEvent)`

**Declaration:** cubic.c:755-784

**Summary:** Handles ECN congestion signals; triggers congestion event if new.

**Preconditions:**
- Cc must not be NULL
- EcnEvent must not be NULL

**Postconditions:**
- If !HasHadCongestionEvent OR LargestPacketNumberAcked > RecoverySentPacketNumber:
  - RecoverySentPacketNumber = LargestSentPacketNumber
  - Calls CubicCongestionControlOnCongestionEvent(Cc, FALSE, TRUE)
  - HyStartState = HYSTART_DONE
  - Increments Stats.Send.EcnCongestionCount

**Side Effects:**
- Calls CubicCongestionControlUpdateBlockedState
- Calls QuicConnLogCubic

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 12. CubicCongestionControlOnSpuriousCongestionEvent
**Signature:** `BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(_In_ QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:787-823

**Summary:** Reverts congestion window reduction when congestion event is determined to be spurious.

**Preconditions:**
- Cc must not be NULL

**Postconditions:**
- If !IsInRecovery: returns FALSE, no changes
- If IsInRecovery:
  - Restores previous state: WindowPrior, WindowMax, WindowLastMax, KCubic, SlowStartThreshold, CongestionWindow, AimdWindow
  - IsInRecovery = FALSE
  - HasHadCongestionEvent = FALSE
  - Returns TRUE if became unblocked

**Side Effects:**
- Calls QuicTraceEvent for logging
- Calls CubicCongestionControlUpdateBlockedState
- Calls QuicConnLogCubic

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

**Note:** Only reverts non-ECN congestion events (saved state only available for those)

---

### 13. CubicCongestionControlGetNetworkStatistics
**Signature:** `void CubicCongestionControlGetNetworkStatistics(_In_ const QUIC_CONNECTION* const Connection, _In_ const QUIC_CONGESTION_CONTROL* const Cc, _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics)`

**Declaration:** cubic.c:418-434

**Summary:** Retrieves current network statistics for monitoring.

**Preconditions:**
- Connection must not be NULL
- Cc must not be NULL
- NetworkStatistics must not be NULL (output parameter)

**Postconditions:**
- NetworkStatistics populated with:
  - BytesInFlight = Cubic.BytesInFlight
  - PostedBytes = Connection->SendBuffer.PostedBytes
  - IdealBytes = Connection->SendBuffer.IdealBytes
  - SmoothedRTT = Path->SmoothedRtt
  - CongestionWindow = Cubic.CongestionWindow
  - Bandwidth = CongestionWindow / SmoothedRtt

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 14. CubicCongestionControlLogOutFlowStatus
**Signature:** `void CubicCongestionControlLogOutFlowStatus(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:825-846

**Summary:** Logs current outflow status for diagnostics.

**Preconditions:**
- Cc must not be NULL

**Postconditions:** No state changes

**Side Effects:** Calls QuicTraceEvent for telemetry

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 15. CubicCongestionControlGetBytesInFlightMax
**Signature:** `uint32_t CubicCongestionControlGetBytesInFlightMax(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:848-854

**Summary:** Returns the maximum BytesInFlight observed since last reset.

**Preconditions:**
- Cc must not be NULL

**Postconditions:**
- Returns Cubic.BytesInFlightMax
- No state changes

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Callable at any IRQL <= DISPATCH_LEVEL

---

### 16. CubicCongestionControlGetExemptions
**Signature:** `uint8_t CubicCongestionControlGetExemptions(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:857-863

**Summary:** Returns the current number of exemption packets.

**Preconditions:**
- Cc must not be NULL

**Postconditions:**
- Returns Cubic.Exemptions
- No state changes

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 17. CubicCongestionControlGetCongestionWindow
**Signature:** `uint32_t CubicCongestionControlGetCongestionWindow(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:865-871

**Summary:** Returns the current congestion window size.

**Preconditions:**
- Cc must not be NULL

**Postconditions:**
- Returns Cubic.CongestionWindow
- No state changes

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 18. CubicCongestionControlIsAppLimited
**Signature:** `BOOLEAN CubicCongestionControlIsAppLimited(_In_ const QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:874-881

**Summary:** Checks if connection is application-limited (always returns FALSE for CUBIC).

**Preconditions:**
- Cc must not be NULL (unused)

**Postconditions:**
- Always returns FALSE
- No state changes

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 19. CubicCongestionControlSetAppLimited
**Signature:** `void CubicCongestionControlSetAppLimited(_In_ struct QUIC_CONGESTION_CONTROL* Cc)`

**Declaration:** cubic.c:884-890

**Summary:** No-op for CUBIC (used by BBR).

**Preconditions:**
- Cc must not be NULL (unused)

**Postconditions:** No state changes

**Side Effects:** None

**Error Contract:** No errors

**Thread Safety:** Must be called at IRQL <= DISPATCH_LEVEL

---

### 20. CubeRoot (Internal but exercised by public APIs)
**Signature:** `uint32_t CubeRoot(uint32_t Radicand)`

**Declaration:** cubic.c:44-62

**Summary:** Computes integer cube root using shifting nth root algorithm.

**Preconditions:**
- Radicand can be any uint32_t value

**Postconditions:**
- Returns y such that y^3 <= Radicand < (y+1)^3
- For Radicand = 0: returns 0
- For Radicand = 1: returns 1
- For Radicand = 8: returns 2
- For Radicand = UINT32_MAX: returns 1625

**Side Effects:** None (pure function)

**Error Contract:** No errors

**Thread Safety:** Thread-safe (no shared state)

**Algorithm:** Shifting nth root with 3-bit chunks (2^3 = 8)

---

## B. Type/Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC Structure

**Invariants (must always hold for valid instances):**

1. **Congestion Window Bounds:**
   - CongestionWindow >= 2 * DatagramPayloadLength (minimum window is 2 packets)
   - CongestionWindow <= 2 * BytesInFlightMax (growth limited by actual usage)

2. **Slow Start Threshold:**
   - SlowStartThreshold <= UINT32_MAX
   - After first congestion event: SlowStartThreshold < UINT32_MAX

3. **Recovery State:**
   - If IsInRecovery == TRUE: HasHadCongestionEvent == TRUE
   - If IsInPersistentCongestion == TRUE: IsInRecovery == TRUE
   - If HasHadCongestionEvent == TRUE: RecoverySentPacketNumber is valid

4. **BytesInFlight Accounting:**
   - BytesInFlight <= BytesInFlightMax (unless increased by OnDataSent)
   - BytesInFlight >= 0

5. **HyStart State:**
   - HyStartState in {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}
   - If HyStartState == HYSTART_ACTIVE: CWndSlowStartGrowthDivisor == 4
   - If HyStartState != HYSTART_ACTIVE: CWndSlowStartGrowthDivisor == 1
   - MinRttInCurrentRound <= MinRttInLastRound (typically, until reset)

6. **CUBIC Parameters:**
   - WindowMax >= CongestionWindow (after congestion event)
   - WindowLastMax >= WindowMax (or equals during fast convergence)
   - KCubic computed from WindowMax, BETA, C
   - AimdWindow tracks TCP-friendly AIMD growth

7. **Time Tracking:**
   - If TimeOfLastAckValid == TRUE: TimeOfLastAck is a valid timestamp
   - TimeOfCongAvoidStart is valid after first transition to congestion avoidance

8. **Exemptions:**
   - Exemptions <= 255 (uint8_t)
   - Decrements on each OnDataSent while > 0

### State Machine: CUBIC Recovery States

**States:**
1. **Normal Operation** (IsInRecovery = FALSE)
2. **Recovery** (IsInRecovery = TRUE, IsInPersistentCongestion = FALSE)
3. **Persistent Congestion** (IsInPersistentCongestion = TRUE)

**Transitions:**

```
Normal Operation
    | OnDataLost / OnEcn (new loss)
    v
Recovery
    | OnDataLost (persistent congestion detected)
    v
Persistent Congestion
    | OnDataAcknowledged (ACK > RecoverySentPacketNumber)
    v
Normal Operation

Recovery
    | OnDataAcknowledged (ACK > RecoverySentPacketNumber)
    v
Normal Operation

Any State
    | OnSpuriousCongestionEvent (if IsInRecovery)
    v
Normal Operation

Any State
    | Reset
    v
Normal Operation
```

**State Invariants:**

**Normal Operation:**
- IsInRecovery = FALSE
- IsInPersistentCongestion = FALSE
- CongestionWindow growth active (slow start or congestion avoidance)

**Recovery:**
- IsInRecovery = TRUE
- IsInPersistentCongestion = FALSE
- HasHadCongestionEvent = TRUE
- RecoverySentPacketNumber is valid
- CongestionWindow reduced by BETA
- No window growth until recovery exit

**Persistent Congestion:**
- IsInPersistentCongestion = TRUE
- IsInRecovery = TRUE
- CongestionWindow = 2 * DatagramPayloadLength
- SlowStartThreshold = old_window * BETA
- KCubic = 0

### State Machine: HyStart States

**States:**
1. **HYSTART_NOT_STARTED** - Normal slow start
2. **HYSTART_ACTIVE** - Conservative slow start (detected delay increase)
3. **HYSTART_DONE** - Exited slow start, in congestion avoidance

**Transitions:**

```
HYSTART_NOT_STARTED
    | HyStartEnabled && MinRtt increase >= Eta
    v
HYSTART_ACTIVE
    | ConservativeSlowStartRounds RTT rounds elapsed
    v
HYSTART_DONE

HYSTART_ACTIVE
    | MinRtt decreased (spurious detection)
    v
HYSTART_NOT_STARTED

Any HyStart State
    | Congestion Event (loss, ECN) or CongestionWindow >= SlowStartThreshold
    v
HYSTART_DONE

Any HyStart State
    | Reset
    v
HYSTART_NOT_STARTED
```

**State Invariants:**

**HYSTART_NOT_STARTED:**
- CWndSlowStartGrowthDivisor = 1
- Normal slow start growth
- Monitoring MinRtt samples

**HYSTART_ACTIVE:**
- CWndSlowStartGrowthDivisor = 4 (QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR)
- ConservativeSlowStartRounds > 0 (counts down)
- CssBaselineMinRtt holds baseline RTT
- Slower growth to avoid overshoot

**HYSTART_DONE:**
- CWndSlowStartGrowthDivisor = 1
- CongestionWindow >= SlowStartThreshold
- In congestion avoidance mode

---

## C. Environment Invariants

1. **Connection Initialization:**
   - Connection structure must be properly initialized before CubicCongestionControlInitialize
   - Paths[0] must have valid Mtu set

2. **Settings Validity:**
   - InitialWindowPackets must be reasonable (typically 10, RFC recommends 10)
   - SendIdleTimeoutMs typically > 0 for idle detection

3. **Packet Number Monotonicity:**
   - Connection->Send.NextPacketNumber must be monotonically increasing
   - LargestAck, LargestSentPacketNumber must follow packet number space rules

4. **RTT Sample Validity:**
   - If GotFirstRttSample == TRUE: SmoothedRtt, MinRtt, RttVariance are valid
   - SmoothedRtt should be > 0 when valid

5. **Function Pointer Table:**
   - After CubicCongestionControlInitialize, all 17 function pointers must remain valid
   - Function pointers must not be modified by external code

6. **Memory Lifetime:**
   - Connection structure must remain valid for entire lifetime of Cc
   - Settings structure only needed during initialization

7. **No Memory Leaks:**
   - CUBIC does not allocate heap memory
   - All state is embedded in QUIC_CONGESTION_CONTROL_CUBIC structure

8. **Telemetry/Logging:**
   - QuicTraceEvent and logging functions must be available
   - Logging failures must not cause functional failures

---

## D. Dependency Map & Call Relationships

### Internal Helper Functions (not public API)

1. **CubicCongestionHyStartChangeState**
   - Called by: Initialize, Reset, OnCongestionEvent, OnDataLost, OnEcn, OnDataAcknowledged
   - Changes HyStart state with proper divisor updates

2. **CubicCongestionHyStartResetPerRttRound**
   - Called by: Reset, OnDataAcknowledged
   - Resets per-RTT HyStart counters

3. **CubicCongestionControlUpdateBlockedState**
   - Called by: OnDataSent, OnDataInvalidated, OnDataAcknowledged, OnDataLost, OnEcn, OnSpuriousCongestionEvent
   - Updates flow control blocked state and logging
   - Returns TRUE if became unblocked

4. **QuicConnLogCubic**
   - Called by: Initialize, Reset, OnDataLost, OnEcn, OnSpuriousCongestionEvent
   - Logs CUBIC state for diagnostics

### Public API Call Dependencies

**CubicCongestionControlInitialize:**
- Calls: QuicCongestionControlGetConnection, QuicPathGetDatagramPayloadSize, CubicCongestionHyStartResetPerRttRound, CubicCongestionHyStartChangeState, QuicConnLogOutFlowStats, QuicConnLogCubic

**CubicCongestionControlOnDataAcknowledged:**
- Calls: QuicCongestionControlGetConnection, CubicCongestionControlCanSend, CubicCongestionHyStartChangeState, CubicCongestionHyStartResetPerRttRound, QuicPathGetDatagramPayloadSize, QuicConnIndicateEvent, CubicCongestionControlUpdateBlockedState, CubeRoot (indirectly through congestion window calculations)

**CubicCongestionControlOnCongestionEvent:**
- Calls: QuicCongestionControlGetConnection, QuicPathGetDatagramPayloadSize, QuicTraceEvent, CubicCongestionHyStartChangeState, CubeRoot

**CubicCongestionControlOnDataLost:**
- Calls: CubicCongestionControlCanSend, CubicCongestionControlOnCongestionEvent, CubicCongestionHyStartChangeState, CubicCongestionControlUpdateBlockedState, QuicConnLogCubic

**CubicCongestionControlOnEcn:**
- Calls: CubicCongestionControlCanSend, QuicCongestionControlGetConnection, CubicCongestionControlOnCongestionEvent, CubicCongestionHyStartChangeState, CubicCongestionControlUpdateBlockedState, QuicConnLogCubic

**CubicCongestionControlOnSpuriousCongestionEvent:**
- Calls: QuicCongestionControlGetConnection, QuicCongestionControlCanSend, QuicTraceEvent, CubicCongestionControlUpdateBlockedState, QuicConnLogCubic

### External Dependencies (from Connection/Path structures)

- **QuicCongestionControlGetConnection**: Macro to get Connection from Cc pointer (CXPLAT_CONTAINING_RECORD)
- **QuicPathGetDatagramPayloadSize**: Gets MTU minus headers
- **QuicConnLogOutFlowStats**: Telemetry logging
- **QuicTraceEvent**: Event tracing
- **QuicConnIndicateEvent**: Event indication to application
- **QuicSendBufferConnectionAdjust**: Adjusts send buffer on BytesInFlightMax increase
- **QuicConnAddOutFlowBlockedReason / QuicConnRemoveOutFlowBlockedReason**: Flow control state management
- **CxPlatTimeUs64**: Current time in microseconds
- **CxPlatTimeDiff64**: Time difference calculation
- **CxPlatTimeAtOrBefore64**: Time comparison

### Callback/Indirect Call Points

- All 17 public APIs are invoked via function pointers in QUIC_CONGESTION_CONTROL
- Connection event callback (QuicConnIndicateEvent) for NETWORK_STATISTICS events
- Telemetry/logging callbacks (non-blocking, best-effort)

---

## E. Test Strategy & Scenario Catalog

### Scenario Categories

**1. Initialization Scenarios:**
- Default initialization
- Boundary MTUs (minimum, maximum)
- Boundary InitialWindowPackets (1, 10, 1000)
- Re-initialization with different settings

**2. Slow Start Scenarios:**
- Basic slow start growth (exponential)
- Slow start with HyStart disabled
- HyStart detection of delay increase
- Conservative slow start after HyStart trigger
- Transition to congestion avoidance

**3. Congestion Avoidance Scenarios:**
- CUBIC window growth (concave region, convex region)
- AIMD window growth (TCP-friendly)
- Window selection (max of CUBIC and AIMD)
- Time-based growth freeze (large ACK gaps)
- Window capped by BytesInFlightMax

**4. Loss Recovery Scenarios:**
- First congestion event (no prior recovery)
- Repeated loss in same recovery period
- Loss after recovery exit
- Persistent congestion detection
- Spurious congestion event revert

**5. ECN Scenarios:**
- ECN congestion signal (first event)
- ECN during recovery
- ECN state saved vs not saved

**6. HyStart State Transitions:**
- NOT_STARTED -> ACTIVE (delay increase)
- ACTIVE -> DONE (CSS rounds exhausted)
- ACTIVE -> NOT_STARTED (RTT decreased, spurious)
- Any state -> DONE (congestion event)

**7. Pacing Scenarios:**
- Pacing disabled
- Pacing with no RTT sample
- Pacing with valid RTT
- Pacing rate calculation in slow start vs congestion avoidance
- Pacing overflow protection

**8. Exemptions Scenarios:**
- Setting exemptions
- Consuming exemptions on send
- Sending with exemptions when blocked

**9. BytesInFlight Accounting:**
- OnDataSent increases BytesInFlight
- OnDataAcknowledged decreases BytesInFlight
- OnDataInvalidated decreases BytesInFlight
- OnDataLost decreases BytesInFlight
- BytesInFlightMax tracking

**10. Edge Cases:**
- Zero bytes acknowledged
- Very large congestion windows
- Overflow protection in calculations
- CubeRoot boundary values
- Reset during recovery

---

## F. Coverage Goals

### Line Coverage Target: 100% of cubic.c

### Critical Paths to Cover:

1. **CubeRoot function**: all radicand ranges (0, 1, 8, 27, powers of 2, UINT32_MAX)
2. **GetSendAllowance**: all branches (blocked, pacing disabled, pacing active, overflow)
3. **OnDataAcknowledged**:
   - Recovery vs normal operation
   - Slow start vs congestion avoidance
   - HyStart transitions (all 3 states)
   - CUBIC vs AIMD window selection
   - Time-based growth freeze
   - Window capping
4. **OnCongestionEvent**:
   - Persistent congestion path
   - Normal congestion path
   - Fast convergence vs normal
   - ECN vs non-ECN (state saving)
5. **OnDataLost**: new loss vs repeated loss in recovery
6. **OnEcn**: new ECN vs ECN during recovery
7. **OnSpuriousCongestionEvent**: in recovery vs not in recovery
8. **HyStart logic**:
   - Sample collection (< N samples)
   - Delay increase detection
   - RTT decrease (spurious exit)
   - Round-end detection
9. **Reset**: FullReset TRUE vs FALSE

### Function Coverage: 100% of public APIs

All 19 public APIs (plus CubeRoot) must be directly invoked by tests.

---

## G. Known Assumptions & Approximations

1. **Connection Structure Layout:**
   - Assumes QUIC_CONGESTION_CONTROL is embedded in QUIC_CONNECTION
   - QuicCongestionControlGetConnection uses CXPLAT_CONTAINING_RECORD macro
   - Tests must use real QUIC_CONNECTION structure for correct pointer arithmetic

2. **Telemetry Non-Blocking:**
   - Assumes logging/tracing never blocks or fails critically
   - Telemetry failures do not affect functional correctness

3. **Time Monotonicity:**
   - Assumes CxPlatTimeUs64() provides monotonically increasing timestamps
   - No backward time jumps

4. **Packet Number Monotonicity:**
   - Assumes packet numbers are strictly increasing
   - No packet number space wrapping in tests

5. **Single Path:**
   - CUBIC operates on Paths[0]
   - Multi-path scenarios not covered in basic CUBIC logic

6. **No External State Modification:**
   - Tests must not directly modify Cubic structure fields (use public APIs only)
   - Exception: Setup of mock state for specific scenarios

7. **Integer Arithmetic Assumptions:**
   - CubeRoot algorithm assumes 32-bit integers
   - Overflow protection via clamping and saturating arithmetic

---

## H. Summary

The CUBIC congestion control implementation provides 19 public APIs plus an internal CubeRoot utility function. The component implements RFC 8312bis with HyStart++ support for safe slow start exit. Key features:

- **Initialization**: Single initialization function with settings
- **Core Algorithms**: Slow start, congestion avoidance (CUBIC + AIMD), recovery
- **State Machines**: Recovery state (Normal/Recovery/Persistent), HyStart state (NOT_STARTED/ACTIVE/DONE)
- **Pacing**: Optional pacing based on RTT and congestion window
- **ECN Support**: Handles explicit congestion notification
- **Spurious Loss**: Can revert window reduction on spurious congestion events
- **Telemetry**: Comprehensive logging and statistics

All public APIs are contract-safe, with well-defined preconditions and postconditions. No dynamic memory allocation. Thread-safe at IRQL <= DISPATCH_LEVEL.

Test generation should focus on:
1. Complete API coverage (all 19 functions)
2. State machine transitions (Recovery, HyStart)
3. CUBIC algorithm correctness (window growth/reduction)
4. Edge cases (overflows, boundary values)
5. Integration scenarios (ACK/Loss/ECN sequences)

The goal is 100% line coverage of cubic.c through scenario-based, contract-safe public API testing.
