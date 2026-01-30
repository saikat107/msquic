# Repository Contract Index: QUIC_CUBIC_CONGESTION_CONTROL

## A) Public API Inventory

### 1. CubicCongestionControlInitialize
- **Declaration**: `cubic.h` (line 119-124)
- **Signature**: `void CubicCongestionControlInitialize(QUIC_CONGESTION_CONTROL* Cc, const QUIC_SETTINGS_INTERNAL* Settings)`
- **Summary**: Initializes the CUBIC congestion control algorithm with settings
- **Preconditions**: 
  - `Cc` must be non-null and part of valid `QUIC_CONNECTION` structure
  - `Settings` must be non-null with valid InitialWindowPackets and SendIdleTimeoutMs
- **Postconditions**:
  - All function pointers set to CUBIC implementations
  - CongestionWindow = DatagramPayloadLength * InitialWindowPackets
  - SlowStartThreshold = UINT32_MAX
  - BytesInFlightMax = CongestionWindow / 2
  - All state flags cleared (HasHadCongestionEvent, IsInRecovery, IsInPersistentCongestion)
  - HyStart state = HYSTART_NOT_STARTED
- **Side Effects**: Logs CUBIC state via QuicConnLogOutFlowStats and QuicConnLogCubic

### 2. CubicCongestionControlCanSend
- **Signature**: `BOOLEAN CubicCongestionControlCanSend(QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Returns TRUE if more bytes can be sent (BytesInFlight < CongestionWindow OR Exemptions > 0)
- **Preconditions**: Cc must be initialized
- **Postconditions**: None (read-only)
- **Return**: TRUE if can send, FALSE if congestion blocked

### 3. CubicCongestionControlSetExemption
- **Signature**: `void CubicCongestionControlSetExemption(QUIC_CONGESTION_CONTROL* Cc, uint8_t NumPackets)`
- **Summary**: Sets number of packets that can bypass congestion control
- **Preconditions**: Cc must be initialized
- **Postconditions**: Cubic.Exemptions = NumPackets

### 4. CubicCongestionControlReset
- **Signature**: `void CubicCongestionControlReset(QUIC_CONGESTION_CONTROL* Cc, BOOLEAN FullReset)`
- **Summary**: Resets CUBIC state for connection migration/recovery
- **Preconditions**: Cc must be initialized
- **Postconditions**:
  - SlowStartThreshold = UINT32_MAX
  - IsInRecovery = FALSE, HasHadCongestionEvent = FALSE
  - CongestionWindow recalculated from MTU and InitialWindowPackets
  - If FullReset: BytesInFlight = 0

### 5. CubicCongestionControlGetSendAllowance
- **Signature**: `uint32_t CubicCongestionControlGetSendAllowance(QUIC_CONGESTION_CONTROL* Cc, uint64_t TimeSinceLastSend, BOOLEAN TimeSinceLastSendValid)`
- **Summary**: Returns bytes allowed to send considering pacing and congestion window
- **Preconditions**: Cc must be initialized
- **Postconditions**: May update LastSendAllowance when pacing
- **Return**: 
  - 0 if congestion blocked (BytesInFlight >= CongestionWindow)
  - Full available window if not pacing
  - Paced allowance if pacing enabled

### 6. CubicCongestionControlOnDataSent
- **Signature**: `void CubicCongestionControlOnDataSent(QUIC_CONGESTION_CONTROL* Cc, uint32_t NumRetransmittableBytes)`
- **Summary**: Called when retransmittable data is sent
- **Preconditions**: Cc must be initialized, NumRetransmittableBytes > 0 typical
- **Postconditions**:
  - BytesInFlight += NumRetransmittableBytes
  - BytesInFlightMax updated if new maximum
  - LastSendAllowance decremented
  - Exemptions decremented if > 0

### 7. CubicCongestionControlOnDataInvalidated
- **Signature**: `BOOLEAN CubicCongestionControlOnDataInvalidated(QUIC_CONGESTION_CONTROL* Cc, uint32_t NumRetransmittableBytes)`
- **Summary**: Removes bytes from in-flight without loss/ack (e.g., key phase change)
- **Preconditions**: BytesInFlight >= NumRetransmittableBytes
- **Postconditions**: BytesInFlight -= NumRetransmittableBytes
- **Return**: TRUE if became unblocked

### 8. CubicCongestionControlOnDataAcknowledged
- **Signature**: `BOOLEAN CubicCongestionControlOnDataAcknowledged(QUIC_CONGESTION_CONTROL* Cc, const QUIC_ACK_EVENT* AckEvent)`
- **Summary**: Called when data is acknowledged - grows congestion window
- **Preconditions**: Cc initialized, AckEvent valid
- **Postconditions**:
  - BytesInFlight -= AckEvent->NumRetransmittableBytes
  - CongestionWindow may grow (slow start or congestion avoidance)
  - Recovery may exit if ack > RecoverySentPacketNumber
  - HyStart state may transition
- **Return**: TRUE if became unblocked

### 9. CubicCongestionControlOnDataLost
- **Signature**: `void CubicCongestionControlOnDataLost(QUIC_CONGESTION_CONTROL* Cc, const QUIC_LOSS_EVENT* LossEvent)`
- **Summary**: Called when packet loss detected - reduces congestion window
- **Preconditions**: Cc initialized, LossEvent valid, BytesInFlight >= LossEvent->NumRetransmittableBytes
- **Postconditions**:
  - BytesInFlight -= NumRetransmittableBytes
  - If new congestion event: CongestionWindow reduced by BETA (0.7)
  - IsInRecovery = TRUE, HasHadCongestionEvent = TRUE
  - SlowStartThreshold updated

### 10. CubicCongestionControlOnEcn
- **Signature**: `void CubicCongestionControlOnEcn(QUIC_CONGESTION_CONTROL* Cc, const QUIC_ECN_EVENT* EcnEvent)`
- **Summary**: Called when ECN congestion signal received
- **Preconditions**: Cc initialized, EcnEvent valid
- **Postconditions**: Same as OnDataLost for new congestion event

### 11. CubicCongestionControlOnSpuriousCongestionEvent
- **Signature**: `BOOLEAN CubicCongestionControlOnSpuriousCongestionEvent(QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Reverts congestion window reduction if loss was spurious
- **Preconditions**: Cc initialized
- **Postconditions**: 
  - If IsInRecovery: restores all Prev* state values
  - IsInRecovery = FALSE, HasHadCongestionEvent = FALSE
- **Return**: TRUE if became unblocked, FALSE if not in recovery

### 12. CubicCongestionControlLogOutFlowStatus
- **Signature**: `void CubicCongestionControlLogOutFlowStatus(const QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Logs outflow statistics for diagnostics
- **Preconditions**: Cc initialized
- **Postconditions**: None (logging only)

### 13. CubicCongestionControlGetExemptions
- **Signature**: `uint8_t CubicCongestionControlGetExemptions(const QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Returns current exemptions count
- **Return**: Cubic.Exemptions

### 14. CubicCongestionControlGetBytesInFlightMax
- **Signature**: `uint32_t CubicCongestionControlGetBytesInFlightMax(const QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Returns maximum bytes in flight observed
- **Return**: Cubic.BytesInFlightMax

### 15. CubicCongestionControlGetCongestionWindow
- **Signature**: `uint32_t CubicCongestionControlGetCongestionWindow(const QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Returns current congestion window size
- **Return**: Cubic.CongestionWindow

### 16. CubicCongestionControlIsAppLimited
- **Signature**: `BOOLEAN CubicCongestionControlIsAppLimited(const QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Returns whether application is limiting sends (always FALSE for CUBIC)
- **Return**: FALSE

### 17. CubicCongestionControlSetAppLimited
- **Signature**: `void CubicCongestionControlSetAppLimited(QUIC_CONGESTION_CONTROL* Cc)`
- **Summary**: Marks connection as app-limited (no-op for CUBIC)

### 18. CubicCongestionControlGetNetworkStatistics
- **Signature**: `void CubicCongestionControlGetNetworkStatistics(const QUIC_CONNECTION* Connection, const QUIC_CONGESTION_CONTROL* Cc, QUIC_NETWORK_STATISTICS* NetworkStatistics)`
- **Summary**: Populates network statistics structure
- **Preconditions**: All parameters valid
- **Postconditions**: NetworkStatistics populated with current values

---

## B) Type/Object Invariants

### QUIC_CONGESTION_CONTROL_CUBIC
- **Valid States**:
  - Initial: HasHadCongestionEvent=FALSE, IsInRecovery=FALSE, IsInPersistentCongestion=FALSE
  - Normal operation: BytesInFlight <= CongestionWindow (when CanSend returns TRUE without exemptions)
  - Recovery: IsInRecovery=TRUE, RecoverySentPacketNumber valid
  - Persistent Congestion: IsInPersistentCongestion=TRUE, CongestionWindow at minimum

### HyStart State Machine
```
HYSTART_NOT_STARTED ──(delay increase detected)──> HYSTART_ACTIVE
         ^                                              │
         │                                              v
         └────────(RTT decreased)────────────── HYSTART_ACTIVE
                                                        │
                                                        v (ConservativeSlowStartRounds=0)
                                               HYSTART_DONE
```

### State Invariants:
- HYSTART_NOT_STARTED: CWndSlowStartGrowthDivisor = 1
- HYSTART_ACTIVE: CWndSlowStartGrowthDivisor = QUIC_CONSERVATIVE_SLOW_START_DEFAULT_GROWTH_DIVISOR
- HYSTART_DONE: CWndSlowStartGrowthDivisor = 1, SlowStartThreshold <= CongestionWindow

---

## C) Environment Invariants

1. Connection must be initialized with valid paths before CUBIC init
2. Paths[0].Mtu must be set to valid MTU value
3. Settings.InitialWindowPackets and Settings.SendIdleTimeoutMs must be valid
4. BytesInFlight must never exceed actual bytes sent minus acknowledged/invalidated

---

## D) Dependency Map

### Internal Helper Functions (not directly testable):
- `CubeRoot(uint32_t)`: Computes integer cube root
- `QuicConnLogCubic()`: Logging helper
- `CubicCongestionHyStartChangeState()`: HyStart state machine
- `CubicCongestionHyStartResetPerRttRound()`: Resets per-RTT HyStart counters
- `CubicCongestionControlUpdateBlockedState()`: Updates blocked status and returns unblock signal
- `CubicCongestionControlOnCongestionEvent()`: Core congestion handling

### Key Call Relationships:
```
Initialize --> sets function pointers
OnDataSent --> UpdateBlockedState
OnDataAcknowledged --> HyStartChangeState, HyStartResetPerRttRound, UpdateBlockedState
OnDataLost --> OnCongestionEvent, HyStartChangeState, UpdateBlockedState
OnEcn --> OnCongestionEvent, HyStartChangeState, UpdateBlockedState
OnSpuriousCongestionEvent --> UpdateBlockedState
Reset --> HyStartResetPerRttRound, HyStartChangeState
```

---

## Uncovered Scenarios Analysis

Based on existing 17 tests, the following scenarios need coverage:

1. **Pacing with estimated window calculation** (lines 220-239)
2. **Slow start to congestion avoidance transition** (lines 541-560)
3. **Congestion avoidance window growth** (lines 563-670)
4. **CUBIC window calculation with DeltaT** (lines 610-631)
5. **AIMD window calculation** (lines 633-657)
6. **Window limiting by BytesInFlightMax** (lines 683-685)
7. **Fast convergence in congestion event** (lines 335-343)
8. **Persistent congestion handling** (lines 307-328)
9. **Recovery exit via ACK** (lines 453-468)
10. **HyStart RTT sampling** (lines 476-518)
11. **HyStart round reset** (lines 524-538)
12. **Time gap handling in congestion avoidance** (lines 580-588)
13. **Spurious congestion recovery** (lines 786-823)
