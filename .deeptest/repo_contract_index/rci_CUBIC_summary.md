# Repository Contract Index: CUBIC Congestion Control

**Component**: CUBIC  
**Source**: `src/core/cubic.c` (940 lines)  
**Header**: `src/core/cubic.h`  
**Algorithm**: RFC 8312bis (CUBIC) with HyStart++ (draft-ietf-tcpm-hystartplusplus)

---

## Public API Surface

### Only Public Function: `CubicCongestionControlInitialize`

``````c
void CubicCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings
);
``````

**What it does**: Initializes CUBIC congestion control by setting up all 17 function pointers and initial state.

**Preconditions**:
- `Cc` must be embedded in valid `QUIC_CONNECTION` (uses `CXPLAT_CONTAINING_RECORD`)
- `Settings` must contain valid `InitialWindowPackets` and `SendIdleTimeoutMs`
- `Connection->Paths[0]` must be initialized with valid MTU

**Postconditions**:
- All function pointers set to CUBIC implementations
- `CongestionWindow = MTU * InitialWindowPackets`
- `SlowStartThreshold = UINT32_MAX` (not yet in congestion avoidance)
- `HyStartState = HYSTART_NOT_STARTED`
- `BytesInFlightMax = CongestionWindow / 2`

**Error handling**: None - assumes valid inputs (caller responsibility)

---

## Internal Operations (via Function Pointers)

After initialization, all CUBIC operations are invoked via function pointers set in `Cc`:

| Operation | Purpose | Complexity |
|-----------|---------|------------|
| **CanSend** | Check if can send: `BytesInFlight < CongestionWindow OR Exemptions > 0` | Simple |
| **SetExemption** | Allow N packets to bypass CongestionWindow | Simple |
| **Reset** | Reset to initial state (optionally clear BytesInFlight) | Simple |
| **GetSendAllowance** | Calculate send allowance considering pacing | Medium |
| **OnDataSent** | Update BytesInFlight, decrement Exemptions and LastSendAllowance | Simple |
| **OnDataInvalidated** | Decrement BytesInFlight when packet cancelled | Simple |
| **OnDataAcknowledged** | **Core CUBIC algorithm** (~280 lines): HyStart++, slow start, congestion avoidance | **Complex** |
| **OnDataLost** | Trigger congestion event on loss | Medium |
| **OnEcn** | Trigger congestion event on ECN signal | Medium |
| **OnSpuriousCongestionEvent** | Revert to saved state if congestion was spurious | Simple |
| **LogOutFlowStatus** | Logging | Simple |
| **GetExemptions** | Return Exemptions | Trivial |
| **GetBytesInFlightMax** | Return BytesInFlightMax | Trivial |
| **IsAppLimited** | Always returns FALSE (not implemented) | Trivial |
| **SetAppLimited** | No-op (not implemented) | Trivial |
| **GetCongestionWindow** | Return CongestionWindow | Trivial |
| **GetNetworkStatistics** | Populate stats structure | Simple |

---

## State Machines

### Recovery State Machine

``````
┌────────────────┐
│ NOT_IN_RECOVERY│
│ (normal)       │
└───────┬────────┘
        │
        │ OnDataLost / OnEcn
        │ (triggers congestion event)
        ▼
   ┌────────────────────┐
   │   IN_RECOVERY      │──────┐
   │ (reducing window)  │      │ IsPersistentCongestion=TRUE
   └────────┬───────────┘      │
            │                  ▼
            │         ┌─────────────────────────┐
            │         │ IN_PERSISTENT_CONGESTION│
            │         │ (severe: reset to 2 MTU)│
            │         └────────┬────────────────┘
            │                  │
            │ ACK > RecoverySentPacketNumber
            └──────────────────┴──────────────────────►
                                                   (exit recovery)
``````

**State Invariants**:
- **NOT_IN_RECOVERY**: `IsInRecovery = FALSE`, window growing normally
- **IN_RECOVERY**: `IsInRecovery = TRUE`, `HasHadCongestionEvent = TRUE`, `RecoverySentPacketNumber` valid
- **IN_PERSISTENT_CONGESTION**: Additionally `IsInPersistentCongestion = TRUE`, window reset to 2*MTU

### HyStart++ State Machine

``````
┌─────────────────┐
│HYSTART_NOT_STARTED│
│ (regular SS)    │
│ divisor = 1     │
└────┬────────────┘
     │
     │ RTT increase detected
     │ (MinRtt_current >= MinRtt_last + Eta)
     ▼
┌──────────────────┐              ┌────────────────┐
│ HYSTART_ACTIVE   │──────────────│  HYSTART_DONE  │
│(conservative SS) │  N rounds    │ (in CA or done)│
│ divisor = 2      │  elapsed     │  divisor = 1   │
└────┬─────────────┘              └────────────────┘
     │                                     ▲
     │ RTT decreased                       │
     │ (MinRtt_current < Baseline)         │
     └─────────────────────────────────────┘
                   │
                   │ Congestion event
                   └──────────────────────────► HYSTART_DONE
``````

**State Invariants**:
- **HYSTART_NOT_STARTED**: 
  - `CWndSlowStartGrowthDivisor = 1` (normal exponential growth)
  - `CongestionWindow < SlowStartThreshold`
  
- **HYSTART_ACTIVE** (Conservative Slow Start):
  - `CWndSlowStartGrowthDivisor = 2` (half the growth rate)
  - `ConservativeSlowStartRounds > 0`
  - `CssBaselineMinRtt` = RTT when entered this state
  
- **HYSTART_DONE**:
  - `CWndSlowStartGrowthDivisor = 1`
  - Usually `CongestionWindow >= SlowStartThreshold` (in CA)

---

## Key Algorithms

### 1. CUBIC Window Function

**Formula**: `W_cubic(t) = C * (t - K)^3 + WindowMax`

Where:
- **C** = 0.4 (constant from RFC 8312)
- **t** = time since congestion avoidance started + SmoothedRtt
- **K** = `CubeRoot((WindowMax / MTU) * (1 - BETA) / C)` in seconds
- **BETA** = 0.7 (multiplicative decrease factor)
- **WindowMax** = CongestionWindow at last congestion event

**Regions**:
- **Concave** (t < K): Window < WindowMax, slow growth recovering to previous max
- **Inflection** (t = K): Window = WindowMax, matched previous maximum
- **Convex** (t > K): Window > WindowMax, probing for more bandwidth

### 2. AIMD (Reno-Friendly) Window

**Purpose**: Ensure fairness with Reno TCP (RFC 8312 Section 4.2)

**Growth rate**:
- `0.5 MSS/RTT` when `AimdWindow < WindowPrior` (cautious)
- `1.0 MSS/RTT` when `AimdWindow >= WindowPrior` (match Reno)

**Implementation**: Appropriate Byte Counting (RFC 3465)
- Accumulate ACKed bytes
- Grow window by 1 MTU when enough bytes accumulated

**Final window**: `CongestionWindow = max(CubicWindow, AimdWindow)` (capped at 2*BytesInFlightMax)

### 3. HyStart++ Early Slow Start Exit

**Problem**: Traditional slow start exits only on loss, may overshoot

**Solution**: Detect RTT increase as signal of queue buildup

**Detection**:
```
Eta = max(MIN_ETA, min(MAX_ETA, MinRttInLastRound / 8))
if MinRttInCurrentRound >= MinRttInLastRound + Eta:
    exit slow start
```

**Conservative Slow Start**: After detection, grow at half rate for N rounds before full congestion avoidance

### 4. Pacing

**Without pacing**: Send allowance = `CongestionWindow - BytesInFlight`

**With pacing**: Spread CongestionWindow over RTT
```
EstimatedWnd = {
    2 * CongestionWindow               if in slow start
    1.25 * CongestionWindow            if in CA
}
SendAllowance = (EstimatedWnd * TimeSinceLastSend) / SmoothedRtt
```

---

## Object Invariants (MUST always hold)

```
CongestionWindow > 0
BytesInFlightMax > 0
InitialWindowPackets > 0
Exemptions >= 0 (uint8_t range)

IsInRecovery => HasHadCongestionEvent
HasHadCongestionEvent => RecoverySentPacketNumber is valid

SlowStartThreshold = UINT32_MAX (not set) OR valid byte count > 0

CongestionWindow < SlowStartThreshold  => in slow start
CongestionWindow >= SlowStartThreshold => in congestion avoidance

CongestionWindow <= 2 * BytesInFlightMax (growth limit)

HyStartState in {HYSTART_NOT_STARTED, HYSTART_ACTIVE, HYSTART_DONE}
```

---

## Testing Strategy

### Scenario-Based Coverage

Since only `CubicCongestionControlInitialize` is publicly callable, tests must:

1. Call `CubicCongestionControlInitialize()` with various settings
2. Access operations via function pointers (e.g., `Cc->QuicCongestionControlCanSend(Cc)`)
3. Simulate packet lifecycle: Send → ACK/Loss → Send
4. Observe outputs via getter functions and CongestionWindow changes

### Key Scenarios

**Initialization & Basic Operation**:
- [ ] Initialize with various MTU (1200, 1280, 65535) and InitialWindowPackets (1, 10, 1000)
- [ ] CanSend returns TRUE when BytesInFlight < CongestionWindow
- [ ] CanSend returns TRUE when Exemptions > 0 even if over window
- [ ] SetExemption allows bypass
- [ ] OnDataSent increments BytesInFlight, decrements Exemptions

**Slow Start**:
- [ ] Exponential growth: window doubles per RTT (divisor = 1)
- [ ] Transition to CA when CongestionWindow >= SlowStartThreshold
- [ ] HyStart++ detection: exit SS when RTT increases

**HyStart++ State Machine**:
- [ ] NOT_STARTED → ACTIVE on RTT increase
- [ ] ACTIVE → DONE after N rounds
- [ ] ACTIVE → NOT_STARTED on RTT decrease (spurious)
- [ ] Any state → DONE on congestion event

**Congestion Avoidance**:
- [ ] CUBIC concave region (t < K): slow growth to WindowMax
- [ ] CUBIC convex region (t > K): probe beyond WindowMax
- [ ] AIMD window grows conservatively
- [ ] Final window = max(CUBIC, AIMD)

**Congestion Events**:
- [ ] OnDataLost triggers congestion event
- [ ] OnEcn triggers congestion event
- [ ] Window reduced by BETA (0.7)
- [ ] Fast convergence: if multiple congestions, reduce WindowMax
- [ ] Persistent congestion: window reset to 2 MTU

**Recovery**:
- [ ] IsInRecovery set after congestion event
- [ ] No window growth during recovery
- [ ] Exit recovery when ACK > RecoverySentPacketNumber
- [ ] Spurious congestion: revert to saved state

**Pacing**:
- [ ] Without pacing: full available window
- [ ] With pacing: spread over RTT based on time since last send
- [ ] Pacing uses estimated window (2x in SS, 1.25x in CA)

**Boundary & Edge Cases**:
- [ ] Minimum window (2 MTU after persistent congestion)
- [ ] Maximum window (2 * BytesInFlightMax cap)
- [ ] DeltaT overflow protection (> 2.5M ms)
- [ ] CubicWindow overflow (saturate to 2*BytesInFlightMax)
- [ ] Reset with FullReset=TRUE and FALSE

**Getters**:
- [ ] GetCongestionWindow, GetBytesInFlightMax, GetExemptions
- [ ] GetNetworkStatistics populates all fields
- [ ] IsAppLimited returns FALSE, SetAppLimited is no-op

---

## Contract-Safety Rules for Tests

**DO**:
- ✅ Call `CubicCongestionControlInitialize()` to set up
- ✅ Use function pointers to invoke operations
- ✅ Simulate realistic packet flows (Send → ACK/Loss cycles)
- ✅ Use getter functions to observe state
- ✅ Test boundary values within valid ranges

**DO NOT**:
- ❌ Access private fields directly (e.g., `Cubic->BytesInFlight = 123`)
- ❌ Call internal helpers like `CubicCongestionHyStartChangeState()` directly
- ❌ Violate preconditions (e.g., NULL Cc, invalid MTU in Connection)
- ❌ Bypass state machine transitions
- ❌ Test with uninitialized Connection/Path structures

**Setup Pattern**:
```c
// Valid test setup
QUIC_CONNECTION Connection;
InitializeMockConnection(Connection, 1280); // MTU
QUIC_SETTINGS_INTERNAL Settings{};
Settings.InitialWindowPackets = 10;
CubicCongestionControlInitialize(&Connection.CongestionControl, &Settings);

// Now use function pointers
BOOLEAN canSend = Connection.CongestionControl.QuicCongestionControlCanSend(&Connection.CongestionControl);
```

---

## Coverage Goals

- **100% statement coverage** of `cubic.c`
- All state transitions in Recovery and HyStart++ state machines
- All branches in CUBIC window calculation
- Boundary conditions for overflow protection
- All 17 function pointer operations exercised

---

**Generated**: 2026-02-06  
**For**: DeepTest PR #15 Analysis
