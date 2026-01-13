# BBR Test Reorganization Plan

## Current Structure (Tests 1-44)

### CATEGORY 1: INITIALIZATION AND RESET TESTS (Tests 1-2 → 1-2)
- Test 1: Comprehensive initialization verification
- Test 2: Initialization with boundary parameter values

### CATEGORY 2: BASIC DATA SENDING AND ACKNOWLEDGMENT TESTS (Tests 3-4 → 3-4)
- Test 3: OnDataSent updates BytesInFlight
- Test 4: OnDataInvalidated decreases BytesInFlight

### CATEGORY 3: LOSS DETECTION AND RECOVERY TESTS (Tests 5-7 → 5-7, ADD 35, 42 → 5-9)
- Test 5: OnDataLost enters recovery
- Test 6: Reset returns to STARTUP
- Test 7: Reset with FullReset=FALSE preserves BytesInFlight
- **NEW Test 8** (was 35): Recovery Window Non-Zero During Recovery
- **NEW Test 9** (was 42): Recovery GROWTH State - Window Expansion

### CATEGORY 4: CONGESTION CONTROL WINDOW AND SENDING TESTS (Tests 8-10 → 10-12, ADD 36, 39 → 10-14)
- Test 10 (was 8): CanSend respects congestion window
- Test 11 (was 9): Exemptions bypass congestion control
- Test 12 (was 10): OnSpuriousCongestionEvent returns FALSE
- **NEW Test 13** (was 36): Send Allowance When Fully CC Blocked
- **NEW Test 14** (was 39): ACK Aggregation in Target Window Calculation

### CATEGORY 5: PACING AND SEND ALLOWANCE TESTS (Tests 11-13, 20 → 15-18, ADD 37, 38, 43, 44 → 15-22)
- Test 15 (was 11): Basic OnDataAcknowledged without state transition
- Test 16 (was 12): GetSendAllowance with pacing disabled
- Test 17 (was 13): GetSendAllowance with pacing enabled
- Test 18 (was 20): Pacing with high bandwidth for send quantum tiers
- **NEW Test 19** (was 37): Bandwidth-Based Send Allowance Calculation (Non-STARTUP)
- **NEW Test 20** (was 38): High Pacing Rate Send Quantum
- **NEW Test 21** (was 43): Send Allowance Bandwidth-Only Path (Non-STARTUP)
- **NEW Test 22** (was 44): High Pacing Rate Send Quantum - Large Burst Size

### CATEGORY 6: NETWORK STATISTICS AND MONITORING TESTS (Tests 14, 21 → 23-24)
- Test 23 (was 14): GetNetworkStatistics through callback
- Test 24 (was 21): NetStatsEvent triggers GetNetworkStatistics and LogOutFlowStatus

### CATEGORY 7: FLOW CONTROL AND STATE FLAG TESTS (Tests 15-17 → 25-27)
- Test 25 (was 15): CanSend with flow control unblocking
- Test 26 (was 16): ExitingQuiescence flag set
- Test 27 (was 17): Recovery window growth during RECOVERY

### CATEGORY 8: EDGE CASES AND ERROR HANDLING TESTS (Tests 19, 22-24, 28 → 28-32, ADD 40, 41 → 28-34)
- Test 28 (was 19): Zero-length packet handling in OnDataAcknowledged
- Test 29 (was 22): Persistent congestion resets to minimum window
- Test 30 (was 23): SetAppLimited when congestion-limited (early return)
- Test 31 (was 24): Recovery exit when ACKing packet >= EndOfRecovery
- Test 32 (was 28): Backwards timestamp and zero elapsed time in bandwidth calculation
- **NEW Test 33** (was 40): ACK Timing Edge Case - Fallback to Raw AckTime
- **NEW Test 34** (was 41): Invalid Delivery Rates - Both UINT64_MAX Skip

### CATEGORY 9: PUBLIC API COVERAGE TESTS (Tests 25-27 → 35-37)
- Test 35 (was 25): Call GetBytesInFlightMax function pointer
- Test 36 (was 26): Pacing disabled when setting is OFF
- Test 37 (was 27): Implicit ACK with NetStatsEventEnabled triggers stats

### CATEGORY 10: APP-LIMITED DETECTION TESTS (Test 18 → 38)
- Test 38 (was 18): SetAppLimited success path

### CATEGORY 11: STATE MACHINE TRANSITION TESTS (Tests 29-34 → 39-44)
- Test 39 (was 29): T3 - STARTUP → PROBE_RTT transition via RTT sample expiration
- Test 40 (was 30): T3 - PROBE_BW → PROBE_RTT transition via RTT sample expiration
- Test 41 (was 31): T4 - PROBE_RTT → PROBE_BW transition after probe completion with BtlbwFound
- Test 42 (was 32): T5 - PROBE_RTT → STARTUP transition after probe completion without BtlbwFound
- Test 43 (was 33): T3 - DRAIN → PROBE_RTT transition via RTT expiration
- Test 44 (was 34): PROBE_BW Gain Cycle Drain - Target Reached

## Summary
- Total tests: 44
- Reorganized into 11 logical categories
- All tests renumbered sequentially 1-44
- Tests 35-44 (previously miscategorized) now properly integrated
- Each category has a clear group header (no individual test category comments)
