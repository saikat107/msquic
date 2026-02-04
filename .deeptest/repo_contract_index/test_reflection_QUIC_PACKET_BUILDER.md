# Test Reflection - QUIC_PACKET_BUILDER

This document records the reasoning and coverage expectations for each test generated for the packet_builder component.

## Test 1: InitializeSuccess
**Scenario:** Successful initialization with all required preconditions met (valid connection, path, source CID available).

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Preconditions: Connection != NULL, Path != NULL, Path->DestCid != NULL, Connection->SourceCids.Next != NULL
- All preconditions are explicitly satisfied in the test setup
- No invariants violated

**Expected Coverage:**
- Lines 93-143 in packet_builder.c (QuicPacketBuilderInitialize function)
- Success path including send allowance calculation, source CID lookup, timestamp management

**Non-Redundancy:**
This is the foundational test establishing the happy path for initialization. All other tests depend on this working.

---

## Test 2: InitializeFailureNoSourceCid
**Scenario:** Initialization failure when connection has no source CID available.

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Tests documented failure condition: returns FALSE when Connection->SourceCids.Next == NULL
- This is a valid contract-defined error scenario (lines 109-115 in packet_builder.c)
- No precondition violation - API explicitly handles this case

**Expected Coverage:**
- Lines 93-115 in packet_builder.c (early return path when no source CID)
- Error logging path

**Non-Redundancy:**
First test to cover error handling path. Previous test only covered success.

---

## Test 3: InitializeSendAllowanceCalculation
**Scenario:** Send allowance calculation respects both congestion control and path allowance limits.

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Tests postcondition that SendAllowance is min(CongestionControlAllowance, Path.Allowance)
- Verifies lines 131-138 logic
- Tests boundary conditions (small path allowance, unlimited path allowance)

**Expected Coverage:**
- Lines 131-140 in packet_builder.c (send allowance computation and clamping)
- QuicCongestionControlGetSendAllowance interaction

**Non-Redundancy:**
Tests specific calculation logic not verified in basic initialization test. Covers multiple scenarios within the calculation.

---

## Test 4: CleanupNoBatchSent
**Scenario:** Cleanup when no packets were sent (no timer update needed).

**Primary API Target:** QuicPacketBuilderCleanup

**Contract Reasoning:**
- Tests that cleanup works when PacketBatchSent == FALSE
- Verifies HpMask secure zeroing (postcondition)
- Precondition SendData == NULL is implicitly satisfied

**Expected Coverage:**
- Lines 145-160 in packet_builder.c (QuicPacketBuilderCleanup)
- Path without loss detection timer update (line 153-155 skipped)
- Metadata release and secure zero

**Non-Redundancy:**
First test for Cleanup API. Tests the simpler path (no batch sent).

---

## Test 5: HasAllowancePositive
**Scenario:** Query returns TRUE when send allowance is positive.

**Primary API Target:** QuicPacketBuilderHasAllowance (inline)

**Contract Reasoning:**
- Pure query function, no side effects
- Tests return TRUE condition: SendAllowance > 0
- No preconditions beyond Builder != NULL

**Expected Coverage:**
- Inline function in packet_builder.h lines 236-239
- True branch of OR condition

**Non-Redundancy:**
First test of HasAllowance query. Tests simplest true condition.

---

## Test 6: HasAllowanceWithExemptions
**Scenario:** Query returns TRUE when allowance is zero but exemptions exist.

**Primary API Target:** QuicPacketBuilderHasAllowance (inline)

**Contract Reasoning:**
- Tests second condition: exemptions > 0 even when SendAllowance == 0
- Verifies congestion control integration
- Important for control frame sending when data is blocked

**Expected Coverage:**
- Inline function in packet_builder.h lines 236-239
- False branch of first condition, true branch of second condition
- QuicCongestionControlGetExemptions call

**Non-Redundancy:**
Tests exemption logic not covered in Test 5. Critical for control frame behavior.

---

## Test 7: HasAllowanceBlocked
**Scenario:** Query returns FALSE when both allowance and exemptions are zero.

**Primary API Target:** QuicPacketBuilderHasAllowance (inline)

**Contract Reasoning:**
- Tests complete blocking condition
- Both OR branches evaluate to false
- Represents congestion control blocking all sends

**Expected Coverage:**
- Inline function in packet_builder.h lines 236-239
- Both conditions false, return FALSE path

**Non-Redundancy:**
Completes coverage of HasAllowance by testing false return. Previous tests only covered true returns.

---

## Test 8: AddFrameWithRoom
**Scenario:** Successfully add a frame when packet has room for more frames.

**Primary API Target:** QuicPacketBuilderAddFrame (inline)

**Contract Reasoning:**
- Tests normal frame addition when FrameCount < MAX
- Verifies frame metadata recording
- Tests ack-eliciting flag propagation
- Postcondition: FrameCount incremented, frame recorded

**Expected Coverage:**
- Inline function in packet_builder.h lines 248-258
- Normal path: increment, record, return FALSE
- IsAckEliciting flag setting

**Non-Redundancy:**
First test of AddFrame API. Establishes basic frame addition behavior.

---

## Test 9: AddFrameReachingMax
**Scenario:** Add frames until reaching QUIC_MAX_FRAMES_PER_PACKET, verify TRUE return.

**Primary API Target:** QuicPacketBuilderAddFrame (inline)

**Contract Reasoning:**
- Tests boundary condition: FrameCount == QUIC_MAX_FRAMES_PER_PACKET
- Return value changes to TRUE to signal "packet full"
- Important for caller to know when to finalize packet

**Expected Coverage:**
- Inline function in packet_builder.h lines 248-258
- Boundary case: ++FrameCount == QUIC_MAX_FRAMES_PER_PACKET returns TRUE

**Non-Redundancy:**
Tests boundary behavior not covered in Test 8. Tests return TRUE path.

---

## Test 10: AddFrameNonAckEliciting
**Scenario:** Add a non-ack-eliciting frame (e.g., ACK or PADDING).

**Primary API Target:** QuicPacketBuilderAddFrame (inline)

**Contract Reasoning:**
- Tests IsAckEliciting == FALSE parameter
- Verifies flag NOT set when frame doesn't require acknowledgment
- Important for ACK and PADDING frames

**Expected Coverage:**
- Inline function in packet_builder.h lines 248-258
- Path where IsAckEliciting parameter is FALSE
- Flag NOT updated (line 256 with FALSE input)

**Non-Redundancy:**
Tests IsAckEliciting == FALSE case. Test 8 only tested TRUE case.

---

## Test 11: AddMultipleFrames
**Scenario:** Add multiple frames of mixed types, verify cumulative state.

**Primary API Target:** QuicPacketBuilderAddFrame (inline)

**Contract Reasoning:**
- Tests multiple sequential frame additions
- Verifies cumulative FrameCount and IsAckEliciting behavior
- Once IsAckEliciting set to TRUE, stays TRUE (OR semantics)

**Expected Coverage:**
- Inline function in packet_builder.h lines 248-258 (called multiple times)
- Cumulative state management
- Array indexing for multiple frames

**Non-Redundancy:**
Tests interaction of multiple frame additions. Previous tests only added 1-2 frames. Verifies cumulative state behavior.

---

## Test 12: InitializeWithDifferentMtu
**Scenario:** Initialize with various MTU values (minimum, default, large).

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Tests that initialization handles different Path MTU values
- MTU affects send allowance calculation and datagram sizing
- All MTU values within valid range

**Expected Coverage:**
- Lines 93-143 with various MTU values
- Different paths through send allowance calculation based on MTU

**Non-Redundancy:**
Tests MTU sensitivity not covered in basic initialization. Important for PMTUD scenarios.

---

## Test 13: InitializeWithVersion2
**Scenario:** Initialize with QUIC_VERSION_2 instead of QUIC_VERSION_1.

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Tests version independence of initialization
- QUIC v2 has different packet encodings but same initialization
- Verifies no v1-specific assumptions in Initialize

**Expected Coverage:**
- Lines 93-143 with QuicVersion == QUIC_VERSION_2
- Version field propagation

**Non-Redundancy:**
Tests version handling. All previous tests used QUIC_VERSION_1.

---

## Test 14: MetadataPointerInitialization
**Scenario:** Verify Metadata pointer correctly points to embedded MetadataStorage.

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Tests postcondition: Metadata == &MetadataStorage.Metadata
- Verifies memory layout correctness
- Important for preventing dangling pointers

**Expected Coverage:**
- Line 105 in packet_builder.c (Metadata pointer assignment)
- Verifies pointer arithmetic and struct layout

**Non-Redundancy:**
Tests specific pointer relationship not explicitly verified in other tests. Important for memory safety.

---

## Test 15: BatchFieldsInitialization
**Scenario:** Verify batch-related fields are properly initialized.

**Primary API Target:** QuicPacketBuilderInitialize

**Contract Reasoning:**
- Tests specific postconditions for batch fields
- Verifies flags initialized to FALSE
- Important for header protection batching logic

**Expected Coverage:**
- Lines 102-104 in packet_builder.c (flag initialization)
- Batch-related field defaults

**Non-Redundancy:**
Tests specific field initialization not explicitly asserted in other tests. Important for batch send behavior.

---

## Coverage Summary

**Public APIs Tested:**
1. ✅ QuicPacketBuilderInitialize - comprehensive (success, failure, various conditions)
2. ✅ QuicPacketBuilderCleanup - basic (no batch sent scenario)
3. ❌ QuicPacketBuilderPrepareForControlFrames - NOT TESTED (requires mock SendData allocation)
4. ❌ QuicPacketBuilderPrepareForPathMtuDiscovery - NOT TESTED (requires mock SendData allocation)
5. ❌ QuicPacketBuilderPrepareForStreamFrames - NOT TESTED (requires mock SendData allocation)
6. ❌ QuicPacketBuilderFinalize - NOT TESTED (requires complex packet/datagram state)
7. ✅ QuicPacketBuilderHasAllowance - complete (all branches)
8. ✅ QuicPacketBuilderAddFrame - comprehensive (normal, boundary, ack-eliciting variants)
9. ❌ QuicPacketBuilderAddStreamFrame - NOT TESTED (requires QUIC_STREAM mock)

**Lines Covered (Estimated):**
- QuicPacketBuilderInitialize: ~90% (lines 93-143, missing some edge cases)
- QuicPacketBuilderCleanup: ~60% (lines 145-160, missing PacketBatchSent==TRUE path)
- QuicPacketBuilderHasAllowance: 100% (inline, all branches)
- QuicPacketBuilderAddFrame: 100% (inline, all branches)

**Lines NOT Covered:**
- Lines 162-489: QuicPacketBuilderPrepare (internal, complex allocation/crypto logic)
- Lines 491-612: QuicPacketBuilderGetPacketTypeAndKeyForControlFrames (internal)
- Lines 617-677: Prepare* wrapper functions (require mock bindings/datapath)
- Lines 679-1099: Finalize, encryption, header protection, send logic

**Reason for Incomplete Coverage:**
The Prepare*/Finalize functions require extensive mocking infrastructure:
- CxPlatSendDataAlloc/CxPlatSendDataAllocBuffer (datapath layer)
- CxPlatEncrypt/CxPlatHpComputeMask (crypto layer)
- QuicBindingSend (binding layer)
- Real packet keys with crypto material

These are integration-level tests better suited for test/bin/ (functional tests with real MsQuic library) rather than unittest/ (low-level unit tests). The current tests cover all **low-level utility functions** that can be tested in isolation (Initialize, Cleanup, query functions, inline helpers).

**Contract Safety:**
All 15 tests are contract-safe:
- No NULL pointer dereferences to required parameters
- No precondition violations
- No invalid state transitions
- All test scenarios use valid public API sequences
- Mock structures satisfy all asserted invariants
