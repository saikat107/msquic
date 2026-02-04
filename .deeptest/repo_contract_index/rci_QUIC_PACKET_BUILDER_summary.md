# Repository Contract Index (RCI) - QUIC_PACKET_BUILDER

## Component Overview
The QUIC_PACKET_BUILDER component (`src/core/packet_builder.c`) abstracts the logic to build up a chain of UDP datagrams, each potentially containing multiple coalesced QUIC packets. It handles packet header construction, encryption, header protection, and batch transmission.

## A) Public API Inventory

### 1. QuicPacketBuilderInitialize
**Signature:**
```c
_IRQL_requires_max_(DISPATCH_LEVEL)
_Success_(return != FALSE)
BOOLEAN QuicPacketBuilderInitialize(
    _Inout_ QUIC_PACKET_BUILDER* Builder,
    _In_ QUIC_CONNECTION* Connection,
    _In_ QUIC_PATH* Path
);
```

**Declaration:** `src/core/packet_builder.h:169`

**Purpose:** Initializes the packet builder for general use with a connection and path.

**Preconditions:**
- `Builder` must not be NULL
- `Connection` must not be NULL
- `Path` must not be NULL
- `Path->DestCid` must not be NULL (asserted)
- `Connection->SourceCids.Next` must not be NULL (returns FALSE if NULL)

**Postconditions:**
- On success (TRUE): Builder is initialized with connection state, path, source CID, send allowance calculated from congestion control
- On failure (FALSE): Builder is not usable, no resources allocated
- Sets `Builder->Connection`, `Builder->Path`, `Builder->SourceCid`, `Builder->SendAllowance`
- Initializes flags: `PacketBatchSent`, `PacketBatchRetransmittable`, `WrittenConnectionCloseFrame` to FALSE
- Sets `Builder->Metadata` to point to `Builder->MetadataStorage.Metadata`
- Sets `Builder->EncryptionOverhead` to `CXPLAT_ENCRYPTION_OVERHEAD`

**Side Effects:**
- Updates `Connection->Send.LastFlushTime` with current time
- Sets `Connection->Send.LastFlushTimeValid` to TRUE
- Logs warning if no source CID available

**Error Handling:**
- Returns FALSE if `Connection->SourceCids.Next == NULL`
- Otherwise returns TRUE

**Thread Safety:** Can be called at DISPATCH_LEVEL or below

**Resource/Ownership:**
- Builder is owned by caller
- References Connection and Path but doesn't take ownership
- No allocations performed

---

### 2. QuicPacketBuilderCleanup
**Signature:**
```c
_IRQL_requires_max_(PASSIVE_LEVEL)
void QuicPacketBuilderCleanup(
    _Inout_ QUIC_PACKET_BUILDER* Builder
);
```

**Declaration:** `src/core/packet_builder.h:180`

**Purpose:** Cleans up any leftover data still buffered for send.

**Preconditions:**
- `Builder` must not be NULL
- `Builder->SendData` must be NULL (asserted - all sends must be flushed before cleanup)

**Postconditions:**
- If `PacketBatchSent && PacketBatchRetransmittable`, loss detection timer is updated
- Metadata frames are released
- HpMask is securely zeroed

**Side Effects:**
- May update loss detection timer via `QuicLossDetectionUpdateTimer`
- Releases frames in metadata via `QuicSentPacketMetadataReleaseFrames`
- Zeros sensitive crypto material in `Builder->HpMask`

**Error Handling:** None - void function

**Thread Safety:** Must be called at PASSIVE_LEVEL

**Resource/Ownership:**
- Does not free Builder itself
- Releases frame metadata resources
- Caller must ensure SendData is NULL (flushed) before calling

---

### 3. QuicPacketBuilderPrepareForControlFrames
**Signature:**
```c
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN QuicPacketBuilderPrepareForControlFrames(
    _Inout_ QUIC_PACKET_BUILDER* Builder,
    _In_ BOOLEAN IsTailLossProbe,
    _In_ uint32_t SendFlags
);
```

**Declaration:** `src/core/packet_builder.h:190`

**Purpose:** Prepares the packet builder for framing control payload (ACK, CRYPTO, PING, CONNECTION_CLOSE, etc.).

**Preconditions:**
- `Builder` must not be NULL and properly initialized
- `SendFlags` must not include `QUIC_CONN_SEND_FLAG_DPLPMTUD` (asserted)
- `SendFlags` must not be 0 (asserted in helper)

**Postconditions:**
- On success (TRUE): Builder is prepared with appropriate packet type and key for the requested control frames
- On failure (FALSE): Builder state unchanged, cannot send requested frames
- Selects appropriate encryption level based on SendFlags and connection state
- May finalize existing packet if type changes

**Side Effects:**
- May allocate send buffers and datagrams
- May finalize and send existing packets
- May increment partition send batch ID and packet ID counters
- Logs packet creation events

**Error Handling:**
- Returns FALSE if:
  - No appropriate key available for requested frames
  - Send buffer allocation fails
  - Datagram buffer allocation fails
  - Connection has NULL key (silently aborts connection)

**Thread Safety:** Must be called at PASSIVE_LEVEL

**Resource/Ownership:**
- May allocate `CXPlAT_SEND_DATA` and datagram buffers
- Caller responsible for finalizing and cleanup

---

### 4. QuicPacketBuilderPrepareForPathMtuDiscovery
**Signature:**
```c
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN QuicPacketBuilderPrepareForPathMtuDiscovery(
    _Inout_ QUIC_PACKET_BUILDER* Builder
);
```

**Declaration:** `src/core/packet_builder.h:201`

**Purpose:** Prepares the packet builder for PMTUD (Path MTU Discovery) probe packets.

**Preconditions:**
- `Builder` must not be NULL and properly initialized
- PMTUD should only happen after source address validation (asserted)

**Postconditions:**
- On success (TRUE): Builder prepared with 1-RTT key, IsPathMtuDiscovery flag set
- On failure (FALSE): Cannot send PMTUD probe
- Forces flush of batched datagrams
- Sets minimum datagram length to probe size

**Side Effects:**
- Forces finalization of existing packets
- Allocates send data with probe size
- Sets `Metadata->Flags.IsMtuProbe` to TRUE

**Error Handling:**
- Returns FALSE if 1-RTT key unavailable or allocation fails

**Thread Safety:** Must be called at PASSIVE_LEVEL

**Resource/Ownership:**
- Allocates datagram buffers for MTU probe
- Caller responsible for finalization

---

### 5. QuicPacketBuilderPrepareForStreamFrames
**Signature:**
```c
_IRQL_requires_max_(PASSIVE_LEVEL)
_Success_(return != FALSE)
BOOLEAN QuicPacketBuilderPrepareForStreamFrames(
    _Inout_ QUIC_PACKET_BUILDER* Builder,
    _In_ BOOLEAN IsTailLossProbe
);
```

**Declaration:** `src/core/packet_builder.h:212`

**Purpose:** Prepares the packet builder for stream payload (application data).

**Preconditions:**
- `Builder` must not be NULL and properly initialized
- At least one of 0-RTT or 1-RTT keys must be available (asserted for 1-RTT)

**Postconditions:**
- On success (TRUE): Builder prepared with 0-RTT or 1-RTT key
- Uses 0-RTT key only if 1-RTT unavailable
- On failure (FALSE): Cannot send stream data

**Side Effects:**
- May allocate send buffers and datagrams
- May finalize existing packets
- Selects encryption level based on available keys

**Error Handling:**
- Returns FALSE if no suitable key available or allocation fails

**Thread Safety:** Must be called at PASSIVE_LEVEL

**Resource/Ownership:**
- May allocate send resources
- Caller responsible for finalization

---

### 6. QuicPacketBuilderFinalize
**Signature:**
```c
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN QuicPacketBuilderFinalize(
    _Inout_ QUIC_PACKET_BUILDER* Builder,
    _In_ BOOLEAN FlushBatchedDatagrams
);
```

**Declaration:** `src/core/packet_builder.h:222`

**Purpose:** Finishes up the current packet so it can be sent. Updates headers, encrypts payload, applies header protection, and optionally sends batched datagrams.

**Preconditions:**
- `Builder` must not be NULL and properly initialized

**Postconditions:**
- On success (TRUE): Packet is finalized, encrypted, and potentially sent
- Returns FALSE (CanKeepSending=FALSE) if no frames were added (packet header removed, packet number reverted)
- If `FlushBatchedDatagrams==TRUE`: All batched datagrams are sent, SendData becomes NULL
- Packet metadata recorded for loss detection
- Congestion control allowance updated

**Side Effects:**
- Adds padding if required (for Initial packets, TLP, or minimum datagram size)
- Encrypts packet payload
- Applies header protection (batched for short header, immediate for long header)
- Updates payload length field for long header packets
- Logs packet details
- Sends packet via `QuicBindingSend` if flushing or buffer full
- Updates `PacketBatchSent` and `PacketBatchRetransmittable` flags
- Records packet in loss detection
- For RETRY packets, closes connection

**Error Handling:**
- If no frames in packet: reverts packet number, removes header, returns FALSE
- If encryption fails: calls `QuicConnFatalError`, returns FALSE (CanKeepSending=TRUE)
- If header protection fails: calls `QuicConnFatalError`, returns FALSE
- If amplification limited and no data sent: adds flow blocked reason

**Thread Safety:** Must be called at PASSIVE_LEVEL

**Resource/Ownership:**
- May free datagram buffers if flushing with no data
- May free SendData if flushing
- After flushing, Builder->SendData is NULL
- Caller must not use SendData after flush

---

### 7. QuicPacketBuilderHasAllowance (inline)
**Signature:**
```c
_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_INLINE
BOOLEAN QuicPacketBuilderHasAllowance(
    _In_ const QUIC_PACKET_BUILDER* Builder
);
```

**Declaration:** `src/core/packet_builder.h:233`

**Purpose:** Returns TRUE if congestion control isn't currently blocking sends.

**Preconditions:**
- `Builder` must not be NULL

**Postconditions:**
- Returns TRUE if `SendAllowance > 0` OR congestion control has exemptions
- Returns FALSE otherwise
- No side effects

**Side Effects:** None (pure query function)

**Error Handling:** None

**Thread Safety:** Can be called at DISPATCH_LEVEL or below

**Resource/Ownership:** No ownership, read-only query

---

### 8. QuicPacketBuilderAddFrame (inline)
**Signature:**
```c
_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_INLINE
BOOLEAN QuicPacketBuilderAddFrame(
    _Inout_ QUIC_PACKET_BUILDER* Builder,
    _In_ uint8_t FrameType,
    _In_ BOOLEAN IsAckEliciting
);
```

**Declaration:** `src/core/packet_builder.h:248`

**Purpose:** Adds a frame to the current packet metadata. Returns TRUE if the packet has run out of room for frames.

**Preconditions:**
- `Builder` must not be NULL
- `Builder->Metadata->FrameCount < QUIC_MAX_FRAMES_PER_PACKET` (asserted)

**Postconditions:**
- Frame added to metadata at index `FrameCount`
- `FrameCount` incremented
- If `IsAckEliciting==TRUE`, sets `Metadata->Flags.IsAckEliciting`
- Returns TRUE if `FrameCount == QUIC_MAX_FRAMES_PER_PACKET` after increment

**Side Effects:**
- Modifies Builder->Metadata->FrameCount
- May set IsAckEliciting flag

**Error Handling:** None - asserts if called when already at max frames

**Thread Safety:** Can be called at DISPATCH_LEVEL or below

**Resource/Ownership:** No allocations, updates metadata in-place

---

### 9. QuicPacketBuilderAddStreamFrame (inline)
**Signature:**
```c
_IRQL_requires_max_(DISPATCH_LEVEL)
QUIC_INLINE
BOOLEAN QuicPacketBuilderAddStreamFrame(
    _Inout_ QUIC_PACKET_BUILDER* Builder,
    _In_ QUIC_STREAM* Stream,
    _In_ uint8_t FrameType
);
```

**Declaration:** `src/core/packet_builder.h:266`

**Purpose:** Adds a stream frame to the current packet metadata. Automatically increments stream's sent metadata counter. Returns TRUE if packet is full.

**Preconditions:**
- `Builder` must not be NULL
- `Stream` must not be NULL
- `Builder->Metadata->FrameCount < QUIC_MAX_FRAMES_PER_PACKET` (asserted via AddFrame)

**Postconditions:**
- Stream reference stored in metadata
- Stream's sent metadata counter incremented
- Frame marked as ack-eliciting
- Returns TRUE if packet full

**Side Effects:**
- Calls `QuicStreamSentMetadataIncrement(Stream)` to track outstanding metadata
- Delegates to `QuicPacketBuilderAddFrame` with `IsAckEliciting=TRUE`

**Error Handling:** None - delegates to AddFrame which asserts

**Thread Safety:** Can be called at DISPATCH_LEVEL or below

**Resource/Ownership:**
- Stores Stream pointer (borrowed reference)
- Increments refcount on stream metadata tracking

---

## B) Type/Object Invariants

### QUIC_PACKET_BUILDER Structure
**Defined in:** `src/core/packet_builder.h:11`

**Object Invariants (always hold for valid instances):**
1. `Path != NULL && Path->DestCid != NULL`
2. `BatchCount <= QUIC_MAX_CRYPTO_BATCH_COUNT`
3. `EncryptionOverhead <= 16`
4. If `Key != NULL`, then `Key->PacketKey != NULL && Key->HeaderKey != NULL`
5. If `SendData == NULL`, then `Datagram == NULL`
6. If `Datagram != NULL`:
   - `Datagram->Length != 0 && Datagram->Length <= UINT16_MAX`
   - `Datagram->Length >= MinimumDatagramLength`
   - `Datagram->Length >= DatagramLength + EncryptionOverhead`
   - `DatagramLength >= PacketStart`
   - `DatagramLength >= HeaderLength`
   - `DatagramLength >= PacketStart + HeaderLength`
7. If `Datagram == NULL`, then `DatagramLength == 0 && Metadata->FrameCount == 0`
8. If `PacketType != SEND_PACKET_SHORT_HEADER_TYPE` and `Datagram != NULL`, then `PayloadLengthOffset != 0`
9. `Metadata != NULL` (always points to `MetadataStorage.Metadata`)

**State Machine:** The packet builder operates in states:
1. **Uninitialized** → `QuicPacketBuilderInitialize` → **Initialized**
2. **Initialized** → `QuicPacketBuilderPrepare*` → **Prepared** (with allocated buffers)
3. **Prepared** → Add frames → **Building** (FrameCount > 0)
4. **Building** → `QuicPacketBuilderFinalize` → **Finalized** (packet sent or ready to send)
5. **Finalized** → Repeat Prepare/Build/Finalize or → **Cleanup**
6. **Cleanup** → `QuicPacketBuilderCleanup` → **Uninitialized**

**State Invariants:**
- **Initialized**: `Connection != NULL`, `Path != NULL`, `SendData == NULL`, `Datagram == NULL`
- **Prepared**: `Key != NULL`, `SendData != NULL`, `Datagram != NULL`, `HeaderLength != 0`, `FrameCount == 0`
- **Building**: `FrameCount > 0`, `Key != NULL`, `SendData != NULL`, `Datagram != NULL`
- **Finalized (partial)**: `Datagram == NULL`, previous packet metadata committed
- **Finalized (flushed)**: `SendData == NULL`, all packets sent

**Ownership/Lifetime:**
- Builder does not own Connection or Path (borrowed)
- Builder owns SendData once allocated (must free if not sent)
- Builder owns Datagram once allocated (must free if not used)
- Builder owns HpMask crypto material (must zero on cleanup)

---

### QUIC_SENT_PACKET_METADATA
**Related structure** for tracking sent packet information.

**Invariants:**
- `FrameCount <= QUIC_MAX_FRAMES_PER_PACKET`
- If `FrameCount == 0`, packet not yet built or already processed
- `PacketNumber` is monotonically increasing (except skipped numbers)

---

## C) Environment Invariants

1. **Initialization Required**: `QuicPacketBuilderInitialize` must be called before any Prepare functions
2. **Flush Before Cleanup**: `SendData` must be NULL (flushed) before `QuicPacketBuilderCleanup`
3. **IRQL Constraints**:
   - Initialize can be called at DISPATCH_LEVEL
   - All other public APIs require PASSIVE_LEVEL
4. **Key Availability**: Appropriate write keys must be available in `Connection->Crypto.TlsState.WriteKeys[]` for the desired packet type
5. **Source CID Required**: Connection must have at least one source CID available
6. **No Leaks**: Caller must ensure all allocated SendData is either sent or freed
7. **Crypto Material Zeroization**: HpMask must be securely zeroed on cleanup
8. **Single-Threaded**: Packet builder is not thread-safe; single thread must control from init to cleanup

---

## D) Dependency Map & Internal Flow

### Public API Call Graph

```
QuicPacketBuilderInitialize
├─ (no internal prepare/build calls)

QuicPacketBuilderPrepareForControlFrames
├─ QuicPacketBuilderGetPacketTypeAndKeyForControlFrames (internal)
│  └─ QuicSendValidate
└─ QuicPacketBuilderPrepare (internal)
   ├─ QuicPacketBuilderFinalize (if type change or space limit)
   ├─ CxPlatSendDataAlloc
   ├─ CxPlatSendDataAllocBuffer
   ├─ QuicPacketEncodeShortHeaderV1 or QuicPacketEncodeLongHeaderV1
   └─ QuicConnSilentlyAbort (if NULL key error)

QuicPacketBuilderPrepareForPathMtuDiscovery
└─ QuicPacketBuilderPrepare (internal, with IsPathMtuDiscovery=TRUE)

QuicPacketBuilderPrepareForStreamFrames
└─ QuicPacketBuilderPrepare (internal)

QuicPacketBuilderFinalize
├─ QuicPacketBuilderValidate (debug only)
├─ CxPlatSendDataFreeBuffer (if empty packet)
├─ CxPlatEncrypt (packet encryption)
├─ CxPlatHpComputeMask (header protection)
├─ QuicPacketBuilderFinalizeHeaderProtection (internal, batched)
├─ QuicCryptoGenerateNewKeys (if key phase rollover needed)
├─ QuicCryptoUpdateKeyPhase
├─ QuicLossDetectionOnPacketSent
├─ QuicPacketBuilderSendBatch (internal)
│  └─ QuicBindingSend
└─ QuicConnCloseLocally (for RETRY packets)

QuicPacketBuilderCleanup
├─ QuicLossDetectionUpdateTimer
├─ QuicSentPacketMetadataReleaseFrames
└─ CxPlatSecureZeroMemory

QuicPacketBuilderHasAllowance
└─ QuicCongestionControlGetExemptions

QuicPacketBuilderAddFrame
└─ (inline, no calls)

QuicPacketBuilderAddStreamFrame
├─ QuicStreamSentMetadataIncrement
└─ QuicPacketBuilderAddFrame
```

### Key Internal Functions (not public)
- `QuicPacketBuilderPrepare`: Core preparation logic, handles buffer allocation, packet type switching, header encoding
- `QuicPacketBuilderGetPacketTypeAndKeyForControlFrames`: Determines appropriate encryption level for control frames
- `QuicPacketBuilderSendBatch`: Sends accumulated datagrams via binding
- `QuicPacketBuilderFinalizeHeaderProtection`: Applies batched header protection for short header packets
- `QuicPacketBuilderValidate`: Debug-only validation of invariants

### Indirect Calls/Callbacks
- No direct callback registration in packet_builder
- Interacts with platform layer via `CxPlat*` functions (datapath, crypto)
- Logs events via `QuicTraceEvent` and `QuicTraceLogConn*`
- Calls into congestion control, loss detection, and binding modules

### State Transition Diagram

```
┌─────────────────┐
│ Uninitialized   │
└────────┬────────┘
         │ QuicPacketBuilderInitialize
         v
┌─────────────────┐
│  Initialized    │◄─────────────────────┐
│ (no buffers)    │                      │
└────────┬────────┘                      │
         │ Prepare*                      │
         v                               │
┌─────────────────┐                      │
│   Prepared      │                      │
│ (with buffers,  │                      │
│  header ready)  │                      │
└────────┬────────┘                      │
         │ AddFrame(s)                   │
         v                               │
┌─────────────────┐                      │
│   Building      │                      │
│ (FrameCount>0)  │                      │
└────────┬────────┘                      │
         │ Finalize(flush=FALSE)         │
         v                               │
┌─────────────────┐                      │
│   Finalized     │──────────────────────┘
│ (packet ready,  │     (loop: prepare more packets)
│  may coalesce)  │
└────────┬────────┘
         │ Finalize(flush=TRUE)
         v
┌─────────────────┐
│     Sent        │
│ (SendData=NULL) │
└────────┬────────┘
         │ Cleanup
         v
┌─────────────────┐
│ Uninitialized   │
└─────────────────┘
```

### API Transitions:
1. `Prepare*` APIs may trigger `Finalize` internally if packet type changes or buffer full
2. `Finalize(FlushBatchedDatagrams=FALSE)` allows packet coalescing for multiple packet types in single datagram
3. `Finalize(FlushBatchedDatagrams=TRUE)` forces send and returns to Initialized state
4. Short header packets batch header protection (up to QUIC_MAX_CRYPTO_BATCH_COUNT)
5. Long header packets apply header protection immediately

---

## E) Contract Notes

### Error Propagation
- Most functions return BOOLEAN (TRUE=success, FALSE=failure)
- Fatal errors call `QuicConnFatalError` which triggers connection teardown
- Silent failures (NULL key) call `QuicConnSilentlyAbort` to avoid hanging

### Concurrency
- Not thread-safe - single-threaded use only
- Must be called from connection's worker thread context
- IRQL annotations enforce execution level constraints

### Padding Behavior
- Initial packets: padded to MTU or amplification limit
- TLP (Tail Loss Probe): Short header padded to stateless reset length, long header padded to MTU
- PMTUD probes: padded to probe size
- Minimum padding: 4 bytes (packet number + payload) for crypto safety

### Encryption
- 1-RTT packets: may have encryption disabled (testing) via `State.Disable1RttEncrytion`
- Encryption overhead: 16 bytes (AEAD tag) unless disabled
- Header protection: batched for short header (performance), immediate for long header (different keys)

### Packet Number Management
- Packet numbers are skipped randomly for improved security (every 0-65535 packets)
- Skipped packet numbers logged but not transmitted

### Key Phase Rollover
- Automatic key update triggered when approaching `MaxBytesPerKey` limit
- Only for 1-RTT packets after handshake confirmation
- Awaits peer confirmation before next rollover

### Connection Close Special Cases
- CLOSE frames sent at multiple encryption levels if handshake not confirmed
- RETRY packets automatically close connection after sending
- CONNECTION_CLOSE tracking via `WrittenConnectionCloseFrame` flag

---

## F) Testing Strategy Notes

**Testable Scenarios (Contract-Safe):**
1. **Initialization**: Success and failure cases (with/without source CID)
2. **Prepare for Control**: Various SendFlags combinations, key availability
3. **Prepare for PMTUD**: 1-RTT key available scenario
4. **Prepare for Streams**: 0-RTT vs 1-RTT key selection
5. **Finalization**: With/without frames, flush vs coalesce, padding scenarios
6. **Frame addition**: Normal, boundary (max frames)
7. **Allowance checking**: With/without congestion control exemptions

**Cannot Test (Contract-Unsafe):**
- NULL pointers to required parameters
- Calling Prepare without Initialize
- Calling Cleanup with non-NULL SendData
- Adding frames beyond QUIC_MAX_FRAMES_PER_PACKET (asserted)
- Calling APIs at incorrect IRQL levels (SAL annotations)

**Coverage Challenges:**
- Encryption/header protection require real crypto keys (mock or real TLS handshake)
- Packet send requires real or mock datapath layer
- Congestion control integration requires send allowance setup
- Version-specific packet encoding requires testing multiple QUIC versions
- Key phase rollover requires sending many bytes
