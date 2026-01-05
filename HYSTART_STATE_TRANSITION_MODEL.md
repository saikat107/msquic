# HyStart++ State Transition Model for MsQuic CUBIC Congestion Control

## Overview
HyStart++ is an algorithm designed to safely exit slow start by detecting delay increases, preventing overshoot and unnecessary packet loss. This document provides a formal state transition model with triggering conditions and API call sequences.

## State Definition

The HyStart++ algorithm has three discrete states defined in `src/core/cubic.h`:

```c
typedef enum QUIC_CUBIC_HYSTART_STATE {
    HYSTART_NOT_STARTED = 0,  // Initial state, monitoring RTT samples
    HYSTART_ACTIVE = 1,        // Conservative Slow Start active
    HYSTART_DONE = 2          // Exited to Congestion Avoidance
} QUIC_CUBIC_HYSTART_STATE;
```

## State Transition Diagram

```
    ┌─────────────────────┐
    │ HYSTART_NOT_STARTED │ (Initial State)
    └──────────┬──────────┘
               │
               │ T1: RTT Increase Detected
               │     MinRttCurrent >= MinRttLast + η
               │     where η ∈ [4ms, 16ms]
               ▼
    ┌─────────────────────┐
    │   HYSTART_ACTIVE    │ (Conservative Slow Start)
    └──────────┬──────────┘
               │
               ├──► T2: Conservative Rounds Complete
               │    (ConservativeSlowStartRounds → 0)
               │
               │    OR
               │
               ├──► T3: Packet Loss / ECN Signal
               │
               │    OR
               │
               └──► T4: Persistent Congestion
                    
               │
               ▼
    ┌─────────────────────┐
    │    HYSTART_DONE     │ (Terminal State)
    └─────────────────────┘
               ▲
               │
    ┌──────────┴──────────┐
    │                     │
    │ T5: Direct from     │ T6: RTT Decrease in ACTIVE
    │ NOT_STARTED via:    │     MinRttCurrent < CssBaselineMinRtt
    │ - Loss              │     (returns to NOT_STARTED)
    │ - ECN               │
    │ - Persistent        │
    │   Congestion        │
    └─────────────────────┘
```

## State Transition Table

| From State          | To State            | Trigger | Condition | Location in Code |
|---------------------|---------------------|---------|-----------|------------------|
| HYSTART_NOT_STARTED | HYSTART_ACTIVE      | T1      | RTT increase detected | cubic.c:504 |
| HYSTART_NOT_STARTED | HYSTART_DONE        | T5      | Loss/ECN/Persistent congestion | cubic.c:328, 360, 744, 779 |
| HYSTART_ACTIVE      | HYSTART_DONE        | T2      | Conservative rounds complete | cubic.c:534 |
| HYSTART_ACTIVE      | HYSTART_DONE        | T3      | Loss/ECN/Persistent congestion | cubic.c:328, 360, 744, 779 |
| HYSTART_ACTIVE      | HYSTART_NOT_STARTED | T6      | RTT decrease detected | cubic.c:516 |
| HYSTART_DONE        | (no transitions)    | -       | Terminal state | - |

## Detailed Transition Conditions

### T1: HYSTART_NOT_STARTED → HYSTART_ACTIVE
**Condition:**
```
Settings.HyStartEnabled = TRUE AND
HyStartAckCount >= N_SAMPLING (8) AND
MinRttInLastRound != UINT64_MAX AND
MinRttInCurrentRound != UINT64_MAX AND
MinRttInCurrentRound >= MinRttInLastRound + η
```
where η (Eta) is calculated as:
```
η = CLAMP(MinRttInLastRound / 8, MIN_ETA=4ms, MAX_ETA=16ms)
```

**Effects:**
- `CWndSlowStartGrowthDivisor` set to 4 (reduces growth rate to 25%)
- `ConservativeSlowStartRounds` set to 5
- `CssBaselineMinRtt` = `MinRttInCurrentRound`

**Code Location:** `cubic.c:487-510`

### T2: HYSTART_ACTIVE → HYSTART_DONE (Conservative Rounds Complete)
**Condition:**
```
HyStartState = HYSTART_ACTIVE AND
LargestAck >= HyStartRoundEnd AND
--ConservativeSlowStartRounds = 0
```

**Effects:**
- `SlowStartThreshold` = `CongestionWindow`
- `TimeOfCongAvoidStart` = current time
- `AimdWindow` = `CongestionWindow`
- `CWndSlowStartGrowthDivisor` reset to 1

**Code Location:** `cubic.c:526-535`

### T3: HYSTART_ACTIVE → HYSTART_DONE (Loss/ECN)
**Condition:**
```
OnDataLost() called OR OnEcn() called OR OnCongestionEvent(IsPersistentCongestion=FALSE)
```

**Effects:**
- Congestion window reduced by BETA (0.7)
- `SlowStartThreshold` set to reduced window
- `IsInRecovery` = TRUE
- `CWndSlowStartGrowthDivisor` reset to 1

**Code Location:** `cubic.c:328, 360, 744, 779`

### T4: HYSTART_ACTIVE → HYSTART_DONE (Persistent Congestion)
**Condition:**
```
OnDataLost() called with PersistentCongestion = TRUE OR
OnCongestionEvent(IsPersistentCongestion=TRUE)
```

**Effects:**
- Congestion window drastically reduced to 2 packets
- All CUBIC state reset
- `IsInPersistentCongestion` = TRUE
- `CWndSlowStartGrowthDivisor` reset to 1

**Code Location:** `cubic.c:307-328`

### T5: HYSTART_NOT_STARTED → HYSTART_DONE (Direct Loss/ECN)
**Condition:**
```
HyStartState = HYSTART_NOT_STARTED AND
(OnDataLost() OR OnEcn() OR OnCongestionEvent())
```

**Effects:** Same as T3

**Code Location:** `cubic.c:328, 360, 744, 779`

### T6: HYSTART_ACTIVE → HYSTART_NOT_STARTED (RTT Decrease - Spurious Detection)
**Condition:**
```
HyStartState = HYSTART_ACTIVE AND
MinRttInCurrentRound < CssBaselineMinRtt
```

**Effects:**
- Resume normal slow start
- `CWndSlowStartGrowthDivisor` reset to 1

**Code Location:** `cubic.c:515-517`

## Public API Call Sequences to Trigger Transitions

### Prerequisites
All transitions require:
```c
Connection.Settings.HyStartEnabled = TRUE;
Connection.Paths[0].GotFirstRttSample = TRUE;
Connection.CongestionControl.Cubic.CongestionWindow < SlowStartThreshold;
```

### Sequence for T1: NOT_STARTED → ACTIVE

```c
// 1. Initialize connection in slow start
CubicCongestionControlInitialize(&Cc, &Settings);
ASSERT(Cubic->HyStartState == HYSTART_NOT_STARTED);

// 2. Establish baseline RTT through first ACK round
for (int i = 0; i < 8; i++) {
    OnDataSent(&Cc, 1200);
    
    QUIC_ACK_EVENT AckEvent = {
        .TimeNow = baseTime + (i * 10000),
        .LargestAck = i,
        .NumRetransmittableBytes = 1200,
        .MinRtt = 50000,  // Baseline: 50ms
        .MinRttValid = TRUE,
        .SmoothedRtt = 50000,
        // ... other fields
    };
    OnDataAcknowledged(&Cc, &AckEvent);
}

// 3. Cross round boundary to establish MinRttInLastRound
Send.NextPacketNumber = 100;  // Force HyStartRoundEnd boundary
QUIC_ACK_EVENT RoundCrossAck = {
    .LargestAck = 100,
    // ... triggers CubicCongestionHyStartResetPerRttRound()
};
OnDataAcknowledged(&Cc, &RoundCrossAck);

// 4. In next round, send ACKs with increased RTT
for (int i = 0; i < 8; i++) {
    OnDataSent(&Cc, 1200);
    
    QUIC_ACK_EVENT AckEvent = {
        .MinRtt = 57000,  // Increased by 7ms (> η = 50ms/8 = 6.25ms)
        .MinRttValid = TRUE,
        // ... other fields
    };
    OnDataAcknowledged(&Cc, &AckEvent);
}

// After 8th ACK with increased RTT, transition to ACTIVE occurs
ASSERT(Cubic->HyStartState == HYSTART_ACTIVE);
ASSERT(Cubic->CWndSlowStartGrowthDivisor == 4);
ASSERT(Cubic->ConservativeSlowStartRounds == 5);
```

### Sequence for T2: ACTIVE → DONE (Conservative Rounds)

```c
// Prerequisite: Already in HYSTART_ACTIVE state from T1

// Cross 5 round boundaries while in ACTIVE state
for (int round = 0; round < 5; round++) {
    uint64_t roundStart = Send.NextPacketNumber;
    
    // Send and ACK packets within the round
    for (int pkt = 0; pkt < 10; pkt++) {
        OnDataSent(&Cc, 1200);
        
        QUIC_ACK_EVENT AckEvent = {
            .LargestAck = roundStart + pkt,
            .NumRetransmittableBytes = 1200,
            .MinRtt = 57000,
            .MinRttValid = TRUE,
            // ... other fields
        };
        OnDataAcknowledged(&Cc, &AckEvent);
    }
    
    // Cross round boundary
    Send.NextPacketNumber = roundStart + 20;
    QUIC_ACK_EVENT BoundaryAck = {
        .LargestAck = Send.NextPacketNumber - 1,
        .NumRetransmittableBytes = 1200,
        // ... other fields
    };
    OnDataAcknowledged(&Cc, &BoundaryAck);
}

// After 5th round boundary, transition to DONE occurs
ASSERT(Cubic->HyStartState == HYSTART_DONE);
ASSERT(Cubic->SlowStartThreshold == Cubic->CongestionWindow);
ASSERT(Cubic->CWndSlowStartGrowthDivisor == 1);
```

### Sequence for T3/T5: ANY → DONE (Loss)

```c
// From any state (NOT_STARTED or ACTIVE)
OnDataSent(&Cc, 5000);

QUIC_LOSS_EVENT LossEvent = {
    .NumRetransmittableBytes = 1200,
    .PersistentCongestion = FALSE,
    .LargestPacketNumberLost = 5,
    .LargestSentPacketNumber = 10
};
OnDataLost(&Cc, &LossEvent);

// Immediate transition to DONE
ASSERT(Cubic->HyStartState == HYSTART_DONE);
ASSERT(Cubic->IsInRecovery == TRUE);
ASSERT(Cubic->CWndSlowStartGrowthDivisor == 1);
```

### Sequence for T3/T5: ANY → DONE (ECN)

```c
// From any state (NOT_STARTED or ACTIVE)
OnDataSent(&Cc, 5000);

QUIC_ECN_EVENT EcnEvent = {
    .LargestPacketNumberAcked = 10,
    .LargestSentPacketNumber = 15
};
OnEcn(&Cc, &EcnEvent);

// Immediate transition to DONE
ASSERT(Cubic->HyStartState == HYSTART_DONE);
ASSERT(Cubic->IsInRecovery == TRUE);
```

### Sequence for T4: ANY → DONE (Persistent Congestion)

```c
// From any state
OnDataSent(&Cc, 10000);

QUIC_LOSS_EVENT PersistentLoss = {
    .NumRetransmittableBytes = 5000,
    .PersistentCongestion = TRUE,  // Key difference
    .LargestPacketNumberLost = 8,
    .LargestSentPacketNumber = 10
};
OnDataLost(&Cc, &PersistentLoss);

// Immediate transition to DONE with drastic reduction
ASSERT(Cubic->HyStartState == HYSTART_DONE);
ASSERT(Cubic->IsInPersistentCongestion == TRUE);
ASSERT(Cubic->CongestionWindow <= 2 * DatagramPayloadLength);
```

### Sequence for T6: ACTIVE → NOT_STARTED (RTT Decrease)

```c
// Prerequisite: Already in HYSTART_ACTIVE with CssBaselineMinRtt = 57000

// Send ACKs with decreased RTT
OnDataSent(&Cc, 1200);

QUIC_ACK_EVENT AckEvent = {
    .MinRtt = 50000,  // Decreased below CssBaselineMinRtt (57000)
    .MinRttValid = TRUE,
    .NumRetransmittableBytes = 1200,
    // ... other fields
};
OnDataAcknowledged(&Cc, &AckEvent);

// Transition back to NOT_STARTED
ASSERT(Cubic->HyStartState == HYSTART_NOT_STARTED);
ASSERT(Cubic->CWndSlowStartGrowthDivisor == 1);
```

## Supporting State Variables

These variables are critical for state transitions:

| Variable | Type | Purpose |
|----------|------|---------|
| `HyStartState` | enum | Current HyStart++ state |
| `HyStartAckCount` | uint32 | Number of ACKs with RTT samples in current round |
| `MinRttInLastRound` | uint64 | Minimum RTT observed in previous round (µs) |
| `MinRttInCurrentRound` | uint64 | Minimum RTT observed in current round (µs) |
| `CssBaselineMinRtt` | uint64 | Baseline RTT when entering ACTIVE state (µs) |
| `HyStartRoundEnd` | uint64 | Packet number marking end of current round |
| `CWndSlowStartGrowthDivisor` | uint32 | Divisor for slow start growth (1=normal, 4=conservative) |
| `ConservativeSlowStartRounds` | uint32 | Remaining conservative slow start rounds |

## Round Boundary Detection

A critical concept for HyStart++ is the "RTT round" boundary, detected by:
```c
if (AckEvent.LargestAck >= Cubic->HyStartRoundEnd) {
    // Round boundary crossed
    Cubic->HyStartRoundEnd = Connection->Send.NextPacketNumber;
    CubicCongestionHyStartResetPerRttRound(Cubic);
}
```

This resets per-round state:
```c
void CubicCongestionHyStartResetPerRttRound(QUIC_CONGESTION_CONTROL_CUBIC* Cubic) {
    Cubic->HyStartAckCount = 0;
    Cubic->MinRttInLastRound = Cubic->MinRttInCurrentRound;
    Cubic->MinRttInCurrentRound = UINT64_MAX;
}
```

## Mathematical Proof of Unreachable States

### Claim: It is impossible to transition from HYSTART_DONE back to any other state

**Proof:**
1. Examine all functions that modify `HyStartState`:
   - `CubicCongestionHyStartChangeState()` (lines 83-115)
   - `CubicCongestionControlReset()` (lines 149-175)
   - `CubicCongestionControlInitialize()` (lines 914-940)

2. In `CubicCongestionHyStartChangeState()`:
   ```c
   switch (NewHyStartState) {
       case HYSTART_ACTIVE:
           break;
       case HYSTART_DONE:
       case HYSTART_NOT_STARTED:
           Cubic->CWndSlowStartGrowthDivisor = 1;
           break;
   }
   ```
   This function can SET the state to DONE, but there's no code path that reads current state = DONE and transitions away.

3. In `CubicCongestionControlReset()`:
   - Always sets state to `HYSTART_NOT_STARTED` (line 163)
   - But Reset is only called during connection initialization or after idle timeout
   - These are connection-level events, not state transitions within the algorithm

4. In `OnDataAcknowledged()` (the primary state machine driver):
   - Lines 476-539 contain ALL HyStart++ logic
   - Line 476: `if (Connection->Settings.HyStartEnabled && Cubic->HyStartState != HYSTART_DONE)`
   - **The entire state machine code is skipped when state = DONE**

5. Therefore, once `HyStartState = HYSTART_DONE`:
   - All HyStart++ logic is bypassed (guard at line 476)
   - No subsequent ACK processing can modify the state
   - Only connection-level Reset (which is outside the congestion control state machine) can change it

**Q.E.D.** HYSTART_DONE is an absorbing state in the HyStart++ state machine.

### Claim: Direct transition ACTIVE → DONE via Loss/ECN does not require round boundaries

**Proof:**
1. Examine `CubicCongestionControlOnDataLost()` at line 744:
   ```c
   CubicCongestionHyStartChangeState(Cc, HYSTART_DONE);
   ```
   This is unconditional once congestion event criteria are met.

2. The function `CubicCongestionHyStartChangeState()` has NO guard based on round boundaries:
   ```c
   void CubicCongestionHyStartChangeState(..., QUIC_CUBIC_HYSTART_STATE NewHyStartState) {
       if (!Connection->Settings.HyStartEnabled) return;  // Only guard
       // ... state change logic
       Cubic->HyStartState = NewHyStartState;  // Unconditional assignment
   }
   ```

3. Loss detection (`OnDataLost`) and ECN signaling (`OnEcn`) are independent of:
   - `HyStartAckCount`
   - `HyStartRoundEnd`
   - Any RTT sampling

4. Therefore, T3 (ACTIVE → DONE via Loss/ECN) can occur at ANY point, even mid-round.

**Q.E.D.** Transitions T3, T4, T5 are asynchronous with respect to round boundaries.

## State Invariants

1. **Mutual Exclusion:** At any time, `HyStartState` is exactly one of {NOT_STARTED, ACTIVE, DONE}

2. **Growth Divisor Invariant:**
   - `HyStartState = NOT_STARTED` ⟹ `CWndSlowStartGrowthDivisor = 1`
   - `HyStartState = ACTIVE` ⟹ `CWndSlowStartGrowthDivisor = 4`
   - `HyStartState = DONE` ⟹ `CWndSlowStartGrowthDivisor = 1`

3. **Round Count Invariant:**
   - `HyStartState = ACTIVE` ⟹ `ConservativeSlowStartRounds ∈ [1, 5]`
   - `HyStartState ≠ ACTIVE` ⟹ `ConservativeSlowStartRounds` is don't-care

4. **RTT Baseline Invariant:**
   - `HyStartState = ACTIVE` ⟹ `CssBaselineMinRtt` is valid (was set on entry)
   - `HyStartState ≠ ACTIVE` ⟹ `CssBaselineMinRtt` is stale/irrelevant

5. **Absorbing State Invariant:**
   - If `HyStartState = DONE` at time T, then `HyStartState = DONE` at all times > T (until Reset)

## Test Coverage Analysis

From `src/core/unittest/CubicTest.cpp`, the following transitions are directly tested:

| Transition | Test Function | Status |
|------------|---------------|--------|
| T1 (NOT_STARTED → ACTIVE) | None | ❌ Not unit testable |
| T2 (ACTIVE → DONE via rounds) | None | ❌ Not unit testable |
| T3 (ACTIVE → DONE via loss) | `OnDataLost_WindowReduction` | ✅ Covered (line 587) |
| T4 (ANY → DONE persistent) | `PersistentCongestion_WindowReset` | ✅ Covered (line 866) |
| T5 (NOT_STARTED → DONE) | `OnDataLost_WindowReduction` | ✅ Covered (line 587) |
| T6 (ACTIVE → NOT_STARTED) | None | ❌ Not unit testable |

**Note:** Test author's comment at lines 1689-1704 explains that T1, T2, T6 cannot be reliably tested in unit tests because:
- Complex state combinations required (specific HyStartAckCount, MinRtt values, round boundaries)
- Window growth dynamics interfere with state setup
- Better suited for integration/system tests with real network conditions

## Implementation Notes

1. **Disabled by Default:** HyStart++ requires explicit opt-in via `Settings.HyStartEnabled = TRUE`

2. **Early Exit Guard:** Line 89 in `CubicCongestionHyStartChangeState()`:
   ```c
   if (!Connection->Settings.HyStartEnabled) return;
   ```

3. **Performance Optimization:** All HyStart++ code is skipped once state = DONE (line 476 guard)

4. **Conservative Parameter:** Growth divisor of 4 means window grows at 25% of normal slow start rate during ACTIVE state

5. **Round Duration:** Typically 1 RTT, but can be longer if ACKs are delayed or lost

## References

- **RFC Draft:** draft-ietf-tcpm-hystartplusplus
- **Implementation:** `src/core/cubic.c` lines 476-539
- **Constants:** `src/core/quicdef.h` lines 571-592
- **State Definition:** `src/core/cubic.h` lines 14-18
- **Test Coverage:** `src/core/unittest/CubicTest.cpp` lines 760-808, 815-858

---

**Document Version:** 1.0  
**Date:** 2026-01-05  
**Author:** Generated from source code analysis
