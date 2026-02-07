# Repository Contract Index (RCI) for CUBIC Congestion Control

## Component Overview
**Component**: CUBIC Congestion Control  
**Source Files**: `src/core/cubic.c`, `src/core/cubic.h`  
**Purpose**: Implements the CUBIC congestion control algorithm (RFC 8312bis) for QUIC connections  
**Test Harness**: `src/core/unittest/CubicTest.cpp`

## Public API Inventory

### 1. `CubicCongestionControlInitialize`
**Signature**: `void CubicCongestionControlInitialize(QUIC_CONGESTION_CONTROL* Cc, const QUIC_SETTINGS_INTERNAL* Settings)`  
**Declaration**: `src/core/cubic.h:121-124`  
**Purpose**: Initializes CUBIC congestion control state for a connection  
**Preconditions**:
- `Cc` must point to a valid `QUIC_CONGESTION_CONTROL` structure embedded in a `QUIC_CONNECTION`
- `Settings` must point to valid initialized settings with `InitialWindowPackets` and `SendIdleTimeoutMs`
- Connection's `Paths[0].Mtu` must be set to a valid MTU value
- Connection's `Send.NextPacketNumber` must be initialized

**Postconditions**:
- All 17 function pointers in `Cc` are set to CUBIC implementations
- `Cubic.CongestionWindow` = MTU * `InitialWindowPackets`
- `Cubic.SlowStartThreshold` = `UINT32_MAX`
- `Cubic.BytesInFlightMax` = `CongestionWindow / 2`
- `Cubic.HyStartState` = `HYSTART_NOT_STARTED`
- All boolean flags (`HasHadCongestionEvent`, `IsInRecovery`, etc.) = FALSE
- `Cubic.MinRttInCurrentRound` = `UINT64_MAX`
- `Cubic.CWndSlowStartGrowthDivisor` = 1

**Side Effects**: Logs connection flow stats and CUBIC state via tracing  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL; not thread-safe (caller must synchronize)

### 2. `CubicCongestionControlCanSend` (via function pointer)
**Signature**: `BOOLEAN (*QuicCongestionControlCanSend)(QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Determines if congestion control allows sending data  
**Preconditions**: CUBIC must be initialized  
**Postconditions**: Returns TRUE if `BytesInFlight < CongestionWindow OR Exemptions > 0`  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 3. `CubicCongestionControlSetExemption` (via function pointer)
**Signature**: `void (*QuicCongestionControlSetExemption)(QUIC_CONGESTION_CONTROL* Cc, uint8_t NumPackets)`  
**Purpose**: Sets exemption count to allow sending packets bypassing congestion window  
**Preconditions**: CUBIC must be initialized  
**Postconditions**: `Cubic.Exemptions` = `NumPackets`  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 4. `CubicCongestionControlReset` (via function pointer)
**Signature**: `void (*QuicCongestionControlReset)(QUIC_CONGESTION_CONTROL* Cc, BOOLEAN FullReset)`  
**Purpose**: Resets CUBIC state to initial conditions  
**Preconditions**: CUBIC must be initialized  
**Postconditions**:
- Resets window, thresholds, and HyStart state
- If `FullReset` is TRUE, also resets `BytesInFlight` to 0
- `SlowStartThreshold` = `UINT32_MAX`
- `CongestionWindow` = MTU * `InitialWindowPackets`
- `HyStartState` = `HYSTART_NOT_STARTED`

**Side Effects**: Logs stats  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 5. `CubicCongestionControlGetSendAllowance` (via function pointer)
**Signature**: `uint32_t (*QuicCongestionControlGetSendAllowance)(QUIC_CONGESTION_CONTROL* Cc, uint64_t TimeSinceLastSend, BOOLEAN TimeSinceLastSendValid)`  
**Purpose**: Calculates how many bytes can be sent, considering pacing  
**Preconditions**: CUBIC must be initialized  
**Postconditions**: Returns send allowance in bytes (0 if congestion blocked)  
**Logic**:
- If `BytesInFlight >= CongestionWindow`: returns 0
- If pacing disabled or no RTT sample: returns `CongestionWindow - BytesInFlight`
- If pacing enabled: calculates paced allowance based on estimated window and time since last send

**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 6. `CubicCongestionControlOnDataSent` (via function pointer)
**Signature**: `void (*QuicCongestionControlOnDataSent)(QUIC_CONGESTION_CONTROL* Cc, uint32_t NumRetransmittableBytes)`  
**Purpose**: Updates state when data is sent  
**Preconditions**: CUBIC must be initialized, `NumRetransmittableBytes` > 0  
**Postconditions**:
- `BytesInFlight` += `NumRetransmittableBytes`
- Updates `BytesInFlightMax` if new max reached
- Decrements `Exemptions` if > 0
- Adjusts `LastSendAllowance`

**Side Effects**: May trigger flow control blocked/unblocked events  
**Thread Safety**: Requires IRQL <= PASSIVE_LEVEL

### 7. `CubicCongestionControlOnDataInvalidated` (via function pointer)
**Signature**: `BOOLEAN (*QuicCongestionControlOnDataInvalidated)(QUIC_CONGESTION_CONTROL* Cc, uint32_t NumRetransmittableBytes)`  
**Purpose**: Updates state when previously sent data is invalidated (e.g., packet discarded)  
**Preconditions**: `BytesInFlight >= NumRetransmittableBytes`  
**Postconditions**: `BytesInFlight` -= `NumRetransmittableBytes`  
**Returns**: TRUE if became unblocked  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 8. `CubicCongestionControlOnDataAcknowledged` (via function pointer)
**Signature**: `BOOLEAN (*QuicCongestionControlOnDataAcknowledged)(QUIC_CONGESTION_CONTROL* Cc, const QUIC_ACK_EVENT* AckEvent)`  
**Purpose**: Main CUBIC logic - updates congestion window on ACK  
**Preconditions**: CUBIC initialized, `AckEvent` valid  
**Postconditions**:
- `BytesInFlight` reduced by acked bytes
- In Slow Start: `CongestionWindow` increases by `BytesAcked / CWndSlowStartGrowthDivisor`
- In Congestion Avoidance: CUBIC or AIMD formula applied
- HyStart++ logic processes RTT samples
- May exit recovery if ack for post-recovery packet
- Updates `TimeOfLastAck`

**Returns**: TRUE if became unblocked  
**Side Effects**: May emit network statistics event if enabled  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 9. `CubicCongestionControlOnDataLost` (via function pointer)
**Signature**: `void (*QuicCongestionControlOnDataLost)(QUIC_CONGESTION_CONTROL* Cc, const QUIC_LOSS_EVENT* LossEvent)`  
**Purpose**: Handles packet loss detection  
**Preconditions**: CUBIC initialized, `LossEvent` valid  
**Postconditions**:
- If loss is new congestion event: reduces window by BETA (0.7)
- `BytesInFlight` reduced by lost bytes
- Enters recovery state
- Sets `RecoverySentPacketNumber`
- If persistent congestion: resets to minimum window

**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 10. `CubicCongestionControlOnEcn` (via function pointer)
**Signature**: `void (*QuicCongestionControlOnEcn)(QUIC_CONGESTION_CONTROL* Cc, const QUIC_ECN_EVENT* EcnEvent)`  
**Purpose**: Handles ECN (Explicit Congestion Notification) signals  
**Preconditions**: CUBIC initialized, `EcnEvent` valid  
**Postconditions**: Similar to loss event but triggered by ECN mark  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 11. `CubicCongestionControlOnSpuriousCongestionEvent` (via function pointer)
**Signature**: `BOOLEAN (*QuicCongestionControlOnSpuriousCongestionEvent)(QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Reverts CUBIC state when congestion event determined to be spurious  
**Preconditions**: CUBIC in recovery  
**Postconditions**: Restores previous window, thresholds, K value, exits recovery  
**Returns**: FALSE if not in recovery, TRUE if became unblocked  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 12. `CubicCongestionControlLogOutFlowStatus` (via function pointer)
**Signature**: `void (*QuicCongestionControlLogOutFlowStatus)(const QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Logs current flow statistics for diagnostics  
**Preconditions**: CUBIC initialized  
**Side Effects**: Emits trace event  
**Thread Safety**: No restrictions

### 13. `CubicCongestionControlGetExemptions` (via function pointer)
**Signature**: `uint8_t (*QuicCongestionControlGetExemptions)(const QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Returns current exemption count  
**Preconditions**: CUBIC initialized  
**Postconditions**: Returns `Cubic.Exemptions`  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 14. `CubicCongestionControlGetBytesInFlightMax` (via function pointer)
**Signature**: `uint32_t (*QuicCongestionControlGetBytesInFlightMax)(const QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Returns maximum bytes in flight observed  
**Preconditions**: CUBIC initialized  
**Postconditions**: Returns `Cubic.BytesInFlightMax`  
**Thread Safety**: No restrictions

### 15. `CubicCongestionControlGetCongestionWindow` (via function pointer)
**Signature**: `uint32_t (*QuicCongestionControlGetCongestionWindow)(const QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Returns current congestion window  
**Preconditions**: CUBIC initialized  
**Postconditions**: Returns `Cubic.CongestionWindow`  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 16. `CubicCongestionControlIsAppLimited` (via function pointer)
**Signature**: `BOOLEAN (*QuicCongestionControlIsAppLimited)(const QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Checks if connection is application-limited  
**Preconditions**: CUBIC initialized  
**Postconditions**: Always returns FALSE (not implemented for CUBIC)  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 17. `CubicCongestionControlSetAppLimited` (via function pointer)
**Signature**: `void (*QuicCongestionControlSetAppLimited)(QUIC_CONGESTION_CONTROL* Cc)`  
**Purpose**: Marks connection as application-limited  
**Preconditions**: CUBIC initialized  
**Postconditions**: No-op (not implemented for CUBIC)  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 18. `CubicCongestionControlGetNetworkStatistics` (via function pointer)
**Signature**: `void (*QuicCongestionControlGetNetworkStatistics)(const QUIC_CONNECTION* Connection, const QUIC_CONGESTION_CONTROL* Cc, QUIC_NETWORK_STATISTICS* NetworkStatistics)`  
**Purpose**: Populates network statistics structure  
**Preconditions**: All pointers valid, CUBIC initialized  
**Postconditions**: Fills `NetworkStatistics` with current metrics  
**Thread Safety**: Requires IRQL <= DISPATCH_LEVEL

### 19. `CubeRoot` (Internal but algorithmically important)
**Signature**: `uint32_t CubeRoot(uint32_t Radicand)`  
**Purpose**: Computes integer cube root using shifting algorithm  
**Preconditions**: None  
**Postconditions**: Returns cube root of input  
**Note**: Used internally for K calculation in CUBIC formula

### 20. `QuicConnLogCubic` (Internal helper)
**Purpose**: Logs CUBIC-specific state  
**Side Effects**: Emits trace event

### 21. Internal HyStart++ Helpers
- `CubicCongestionHyStartChangeState`: Manages HyStart state machine
- `CubicCongestionHyStartResetPerRttRound`: Resets per-RTT HyStart tracking
- `CubicCongestionControlUpdateBlockedState`: Updates flow control blocked state

## Type/Object Invariants

### `QUIC_CONGESTION_CONTROL_CUBIC` Invariants
1. **Window Constraints**: `CongestionWindow >= MTU * 2` (minimum window size)
2. **Bytes In Flight**: `0 <= BytesInFlight <= BytesInFlightMax`
3. **Slow Start**: `CongestionWindow < SlowStartThreshold` implies slow start mode
4. **Recovery State**: If `IsInRecovery`, then `HasHadCongestionEvent` must be TRUE
5. **HyStart State Machine**: 
   - States: `HYSTART_NOT_STARTED` → `HYSTART_ACTIVE` → `HYSTART_DONE`
   - Cannot transition backwards except `HYSTART_ACTIVE` → `HYSTART_NOT_STARTED` on RTT decrease
6. **Time Validity**: If `TimeOfLastAckValid`, then `TimeOfLastAck` contains valid timestamp
7. **Window Max Relations**: `WindowLastMax <= WindowMax` after convergence
8. **AIMD Accumulator**: `0 <= AimdAccumulator < AimdWindow`

### State Machine: CUBIC Congestion States

```
┌─────────────────┐
│  Initialization │
│  (SlowStart)    │
└────────┬────────┘
         │ ACKs received
         ▼
┌─────────────────┐        ┌──────────────────┐
│   Slow Start    │───────→│ Congestion       │
│   (CWND < SST)  │  Loss/ │ Avoidance        │
│                 │  ECN   │ (CWND >= SST)    │
└────────┬────────┘        └────────┬─────────┘
         │                          │
         │ Loss/ECN                 │ Loss/ECN
         ▼                          ▼
┌─────────────────────────────────────────────┐
│         Recovery State                       │
│  (IsInRecovery = TRUE)                      │
│  - Window reduced by BETA (0.7)             │
│  - No window increases until recovery exit   │
└────────┬────────────────────────────────────┘
         │ ACK for post-recovery packet
         ▼
    [Return to SlowStart or CongAvoid]

Special: Persistent Congestion
┌─────────────────────────────────────────────┐
│   Persistent Congestion                      │
│  (IsInPersistentCongestion = TRUE)          │
│  - CWND = 2 * MTU (minimum)                 │
│  - All windows reduced                       │
└─────────────────────────────────────────────┘
```

### State Machine: HyStart++ States
```
┌──────────────────┐
│ HYSTART_NOT_     │
│ _STARTED         │◄────────────┐
└────────┬─────────┘             │
         │ RTT increase          │ RTT decrease
         │ detected              │
         ▼                       │
┌──────────────────┐             │
│ HYSTART_ACTIVE   │─────────────┘
│ (Conservative SS)│
└────────┬─────────┘
         │ N rounds elapsed
         ▼
┌──────────────────┐
│ HYSTART_DONE     │
│ (Regular CA)     │
└──────────────────┘
```

## Environment Invariants

1. **Connection Context**: CUBIC structure must be embedded in a valid `QUIC_CONNECTION` at fixed offset
   - `QuicCongestionControlGetConnection()` uses `CXPLAT_CONTAINING_RECORD` pointer arithmetic
2. **Path Validity**: `Connection->Paths[0]` must be initialized with valid MTU
3. **Tracing Availability**: Logging/tracing infrastructure must be available (ETW on Windows, LTTng on Linux)
4. **No Memory Allocation**: All CUBIC state is pre-allocated within connection structure; no dynamic allocation
5. **Single-Threaded per Connection**: While CUBIC is thread-safe at DISPATCH_LEVEL, a single connection's CUBIC state should not be modified concurrently

## Dependency Map

### Key Dependencies
- **Platform Abstraction**: Uses `CxPlat*` functions for time, logging, memory
- **Connection APIs**: Relies on `QuicConn*` functions for logging, flow control
- **Path Management**: Accesses `Connection->Paths[0]` for MTU, RTT
- **Send Buffer**: Interacts with `Connection->SendBuffer` for flow control

### Public API Call Relationships
1. **Initialization Flow**: `CubicCongestionControlInitialize` → sets all 17 function pointers
2. **Send Flow**: `CanSend` → `GetSendAllowance` → `OnDataSent`
3. **ACK Flow**: `OnDataAcknowledged` → may call internal HyStart helpers
4. **Loss Flow**: `OnDataLost` → `OnCongestionEvent` (internal)
5. **Recovery**: `OnSpuriousCongestionEvent` reverses effects of congestion event
6. **Exemptions**: `SetExemption` → affects `CanSend` decision

### Callback Registration
CUBIC registers itself by setting function pointers in `QUIC_CONGESTION_CONTROL` structure. The core QUIC engine calls these via function pointers.

## Contract-Unreachable Code Regions

After analyzing the code, these regions may be unreachable through public APIs alone without contract violations:

1. **Overflow Protection in GetSendAllowance** (lines 234-236): Requires arithmetic overflow which should not occur with valid inputs
2. **Internal Assertions**: `CXPLAT_DBG_ASSERT` statements are debug-only and fire only on contract violations
3. **Spurious Congestion without Recovery** (line 794-796): `OnSpuriousCongestionEvent` returns FALSE if not in recovery; this is a valid contract case but the FALSE path is trivial

## Testing Strategy

### Scenario Categories
1. **Initialization**: Various MTU/window/timeout combinations
2. **Slow Start**: Window growth during slow start with/without HyStart++
3. **Congestion Avoidance**: CUBIC formula, AIMD fallback, window growth limits
4. **Loss Handling**: Regular loss, persistent congestion, multiple losses
5. **ECN Handling**: ECN-triggered congestion events
6. **Recovery**: Entry, duration, exit from recovery
7. **Spurious Congestion**: Reverting state on spurious loss
8. **HyStart++**: State transitions, RTT sampling, Conservative Slow Start
9. **Pacing**: Send allowance calculation with pacing enabled
10. **Exemptions**: Bypassing congestion window
11. **Reset**: Full and partial resets
12. **Getters**: All read-only query functions
13. **Edge Cases**: Boundary values, arithmetic limits, state transitions

### Coverage Goals
- **100% line coverage** of `cubic.c`
- **All public API functions** exercised via function pointers
- **All state machine transitions** tested
- **All HyStart++ transitions** tested
