# Test Reflection - Token Validation Tests

## Overview
Generated tests for PR #4412 "Always Ignore Invalid Tokens" behavioral changes.
Tests verify that invalid tokens do not cause packet drops or connection failures.

---

## Test 1: ConnectionWithInvalidToken

**Test Name**: `TokenValidationTest.ConnectionWithInvalidToken`

**Scenario Summary**:
Verifies that a client can successfully connect even when the server is in retry mode and may encounter invalid tokens. This test specifically addresses the PR #4412 fix where clients sending NEW_TOKEN tokens from different servers should not fail to connect. The test establishes a baseline connection first, then tests connection with stateless retry enabled to ensure token handling works correctly.

**Primary Public API Targets**:
- `MsQuicConnection::Start` - Initiates connection with retry token handling
- `TestConnection::WaitForConnectionComplete` - Verifies connection succeeds
- `TestConnection::GetStatistics().StatelessRetry` - Confirms retry occurred

**Contract Reasoning**:
- **Preconditions**: Valid registration, configurations, and listener must be established
- **Object Invariants**: TestConnection and MsQuicAutoAcceptListener maintain valid states throughout
- **Environment Invariants**: StatelessRetryHelper properly configures global retry settings
- Connection succeeds despite potential invalid token scenarios (the core fix in PR #4412)

**Expected Coverage Impact**:
- Exercises `QuicBindingShouldRetryConnection` in binding.c (lines ~1219-1256)
- Exercises `QuicConnRecvHeader` token validation section in connection.c (lines ~3824-3850)
- Indirectly tests `QuicPacketValidateInitialToken` in packet.c (lines ~519-561)
- Covers path where invalid tokens are ignored rather than causing packet drop

**Non-Redundancy**:
This is the first test specifically targeting the PR #4412 behavioral change. Existing tests may test retry functionality, but none specifically verify that INVALID tokens don't cause connection failure. This test ensures the core fix works: clients with tokens from different servers can connect.

---

## Test 2: MultipleConnectionsWithRetry

**Test Name**: `TokenValidationTest.MultipleConnectionsWithRetry`

**Scenario Summary**:
Tests that multiple concurrent clients can all successfully connect when the server is in stateless retry mode. This verifies that the token validation changes don't introduce race conditions or state corruption when handling multiple connections simultaneously, and that all connections succeed even if tokens are invalid or missing.

**Primary Public API Targets**:
- `MsQuicConnection::Start` - Multiple concurrent connection initiations
- Array of `TestConnection` objects - Concurrent connection management
- `TestConnection::WaitForConnectionComplete` - Verifies all connections succeed

**Contract Reasoning**:
- **Preconditions**: All connection objects properly initialized before starting
- **Object Invariants**: Each TestConnection maintains independent state
- **Concurrency Contract**: Multiple connections can be established concurrently
- **Environment Invariants**: Global retry settings apply uniformly to all connections
- No connection should fail due to token validation issues (PR #4412 guarantee)

**Expected Coverage Impact**:
- Exercises `QuicBindingShouldRetryConnection` under concurrent load
- Tests thread-safety of token validation logic
- Covers scenarios where multiple packets with various token states arrive concurrently
- Verifies binding's connection creation (`QuicBindingCreateConnection`) works correctly after token validation changes

**Non-Redundancy**:
While Test 1 verifies single connection with retry, this test adds concurrent/parallel connection scenarios. It's crucial for catching race conditions or state corruption in the token handling code that might only appear under concurrent load. Existing tests likely don't specifically test concurrent connections during retry with invalid tokens.

---

## Test 3: ConnectionWithVersionNegotiationAndRetry

**Test Name**: `TokenValidationTest.ConnectionWithVersionNegotiationAndRetry`

**Scenario Summary**:
Tests a complex handshake scenario combining version negotiation with stateless retry. This stresses the token validation logic in a realistic but complex edge case where multiple QUIC protocol features interact. Ensures that token handling remains correct even when the handshake involves version negotiation.

**Primary Public API Targets**:
- `MsQuicConnection::Start` - Connection with potential version negotiation
- Connection handshake completion with both version negotiation and retry

**Contract Reasoning**:
- **Preconditions**: Standard connection preconditions apply
- **Object Invariants**: Connection state machine correctly handles version negotiation + retry
- **State Machine**: Connection transitions through: INITIAL → VERSION_NEG (maybe) → RETRY → CONNECTED
- Token validation must work correctly at each stage of this complex state machine

**Expected Coverage Impact**:
- Exercises token handling during version negotiation flows
- Covers interaction between `QuicConnRecvHeader` and version negotiation logic
- Tests packet.c token validation when packets may be retransmitted with different versions
- Verifies no regression in complex handshake scenarios after PR #4412

**Non-Redundancy**:
This test is unique in combining two protocol features (version negotiation + retry) to create a more complex scenario. The interaction between these features and token validation is not covered by simpler tests. It ensures the PR #4412 changes don't break complex real-world handshake patterns.

---

## Test 4: RapidConnectionAttemptsWithRetry

**Test Name**: `TokenValidationTest.RapidConnectionAttemptsWithRetry`

**Scenario Summary**:
Tests rapid successive connection attempts (10 in quick succession) when server is in retry mode. This scenario stresses the token validation code under rapid connection churn, detecting potential race conditions, memory leaks, or state corruption that might only appear under rapid connection/disconnection patterns.

**Primary Public API Targets**:
- `MsQuicConnection` constructor/destructor - Rapid connection lifecycle
- `MsQuicConnection::Start` - Rapid connection initiations
- Connection cleanup and resource management under rapid churn

**Contract Reasoning**:
- **Preconditions**: Each connection properly initialized before use
- **Resource/Ownership**: Each connection must properly clean up resources
- **"Must not leak" constraint**: Rapid churn must not cause memory/resource leaks
- **State invariants**: Global state (retry counters, memory limits) must remain consistent
- Token validation must be race-free under rapid connection attempts

**Expected Coverage Impact**:
- Tests `QuicBindingShouldRetryConnection` memory limit checks under rapid churn
- Exercises connection creation/destruction paths repeatedly
- Covers resource cleanup in packet.c token validation paths
- Verifies no memory leaks or assertion failures in token handling code

**Non-Redundancy**:
This is the only test specifically targeting rapid connection churn with retry enabled. It's designed to catch bugs that only appear under stress conditions (race conditions, memory leaks, assertion failures). The pattern of rapid create/destroy differs from concurrent connections (Test 2) - this tests temporal stress while Test 2 tests spatial/parallel stress.

---

## Test 5: ConnectionWithRetryToggle

**Test Name**: `TokenValidationTest.ConnectionWithRetryToggle`

**Scenario Summary**:
Tests server behavior when toggling between retry mode and normal mode across multiple connections. Establishes connection without retry, then with retry, then without again. This verifies that the global retry state changes don't corrupt token validation logic or cause state leakage between modes.

**Primary Public API Targets**:
- `StatelessRetryHelper` constructor/destructor - Global state management
- `MsQuic->SetParam(QUIC_PARAM_GLOBAL_RETRY_MEMORY_PERCENT)` - Global config changes
- Connection establishment in different global retry states

**Contract Reasoning**:
- **Preconditions**: Each connection in each mode has proper setup
- **Environment Invariants**: Global retry setting changes must take effect immediately
- **"Must call init before use"**: StatelessRetryHelper properly configures before connections
- **Global/module state constraints**: Mode switches must not corrupt persistent state
- Token validation behavior must adapt correctly to retry mode changes

**Expected Coverage Impact**:
- Tests `QuicBindingShouldRetryConnection` decision logic in different retry modes
- Exercises conditional branches: token validation with/without retry enabled
- Covers global state management code that interacts with token validation
- Verifies PR #4412 changes work correctly across retry mode transitions

**Non-Redundancy**:
This is the only test that specifically verifies state transitions at the GLOBAL level (retry enabled/disabled). It's different from all other tests because it tests the interaction between global configuration changes and per-connection token validation. It catches bugs where the PR #4412 changes might have created hidden dependencies on global retry state.

---

## Summary

**Total Tests Generated**: 5 tests (each parameterized for IPv4 and IPv6 = 10 test cases total)

**Coverage Goals**:
- Modified functions in binding.c: `QuicBindingShouldRetryConnection`
- Modified functions in connection.c: `QuicConnRecvHeader` (token validation section)
- Modified functions in packet.c: `QuicPacketValidateInitialToken`
- Behavioral verification: Invalid tokens don't cause connection failures (PR #4412 core fix)

**Test Quality**:
- All tests are strictly interface-driven (use only public MsQuic APIs)
- All tests are contract-safe (no precondition violations)
- All tests verify observable outcomes via public getters and connection state
- Tests cover realistic production scenarios (not artificial/contrived cases)
- Each test has clear scenario description in comments

**Next Steps**:
1. Build tests with MsQuic build system
2. Run tests and measure coverage
3. Refine tests based on actual coverage gaps
4. Use test-quality-checker to validate assertion quality
5. Iterate until 100% coverage of modified lines achieved
