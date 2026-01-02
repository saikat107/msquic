# CUBIC Test Refactoring Required

## Issue
The newly added tests (Tests 18-33) directly manipulate internal CUBIC state using pointer access (e.g., `Cubic->BytesInFlight = 5000`). This violates encapsulation and can break object invariants.

## Current State Analysis
**Direct State Modifications Found:**
- BytesInFlight: 31 instances
- SlowStartThreshold: 9 instances  
- IsInRecovery: 5 instances
- TimeOfCongAvoidStart: 5 instances
- Exemptions, AimdWindow, WindowMax, WindowPrior, etc.: Multiple instances

## Public API Functions Available
Tests should ONLY use these functions:
1. CubicCongestionControlInitialize() - Initialize CC state
2. QuicCongestionControlOnDataSent() - Simulate sending data
3. QuicCongestionControlOnDataAcknowledged() - Simulate ACKs
4. QuicCongestionControlOnDataLost() - Simulate packet loss
5. QuicCongestionControlOnEcn() - Simulate ECN feedback
6. QuicCongestionControlOnSpuriousCongestionEvent() - Rollback spurious events
7. QuicCongestionControlReset() - Reset CC state
8. QuicCongestionControlGetSendAllowance() - Query send allowance
9. QuicCongestionControlCanSend() - Query if can send
10. Getter functions (GetCongestionWindow, GetExemptions, etc.)

## Recommended Actions

### Option 1: Remove Problematic Tests (RECOMMENDED)
Remove tests 19-31 that cannot easily reach required states via public API:
- Test 19-20: HyStart state transitions  
- Test 21-23: Recovery and persistent congestion states
- Test 25-31: Pacing, AIMD, overflow scenarios requiring specific internal states

**Keep only:** Tests 1-18, 24, 32-33 (those using proper API or minimal state inspection)

### Option 2: Refactor Tests (COMPLEX)
Refactor each test to build up state through proper API call sequences:
- Use OnDataSent() to increase BytesInFlight
- Use OnDataAcknowledged() to decrease BytesInFlight and grow window
- Use OnDataLost() to trigger congestion events
- May not be possible to reach all internal states this way

## Next Steps
1. Review tests 1-17 for any direct state manipulation
2. Remove or refactor tests 19-31
3. Renumber remaining tests sequentially  
4. Document test scenarios that cannot be tested via public API
5. Consider requesting internal test helpers if certain scenarios are critical

## Files to Modify
- src/core/unittest/CubicTest.cpp
