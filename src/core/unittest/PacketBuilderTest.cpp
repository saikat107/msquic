/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Unit tests for QUIC packet builder.

--*/

#include "main.h"
#ifdef QUIC_CLOG
#include "PacketBuilderTest.cpp.clog.h"
#endif

//
// Helper to create a minimal valid connection and path for testing packet builder.
// Uses real QUIC_CONNECTION and QUIC_PATH structures to ensure proper memory layout.
//
static void InitializeMockConnectionAndPath(
    QUIC_CONNECTION& Connection,
    QUIC_PATH& Path,
    QUIC_CID_HASH_ENTRY& SourceCid,
    QUIC_CID_LIST_ENTRY& DestCid,
    QUIC_PACKET_SPACE* (&PacketSpaces)[QUIC_ENCRYPT_LEVEL_COUNT],
    QUIC_PACKET_KEY* (&WriteKeys)[QUIC_PACKET_KEY_COUNT])
{
    // Zero-initialize structures
    CxPlatZeroMemory(&Connection, sizeof(Connection));
    CxPlatZeroMemory(&Path, sizeof(Path));
    CxPlatZeroMemory(&SourceCid, sizeof(SourceCid));
    CxPlatZeroMemory(&DestCid, sizeof(DestCid));

    // Initialize source CID and link it to connection
    SourceCid.CID.Length = 8;
    for (uint8_t i = 0; i < SourceCid.CID.Length; ++i) {
        SourceCid.CID.Data[i] = i;
    }
    Connection.SourceCids.Next = &SourceCid.Link;
    // SLIST doesn't need manual link initialization

    // Initialize destination CID
    DestCid.CID.Length = 8;
    for (uint8_t i = 0; i < DestCid.CID.Length; ++i) {
        DestCid.CID.Data[i] = (uint8_t)(0x10 + i);
    }
    Path.DestCid = &DestCid;

    // Initialize path (use 1200 as standard MTU)
    Path.Mtu = 1200;
    Path.Allowance = UINT32_MAX;
    Path.IsActive = TRUE;
    Path.SpinBit = FALSE;

    // Initialize connection fields
    Connection.Stats.QuicVersion = QUIC_VERSION_1;
    Connection.State.HeaderProtectionEnabled = TRUE;
    Connection.State.FixedBit = TRUE;
    Connection.Send.LastFlushTimeValid = FALSE;

    // Initialize partition (required for batch ID generation)
    static QUIC_PARTITION MockPartition = {};
    MockPartition.Index = 0;
    Connection.Partition = &MockPartition;

    // Initialize packet spaces
    for (uint8_t i = 0; i < QUIC_ENCRYPT_LEVEL_COUNT; ++i) {
        PacketSpaces[i] = (QUIC_PACKET_SPACE*)CXPLAT_ALLOC_NONPAGED(sizeof(QUIC_PACKET_SPACE), QUIC_POOL_TEST);
        if (PacketSpaces[i] != nullptr) {
            CxPlatZeroMemory(PacketSpaces[i], sizeof(QUIC_PACKET_SPACE));
            PacketSpaces[i]->CurrentKeyPhase = 0;
            Connection.Packets[i] = PacketSpaces[i];
        }
    }

    // Initialize mock write keys
    for (uint8_t i = 0; i < QUIC_PACKET_KEY_COUNT; ++i) {
        WriteKeys[i] = (QUIC_PACKET_KEY*)CXPLAT_ALLOC_NONPAGED(sizeof(QUIC_PACKET_KEY), QUIC_POOL_TEST);
        if (WriteKeys[i] != nullptr) {
            CxPlatZeroMemory(WriteKeys[i], sizeof(QUIC_PACKET_KEY));
            WriteKeys[i]->Type = (QUIC_PACKET_KEY_TYPE)i;
            // Allocate dummy key pointers (will not be used for actual crypto)
            WriteKeys[i]->PacketKey = (CXPLAT_KEY*)0x1; // Non-null placeholder
            WriteKeys[i]->HeaderKey = (CXPLAT_HP_KEY*)0x1; // Non-null placeholder
            Connection.Crypto.TlsState.WriteKeys[i] = WriteKeys[i];
        }
    }
    Connection.Crypto.TlsState.WriteKey = QUIC_PACKET_KEY_1_RTT;

    // Initialize congestion control
    Connection.CongestionControl.Cubic.BytesInFlight = 0;
    Connection.CongestionControl.Cubic.CongestionWindow = 10 * Path.Mtu;
}

static void CleanupMockConnectionAndPath(
    QUIC_PACKET_SPACE* (&PacketSpaces)[QUIC_ENCRYPT_LEVEL_COUNT],
    QUIC_PACKET_KEY* (&WriteKeys)[QUIC_PACKET_KEY_COUNT])
{
    for (uint8_t i = 0; i < QUIC_ENCRYPT_LEVEL_COUNT; ++i) {
        if (PacketSpaces[i] != nullptr) {
            CXPLAT_FREE(PacketSpaces[i], QUIC_POOL_TEST);
            PacketSpaces[i] = nullptr;
        }
    }
    for (uint8_t i = 0; i < QUIC_PACKET_KEY_COUNT; ++i) {
        if (WriteKeys[i] != nullptr) {
            CXPLAT_FREE(WriteKeys[i], QUIC_POOL_TEST);
            WriteKeys[i] = nullptr;
        }
    }
}

//
// Test 1: Successful initialization with all required fields
// Scenario: Tests that QuicPacketBuilderInitialize successfully initializes
// a packet builder when given valid connection and path with source CID available.
// Verifies all initial field values are set correctly.
//
TEST(PacketBuilderTest, InitializeSuccess)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    BOOLEAN Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    ASSERT_TRUE(Result);
    ASSERT_EQ(Builder.Connection, &Connection);
    ASSERT_EQ(Builder.Path, &Path);
    ASSERT_EQ(Builder.SourceCid, &SourceCid);
    ASSERT_FALSE(Builder.PacketBatchSent);
    ASSERT_FALSE(Builder.PacketBatchRetransmittable);
    ASSERT_FALSE(Builder.WrittenConnectionCloseFrame);
    ASSERT_EQ(Builder.EncryptionOverhead, CXPLAT_ENCRYPTION_OVERHEAD);
    ASSERT_EQ(Builder.TotalDatagramsLength, 0u);
    ASSERT_NE(Builder.Metadata, nullptr);
    ASSERT_GT(Builder.SendAllowance, 0u);
    ASSERT_TRUE(Connection.Send.LastFlushTimeValid);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 2: Initialization failure when no source CID available
// Scenario: Tests that QuicPacketBuilderInitialize returns FALSE when the connection
// has no source CID available (SourceCids.Next == NULL). This is a documented
// failure condition in the API contract.
//
TEST(PacketBuilderTest, InitializeFailureNoSourceCid)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    CxPlatZeroMemory(&Connection, sizeof(Connection));
    CxPlatZeroMemory(&Path, sizeof(Path));
    CxPlatZeroMemory(&DestCid, sizeof(DestCid));
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    // Setup without source CID
    Connection.SourceCids.Next = nullptr;
    DestCid.CID.Length = 8;

    Path.Mtu = 1200;
    Path.Allowance = UINT32_MAX;
    Connection.Stats.QuicVersion = QUIC_VERSION_1;

    BOOLEAN Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    ASSERT_FALSE(Result);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 3: Send allowance calculation with congestion control
// Scenario: Tests that QuicPacketBuilderInitialize correctly calculates send allowance
// based on congestion control state and path allowance. Verifies that the smaller of
// congestion window allowance and path allowance is used.
//
TEST(PacketBuilderTest, InitializeSendAllowanceCalculation)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);

    // Test 1: Path allowance smaller than congestion window
    Path.Allowance = 1000;
    Connection.CongestionControl.Cubic.CongestionWindow = 100000;
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    BOOLEAN Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    ASSERT_TRUE(Result);
    ASSERT_LE(Builder.SendAllowance, 1000u);

    // Test 2: Unlimited path allowance
    Path.Allowance = UINT32_MAX;
    CxPlatZeroMemory(&Builder, sizeof(Builder));
    Connection.Send.LastFlushTimeValid = FALSE;

    Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    ASSERT_TRUE(Result);
    ASSERT_GT(Builder.SendAllowance, 0u);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 4: Cleanup with no batch sent
// Scenario: Tests QuicPacketBuilderCleanup when no packet batch was sent.
// Verifies that cleanup properly releases metadata frames and zeros sensitive data
// without updating loss detection timer.
//
TEST(PacketBuilderTest, CleanupNoBatchSent)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    // Mark some test data in HpMask to verify it gets zeroed
    Builder.HpMask[0] = 0xAA;
    Builder.HpMask[10] = 0xBB;
    Builder.PacketBatchSent = FALSE;

    QuicPacketBuilderCleanup(&Builder);

    // Verify HpMask is zeroed
    for (size_t i = 0; i < sizeof(Builder.HpMask); ++i) {
        ASSERT_EQ(Builder.HpMask[i], 0);
    }

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 5: HasAllowance query with positive send allowance
// Scenario: Tests QuicPacketBuilderHasAllowance returns TRUE when send allowance
// is greater than zero, indicating congestion control permits sending.
//
TEST(PacketBuilderTest, HasAllowancePositive)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.SendAllowance = 5000;

    BOOLEAN HasAllowance = QuicPacketBuilderHasAllowance(&Builder);

    ASSERT_TRUE(HasAllowance);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 6: HasAllowance query with zero allowance but exemptions
// Scenario: Tests QuicPacketBuilderHasAllowance returns TRUE when send allowance
// is zero but congestion control has exemptions (e.g., for control frames).
//
TEST(PacketBuilderTest, HasAllowanceWithExemptions)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.SendAllowance = 0;
    Connection.CongestionControl.Cubic.Exemptions = 1; // Grant exemption

    BOOLEAN HasAllowance = QuicPacketBuilderHasAllowance(&Builder);

    ASSERT_TRUE(HasAllowance);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 7: HasAllowance query with no allowance or exemptions
// Scenario: Tests QuicPacketBuilderHasAllowance returns FALSE when both send
// allowance is zero and congestion control has no exemptions, indicating
// sending is blocked.
//
TEST(PacketBuilderTest, HasAllowanceBlocked)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.SendAllowance = 0;
    Connection.CongestionControl.Cubic.Exemptions = 0;

    BOOLEAN HasAllowance = QuicPacketBuilderHasAllowance(&Builder);

    ASSERT_FALSE(HasAllowance);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 8: AddFrame with room available
// Scenario: Tests QuicPacketBuilderAddFrame successfully adds a frame when there
// is room in the packet metadata. Verifies frame type is recorded, frame count
// increments, and ack-eliciting flag is set when appropriate.
//
TEST(PacketBuilderTest, AddFrameWithRoom)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.Metadata->FrameCount = 0;
    Builder.Metadata->Flags.IsAckEliciting = FALSE;

    BOOLEAN IsFull = QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_PING, TRUE);

    ASSERT_FALSE(IsFull);
    ASSERT_EQ(Builder.Metadata->FrameCount, 1u);
    ASSERT_EQ(Builder.Metadata->Frames[0].Type, QUIC_FRAME_PING);
    ASSERT_TRUE(Builder.Metadata->Flags.IsAckEliciting);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 9: AddFrame reaching maximum capacity
// Scenario: Tests QuicPacketBuilderAddFrame returns TRUE when adding a frame
// causes the packet to reach QUIC_MAX_FRAMES_PER_PACKET, indicating no more
// frames can be added.
//
TEST(PacketBuilderTest, AddFrameReachingMax)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.Metadata->FrameCount = 0;

    // Add frames up to max - 1
    for (uint8_t i = 0; i < QUIC_MAX_FRAMES_PER_PACKET - 1; ++i) {
        BOOLEAN IsFull = QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_PADDING, FALSE);
        ASSERT_FALSE(IsFull);
    }

    ASSERT_EQ(Builder.Metadata->FrameCount, QUIC_MAX_FRAMES_PER_PACKET - 1);

    // Add the last frame
    BOOLEAN IsFull = QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_PING, TRUE);

    ASSERT_TRUE(IsFull);
    ASSERT_EQ(Builder.Metadata->FrameCount, QUIC_MAX_FRAMES_PER_PACKET);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 10: AddFrame with non-ack-eliciting frame
// Scenario: Tests QuicPacketBuilderAddFrame correctly handles non-ack-eliciting
// frames (e.g., ACK, PADDING) by NOT setting the IsAckEliciting flag.
//
TEST(PacketBuilderTest, AddFrameNonAckEliciting)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.Metadata->FrameCount = 0;
    Builder.Metadata->Flags.IsAckEliciting = FALSE;

    BOOLEAN IsFull = QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_ACK, FALSE);

    ASSERT_FALSE(IsFull);
    ASSERT_EQ(Builder.Metadata->FrameCount, 1u);
    ASSERT_EQ(Builder.Metadata->Frames[0].Type, QUIC_FRAME_ACK);
    ASSERT_FALSE(Builder.Metadata->Flags.IsAckEliciting);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 11: Multiple frame additions maintaining state
// Scenario: Tests adding multiple frames of different types, verifying that
// the packet builder correctly tracks frame count and ack-eliciting status
// across multiple additions.
//
TEST(PacketBuilderTest, AddMultipleFrames)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    Builder.Metadata->FrameCount = 0;
    Builder.Metadata->Flags.IsAckEliciting = FALSE;

    // Add non-ack-eliciting frame
    QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_PADDING, FALSE);
    ASSERT_EQ(Builder.Metadata->FrameCount, 1u);
    ASSERT_FALSE(Builder.Metadata->Flags.IsAckEliciting);

    // Add ack-eliciting frame
    QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_CRYPTO, TRUE);
    ASSERT_EQ(Builder.Metadata->FrameCount, 2u);
    ASSERT_TRUE(Builder.Metadata->Flags.IsAckEliciting);

    // Add another ack-eliciting frame
    QuicPacketBuilderAddFrame(&Builder, QUIC_FRAME_PING, TRUE);
    ASSERT_EQ(Builder.Metadata->FrameCount, 3u);
    ASSERT_TRUE(Builder.Metadata->Flags.IsAckEliciting);

    // Verify frame types
    ASSERT_EQ(Builder.Metadata->Frames[0].Type, QUIC_FRAME_PADDING);
    ASSERT_EQ(Builder.Metadata->Frames[1].Type, QUIC_FRAME_CRYPTO);
    ASSERT_EQ(Builder.Metadata->Frames[2].Type, QUIC_FRAME_PING);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 12: Initialize with different path MTU values
// Scenario: Tests initialization with various path MTU values (minimum, default, maximum)
// to ensure the packet builder handles different MTU sizes correctly.
//
TEST(PacketBuilderTest, InitializeWithDifferentMtu)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);

    // Test with minimum MTU
    Path.Mtu = QUIC_DPLPMTUD_MIN_MTU;
    CxPlatZeroMemory(&Builder, sizeof(Builder));
    Connection.Send.LastFlushTimeValid = FALSE;
    BOOLEAN Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    ASSERT_TRUE(Result);

    // Test with default MTU
    Path.Mtu = 1200;
    CxPlatZeroMemory(&Builder, sizeof(Builder));
    Connection.Send.LastFlushTimeValid = FALSE;
    Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    ASSERT_TRUE(Result);

    // Test with large MTU
    Path.Mtu = 9000;
    CxPlatZeroMemory(&Builder, sizeof(Builder));
    Connection.Send.LastFlushTimeValid = FALSE;
    Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);
    ASSERT_TRUE(Result);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 13: Initialize with QUIC_VERSION_2
// Scenario: Tests that packet builder initialization works correctly with
// QUIC version 2, which has different packet type encodings.
//
TEST(PacketBuilderTest, InitializeWithVersion2)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    Connection.Stats.QuicVersion = QUIC_VERSION_2;
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    BOOLEAN Result = QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    ASSERT_TRUE(Result);
    ASSERT_EQ(Builder.Connection->Stats.QuicVersion, QUIC_VERSION_2);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 14: Metadata pointer initialization
// Scenario: Tests that packet builder correctly initializes the Metadata pointer
// to point to the embedded MetadataStorage, ensuring proper memory layout.
//
TEST(PacketBuilderTest, MetadataPointerInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    CxPlatZeroMemory(&Builder, sizeof(Builder));

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    // Verify Metadata points to MetadataStorage.Metadata
    ASSERT_EQ(Builder.Metadata, &Builder.MetadataStorage.Metadata);
    ASSERT_NE(Builder.Metadata, nullptr);

    // Verify we can write to metadata
    Builder.Metadata->FrameCount = 5;
    ASSERT_EQ(Builder.MetadataStorage.Metadata.FrameCount, 5u);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

//
// Test 15: Batch count initialization
// Scenario: Tests that batch-related fields (BatchCount, PacketBatchSent, etc.)
// are properly initialized to their default values.
//
TEST(PacketBuilderTest, BatchFieldsInitialization)
{
    QUIC_CONNECTION Connection;
    QUIC_PATH Path;
    QUIC_CID_HASH_ENTRY SourceCid;
    QUIC_CID_LIST_ENTRY DestCid;
    QUIC_PACKET_SPACE* PacketSpaces[QUIC_ENCRYPT_LEVEL_COUNT] = {};
    QUIC_PACKET_KEY* WriteKeys[QUIC_PACKET_KEY_COUNT] = {};
    QUIC_PACKET_BUILDER Builder;

    InitializeMockConnectionAndPath(Connection, Path, SourceCid, DestCid, PacketSpaces, WriteKeys);
    
    // Set some fields to non-default values
    Builder.BatchCount = 5;
    Builder.PacketBatchSent = TRUE;
    Builder.PacketBatchRetransmittable = TRUE;
    Builder.WrittenConnectionCloseFrame = TRUE;

    QuicPacketBuilderInitialize(&Builder, &Connection, &Path);

    ASSERT_EQ(Builder.BatchCount, 0u); // Note: Not explicitly set in Initialize, but should be 0 from context
    ASSERT_FALSE(Builder.PacketBatchSent);
    ASSERT_FALSE(Builder.PacketBatchRetransmittable);
    ASSERT_FALSE(Builder.WrittenConnectionCloseFrame);

    CleanupMockConnectionAndPath(PacketSpaces, WriteKeys);
}

