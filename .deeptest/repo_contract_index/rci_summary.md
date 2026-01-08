# Repository Contract Index (RCI) Summary
# BBR Congestion Control Component

## Component Overview
**Source**: `src\core\bbr.c`  
**Header**: `src\core\bbr.h`  
**Type**: Congestion Control Algorithm (BBR - Bottleneck Bandwidth and RTT)

BBR is a congestion control algorithm that estimates bandwidth and RTT to determine sending rate and congestion window. It operates through four states: STARTUP, DRAIN, PROBE_BW, and PROBE_RTT.

---

## A) Public API Inventory

### 1. BbrCongestionControlInitialize
**Signature**: 
```c
void BbrCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings
);
```
**Declaration**: `src\core\bbr.h:224-227`  
**Implementation**: `src\core\bbr.c:1089-1160`  
**Visibility**: Public (exported function)

**Purpose**: Initializes BBR congestion control state and function pointers.

**Preconditions**:
- `Cc` must be non-NULL and point to valid `QUIC_CONGESTION_CONTROL` structure
- `Cc` must be embedded in a valid `QUIC_CONNECTION` structure (accessed via `QuicCongestionControlGetConnection`)
- `Settings` must be non-NULL and contain valid `InitialWindowPackets` (typically 1-1000)
- `Connection->Paths[0]` must be initialized with valid MTU via `QuicPathGetDatagramPayloadSize`

**Postconditions**:
- All function pointers in `Cc` set to BBR implementations
- `Bbr` state initialized to STARTUP
- Congestion window set to `InitialWindowPackets * DatagramPayloadLength`
- All sliding window filters initialized
- `BytesInFlight = 0`, `Exemptions = 0`
- `MinRtt = UINT64_MAX` (no RTT samples yet)
- Bandwidth filter initialized as empty

**Side Effects**:
- Calls `QuicCongestionControlGetConnection` to access parent connection
- Calls `QuicPathGetDatagramPayloadSize` to get MTU
- Calls `CxPlatTimeUs64()` for timestamp initialization
- Initializes two sliding window extremum filters (bandwidth and ack height)
- May invoke logging via `QuicConnLogOutFlowStats` and `QuicConnLogBbr`

**Error Handling**: None (void return). All operations assumed successful.

**Thread Safety**: Must be called at connection initialization. Not thread-safe during call; caller responsible for synchronization.

**Resource/Ownership**: 
- `Cc` remains owned by caller
- Filter storage uses pre-allocated entries in `Bbr` struct (no dynamic allocation)

---

### 2. BbrCongestionControlGetBandwidth
**Signature**:
```c
uint64_t BbrCongestionControlGetBandwidth(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:192-202`  
**Visibility**: Internal (static linkage implied by lack of export)

**Purpose**: Returns the current estimated bandwidth in (bytes/8) per second.

**Preconditions**:
- `Cc` must be non-NULL and previously initialized via `BbrCongestionControlInitialize`

**Postconditions**:
- Returns current max bandwidth estimate from windowed max filter
- Returns 0 if no bandwidth samples exist yet

**Side Effects**: None (read-only operation)

**Thread Safety**: Read-only, safe if no concurrent modifications to `Cc->Bbr.BandwidthFilter`

---

### 3. BbrCongestionControlGetCongestionWindow
**Signature**:
```c
uint32_t BbrCongestionControlGetCongestionWindow(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:215-236`  
**Visibility**: Internal via function pointer in vtable

**Purpose**: Returns the effective congestion window in bytes based on BBR state.

**Preconditions**:
- `Cc` must be non-NULL and initialized
- `Connection` accessible via `QuicCongestionControlGetConnection`
- `Connection->Paths[0]` must have valid MTU

**Postconditions**:
- In PROBE_RTT: returns minimum congestion window (4 MSS)
- In RECOVERY: returns min(CongestionWindow, RecoveryWindow)
- Otherwise: returns CongestionWindow

**Side Effects**: None (read-only)

**Thread Safety**: Read-only operation

---

### 4. BbrCongestionControlCanSend
**Signature**:
```c
BOOLEAN BbrCongestionControlCanSend(
    _In_ QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:348-354`  
**Visibility**: Internal via function pointer

**Purpose**: Determines if more data can be sent based on congestion window and exemptions.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- Returns TRUE if `BytesInFlight < CongestionWindow` OR `Exemptions > 0`
- Returns FALSE otherwise

**Side Effects**: None

**Thread Safety**: Read-only

---

### 5. BbrCongestionControlSetExemption
**Signature**:
```c
void BbrCongestionControlSetExemption(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint8_t NumPackets
);
```
**Implementation**: `src\core\bbr.c:426-432`  
**Visibility**: Internal via function pointer

**Purpose**: Sets number of packets that can bypass congestion control (for probe packets).

**Preconditions**:
- `Cc` must be initialized
- `NumPackets` typically 0-255

**Postconditions**:
- `Cc->Bbr.Exemptions = NumPackets`

**Side Effects**: Modifies `Exemptions` field

**Thread Safety**: Not thread-safe; caller must synchronize

---

### 6. BbrCongestionControlGetSendAllowance
**Signature**:
```c
uint32_t BbrCongestionControlGetSendAllowance(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint64_t TimeSinceLastSend,
    _In_ BOOLEAN TimeSinceLastSendValid
);
```
**Implementation**: `src\core\bbr.c:618-671`  
**Visibility**: Internal via function pointer

**Purpose**: Calculates the number of bytes that can be sent immediately, considering pacing.

**Preconditions**:
- `Cc` must be initialized
- `TimeSinceLastSend` in microseconds if `TimeSinceLastSendValid` is TRUE
- `Connection` must be accessible

**Postconditions**:
- Returns 0 if `BytesInFlight >= CongestionWindow` (congestion blocked)
- Returns full available window if pacing disabled or conditions not met
- Returns paced allowance if pacing enabled with valid RTT and time

**Side Effects**: None (read-only calculation)

**Thread Safety**: Read-only

---

### 7. BbrCongestionControlOnDataSent
**Signature**:
```c
void BbrCongestionControlOnDataSent(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
);
```
**Implementation**: `src\core\bbr.c:436-460`  
**Visibility**: Internal via function pointer

**Purpose**: Updates state when retransmittable data is sent.

**Preconditions**:
- `Cc` must be initialized
- `NumRetransmittableBytes` must represent actual bytes sent

**Postconditions**:
- `BytesInFlight` increased by `NumRetransmittableBytes`
- `BytesInFlightMax` updated if new maximum reached
- `Exemptions` decremented if > 0
- May update `ExitingQuiescence` flag if transitioning from idle

**Side Effects**:
- May call `QuicSendBufferConnectionAdjust` if new flight max
- Calls `BbrCongestionControlUpdateBlockedState` which may add/remove flow control block

**Thread Safety**: Not thread-safe; caller must synchronize

---

### 8. BbrCongestionControlOnDataInvalidated
**Signature**:
```c
BOOLEAN BbrCongestionControlOnDataInvalidated(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ uint32_t NumRetransmittableBytes
);
```
**Implementation**: `src\core\bbr.c:464-477`  
**Visibility**: Internal via function pointer

**Purpose**: Removes bytes from in-flight count when data is discarded (not lost or acked).

**Preconditions**:
- `Cc` must be initialized
- `NumRetransmittableBytes <= Bbr->BytesInFlight` (ASSERTED)

**Postconditions**:
- `BytesInFlight` decreased by `NumRetransmittableBytes`
- Returns TRUE if became unblocked, FALSE otherwise

**Side Effects**:
- Updates blocked state which may trigger flow control notifications

**Thread Safety**: Not thread-safe

---

### 9. BbrCongestionControlOnDataAcknowledged
**Signature**:
```c
BOOLEAN BbrCongestionControlOnDataAcknowledged(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_ACK_EVENT* AckEvent
);
```
**Implementation**: `src\core\bbr.c:772-903`  
**Visibility**: Internal via function pointer

**Purpose**: Main BBR logic triggered on receiving ACK. Updates bandwidth estimates, RTT, congestion window, and state transitions.

**Preconditions**:
- `Cc` must be initialized
- `AckEvent` must be non-NULL with valid fields:
  - `NumRetransmittableBytes <= Bbr->BytesInFlight`
  - `TimeNow`, `LargestAck`, `LargestSentPacketNumber` must be valid
  - `AckedPackets` linked list must be well-formed (or NULL)

**Postconditions**:
- `BytesInFlight` decreased by `AckEvent->NumRetransmittableBytes`
- Bandwidth filter updated with delivery rate samples
- MinRtt updated if new sample is lower or previous expired
- Round trip counter incremented if new round detected
- Recovery state updated if in recovery
- BBR state may transition (STARTUP → DRAIN → PROBE_BW → PROBE_RTT)
- Congestion window updated
- Returns TRUE if became unblocked, FALSE otherwise

**Side Effects**:
- Calls `BbrBandwidthFilterOnPacketAcked` which updates bandwidth sliding window
- May call `BbrCongestionControlTransitTo*` functions for state transitions
- Calls `BbrCongestionControlUpdateAckAggregation`
- Calls `BbrCongestionControlUpdateCongestionWindow`
- May invoke `BbrCongestionControlIndicateConnectionEvent` if stats events enabled
- Updates blocked state

**Thread Safety**: Not thread-safe

**Key Contract Points**:
- Handles both implicit ACKs (`IsImplicit = TRUE`) and explicit ACKs
- Respects app-limited samples (won't use for bandwidth estimation unless conditions met)
- Never decreases MinRtt except on expiration (10 second timeout)

---

### 10. BbrCongestionControlOnDataLost
**Signature**:
```c
void BbrCongestionControlOnDataLost(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_LOSS_EVENT* LossEvent
);
```
**Implementation**: `src\core\bbr.c:907-965`  
**Visibility**: Internal via function pointer

**Purpose**: Handles packet loss by entering recovery and reducing recovery window.

**Preconditions**:
- `Cc` must be initialized
- `LossEvent` must be non-NULL with:
  - `NumRetransmittableBytes > 0` (ASSERTED)
  - `NumRetransmittableBytes <= Bbr->BytesInFlight` (ASSERTED)

**Postconditions**:
- Enters or continues RECOVERY state (CONSERVATIVE then GROWTH)
- `BytesInFlight` decreased by lost bytes
- `RecoveryWindow` set (to BytesInFlight for new recovery, reduced for continued)
- `EndOfRecovery` set to `LargestSentPacketNumber`
- If `PersistentCongestion`: `RecoveryWindow` reset to minimum

**Side Effects**:
- Increments `Connection->Stats.Send.CongestionCount`
- May increment `PersistentCongestionCount`
- Logs congestion event via `QuicTraceEvent`
- Updates blocked state
- Calls `QuicConnLogBbr` for logging

**Thread Safety**: Not thread-safe

**Error Handling**: None; all operations expected to succeed

---

### 11. BbrCongestionControlOnSpuriousCongestionEvent
**Signature**:
```c
BOOLEAN BbrCongestionControlOnSpuriousCongestionEvent(
    _In_ QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:969-975`  
**Visibility**: Internal via function pointer

**Purpose**: Called when loss detection was spurious (data actually ACKed, not lost).

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- BBR always returns FALSE (does not revert on spurious detection)

**Side Effects**: None

**Thread Safety**: Read-only

---

### 12. BbrCongestionControlReset
**Signature**:
```c
void BbrCongestionControlReset(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ BOOLEAN FullReset
);
```
**Implementation**: `src\core\bbr.c:998-1063`  
**Visibility**: Internal via function pointer

**Purpose**: Resets BBR state to initial conditions, either fully or partially.

**Preconditions**:
- `Cc` must be initialized
- `Connection` must be accessible

**Postconditions**:
- Congestion window reset to initial value
- All state variables reset to initialization values
- Bandwidth and ack height filters reset
- If `FullReset = TRUE`: `BytesInFlight = 0`
- If `FullReset = FALSE`: `BytesInFlight` unchanged

**Side Effects**:
- Calls `QuicSlidingWindowExtremumReset` on both filters
- Calls logging functions

**Thread Safety**: Not thread-safe

---

### 13. BbrCongestionControlSetAppLimited
**Signature**:
```c
void BbrCongestionControlSetAppLimited(
    _In_ QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:979-994`  
**Visibility**: Internal via function pointer

**Purpose**: Marks that bandwidth is currently limited by application (not network).

**Preconditions**:
- `Cc` must be initialized
- `Connection->LossDetection.LargestSentPacketNumber` must be valid

**Postconditions**:
- If `BytesInFlight <= CongestionWindow`: sets `AppLimited = TRUE` and `AppLimitedExitTarget`
- Otherwise: no change (still congestion limited)

**Side Effects**: None

**Thread Safety**: Not thread-safe

---

### 14. BbrCongestionControlIsAppLimited
**Signature**:
```c
BOOLEAN BbrCongestionControlIsAppLimited(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:272-277`  
**Visibility**: Internal via function pointer

**Purpose**: Returns whether BBR is currently in app-limited mode.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- Returns current value of `Bbr->BandwidthFilter.AppLimited`

**Side Effects**: None

**Thread Safety**: Read-only

---

### 15. BbrCongestionControlGetExemptions
**Signature**:
```c
uint8_t BbrCongestionControlGetExemptions(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:417-422`  
**Visibility**: Internal via function pointer

**Purpose**: Returns current exemption count.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- Returns `Bbr->Exemptions`

**Side Effects**: None

**Thread Safety**: Read-only

---

### 16. BbrCongestionControlGetBytesInFlightMax
**Signature**:
```c
uint32_t BbrCongestionControlGetBytesInFlightMax(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:408-413`  
**Visibility**: Internal via function pointer

**Purpose**: Returns maximum bytes in flight achieved.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- Returns `Bbr->BytesInFlightMax`

**Side Effects**: None

**Thread Safety**: Read-only

---

### 17. BbrCongestionControlLogOutFlowStatus
**Signature**:
```c
void BbrCongestionControlLogOutFlowStatus(
    _In_ const QUIC_CONGESTION_CONTROL* Cc
);
```
**Implementation**: `src\core\bbr.c:357-377`  
**Visibility**: Internal via function pointer

**Purpose**: Logs current congestion control state for diagnostics.

**Preconditions**:
- `Cc` must be initialized

**Postconditions**:
- Emits trace event with current state

**Side Effects**:
- Calls `QuicTraceEvent` (logging)

**Thread Safety**: Read-only

---

### 18. BbrCongestionControlGetNetworkStatistics
**Signature**:
```c
void BbrCongestionControlGetNetworkStatistics(
    _In_ const QUIC_CONNECTION* const Connection,
    _In_ const QUIC_CONGESTION_CONTROL* const Cc,
    _Out_ QUIC_NETWORK_STATISTICS* NetworkStatistics
);
```
**Implementation**: `src\core\bbr.c:304-319`  
**Visibility**: Internal via function pointer

**Purpose**: Fills in network statistics structure for application.

**Preconditions**:
- `Connection` must be non-NULL and valid
- `Cc` must be initialized
- `NetworkStatistics` must be non-NULL

**Postconditions**:
- `NetworkStatistics` populated with current values

**Side Effects**: None

**Thread Safety**: Read-only

---

## B) Type/Object Invariants

### QUIC_CONGESTION_CONTROL_BBR Invariants

1. **State Machine Integrity**:
   - `BbrState` must be one of: `BBR_STATE_STARTUP`, `BBR_STATE_DRAIN`, `BBR_STATE_PROBE_BW`, `BBR_STATE_PROBE_RTT`
   - Valid transitions:
     - STARTUP → DRAIN (when `BtlbwFound = TRUE`)
     - DRAIN → PROBE_BW (when `BytesInFlight <= BDP`)
     - PROBE_BW → PROBE_RTT (when `RttSampleExpired = TRUE` and not exiting quiescence)
     - PROBE_RTT → PROBE_BW or STARTUP (after probe duration)

2. **Recovery State Integrity**:
   - `RecoveryState` must be one of: `NOT_RECOVERY`, `CONSERVATIVE`, `GROWTH`
   - `EndOfRecoveryValid` must be TRUE when `RecoveryState != NOT_RECOVERY`

3. **Congestion Window Bounds**:
   - `CongestionWindow >= kMinCwndInMss * DatagramPayloadLength` (minimum 4 MSS)
   - `InitialCongestionWindow = InitialCongestionWindowPackets * DatagramPayloadLength`

4. **BytesInFlight Consistency**:
   - `BytesInFlight >= 0` (always)
   - `BytesInFlight <= BytesInFlightMax` (max is monotonically increasing)
   - Decreased only on ACK, loss, or invalidation
   - Increased only on data sent

5. **MinRtt Validity**:
   - If `MinRttTimestampValid = FALSE`: `MinRtt = UINT64_MAX` (no sample yet)
   - If `MinRttTimestampValid = TRUE`: `MinRtt` is valid measurement, `MinRttTimestamp` is sample time

6. **Round Trip Tracking**:
   - `EndOfRoundTrip` valid only if `EndOfRoundTripValid = TRUE`
   - `RoundTripCounter` monotonically increases

7. **Filter Consistency**:
   - `BandwidthFilter.WindowedMaxFilter` must maintain valid sliding window state
   - `MaxAckHeightFilter` must maintain valid sliding window state
   - Both use pre-allocated entry arrays (no dynamic allocation)

8. **Pacing Gain Cycle**:
   - `PacingCycleIndex` must be in `[0, GAIN_CYCLE_LENGTH)` (0-7)
   - `PacingGain = kPacingGain[PacingCycleIndex]`

9. **Exemptions Range**:
   - `Exemptions` is uint8_t, range [0, 255]
   - Decremented on send, never negative

---

## C) Environment Invariants

1. **Connection Context**:
   - BBR requires embedding in `QUIC_CONNECTION` structure
   - `QuicCongestionControlGetConnection(Cc)` must always return valid connection
   - Uses `CXPLAT_CONTAINING_RECORD` macro for pointer arithmetic

2. **Path Initialization**:
   - `Connection->Paths[0]` must exist and be initialized
   - `QuicPathGetDatagramPayloadSize(&Connection->Paths[0])` must return valid MTU

3. **Settings Validity**:
   - `Settings->InitialWindowPackets` must be > 0
   - `Settings->PacingEnabled` determines pacing behavior

4. **Time Source**:
   - `CxPlatTimeUs64()` provides monotonic microsecond timestamps
   - All time comparisons use `CxPlatTimeAtOrBefore64` and `CxPlatTimeDiff64`

5. **No Dynamic Allocation**:
   - All filter storage pre-allocated in `QUIC_CONGESTION_CONTROL_BBR` structure
   - No malloc/free within BBR implementation

6. **Logging/Tracing**:
   - May call `QuicTraceEvent` for diagnostics (ETW/LTTng)
   - Logging is non-blocking and cannot fail

7. **Single-Threaded Assumption**:
   - BBR functions not designed for concurrent access
   - Caller (connection layer) responsible for synchronization
   - Read-only functions safe for concurrent reads

---

## D) Dependency Map

### Direct Dependencies (called by BBR)

**From Platform Abstraction (`src/platform/`)**:
- `CxPlatTimeUs64()` - Get current time in microseconds
- `CxPlatTimeDiff64()` - Calculate time difference
- `CxPlatTimeAtOrBefore64()` - Compare timestamps
- `CxPlatRandom()` - Generate random value (for pacing cycle start)

**From Core (`src/core/`)**:
- `QuicCongestionControlGetConnection()` - Get parent connection from Cc
- `QuicPathGetDatagramPayloadSize()` - Get MTU from path
- `QuicSendBufferConnectionAdjust()` - Notify send buffer of capacity change
- `QuicConnAddOutFlowBlockedReason()` - Mark connection as congestion blocked
- `QuicConnRemoveOutFlowBlockedReason()` - Unblock connection
- `QuicConnIndicateEvent()` - Send network statistics to application
- `QuicConnLogOutFlowStats()` - Log flow control state

**Sliding Window Extremum (`src/core/sliding_window_extremum.h`)**:
- `QuicSlidingWindowExtremumInitialize()` - Initialize filter
- `QuicSlidingWindowExtremumReset()` - Reset filter
- `QuicSlidingWindowExtremumGet()` - Get current extremum
- `QuicSlidingWindowExtremumUpdateMax()` - Update with new sample

**Tracing/Logging**:
- `QuicTraceEvent()` - Emit trace events
- `QuicTraceLogConnVerbose()` - Verbose connection logging
- `QuicConnLogBbr()` - Log BBR-specific state

### Called By (callers of BBR)

**Connection Layer** (`src/core/connection.c`):
- Calls `BbrCongestionControlInitialize()` during connection setup
- Invokes function pointers in response to network events

**Loss Detection** (`src/core/loss_detection.c`):
- Calls `OnDataAcknowledged` when ACKs received
- Calls `OnDataLost` when packets declared lost

**Send Path** (`src/core/send.c`):
- Calls `CanSend` before packet transmission
- Calls `GetSendAllowance` for pacing
- Calls `OnDataSent` after packet sent
- Calls `SetAppLimited` when no data to send

### Indirect Dependencies

- **Connection State**: Relies on `Connection->Stats`, `Connection->Send`, `Connection->SendBuffer`
- **Path State**: Relies on `Connection->Paths[0].SmoothedRtt`, `OneWayDelay`, `GotFirstRttSample`
- **Loss Detection**: Relies on `Connection->LossDetection.LargestSentPacketNumber`

---

## E) Key Constants and Thresholds

| Constant | Value | Purpose |
|----------|-------|---------|
| `BW_UNIT` | 8 | Bandwidth measured as (bytes / 8) per second |
| `GAIN_UNIT` | 256 | Gain is measured as (1 / 256) |
| `kMinCwndInMss` | 4 | Minimum congestion window in MSS |
| `kHighGain` | 739 | 2/ln(2) * 256 for STARTUP |
| `kDrainGain` | 87 | 1/kHighGain for DRAIN |
| `kCwndGain` | 512 | 2 * GAIN_UNIT for PROBE_BW |
| `kStartupGrowthTarget` | 320 | 1.25 * GAIN_UNIT |
| `kStartupSlowGrowRoundLimit` | 3 | Rounds to stay in STARTUP |
| `kProbeRttTimeInUs` | 200000 | 200ms minimum ProbeRTT duration |
| `kBbrMinRttExpirationInMicroSecs` | 10000000 | 10 seconds |
| `kBbrMaxBandwidthFilterLen` | 10 | Bandwidth filter window |
| `kBbrMaxAckHeightFilterLen` | 10 | Ack aggregation filter window |
| `GAIN_CYCLE_LENGTH` | 8 | ProbeBw pacing gain cycle length |

---

## F) Contract-Unreachable Code Analysis

### Potentially Unreachable Lines

After analysis, all lines in `bbr.c` appear reachable through valid public API sequences. However, certain conditions are rare:

1. **Lines 121-122** (AppLimitedExitTarget check):
   - Reachable via: Send data → SetAppLimited → Send more → OnDataAcknowledged
   - Requires specific sequence but contract-valid

2. **Lines 163-168** (No LastAckedPacketInfo, time check):
   - Reachable via: First packet ACK with no prior ACK info
   - Valid initial state

3. **Lines 542-550** (ProbeRTT completion with BtlbwFound check):
   - Reachable via: Enter ProbeRTT → wait → ACK → exit
   - Both branches reachable depending on whether bottleneck found

**Conclusion**: No contract-unreachable lines identified. All code paths reachable through valid public API usage.

---

## G) Testing Strategy Implications

### Scenario Categories Needed

1. **Initialization Scenarios**:
   - Fresh initialization with various settings
   - Re-initialization (reset)

2. **State Transition Scenarios**:
   - STARTUP → DRAIN → PROBE_BW
   - PROBE_BW → PROBE_RTT → back to PROBE_BW/STARTUP
   - Recovery entry/exit

3. **Bandwidth Estimation Scenarios**:
   - Building bandwidth estimates from ACKs
   - App-limited vs network-limited
   - Handling expired samples

4. **RTT Tracking Scenarios**:
   - First RTT sample
   - MinRTT updates
   - MinRTT expiration

5. **Pacing Scenarios**:
   - Pacing enabled vs disabled
   - Various time deltas
   - Edge cases (invalid time, no RTT sample)

6. **Congestion Window Scenarios**:
   - Growth in STARTUP
   - Target CWND calculation in PROBE_BW
   - Minimum CWND enforcement

7. **Loss Handling Scenarios**:
   - Entering recovery
   - Recovery growth
   - Persistent congestion

8. **Exemption Scenarios**:
   - Setting exemptions
   - Consuming exemptions during send

9. **Round Trip Detection**:
   - New round trip detection
   - State changes on new round

---

This RCI provides the foundation for generating comprehensive, contract-safe tests for BBR.
