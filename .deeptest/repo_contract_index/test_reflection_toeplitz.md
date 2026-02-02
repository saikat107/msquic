# Test Reflection Document - Toeplitz Component

## Existing Test Coverage Analysis

### Existing Test 1: `IPv4WithTcp`
**Location**: ToeplitzTest.cpp, lines 109-142  
**Scenario**: Validates IPv4 RSS hash computation against known test vectors  
**Public APIs Tested**: `CxPlatToeplitzHashComputeRss`  
**Coverage**:
- Initializes Toeplitz structure with standard RSS key
- Tests 5 different IPv4 source/destination address pairs
- Validates hash output matches expected values from RSS specification
- Exercises IPv4 code path in `CxPlatToeplitzHashComputeRss`

**Contract Reasoning**:
- Ensures SrcAddr and DestAddr have same address family (IPv4)
- Key is properly initialized before use
- Uses valid port numbers and IP addresses

**Expected Lines Covered**:
- `toeplitz.c`: Lines 58-132 (initialization)
- `toeplitz.c`: Lines 139-167 (hash computation)
- `quic_toeplitz.h`: Lines 147-171 (ComputeRss IPv4 branch)

**Non-Redundancy**: First test, establishes baseline for IPv4 RSS hashing

---

### Existing Test 2: `IPv6WithTcp`
**Location**: ToeplitzTest.cpp, lines 144-171  
**Scenario**: Validates IPv6 RSS hash computation against known test vectors  
**Public APIs Tested**: `CxPlatToeplitzHashComputeRss`  
**Coverage**:
- Initializes Toeplitz structure with standard RSS key
- Tests 3 different IPv6 source/destination address pairs
- Validates hash output matches expected values from RSS specification
- Exercises IPv6 code path in `CxPlatToeplitzHashComputeRss`

**Contract Reasoning**:
- Ensures SrcAddr and DestAddr have same address family (IPv6)
- Key is properly initialized before use
- Uses valid IPv6 addresses and port numbers

**Expected Lines Covered**:
- `toeplitz.c`: Lines 58-132 (initialization)
- `toeplitz.c`: Lines 139-167 (hash computation)
- `quic_toeplitz.h`: Lines 172-195 (ComputeRss IPv6 branch)

**Non-Redundancy**: Covers IPv6 code path, complementary to IPv4 test

---

## Coverage Gaps Identified

### Uncovered Public APIs:
1. **`CxPlatToeplitzHashInitialize`** - Not directly tested (only called as setup)
2. **`CxPlatToeplitzHashCompute`** - Not directly tested (only through ComputeRss wrapper)
3. **`CxPlatToeplitzHashComputeAddr`** - Not tested at all

### Uncovered Scenarios:
1. Direct hash computation with raw byte arrays
2. Hash computation with different offsets (HashInputOffset parameter)
3. Zero-length hash input
4. Maximum input size boundaries
5. XOR composition property validation
6. Determinism verification (multiple calls with same input)
7. Single address hashing via ComputeAddr
8. Different InputSize values (only IP=36 tested, QUIC=38 not tested)
9. Partial input hashing (using HashInputLength < full size)

### Uncovered Code Regions (estimated):
- `toeplitz.c`: Lines 150-164 (loop body details, edge cases)
- Boundary conditions in table lookups
- Offset calculation paths
- All of `CxPlatToeplitzHashComputeAddr` inline function

---

## Test Generation Plan

To achieve 100% coverage, we need tests for:

### Priority 1 - Direct API Testing:
1. **Test `CxPlatToeplitzHashCompute` directly** with raw byte arrays
2. **Test `CxPlatToeplitzHashComputeAddr`** for IPv4 and IPv6
3. **Test different HashInputOffset values** to exercise offset logic

### Priority 2 - Scenario Coverage:
4. **XOR composition property** - verify hash(A) XOR hash(B) = hash(A||B)
5. **Determinism test** - same input produces same hash consistently
6. **Zero-length input** - edge case for empty hash
7. **Boundary testing** - max offset, max input length
8. **InputSize variations** - test with QUIC size (38 bytes) vs IP size (36 bytes)

### Priority 3 - Additional Coverage:
9. **Partial hashing** - hash subsets of input using HashInputLength
10. **Loop iteration boundaries** - ensure all nibbles processed correctly

---

## Next Tests to Generate

Starting with high-impact, gap-filling tests:

1. **DirectHashComputation** - Test `CxPlatToeplitzHashCompute` with known input/output
2. **ComputeAddrIPv4** - Test single IPv4 address hashing
3. **ComputeAddrIPv6** - Test single IPv6 address hashing
4. **HashWithOffset** - Test offset parameter behavior
5. **XorCompositionProperty** - Validate XOR composition
6. **ZeroLengthInput** - Edge case testing
7. **MaximumOffset** - Boundary condition testing
8. **QuicInputSize** - Test with CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC
9. **DeterminismCheck** - Verify hash consistency
10. **PartialInputHashing** - Test with HashInputLength < full input

Each test will be added iteratively with quality validation.

