# Tests with Manual BBR State Setting

## Issue
Several tests (30-34, 37, 43) manually set `Bbr->BbrState` directly, which bypasses the state machine and may create inconsistent BBR objects that violate internal invariants.

## Tests Affected

### State Transition Tests (Tests 30-34)

#### Test 30: `StateTransition_ProbeBwToProbeRtt_RttExpired`
- **Line 1667**: `Bbr->BbrState = BBR_STATE_PROBE_BW;`
- **Additional state setup**: Sets `BtlbwFound`, `MinRtt`, `MinRttTimestamp`, bandwidth filter, pacing cycle
- **Purpose**: Test PROBE_BW → PROBE_RTT transition on RTT expiry

#### Test 31: `StateTransition_ProbeRttToProbeBw_ProbeComplete`
- **Line 1719**: `Bbr->BbrState = BBR_STATE_PROBE_RTT;`
- **Additional state setup**: Sets `ProbeRttDoneTimestamp`, `MinRtt`, `MinRttTimestamp`, `BtlbwFound`
- **Purpose**: Test PROBE_RTT → PROBE_BW transition on probe complete

#### Test 32: `StateTransition_ProbeRttToStartup_NoBottleneckFound`
- **Line 1792**: `Bbr->BbrState = BBR_STATE_PROBE_RTT;`
- **Additional state setup**: Sets `ProbeRttDoneTimestamp`, `MinRtt`, `MinRttTimestamp`, `BtlbwFound = FALSE`
- **Purpose**: Test PROBE_RTT → STARTUP transition when no bottleneck

#### Test 33: `StateTransition_DrainToProbeRtt_RttExpired`
- **Line 1865**: `Bbr->BbrState = BBR_STATE_DRAIN;`
- **Additional state setup**: Sets `BtlbwFound`, `MinRtt`, old `MinRttTimestamp`
- **Purpose**: Test DRAIN → PROBE_RTT transition on RTT expiry

#### Test 34: `ProbeBwGainCycleDrain_TargetReached`
- **Line 1926**: `Bbr->BbrState = BBR_STATE_PROBE_BW;`
- **Additional state setup**: Sets `BtlbwFound`, `MinRtt`, bandwidth filter, pacing gain, cycle index
- **Purpose**: Test PROBE_BW gain cycle advancement

### Coverage Tests (Tests 37, 43)

#### Test 37: `SendAllowanceBandwidthBased`
- **Line 2091**: `Bbr->BbrState = BBR_STATE_PROBE_RTT;`
- **Additional state setup**: Sets `MinRtt`, `BtlbwFound`, bandwidth filter
- **Purpose**: Cover bandwidth-based send allowance path

#### Test 43: `SendAllowanceBandwidthOnly_NonStartupPath`
- **Line 2391**: `Bbr->BbrState = BBR_STATE_PROBE_RTT;`
- **Additional state setup**: Sets `MinRtt`, `MinRttTimestamp`, `RttSampleExpired`, bandwidth filter
- **Purpose**: Cover non-STARTUP send allowance path

---

## Problem Analysis

When manually setting `Bbr->BbrState`, we may create inconsistent states because:

1. **Missing invariant maintenance**: State transitions in BBR typically involve:
   - Updating pacing/cwnd gains
   - Setting/clearing flags (BtlbwFound, RttSampleExpired, etc.)
   - Initializing timestamps (CycleStart, ProbeRttDoneTimestamp, etc.)
   - Resetting counters (RoundTripCounter, EndOfRoundTrip, etc.)
   - Updating recovery state

2. **Unknown dependencies**: We don't know all the invariants that must hold for each state

3. **Test fragility**: Tests may pass with invalid state combinations that would never occur in production

---

## Proposed Solutions

### Option A: Evolve State via Public APIs Only (PREFERRED)

For each test, start from STARTUP and use sequences of public API calls to naturally evolve to the target state:

#### Example for PROBE_BW:
```cpp
// Start from STARTUP (after init)
// Send enough data to detect bottleneck bandwidth
// Wait for bandwidth to converge (3 round trips without growth)
// BBR will transition STARTUP → DRAIN → PROBE_BW
```

**Pros:**
- Contract-safe: never violates preconditions or invariants
- Tests realistic scenarios
- Validates state transitions themselves
- More robust to implementation changes

**Cons:**
- More complex test setup
- May require many ACK/send cycles
- Some states may be hard to reach (e.g., DRAIN is transient)

#### Example implementation:
```cpp
// Evolve STARTUP → PROBE_BW
void EvolveToProbeBw(QUIC_CONNECTION& Connection) {
    auto* Bbr = &Connection.CongestionControl.Bbr;
    uint64_t TimeNow = 1000000;
    
    // Send packets and ACK them with stable bandwidth for 3 RTTs
    // This should trigger bottleneck detection and state progression
    for (int round = 0; round < 5; round++) {
        // Send a burst
        for (int i = 0; i < 10; i++) {
            Connection.CongestionControl.QuicCongestionControlOnDataSent(
                &Connection.CongestionControl, 1200);
        }
        
        // ACK the burst with stable delivery rate
        QUIC_ACK_EVENT AckEvent = {};
        AckEvent.TimeNow = TimeNow;
        AckEvent.AdjustedAckTime = TimeNow;
        AckEvent.MinRttValid = TRUE;
        AckEvent.MinRtt = 50000; // 50ms
        AckEvent.NumRetransmittableBytes = 12000;
        AckEvent.NumTotalAckedRetransmittableBytes = 12000;
        AckEvent.LargestAck = (round + 1) * 10;
        AckEvent.LargestSentPacketNumber = (round + 1) * 10;
        AckEvent.IsLargestAckedPacketAppLimited = FALSE;
        
        Connection.CongestionControl.QuicCongestionControlOnDataAcknowledged(
            &Connection.CongestionControl, &AckEvent);
        
        TimeNow += 50000; // Advance by 1 RTT
    }
    
    // At this point, BBR should be in PROBE_BW (or DRAIN → PROBE_BW)
    ASSERT_TRUE(Bbr->BbrState == BBR_STATE_PROBE_BW || 
                Bbr->BbrState == BBR_STATE_DRAIN);
}
```

---

### Option B: Create BBR Object Invariant Validator

Define a function that checks all BBR invariants and call it after manual state setting:

```cpp
bool ValidateBbrInvariants(const QUIC_CONGESTION_CONTROL_BBR* Bbr) {
    // Check basic invariants
    if (Bbr->BytesInFlight > Bbr->CongestionWindow) {
        // May be valid during loss recovery, but check consistency
    }
    
    // State-specific invariants
    switch (Bbr->BbrState) {
        case BBR_STATE_STARTUP:
            // In STARTUP, pacing gain should be high (> GAIN_UNIT)
            if (Bbr->PacingGain <= 1024) return false;
            // CwndGain should also be elevated
            if (Bbr->CwndGain <= 1024) return false;
            break;
            
        case BBR_STATE_DRAIN:
            // In DRAIN, pacing gain should be < GAIN_UNIT
            if (Bbr->PacingGain >= 1024) return false;
            // Should have found bottleneck
            if (!Bbr->BtlbwFound) return false;
            break;
            
        case BBR_STATE_PROBE_BW:
            // Should have found bottleneck
            if (!Bbr->BtlbwFound) return false;
            // Pacing gain should be cycling
            if (Bbr->PacingCycleIndex >= 8) return false;
            // CycleStart should be set
            if (Bbr->CycleStart == 0) return false;
            break;
            
        case BBR_STATE_PROBE_RTT:
            // MinRtt should be valid
            if (Bbr->MinRtt == 0 || Bbr->MinRtt == UINT64_MAX) return false;
            // ProbeRttDoneTimestamp should be set when probing
            // (but may not be set immediately on entry)
            break;
    }
    
    // Bandwidth filter should be initialized
    if (Bbr->BandwidthFilter.WindowedMaxFilter.Submax == 0 &&
        Bbr->BtlbwFound) {
        return false; // If we found bottleneck, bandwidth should be non-zero
    }
    
    // MinRttTimestamp should be reasonable
    if (Bbr->MinRtt != UINT64_MAX && Bbr->MinRttTimestamp == 0) {
        return false; // If we have MinRtt, timestamp should be set
    }
    
    // Recovery state should be consistent
    if (Bbr->RecoveryState != 0 && Bbr->RecoveryWindow == 0) {
        return false; // If in recovery, window should be set
    }
    
    return true;
}
```

**Usage:**
```cpp
TEST(BbrTest, StateTransition_ProbeBwToProbeRtt_RttExpired)
{
    // ... setup ...
    
    Bbr->BbrState = BBR_STATE_PROBE_BW;
    Bbr->BtlbwFound = TRUE;
    Bbr->MinRtt = 50000;
    // ... more setup ...
    
    // Validate invariants after manual setup
    ASSERT_TRUE(ValidateBbrInvariants(Bbr));
    
    // ... rest of test ...
}
```

**Pros:**
- Catches obvious invariant violations
- Simpler than evolving state naturally
- Provides documentation of BBR invariants

**Cons:**
- We may not know all invariants (some are implicit)
- Doesn't validate transition correctness
- May give false confidence
- Still tests unrealistic state combinations

---

## Recommendation

**Use Option A (evolve state via public APIs)** for the following reasons:

1. **Contract safety**: Guarantees we never violate preconditions or create invalid states
2. **Realistic testing**: Tests actual usage patterns
3. **Transition validation**: Tests both the target state AND the transition to it
4. **Robustness**: Less likely to break if implementation changes
5. **Alignment with requirements**: Matches the principle of "public API only" testing

**Option B is a compromise** if Option A is too complex, but it's less desirable because:
- We can't know all invariants without deep implementation knowledge
- It still allows unrealistic state combinations
- It doesn't test state transitions themselves

---

## Implementation Plan for Option A

### 1. Create helper functions (no direct state manipulation):

```cpp
// Evolve to PROBE_BW by detecting bottleneck bandwidth
void EvolveToProbeBw(QUIC_CONNECTION& Connection, uint64_t& TimeNow);

// Evolve to PROBE_RTT from current state by expiring MinRtt
void EvolveToProbeRtt(QUIC_CONNECTION& Connection, uint64_t& TimeNow);

// Evolve to DRAIN (transient state between STARTUP and PROBE_BW)
void EvolveToDrain(QUIC_CONNECTION& Connection, uint64_t& TimeNow);
```

### 2. Update affected tests:

- **Tests 30-34**: Use evolution helpers to reach target states
- **Tests 37, 43**: Use evolution helpers or merge into other tests if redundant

### 3. Document contract-unreachable states:

If certain states prove unreachable via public APIs in unit tests (e.g., DRAIN is very transient), document this in `rci_summary.md` and explain why integration tests are needed.

---

## Status

**Current state**: 7 tests manually set BBR state (Tests 30-34, 37, 43)

**Next steps**:
1. Implement state evolution helper functions using only public APIs
2. Refactor Tests 30-34, 37, 43 to use helpers
3. Validate that all tests still pass and maintain coverage
4. Document any states that remain unreachable in unit tests
