# Repository Contract Index (RCI) - Token Validation Components

## Component: Token Validation (packet.c, binding.c, connection.c)
**PR Context**: PR #4412 - "Always Ignore Invalid Tokens"

---

## A) Public API Inventory

### 1. `QuicPacketValidateInitialToken`

**Signature**:
```c
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
QuicPacketValidateInitialToken(
    _In_ const void* const Owner,
    _In_ const QUIC_RX_PACKET* const Packet,
    _In_range_(>, 0) uint16_t TokenLength,
    _In_reads_(TokenLength) const uint8_t* TokenBuffer
)
```

**Location**: Declared in `src/core/packet.h`, implemented in `src/core/packet.c`

**Summary**: Validates an initial token (either RETRY token or NEW_TOKEN) from a QUIC Initial packet.

**Preconditions**:
- `Owner` must be non-NULL (Binding or Connection)
- `Packet` must be non-NULL and properly initialized QUIC_RX_PACKET
- `Packet->Route` must be non-NULL with valid RemoteAddress
- `TokenLength` must be > 0
- `TokenBuffer` must point to valid memory of at least `TokenLength` bytes

**Postconditions**:
- Returns TRUE if token is valid RETRY token
- Returns FALSE if:
  - Token is NEW_TOKEN (not yet supported)
  - Token length is invalid
  - Token decryption fails
  - Token OrigConnIdLength is invalid
  - Token RemoteAddress doesn't match Packet->Route->RemoteAddress
- **After PR #4412**: Function no longer signals packet drop via parameter

**Side Effects**:
- Calls `QuicPacketLogDrop` for logging (not part of contract)
- After PR: Sets `Packet->ValidToken = TRUE` in calling code if validation succeeds

**Error Contract**:
- No exceptions thrown (C code)
- Returns FALSE for all error conditions
- Invalid tokens are logged but not treated as fatal errors after PR

**Thread Safety**: Can be called at DISPATCH_LEVEL, must respect IRQL constraints

**Resource/Ownership**: Does not allocate or free resources; does not modify input buffers

---

### 2. `QuicBindingShouldRetryConnection`

**Signature**:
```c
_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
QuicBindingShouldRetryConnection(
    _In_ const QUIC_BINDING* const Binding,
    _In_ QUIC_RX_PACKET* Packet,
    _In_ uint16_t TokenLength,
    _In_reads_(TokenLength) const uint8_t* Token
)
```

**Location**: Implemented in `src/core/binding.c` (internal, but key contract point)

**Summary**: Determines whether to respond to a connection attempt with a Retry packet.

**Preconditions**:
- `Binding` must be non-NULL and valid
- `Packet` must be non-NULL
- If `TokenLength` > 0, `Token` must point to valid memory
- If `TokenLength` == 0, `Token` may be NULL

**Postconditions**:
- Returns TRUE if should send Retry packet (memory limit exceeded)
- Returns FALSE if should proceed with connection creation
- **After PR #4412**: Always proceeds to create connection even with invalid token
- Sets `Packet->ValidToken = TRUE` if token validates successfully

**Side Effects**:
- Calls `QuicPacketValidateInitialToken` if token provided
- Checks global `MsQuicLib.CurrentHandshakeMemoryUsage` against limits
- May log packet drop

**Error Contract**:
- Returns FALSE for invalid tokens (but doesn't drop packet after PR)
- Returns TRUE if memory limit exceeded (legitimate Retry scenario)

---

### 3. `QuicConnRecvHeader` (Token Validation Section)

**Location**: `src/core/connection.c`

**Summary**: Connection-side packet header processing, including token validation for peer validation.

**Preconditions**:
- Connection must be in valid state
- Path must exist
- Packet must be valid Initial packet if checking token

**Postconditions**:
- **After PR #4412**: If `QuicPacketValidateInitialToken` returns TRUE, sets `Packet->ValidToken = TRUE`
- If token is valid, uses it for peer validation (Path validation)
- Invalid tokens are ignored, connection proceeds

**Side Effects**:
- May set peer as validated if token is valid
- May decode retry token from packet
- Updates connection state based on token validation

---

## B) Type/Object Invariants

### `QUIC_TOKEN_CONTENTS`

**Structure** (from `binding.h`):
```c
typedef struct QUIC_TOKEN_CONTENTS {
    struct {
        uint64_t IsNewToken : 1;
        uint64_t Timestamp  : 63;
    } Authenticated;
    struct {
        QUIC_ADDR RemoteAddress;
        uint8_t OrigConnId[QUIC_MAX_CONNECTION_ID_LENGTH_V1];
        uint8_t OrigConnIdLength;
    } Encrypted;
    uint8_t EncryptionTag[CXPLAT_ENCRYPTION_OVERHEAD];
} QUIC_TOKEN_CONTENTS;
```

**Invariants**:
- `OrigConnIdLength` must be <= `sizeof(OrigConnId)` (QUIC_MAX_CONNECTION_ID_LENGTH_V1)
- `IsNewToken` bit: 0 = RETRY token, 1 = NEW_TOKEN
- Size must be `sizeof(QUIC_TOKEN_CONTENTS)` for RETRY tokens
- `RemoteAddress` must match the packet's source address for valid RETRY token
- NEW_TOKEN format may differ (currently unsupported)

### `QUIC_RX_PACKET`

**Relevant Fields**:
- `ValidToken`: BOOLEAN, set to TRUE if token validation succeeds
- `Route`: Pointer to route information including RemoteAddress
- Must have valid Route with RemoteAddress for token validation

**State Machine**: N/A for packet structure itself

---

## C) Environment Invariants

### Global State
- `MsQuicLib.CurrentHandshakeMemoryUsage`: Global counter tracking memory used by handshaking connections
- `MsQuicLib.Settings.RetryMemoryLimit`: Configuration threshold for triggering Retry

### Initialization Requirements
- MsQuic library must be initialized before packet/binding operations
- Crypto subsystem must be initialized for token encryption/decryption

### Token Handling Philosophy (After PR #4412)
**Critical Invariant**: Invalid tokens MUST NOT cause packet drops or connection failures
- Rationale: Client may send NEW_TOKEN from different server, which appears as invalid RETRY token
- Old behavior: Drop packet with invalid RETRY token → connection fails
- New behavior: Ignore invalid token, proceed with connection → robust operation

---

## D) Dependency Map

### Token Validation Flow (After PR #4412)

```
[Binding receives Initial packet with token]
          ↓
QuicBindingShouldRetryConnection(Binding, Packet, TokenLength, Token)
          ↓
    [TokenLength > 0?] ─No──→ [Check memory limit] → Return TRUE/FALSE
          ↓ Yes
QuicPacketValidateInitialToken(Binding, Packet, TokenLength, Token)
          ↓
    [Returns TRUE?] ──Yes──→ Set Packet->ValidToken=TRUE → Return FALSE (don't retry)
          ↓ No
    [Ignore invalid token]
          ↓
    [Check memory limit] → Return TRUE/FALSE (may retry for other reasons)
          ↓
QuicBindingCreateConnection(Binding, Packet) ← Always called now (not dropped)
```

### Connection Token Validation Flow

```
[Connection receives Initial packet]
          ↓
QuicConnRecvHeader(Connection, Packet, ...)
          ↓
    [!Path->IsPeerValidated && (ValidToken || TokenLength)?]
          ↓ Yes
    [Packet->ValidToken?] ──Yes──→ QuicPacketDecodeRetryTokenV1() → Use token for validation
          ↓ No (Token provided but not pre-validated)
QuicPacketValidateInitialToken(Connection, Packet, TokenLength, TokenBuffer)
          ↓
    [Returns TRUE?] ──Yes──→ Set Packet->ValidToken=TRUE → Use for peer validation
          ↓ No
    [Ignore, proceed without peer validation from token]
```

### Key Internal Dependencies

1. **QuicRetryTokenDecrypt**: Decrypts token buffer into QUIC_TOKEN_CONTENTS
   - Used by: `QuicPacketValidateInitialToken`
   - Contract: Returns FALSE if decryption fails

2. **QuicAddrCompare**: Compares two QUIC_ADDR structures
   - Used by: `QuicPacketValidateInitialToken`
   - Contract: Returns TRUE if addresses match

3. **QuicPacketLogDrop**: Logging function
   - Used by: `QuicPacketValidateInitialToken`
   - Not part of functional contract (side effect only)

4. **QuicPacketDecodeRetryTokenV1**: Decodes retry token from packet
   - Used by: `QuicConnRecvHeader`
   - Sets Token and TokenLength output parameters

---

## E) Behavior Changes in PR #4412

### Before PR
1. `QuicPacketValidateInitialToken` had `_Inout_ BOOLEAN* DropPacket` parameter
2. Set `*DropPacket = TRUE` for all invalid tokens (NEW_TOKEN, wrong length, decrypt fail, addr mismatch)
3. `QuicBindingShouldRetryConnection` checked `DropPacket` and returned FALSE without creating connection
4. `QuicConnRecvHeader` returned FALSE (drop packet) if `InvalidRetryToken` was TRUE

### After PR #4412
1. `QuicPacketValidateInitialToken` returns TRUE/FALSE only (no DropPacket parameter)
2. Returns FALSE for invalid tokens but doesn't signal packet drop
3. `QuicBindingShouldRetryConnection` always proceeds to connection creation regardless of token validity
4. `QuicConnRecvHeader` checks `Packet->ValidToken` instead of `InvalidRetryToken`
5. **Key Impact**: Clients sending NEW_TOKEN from different server no longer fail to connect

---

## Test Scenarios to Cover

### Scenario Category 1: Valid RETRY Token
- Valid token with correct encryption, length, and matching address
- Expected: `QuicPacketValidateInitialToken` returns TRUE, Packet->ValidToken set

### Scenario Category 2: Invalid Token - NEW_TOKEN
- Token with IsNewToken bit set (not yet supported)
- Expected: Returns FALSE, connection proceeds (not dropped)

### Scenario Category 3: Invalid Token Length
- TokenLength != sizeof(QUIC_TOKEN_CONTENTS)
- Expected: Returns FALSE, connection proceeds

### Scenario Category 4: Token Decryption Failure
- Token buffer that fails `QuicRetryTokenDecrypt`
- Expected: Returns FALSE, connection proceeds

### Scenario Category 5: Token Address Mismatch
- Valid token format but RemoteAddress doesn't match packet source
- Expected: Returns FALSE, connection proceeds (NOT dropped)

### Scenario Category 6: Token OrigConnId Invalid Length
- Token with OrigConnIdLength > QUIC_MAX_CONNECTION_ID_LENGTH_V1
- Expected: Returns FALSE, connection proceeds

### Scenario Category 7: Binding Retry Logic
- No token + memory limit exceeded → Retry
- No token + memory OK → No retry
- Invalid token + memory OK → No retry, create connection
- Valid token → No retry, create connection with validated peer

### Scenario Category 8: Connection Token Handling
- Connection receives packet with valid token → Peer validated
- Connection receives packet with invalid token → Proceeds without peer validation

---

## Notes on Testing Strategy

1. **Unit Tests**: Test `QuicPacketValidateInitialToken` directly with various token scenarios
   - Challenge: Function needs valid `Owner`, `Packet`, and `Token` structures
   - Solution: Use minimal test fixtures with real structures

2. **Component Tests**: Test binding and connection token handling flows
   - Test `QuicBindingShouldRetryConnection` behavior
   - Test connection establishment with various token states

3. **Integration Tests**: End-to-end connection scenarios
   - Client with valid retry token
   - Client with NEW_TOKEN from different server (should succeed after PR)

4. **Coverage Target**: 100% of modified lines in packet.c, binding.c, connection.c

