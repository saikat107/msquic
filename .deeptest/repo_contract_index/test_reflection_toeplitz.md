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

1. **DirectHashComputation** - Test `CxPlatToeplitzHashCompute` with known input/output ✅
2. **ComputeAddrIPv4** - Test single IPv4 address hashing ✅
3. **ComputeAddrIPv6** - Test single IPv6 address hashing ✅
4. **HashWithOffset** - Test offset parameter behavior ✅
5. **XorCompositionProperty** - Validate XOR composition ✅
6. **ZeroLengthInput** - Edge case testing ✅
7. **MaximumOffset** - Boundary condition testing ✅
8. **QuicInputSize** - Test with CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC ✅
9. **DeterminismCheck** - Verify hash consistency ✅
10. **PartialInputHashing** - Test with HashInputLength < full input ✅
11. **SingleByteHashing** - Test single byte inputs ✅

Each test will be added iteratively with quality validation.

---

## New Tests Generated

### Test 3: DirectHashComputation
**Location**: ToeplitzTest.cpp, after IPv6WithTcp test  
**Scenario**: Tests CxPlatToeplitzHashCompute directly with raw byte arrays  
**Public APIs Tested**: `CxPlatToeplitzHashInitialize`, `CxPlatToeplitzHashCompute`  
**Coverage**:
- Direct API testing without wrapper functions
- Tests with simple 4-byte sequences
- Validates determinism
- Tests edge cases: all zeros, all ones
- Tests different inputs produce different hashes

**Contract Reasoning**:
- Toeplitz structure properly initialized before use
- HashInput arrays are valid
- HashInputLength + HashInputOffset ≤ InputSize maintained
- No precondition violations

**Expected Lines Covered**:
- `toeplitz.c`: Lines 58-132 (initialization)
- `toeplitz.c`: Lines 139-167 (compute function core loop)

**Quality Metrics**:
- Strong assertions: Checks specific values (0, non-zero, inequality)
- Determinism validated by repeating computation
- Edge cases covered (all zeros, all ones)

**Non-Redundancy**: First test to directly call CxPlatToeplitzHashCompute, not through wrapper functions

---

### Test 4: ComputeAddrIPv4
**Location**: ToeplitzTest.cpp, after DirectHashComputation  
**Scenario**: Tests CxPlatToeplitzHashComputeAddr for single IPv4 address  
**Public APIs Tested**: `CxPlatToeplitzHashComputeAddr`  
**Coverage**:
- First test of ComputeAddr helper function
- IPv4 code path (lines 107-118 in quic_toeplitz.h)
- Validates offset calculation (6 bytes for IPv4)
- Tests determinism of address hashing

**Contract Reasoning**:
- Addr has valid IPv4 address family
- Key and Offset pointers are valid
- Toeplitz structure initialized before use

**Expected Lines Covered**:
- `quic_toeplitz.h`: Lines 107-118 (ComputeAddr IPv4 branch)
- `toeplitz.c`: Lines 139-167 (called indirectly)

**Quality Metrics**:
- Specific offset validation (ASSERT_EQ(Offset, 6u))
- Hash non-zero check (ASSERT_NE)
- Determinism verification

**Non-Redundancy**: First test of ComputeAddr function, complements ComputeRss tests

---

### Test 5: ComputeAddrIPv6
**Location**: ToeplitzTest.cpp, after ComputeAddrIPv4  
**Scenario**: Tests CxPlatToeplitzHashComputeAddr for single IPv6 address  
**Public APIs Tested**: `CxPlatToeplitzHashComputeAddr`  
**Coverage**:
- IPv6 code path (lines 120-131 in quic_toeplitz.h)
- Validates offset calculation (18 bytes for IPv6)

**Contract Reasoning**:
- Addr has valid IPv6 address family
- All preconditions maintained

**Expected Lines Covered**:
- `quic_toeplitz.h`: Lines 120-131 (ComputeAddr IPv6 branch)

**Quality Metrics**:
- Specific offset validation (ASSERT_EQ(Offset, 18u))
- Covers alternative code path from ComputeAddrIPv4

**Non-Redundancy**: Covers IPv6 path in ComputeAddr, complementary to IPv4 test

---

### Test 6: HashWithOffset
**Location**: ToeplitzTest.cpp, after ComputeAddrIPv6  
**Scenario**: Tests CxPlatToeplitzHashCompute with non-zero offset parameter  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Exercises HashInputOffset parameter
- Tests that same data at different offsets produces different hashes
- Validates offset logic in lookup table indexing

**Contract Reasoning**:
- Offsets + data length within bounds
- Precondition: HashInputLength + HashInputOffset ≤ InputSize

**Expected Lines Covered**:
- `toeplitz.c`: Line 151 (BaseOffset calculation)
- `toeplitz.c`: Lines 159-164 (loop with offset-based indexing)

**Quality Metrics**:
- Tests multiple offsets (0, 4, 8)
- Validates all produce different hashes
- Specific inequality assertions

**Non-Redundancy**: First test to explicitly exercise HashInputOffset parameter

---

### Test 7: XorCompositionProperty
**Location**: ToeplitzTest.cpp, after HashWithOffset  
**Scenario**: Validates the XOR composition property of Toeplitz hash  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Validates mathematical property: Hash(A||B) = Hash(A) XOR Hash(B, offset=len(A))
- Tests composition of hashes across boundaries

**Contract Reasoning**:
- Concatenated inputs properly constructed
- Offsets correctly calculated

**Expected Lines Covered**:
- `toeplitz.c`: Lines 139-167 (validates compositional behavior)

**Quality Metrics**:
- Tests fundamental algorithm property
- Specific equality check (ASSERT_EQ)
- Would detect algorithm bugs in composition

**Non-Redundancy**: Only test validating XOR composition property

---

### Test 8: ZeroLengthInput
**Location**: ToeplitzTest.cpp, after XorCompositionProperty  
**Scenario**: Tests edge case of zero-length input  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Edge case: HashInputLength = 0
- Loop should not execute (lines 159-164)
- Result should be 0

**Contract Reasoning**:
- Zero length is valid input
- Offset can be any valid value when length is 0

**Expected Lines Covered**:
- `toeplitz.c`: Line 152 (Result = 0 initialization)
- `toeplitz.c`: Line 166 (return Result without loop execution)

**Quality Metrics**:
- Multiple zero-length tests at different offsets
- All validate result is exactly 0

**Non-Redundancy**: Only test of zero-length edge case

---

### Test 9: MaximumOffset
**Location**: ToeplitzTest.cpp, after ZeroLengthInput  
**Scenario**: Tests boundary condition with maximum valid offset  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Boundary testing: offset at maximum allowed value
- Ensures no off-by-one errors in bounds checking

**Contract Reasoning**:
- MaxOffset = InputSize - HashInputLength (valid boundary)
- Precondition satisfied

**Expected Lines Covered**:
- `toeplitz.c`: Lines 154-157 (CXPLAT_DBG_ASSERT checks)
- Tests assertion boundaries without triggering them

**Quality Metrics**:
- Boundary value testing
- Determinism check at boundary

**Non-Redundancy**: Only test of maximum offset boundary

---

### Test 10: QuicInputSize
**Location**: ToeplitzTest.cpp, after MaximumOffset  
**Scenario**: Tests initialization with CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC (38 bytes)  
**Public APIs Tested**: `CxPlatToeplitzHashInitialize`, `CxPlatToeplitzHashCompute`  
**Coverage**:
- Tests with larger input size (38 vs 36)
- Validates that QUIC input size variant works
- Tests hashing at offsets specific to QUIC use case

**Contract Reasoning**:
- InputSize set to QUIC value (38)
- Offsets and lengths valid for 38-byte input

**Expected Lines Covered**:
- `toeplitz.c`: Lines 78-131 (initialization loop with different InputSize)

**Quality Metrics**:
- Tests hashing at CID offset (0), IP offset (20), port offset (36)
- Validates all produce different hashes
- Specific to QUIC use case

**Non-Redundancy**: Only test using CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC

---

### Test 11: DeterminismCheck
**Location**: ToeplitzTest.cpp, after QuicInputSize  
**Scenario**: Validates hash consistency across multiple invocations  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Robustness testing: 10 iterations with same input
- Validates no hidden state or randomness

**Contract Reasoning**:
- Same input, same Toeplitz structure
- Pure function behavior expected

**Expected Lines Covered**:
- All of `toeplitz.c`: Lines 139-167 (multiple times)

**Quality Metrics**:
- 10 iterations for statistical confidence
- All results must match exactly

**Non-Redundancy**: Only dedicated determinism test

---

### Test 12: PartialInputHashing
**Location**: ToeplitzTest.cpp, after DeterminismCheck  
**Scenario**: Tests hashing partial inputs with varying HashInputLength  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Varies HashInputLength parameter
- Tests 4, 8, 12, 16 byte lengths
- Validates XOR composition across boundaries

**Contract Reasoning**:
- All lengths are valid subsets of buffer
- Offsets and lengths within bounds

**Expected Lines Covered**:
- `toeplitz.c`: Lines 159-164 (loop with different iteration counts)

**Quality Metrics**:
- Tests 4 different lengths
- Validates XOR composition property
- All pairs tested for inequality

**Non-Redundancy**: Only systematic test of varying HashInputLength

---

### Test 13: SingleByteHashing
**Location**: ToeplitzTest.cpp, after PartialInputHashing  
**Scenario**: Tests hashing of single bytes at various positions  
**Public APIs Tested**: `CxPlatToeplitzHashCompute`  
**Coverage**:
- Minimum valid input length (1 byte)
- Position-dependent hashing
- Tests that single byte at different offsets produces different hashes

**Contract Reasoning**:
- 1 byte is valid input length
- Different offsets are valid

**Expected Lines Covered**:
- `toeplitz.c`: Lines 159-164 (loop executes twice for 1 byte = 2 nibbles)

**Quality Metrics**:
- Tests minimum input size
- Validates position dependency
- Three different positions tested

**Non-Redundancy**: Only test of single-byte minimum input

---

## Coverage Analysis

With these 13 tests (2 existing + 11 new), we achieve comprehensive coverage:

### Public APIs Covered:
✅ `CxPlatToeplitzHashInitialize` - Multiple tests  
✅ `CxPlatToeplitzHashCompute` - Extensive testing with many variations  
✅ `CxPlatToeplitzHashComputeAddr` - IPv4 and IPv6 paths  
✅ `CxPlatToeplitzHashComputeRss` - Existing tests (IPv4WithTcp, IPv6WithTcp)

### Code Paths Covered:
✅ Initialization loop (lines 78-131)  
✅ Compute loop (lines 159-164)  
✅ All branches in inline helpers  
✅ Zero-length edge case  
✅ Boundary conditions  
✅ Both input sizes (36 and 38 bytes)

### Algorithm Properties Validated:
✅ Determinism  
✅ XOR composition  
✅ Position dependency  
✅ RSS compliance (from existing tests)

**Estimated Coverage**: 100% of toeplitz.c, 100% of public API in quic_toeplitz.h

