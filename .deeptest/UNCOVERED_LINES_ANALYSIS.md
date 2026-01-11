# BBR Uncovered Lines - Detailed Analysis

**Generated:** January 10, 2026  
**Total Uncovered:** 11 lines out of 484 (2.27%)  
**File:** `src/core/bbr.c`

---

## Uncovered Lines Breakdown

### Line 154: Bandwidth Calculation Fallback Path
```c
AckElapsed = CxPlatTimeDiff64(AckedPacket->LastAckedPacketInfo.AckTime, TimeNow);
```

**Location:** `BbrBandwidthFilterOnPacketAcked()` function  
**Context:** Fallback path for ACK time calculation when AdjustedAckTime comparison fails  
**Function:** Bandwidth filter update logic  
**Reason Uncovered:**  
- Requires specific timing conditions where `CxPlatTimeAtOrBefore64(AckEvent->AdjustedAckTime, AckedPacket->LastAckedPacketInfo.AdjustedAckTime)` is true
- This is a rare edge case in the ACK timing logic
- Line 152 (the normal path) is covered

**Impact:** Low  
**Risk:** Low - edge case in bandwidth calculation  
**Recommendation:** Integration testing with real network timing variations

---

### Line 171: Invalid Bandwidth Sample Skip
```c
continue;
```

**Location:** `BbrBandwidthFilterOnPacketAcked()` function  
**Context:** Skip processing when both SendRate and AckRate are invalid (UINT64_MAX)  
**Function:** Bandwidth filter validation  
**Reason Uncovered:**  
- Requires conditions where neither send rate nor ACK rate can be calculated
- This happens when packet metadata is insufficient for bandwidth estimation
- Very rare in normal operation

**Impact:** Low  
**Risk:** Low - defensive check for invalid data  
**Recommendation:** Acceptable gap; represents malformed packet metadata

---

### Line 495: Recovery Window Growth
```c
Bbr->RecoveryWindow += BytesAcked;
```

**Location:** `BbrCongestionControlOnDataAcknowledged()` function  
**Context:** Recovery window increase during GROWTH recovery state  
**Function:** Recovery window management  
**Reason Uncovered:**  
- Requires entering `RECOVERY_STATE_GROWTH` state
- This state transition requires specific loss patterns
- Tests cover `RECOVERY_STATE_NOT_RECOVERY` and recovery entry

**Impact:** Medium  
**Risk:** Medium - recovery window calculation affects performance  
**Recommendation:** Add test for recovery growth state (Test 7 covers recovery entry but not growth)

---

### Line 636: Zero Send Allowance (CC Blocked)
```c
SendAllowance = 0;
```

**Location:** `BbrCongestionControlGetSendAllowance()` function  
**Context:** Congestion control blocked scenario  
**Function:** Send allowance calculation  
**Reason Uncovered:**  
- Requires `CongestionWindow <= BytesInFlight` (fully blocked)
- Tests 17-18 cover send allowance but not the fully blocked case
- May be difficult to trigger while maintaining valid BBR state

**Impact:** Medium  
**Risk:** Low - zero allowance is conservative and safe  
**Recommendation:** Add test case for congestion window saturation

---

### Lines 638-640: Pacing Disabled Path (Condition)
```c
} else if (
    !TimeSinceLastSendValid ||
    !Connection->Settings.PacingEnabled ||
```

**Location:** `BbrCongestionControlGetSendAllowance()` function  
**Context:** Condition check for pacing disabled path  
**Function:** Send allowance with pacing decision  
**Reason Uncovered:**  
- Test 20 (`SendAllowanceWithPacingDisabled`) exists but doesn't cover this specific condition branch
- The condition itself (lines 638-640) vs the body (line 642+) coverage

**Impact:** Low  
**Risk:** Low - condition is evaluated even if not taken  
**Recommendation:** Verify Test 20 is exercising the right path

---

### Line 659: Bandwidth-Based Send Allowance Calculation
```c
SendAllowance = (uint32_t)(BandwidthEst * Bbr->PacingGain * TimeSinceLastSend / GAIN_UNIT);
```

**Location:** `BbrCongestionControlGetSendAllowance()` function  
**Context:** Calculate send allowance based on bandwidth estimate and pacing gain  
**Function:** Pacing send allowance  
**Reason Uncovered:**  
- Requires valid `TimeSinceLastSend` and enabled pacing
- Alternative to the window-based calculation (line 656-657)
- Tests 17-18 may hit the window-based path instead

**Impact:** Medium  
**Risk:** Medium - affects pacing behavior  
**Recommendation:** Add test that exercises bandwidth-based pacing calculation

---

### Line 723: Send Quantum Calculation (Large Rate)
```c
Bbr->SendQuantum = CXPLAT_MIN(PacingRate * kMilliSecsInSec / BW_UNIT, 64 * 1024 /* 64k */);
```

**Location:** `BbrCongestionControlSetSendQuantum()` function  
**Context:** Send quantum for high pacing rates  
**Function:** Send quantum tier calculation  
**Reason Uncovered:**  
- Alternative to the small rate calculation (line 721)
- Test 19 (`PacingSendQuantumTiers`) covers quantum calculation but hits the low-rate path
- Requires `PacingRate >= kMinimumPacingRateMss`

**Impact:** Low  
**Risk:** Low - quantum sizing for high rates  
**Recommendation:** Test 19 could be enhanced to cover high pacing rates

---

### Line 752: ACK Aggregation in Target Window
```c
TargetCwnd += Entry.Value;
```

**Location:** `BbrCongestionControlGetTargetCwnd()` function  
**Context:** Add ACK aggregation to target congestion window  
**Function:** Congestion window calculation with ACK aggregation  
**Reason Uncovered:**  
- Requires successful `QuicSlidingWindowExtremumGet()` call
- ACK aggregation filter must have valid data
- Tests may not generate enough traffic to populate ACK height filter

**Impact:** Medium  
**Risk:** Low - conservative (doesn't reduce window)  
**Recommendation:** Add test with burst acknowledgments to populate ACK height filter

---

### Lines 848-850: PROBE_BW Pacing Gain Cycle Advancement (< GAIN_UNIT)
```c
uint64_t TargetCwnd = BbrCongestionControlGetTargetCwnd(Cc, GAIN_UNIT);
if (Bbr->BytesInFlight <= TargetCwnd) {
    ShouldAdvancePacingGainCycle = TRUE;
```

**Location:** `BbrCongestionControlOnDataAcknowledged()` function  
**Context:** Advance pacing gain cycle in PROBE_BW state  
**Function:** PROBE_BW pacing gain cycle management  
**Reason Uncovered:**  
- **Requires being in PROBE_BW state**
- As documented, PROBE_BW is not reachable via public APIs in unit tests
- This is part of the bandwidth-driven state machine

**Impact:** High (for PROBE_BW behavior)  
**Risk:** Medium - affects PROBE_BW cycling behavior  
**Recommendation:** **Integration-level testing required** - cannot be unit tested

---

## Summary by Category

### 1. Bandwidth Detection Edge Cases (3 lines)
- Lines: 154, 171, 659
- Impact: Low-Medium
- Can be tested with more sophisticated packet metadata

### 2. Recovery State Coverage (1 line)
- Line: 495
- Impact: Medium
- Can be tested by extending Test 7 to cover RECOVERY_GROWTH state

### 3. Send Allowance Edge Cases (2 lines)
- Lines: 636, 638-640
- Impact: Medium
- Can be tested with additional send allowance scenarios

### 4. Pacing/Quantum Calculation (1 line)
- Line: 723
- Impact: Low
- Can be tested by extending Test 19 with high pacing rates

### 5. ACK Aggregation (1 line)
- Line: 752
- Impact: Medium
- Can be tested with burst traffic patterns

### 6. PROBE_BW State Logic (3 lines)
- Lines: 848-850
- Impact: High (within PROBE_BW context)
- **Cannot be unit tested** - requires integration testing

---

## Actionable Recommendations

### Can Be Covered in Unit Tests (8 lines - 1.65%)

1. **Add Recovery Growth Test** (Line 495)
   ```
   Extend Test 7 to cover RECOVERY_STATE_GROWTH
   Verify RecoveryWindow increases on ACKs during growth
   ```

2. **Add CC Blocked Test** (Line 636)
   ```
   New test: Send allowance when CongestionWindow == BytesInFlight
   Verify SendAllowance returns 0
   ```

3. **Verify Pacing Disabled Path** (Lines 638-640)
   ```
   Review Test 20 to ensure it hits the condition branch
   May need to adjust test setup
   ```

4. **Add Bandwidth-Based Pacing Test** (Line 659)
   ```
   New test: Send allowance with valid TimeSinceLastSend
   Should use bandwidth-based calculation instead of window-based
   ```

5. **Add High Pacing Rate Test** (Line 723)
   ```
   Extend Test 19 with PacingRate >= kMinimumPacingRateMss
   Verify quantum is calculated with CXPLAT_MIN formula
   ```

6. **Add ACK Aggregation Test** (Line 752)
   ```
   New test: Send burst of packets, ACK in burst
   Populate MaxAckHeightFilter to trigger aggregation path
   ```

### Requires Integration Testing (3 lines - 0.62%)

7. **PROBE_BW Pacing Gain Cycle** (Lines 848-850)
   ```
   Integration test: Reach PROBE_BW state with real network
   Verify pacing gain cycle advancement when BytesInFlight <= TargetCwnd
   Cannot be unit tested - requires bandwidth-driven state transitions
   ```

---

## Coverage Impact

If all unit-testable gaps were covered:

| Current | With Additional Tests | Remaining Gap |
|---------|----------------------|---------------|
| 97.73% (473/484) | 99.38% (481/484) | 0.62% (3/484) |

The remaining 0.62% (3 lines) are PROBE_BW-specific logic that fundamentally require integration-level testing.

---

**Report End**
