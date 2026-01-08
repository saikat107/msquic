# BBR State Machine Analysis

## States

```
BBR_STATE_STARTUP (0)    - Initial state, aggressive bandwidth probing
BBR_STATE_DRAIN (1)      - Drain queue after finding bottleneck  
BBR_STATE_PROBE_BW (2)   - Steady state, cycling pacing gains
BBR_STATE_PROBE_RTT (3)  - Periodically probe for lower RTT
```

## State Transitions (via OnDataAcknowledged)

### 1. STARTUP → DRAIN
**Trigger**: Line 873-875
```c
if (Bbr->BbrState == BBR_STATE_STARTUP && Bbr->BtlbwFound) {
    BbrCongestionControlTransitToDrain(Cc);
}
```
**Condition**: `BtlbwFound = TRUE` (bandwidth growth stalled for 3 rounds)
**How to trigger**: Send multiple packets, ACK them with insufficient bandwidth growth over 3 round trips

### 2. DRAIN → PROBE_BW  
**Trigger**: Line 877-880
```c
if (Bbr->BbrState == BBR_STATE_DRAIN &&
    Bbr->BytesInFlight <= BbrCongestionControlGetTargetCwnd(Cc, GAIN_UNIT)) {
    BbrCongestionControlTransitToProbeBw(Cc, AckEvent->TimeNow);
}
```
**Condition**: BytesInFlight drained to target CWND
**How to trigger**: In DRAIN state, ACK packets until BytesInFlight drops

### 3. Any State → PROBE_RTT
**Trigger**: Line 882-886
```c
if (Bbr->BbrState != BBR_STATE_PROBE_RTT &&
    !Bbr->ExitingQuiescence &&
    Bbr->RttSampleExpired) {
    BbrCongestionControlTransitToProbeRtt(Cc, AckEvent->LargestSentPacketNumber);
}
```
**Condition**: MinRtt sample expired (> 10 seconds old) and not exiting quiescence
**How to trigger**: Wait 10+ seconds without RTT samples, then ACK

### 4. PROBE_RTT → PROBE_BW or STARTUP
**Trigger**: Line 542-550 (in BbrCongestionControlHandleAckInProbeRtt)
```c
if (Bbr->ProbeRttRoundValid && CxPlatTimeAtOrBefore64(Bbr->ProbeRttEndTime, AckTime)) {
    if (Bbr->BtlbwFound) {
        BbrCongestionControlTransitToProbeBw(Cc, AckTime);
    } else {
        BbrCongestionControlTransitToStartup(Cc);
    }
}
```
**Condition**: Stayed in low inflight for 200ms AND completed one RTT round
**How to trigger**: In PROBE_RTT, wait 200ms+ with low flight, complete round trip

## Key Variables Controlling Transitions

- **BtlbwFound**: Set when bandwidth stops growing in STARTUP (lines 857-870)
- **RttSampleExpired**: Set when MinRtt timestamp > 10 seconds old (line 803)
- **BytesInFlight**: Tracks inflight bytes, modified by OnDataSent/OnDataAcknowledged/OnDataLost
- **EndOfRoundTrip**: Packet number marking end of current round (line 810-814)
- **SlowStartupRoundCounter**: Counts rounds without growth (line 867-869)

## Public APIs That Drive State Changes

1. **OnDataAcknowledged**: Main state transition driver (lines 772-903)
2. **OnDataSent**: Updates BytesInFlight (lines 436-460)
3. **OnDataInvalidated**: Decreases BytesInFlight (lines 464-477)
4. **OnDataLost**: Enters recovery, affects BytesInFlight (lines 907-965)
5. **Reset**: Forces back to STARTUP (lines 998-1063)

## Test Strategy for State Machine

### Valid Sequences to Test:

1. **STARTUP → DRAIN → PROBE_BW** (normal path)
   - Initialize → Send packets → ACK with bandwidth growth → stall growth → ACK to drain → ACK to enter PROBE_BW

2. **STARTUP → DRAIN → PROBE_BW → PROBE_RTT → PROBE_BW** (with RTT expiration)
   - Follow sequence 1 → Wait/mock 10s expiration → ACK → drain in PROBE_RTT → ACK to exit

3. **PROBE_RTT → STARTUP** (no bottleneck found)
   - Enter PROBE_RTT without BtlbwFound → complete probe → should return to STARTUP

4. **Recovery interactions**
   - Any state + loss → verify recovery state independent of BBR state

## Bandwidth Growth Detection (BtlbwFound)

Lines 857-870 in OnDataAcknowledged:
```c
if (!Bbr->BtlbwFound && NewRoundTrip && !LastAckedPacketAppLimited) {
    uint64_t BandwidthTarget = Bbr->LastEstimatedStartupBandwidth * kStartupGrowthTarget / GAIN_UNIT;
    uint64_t CurrentBandwidth = BbrCongestionControlGetBandwidth(Cc);
    
    if (CurrentBandwidth >= BandwidthTarget) {
        // Growing - reset counter
        Bbr->LastEstimatedStartupBandwidth = CurrentBandwidth;
        Bbr->SlowStartupRoundCounter = 0;
    } else if (++Bbr->SlowStartupRoundCounter >= kStartupSlowGrowRoundLimit) {
        // Stalled for 3 rounds - found bottleneck
        Bbr->BtlbwFound = TRUE;
    }
}
```
**Required**: 3 consecutive round trips without 25% bandwidth growth

## Round Trip Detection

Lines 810-814:
```c
BOOLEAN NewRoundTrip = FALSE;
if (!Bbr->EndOfRoundTripValid || Bbr->EndOfRoundTrip < AckEvent->LargestAck) {
    Bbr->RoundTripCounter++;
    Bbr->EndOfRoundTrip = AckEvent->LargestSentPacketNumber;
    NewRoundTrip = TRUE;
}
```
**Trigger**: ACK a packet number > previous EndOfRoundTrip

## Contract-Safe Test Approach

**DO**:
- Call OnDataSent to send packets (increases BytesInFlight, sets packet numbers)
- Call OnDataAcknowledged with valid QUIC_ACK_EVENT to acknowledge packets
- Set AckEvent fields properly (TimeNow, LargestAck, LargestSentPacketNumber, etc.)
- Use proper packet metadata structures
- Simulate time passage through AckEvent->TimeNow

**DON'T**:
- Directly modify Bbr->BbrState
- Directly modify Bbr->BytesInFlight
- Directly modify Bbr->RttSampleExpired
- Directly set Bbr->BtlbwFound

**Test Pattern**:
```cpp
// Setup
Initialize BBR
uint64_t PacketNum = 1;
uint64_t TimeNow = 0;

// Send packets
for (int i = 0; i < N; i++) {
    Cc->QuicCongestionControlOnDataSent(Cc, PacketSize);
    PacketNum++;
}

// Create ACK event
QUIC_ACK_EVENT AckEvent = {0};
AckEvent.TimeNow = TimeNow;
AckEvent.LargestAck = PacketNum - 1;
AckEvent.LargestSentPacketNumber = PacketNum;
AckEvent.NumRetransmittableBytes = TotalBytes;
// ... set packet metadata if needed ...

// Process ACK
Cc->QuicCongestionControlOnDataAcknowledged(Cc, &AckEvent);

// Verify state
ASSERT_EQ(Bbr->BbrState, ExpectedState);
```
