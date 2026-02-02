# Repository Contract Index (RCI) - Toeplitz Hash Component

## Component Overview

**Component Name**: CXPLAT_TOEPLITZ (Toeplitz Hash)  
**Source File**: `src/platform/toeplitz.c`  
**Header File**: `src/inc/quic_toeplitz.h`  
**Purpose**: Implements the Toeplitz Hash Algorithm for RSS (Receive Side Scaling) and load balancing purposes in QUIC connections.

## Algorithm Summary

The Toeplitz hash is a keyed hash function that processes input bit-by-bit, but optimized to process 4 bits (nibbles) at a time using pre-computed lookup tables. Key properties:
- Output is always 32 bits
- Key length is (input_size + 32 bits) in bytes
- Hash has XOR composition property: hash(A||B) = hash(A) XOR hash(B)
- Processes input from MSB to LSB (left to right)

---

## Public API Inventory

### 1. `CxPlatToeplitzHashInitialize`

**Signature**:
```c
void CxPlatToeplitzHashInitialize(
    _Inout_ CXPLAT_TOEPLITZ_HASH* Toeplitz
);
```

**Declaration**: `src/inc/quic_toeplitz.h`, line 77-79  
**Purpose**: Initializes the Toeplitz hash structure by building lookup tables from the hash key.

**Preconditions**:
- `Toeplitz` must not be NULL
- `Toeplitz->HashKey` must be populated with valid key bytes (length determined by `InputSize`)
- `Toeplitz->InputSize` must be set to a valid `CXPLAT_TOEPLITZ_INPUT_SIZE` value:
  - `CXPLAT_TOEPLITZ_INPUT_SIZE_IP` (36 bytes) - for IP/port hashing
  - `CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC` (38 bytes) - for QUIC CID hashing
- HashKey must have at least `(InputSize + 4)` bytes valid data

**Postconditions**:
- `Toeplitz->LookupTableArray` is fully populated with pre-computed values
- Each lookup table contains 16 entries for nibble-based lookups
- Total tables = `InputSize * 2` (two nibbles per byte)
- Hash structure is ready for `CxPlatToeplitzHashCompute` calls

**Side Effects**:
- Mutates the `LookupTableArray` field of the `Toeplitz` structure
- No external/global state modified
- No I/O or logging

**Error Handling**:
- No return value (void function)
- No explicit error checking in the implementation
- Relies on caller to ensure preconditions

**Thread Safety**:
- NOT thread-safe for concurrent initialization of the same `Toeplitz` object
- Multiple threads can initialize different `Toeplitz` objects concurrently
- Once initialized, the structure can be read concurrently by `CxPlatToeplitzHashCompute`

**Resource/Ownership**:
- Caller owns the `CXPLAT_TOEPLITZ_HASH` structure
- No dynamic allocation performed
- No cleanup/finalization required

---

### 2. `CxPlatToeplitzHashCompute`

**Signature**:
```c
uint32_t CxPlatToeplitzHashCompute(
    _In_ const CXPLAT_TOEPLITZ_HASH* Toeplitz,
    _In_reads_(HashInputLength) const uint8_t* HashInput,
    _In_ uint32_t HashInputLength,
    _In_ uint32_t HashInputOffset
);
```

**Declaration**: `src/inc/quic_toeplitz.h`, lines 86-93  
**Purpose**: Computes a 32-bit Toeplitz hash over a byte array input using pre-computed lookup tables.

**Preconditions**:
- `Toeplitz` must not be NULL
- `Toeplitz` must have been initialized via `CxPlatToeplitzHashInitialize`
- `HashInput` must not be NULL if `HashInputLength > 0`
- `HashInputLength` + `HashInputOffset` ≤ `Toeplitz->InputSize` (enforced via CXPLAT_DBG_ASSERT)
- `HashInputOffset * 2 + HashInputLength * 2` ≤ `Toeplitz->InputSize * 2` (lookup table bounds)

**Postconditions**:
- Returns a 32-bit hash value
- Same inputs (Toeplitz, HashInput, HashInputLength, HashInputOffset) always produce the same output (deterministic)
- XOR composition property: 
  - `hash(input1, offset1) XOR hash(input2, offset1+len1)` = hash over concatenated inputs

**Side Effects**:
- None - pure function (const inputs)
- No state mutation
- No I/O or logging

**Error Handling**:
- No explicit error return
- Debug assertions check bounds (CXPLAT_DBG_ASSERT)
- Violating preconditions is undefined behavior (likely out-of-bounds access)

**Thread Safety**:
- Thread-safe for concurrent reads of the same `Toeplitz` structure (read-only operation)
- Can be called concurrently from multiple threads

**Resource/Ownership**:
- No resources allocated or freed
- All inputs are read-only

---

### 3. `CxPlatToeplitzHashComputeAddr` (inline helper)

**Signature**:
```c
QUIC_INLINE void CxPlatToeplitzHashComputeAddr(
    _In_ const CXPLAT_TOEPLITZ_HASH* Toeplitz,
    _In_ const QUIC_ADDR* Addr,
    _Inout_ uint32_t* Key,
    _Out_ uint32_t* Offset
);
```

**Declaration**: `src/inc/quic_toeplitz.h`, lines 98-132  
**Purpose**: Computes Toeplitz hash for a single QUIC address (IP + port), XORs into `Key`, and updates `Offset`.

**Preconditions**:
- `Toeplitz` must not be NULL and must be initialized
- `Addr` must not be NULL
- `Addr` must have a valid address family (IPv4 or IPv6)
- `Key` must not be NULL (input/output parameter)
- `Offset` must not be NULL (output parameter)

**Postconditions**:
- `*Key` is XORed with hash of port and IP from `Addr`
- `*Offset` is set to:
  - IPv4: `2 + 4 = 6` (2 bytes port + 4 bytes IP)
  - IPv6: `2 + 16 = 18` (2 bytes port + 16 bytes IP)
- Hash computation order: port first (at offset 0), then IP (at offset 2)

**Side Effects**:
- Modifies `*Key` and `*Offset`
- No other state changes

**Error Handling**:
- No explicit error handling
- Assumes `Addr` has valid address family

**Thread Safety**:
- Thread-safe if `Key` and `Offset` are thread-local
- Concurrent calls with different output pointers are safe

---

### 4. `CxPlatToeplitzHashComputeRss` (inline helper)

**Signature**:
```c
QUIC_INLINE void CxPlatToeplitzHashComputeRss(
    _In_ const CXPLAT_TOEPLITZ_HASH* Toeplitz,
    _In_ const QUIC_ADDR* SrcAddr,
    _In_ const QUIC_ADDR* DestAddr,
    _Inout_ uint32_t* Key,
    _Out_ uint32_t* Offset
);
```

**Declaration**: `src/inc/quic_toeplitz.h`, lines 137-196  
**Purpose**: Computes RSS-style Toeplitz hash over source and destination addresses (IP + port for each).

**Preconditions**:
- `Toeplitz` must not be NULL and must be initialized
- `SrcAddr` and `DestAddr` must not be NULL
- `SrcAddr` and `DestAddr` must have the **same address family** (enforced via CXPLAT_FRE_ASSERT)
- Address family must be IPv4 or IPv6
- `Key` must not be NULL (input/output parameter)
- `Offset` must not be NULL (output parameter)

**Postconditions**:
- `*Key` is XORed with hash computed over:
  - IPv4: SrcIP (4 bytes) || DestIP (4 bytes) || SrcPort (2 bytes) || DestPort (2 bytes)
  - IPv6: SrcIP (16 bytes) || DestIP (16 bytes) || SrcPort (2 bytes) || DestPort (2 bytes)
- `*Offset` is set to:
  - IPv4: `4 + 4 + 2 + 2 = 12`
  - IPv6: `16 + 16 + 2 + 2 = 36`
- Hash computation order: SrcIP, DestIP, SrcPort, DestPort (following RSS standard)

**Side Effects**:
- Modifies `*Key` and `*Offset`

**Error Handling**:
- CXPLAT_FRE_ASSERT enforces same address family (fatal in debug builds)
- CXPLAT_DBG_ASSERT checks IPv6 in else branch

**Thread Safety**:
- Thread-safe if `Key` and `Offset` are thread-local

---

## Type/Object Invariants

### `CXPLAT_TOEPLITZ_HASH`

**Structure** (from `quic_toeplitz.h`, lines 67-71):
```c
typedef struct CXPLAT_TOEPLITZ_HASH {
    CXPLAT_TOEPLITZ_LOOKUP_TABLE LookupTableArray[CXPLAT_TOEPLITZ_LOOKUP_TABLE_COUNT_MAX];
    uint8_t HashKey[CXPLAT_TOEPLITZ_KEY_SIZE_MAX];
    CXPLAT_TOEPLITZ_INPUT_SIZE InputSize;
} CXPLAT_TOEPLITZ_HASH;
```

**Object Invariants**:
1. **Before initialization**:
   - `HashKey` must be populated with valid random/secret key bytes
   - `InputSize` must be set to a valid enum value (36 or 38)
   - `LookupTableArray` contents are undefined

2. **After initialization** (post `CxPlatToeplitzHashInitialize`):
   - `LookupTableArray[0..InputSize*2-1]` contains valid pre-computed lookup tables
   - Each `LookupTableArray[i].Table[0..15]` contains valid 32-bit XOR signatures
   - `HashKey` and `InputSize` remain unchanged

3. **Lifetime invariants**:
   - Once initialized, the structure is immutable for hash computations
   - No state machine - single initialization step

**Capacity Constraints**:
- `LookupTableArray` size: fixed at `CXPLAT_TOEPLITZ_LOOKUP_TABLE_COUNT_MAX = 76` (38 * 2)
- `HashKey` size: fixed at `CXPLAT_TOEPLITZ_KEY_SIZE_MAX = 42` (38 + 4)
- Actual used size determined by `InputSize` field

---

### `CXPLAT_TOEPLITZ_LOOKUP_TABLE`

**Structure** (from `quic_toeplitz.h`, lines 63-65):
```c
typedef struct CXPLAT_TOEPLITZ_LOOKUP_TABLE {
    uint32_t Table[CXPLAT_TOEPLITZ_LOOKUP_TABLE_SIZE];
} CXPLAT_TOEPLITZ_LOOKUP_TABLE;
```

**Invariants**:
- `Table` has exactly 16 entries (one for each 4-bit nibble value 0x0-0xF)
- Each entry is a pre-computed 32-bit value to XOR into the result
- Populated during initialization, read-only thereafter

---

## Environment Invariants

### Global/Module State
- **No global state** - all state is encapsulated in `CXPLAT_TOEPLITZ_HASH` structure
- **No initialization requirements** - no module-level init/cleanup

### Allocator Requirements
- No dynamic allocation performed
- Structures are typically stack-allocated or embedded in larger objects

### Concurrency
- Multiple `CXPLAT_TOEPLITZ_HASH` instances can be used independently
- Once initialized, a single instance can be shared read-only across threads

### Resource Constraints
- No file descriptors, sockets, or OS handles
- Stack usage: ~4.9 KB per `CXPLAT_TOEPLITZ_HASH` structure (76 tables * 16 entries * 4 bytes + 42 bytes key + metadata)

---

## Dependency Map

### Public API Call Graph

```
CxPlatToeplitzHashInitialize (toeplitz.c, lines 58-132)
  └─ (no internal dependencies - pure computation)

CxPlatToeplitzHashCompute (toeplitz.c, lines 139-167)
  └─ (no internal dependencies - table lookups only)

CxPlatToeplitzHashComputeAddr (quic_toeplitz.h, inline)
  ├─ QuicAddrGetFamily
  └─ CxPlatToeplitzHashCompute (multiple calls)

CxPlatToeplitzHashComputeRss (quic_toeplitz.h, inline)
  ├─ QuicAddrGetFamily
  └─ CxPlatToeplitzHashCompute (multiple calls)
```

### External Dependencies

From `toeplitz.c`:
- `platform_internal.h` - platform abstractions
- `CXPLAT_DBG_ASSERT` - debug assertion macro
- `CxPlatCopyMemory` - memory copy utility (used in tests)

From `quic_toeplitz.h`:
- `QUIC_ADDR` - network address structure (standard sockaddr wrappers)
- `QuicAddrGetFamily` - address family accessor
- `QUIC_ADDRESS_FAMILY_INET`, `QUIC_ADDRESS_FAMILY_INET6` - address family constants
- `QUIC_ADDR_V4_PORT_OFFSET`, `QUIC_ADDR_V4_IP_OFFSET` - IPv4 field offsets
- `QUIC_ADDR_V6_PORT_OFFSET`, `QUIC_ADDR_V6_IP_OFFSET` - IPv6 field offsets

### Callback/Indirect Calls
- None - all functions are direct calls

---

## State Machine

**No state machine** - the component is stateless:
- `CxPlatToeplitzHashInitialize` is a one-time setup operation
- `CxPlatToeplitzHashCompute` is a pure read-only operation
- No state transitions between calls

**Lifecycle**:
1. **Uninitialized**: Structure allocated, HashKey and InputSize set by caller
2. **Initialized**: After `CxPlatToeplitzHashInitialize` call
3. **In-use**: Repeated calls to `CxPlatToeplitzHashCompute` (no state changes)
4. **Destroyed**: Structure deallocated (no cleanup needed)

---

## Constants and Enumerations

### `CXPLAT_TOEPLITZ_INPUT_SIZE` (enum)
- `CXPLAT_TOEPLITZ_INPUT_SIZE_IP = 36` - Standard RSS hash (2 IPs + 2 ports for IPv6)
- `CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC = 38` - QUIC-specific (adds 20-byte CID, 16-byte IP, 2-byte port)
- `CXPLAT_TOEPLITZ_INPUT_SIZE_MAX = 38` - Maximum supported input size

### Derived Constants
- `NIBBLES_PER_BYTE = 2`
- `BITS_PER_NIBBLE = 4`
- `CXPLAT_TOEPLITZ_OUPUT_SIZE = 4` (sizeof(uint32_t))
- `CXPLAT_TOEPLITZ_KEY_SIZE_MAX = 42` (38 + 4)
- `CXPLAT_TOEPLITZ_KEY_SIZE_MIN = 40` (36 + 4)
- `CXPLAT_TOEPLITZ_LOOKUP_TABLE_SIZE = 16` (2^4 nibble values)
- `CXPLAT_TOEPLITZ_LOOKUP_TABLE_COUNT_MAX = 76` (38 * 2)

---

## Summary of Testability Constraints

### What CAN be tested via public API:
1. **Initialization correctness**: After init, computed hashes match expected values
2. **Hash determinism**: Same input produces same output
3. **XOR composition property**: hash(A||B) = hash(A, offset=0) XOR hash(B, offset=len(A))
4. **RSS standard compliance**: Known test vectors (from Windows RSS specs)
5. **IPv4 vs IPv6 behavior**: Different address families produce different hashes
6. **Offset parameter behavior**: Hashing subsets of input
7. **Address helpers**: `ComputeAddr` and `ComputeRss` convenience functions

### What CANNOT be tested (contract-unsafe scenarios):
1. Null pointer inputs (violates preconditions)
2. Out-of-bounds access (HashInputLength + Offset > InputSize)
3. Uninitialized `Toeplitz` structure
4. Mixed address families in `ComputeRss`
5. Internal lookup table structure (private implementation detail)

### Coverage Strategy:
- Test all public functions with valid inputs
- Test boundary conditions (max input size, offset at boundary, zero-length inputs)
- Test both IPv4 and IPv6 code paths
- Test XOR composition property
- Validate against known RSS test vectors
