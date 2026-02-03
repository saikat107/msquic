/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

--*/

#define _CRT_SECURE_NO_WARNINGS 1
#include "main.h"
#include "msquic.h"
#include "msquichelper.h"
#include "quic_toeplitz.h"
#include <stdio.h>

#ifdef QUIC_CLOG
#include "ToeplitzTest.cpp.clog.h"
#endif

struct ToeplitzTest : public ::testing::Test
{
    protected:
    static const char* HashKey;

    struct QuicBuffer
    {
        uint8_t* Data;
        uint16_t Length;

        QuicBuffer(const char* HexBytes)
        {
            Length = (uint16_t)(strlen(HexBytes) / 2);
            Data = new uint8_t[Length];

            for (uint16_t i = 0; i < Length; ++i) {
                Data[i] =
                    (DecodeHexChar(HexBytes[i * 2]) << 4) |
                    DecodeHexChar(HexBytes[i * 2 + 1]);
            }
        }

        QuicBuffer(const QuicBuffer&) = delete;
        QuicBuffer(QuicBuffer&&) = delete;
        QuicBuffer& operator=(const QuicBuffer&) = delete;
        QuicBuffer& operator=(QuicBuffer&&) = delete;

        ~QuicBuffer()
        {
            delete [] Data;
        }
    };

    struct QuicTestAddress {
        QUIC_ADDR Addr;
        QuicTestAddress() : Addr{} {}
        QuicTestAddress(const QUIC_ADDR& Address) : Addr(Address) {}
        QuicTestAddress(const char* AddrStr, uint16_t Port) {
            EXPECT_TRUE(QuicAddrFromString(AddrStr, Port, &Addr));
        }
        operator QUIC_ADDR*() { return &Addr; }
        QuicTestAddress& operator=(const QuicTestAddress& Address) {
            Addr = Address.Addr;
            return *this;
        }
    };

    static
    auto
    ValidateRssToeplitzHash(
        _In_ const char* ExpectedHash,
        _In_ const QUIC_ADDR* SourceAddress,
        _In_ const QUIC_ADDR* DestinationAddress,
        _In_ QUIC_ADDRESS_FAMILY Family
        )
    {
        static const QuicBuffer KeyBuffer(HashKey);

        CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
        CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
        ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
        CxPlatToeplitzHashInitialize(&ToeplitzHash);

        QuicBuffer ExpectedHashBuf(ExpectedHash);

        ASSERT_EQ(QuicAddrGetFamily(SourceAddress), Family);

        ASSERT_EQ(QuicAddrGetFamily(DestinationAddress), Family);

        uint32_t Key = 0, Offset = 0;
        CxPlatToeplitzHashComputeRss(&ToeplitzHash, SourceAddress, DestinationAddress, &Key, &Offset);

        // Flip the key around to match the expected hash array
        Key = CxPlatByteSwapUint32(Key);

        if (memcmp(ExpectedHashBuf.Data, &Key, 4)) {
            QUIC_ADDR_STR PrintBuf{};
            printf("Expected Hash: %s, Actual Hash: %x\n", ExpectedHash, CxPlatByteSwapUint32(Key));
            QuicAddrToString(SourceAddress, &PrintBuf);
            printf("Source Address: %s\n", PrintBuf.Address);
            QuicAddrToString(DestinationAddress, &PrintBuf);
            printf("Destination Address: %s\n", PrintBuf.Address);
            ASSERT_TRUE(FALSE);
        }
    }

};

const char* ToeplitzTest::HashKey = "6d5a56da255b0ec24167253d43a38fb0d0ca2bcbae7b30b477cb2da38030f20c6a42b73bbeac01fa";

TEST_F(ToeplitzTest, IPv4WithTcp)
{
    const char* ExpectedHashes[] = {
        "51ccc178",
        "c626b0ea",
        "5c2b394a",
        "afc7327f",
        "10e828a2"
    };
    const QuicTestAddress DestinationAddresses[] = {
        {"161.142.100.80", 1766},
        {"65.69.140.83",   4739},
        {"12.22.207.184", 38024},
        {"209.142.163.6",  2217},
        {"202.188.127.2",  1303},
    };
    const QuicTestAddress SourceAddresses[] = {
        {"66.9.149.187",    2794},
        {"199.92.111.2",   14230},
        {"24.19.198.95",   12898},
        {"38.27.205.30",   48228},
        {"153.39.163.191", 44251},
    };

    for(uint32_t i = 0; i < ARRAYSIZE(ExpectedHashes); i++) {
        printf("Testing Iteration %d...\n", i + 1);

        ValidateRssToeplitzHash(
            ExpectedHashes[i],
            &SourceAddresses[i].Addr,
            &DestinationAddresses[i].Addr,
            QUIC_ADDRESS_FAMILY_INET);
    }
}

TEST_F(ToeplitzTest, IPv6WithTcp)
{
    const char* ExpectedHashes[] = {
        "40207d3d",
        "dde51bbf",
        "02d1feef"
    };
    const QuicTestAddress SourceAddresses[] = {
        {"3ffe:2501:200:1fff::7",                2794},
        {"3ffe:501:8::260:97ff:fe40:efab",      14230},
        {"3ffe:1900:4545:3:200:f8ff:fe21:67cf", 44251}
    };
    const QuicTestAddress DestinationAddresses[] = {
        {"3ffe:2501:200:3::1",        1766},
        {"ff02::1",                   4739},
        {"fe80::200:f8ff:fe21:67cf", 38024},
    };

    for(uint32_t i = 0; i < ARRAYSIZE(ExpectedHashes); i++) {
        printf("Testing Iteration %d...\n", i + 1);

        ValidateRssToeplitzHash(
            ExpectedHashes[i],
            &SourceAddresses[i].Addr,
            &DestinationAddresses[i].Addr,
            QUIC_ADDRESS_FAMILY_INET6);
    }
}

//
// Test: DirectHashComputation
// Scenario: Tests CxPlatToeplitzHashCompute directly with raw byte arrays
// Method: Initialize Toeplitz structure, then hash known byte sequences
// Assertions: Hash output matches expected values computed from Toeplitz algorithm
//
TEST_F(ToeplitzTest, DirectHashComputation)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    // Initialize Toeplitz hash with standard RSS key
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Test 1: Hash a simple 4-byte sequence
    uint8_t Input1[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t Hash1 = CxPlatToeplitzHashCompute(&ToeplitzHash, Input1, sizeof(Input1), 0);
    
    // The hash should be deterministic - computing it again should give the same result
    uint32_t Hash1Again = CxPlatToeplitzHashCompute(&ToeplitzHash, Input1, sizeof(Input1), 0);
    ASSERT_EQ(Hash1, Hash1Again);
    
    // Test 2: Hash a different sequence should give different result
    uint8_t Input2[] = {0x05, 0x06, 0x07, 0x08};
    uint32_t Hash2 = CxPlatToeplitzHashCompute(&ToeplitzHash, Input2, sizeof(Input2), 0);
    ASSERT_NE(Hash1, Hash2);
    
    // Test 3: Hash all zeros
    uint8_t Input3[] = {0x00, 0x00, 0x00, 0x00};
    uint32_t Hash3 = CxPlatToeplitzHashCompute(&ToeplitzHash, Input3, sizeof(Input3), 0);
    ASSERT_EQ(Hash3, 0u); // Hash of all zeros should be zero
    
    // Test 4: Hash all ones
    uint8_t Input4[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t Hash4 = CxPlatToeplitzHashCompute(&ToeplitzHash, Input4, sizeof(Input4), 0);
    ASSERT_NE(Hash4, 0u); // Hash of all ones should be non-zero
    ASSERT_NE(Hash4, 0xFFFFFFFFu); // And not all ones either
}

//
// Test: ComputeAddrIPv4
// Scenario: Tests CxPlatToeplitzHashComputeAddr for single IPv4 address
// Method: Hash a single IPv4 address (port + IP) and verify correct offset
// Assertions: Hash is computed, offset is correct (6 bytes for IPv4), hash is deterministic
//
TEST_F(ToeplitzTest, ComputeAddrIPv4)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Create test IPv4 address
    QuicTestAddress TestAddr("192.168.1.100", 12345);
    ASSERT_EQ(QuicAddrGetFamily(&TestAddr.Addr), QUIC_ADDRESS_FAMILY_INET);
    
    // Compute hash
    uint32_t Key = 0;
    uint32_t Offset = 0;
    CxPlatToeplitzHashComputeAddr(&ToeplitzHash, &TestAddr.Addr, &Key, &Offset);
    
    // Verify offset is correct for IPv4 (2 bytes port + 4 bytes IP = 6)
    ASSERT_EQ(Offset, 6u);
    
    // Verify hash is non-zero
    ASSERT_NE(Key, 0u);
    
    // Verify determinism - computing again should XOR the same value
    uint32_t Key2 = 0;
    uint32_t Offset2 = 0;
    CxPlatToeplitzHashComputeAddr(&ToeplitzHash, &TestAddr.Addr, &Key2, &Offset2);
    ASSERT_EQ(Key, Key2);
    ASSERT_EQ(Offset, Offset2);
}

//
// Test: ComputeAddrIPv6
// Scenario: Tests CxPlatToeplitzHashComputeAddr for single IPv6 address
// Method: Hash a single IPv6 address (port + IP) and verify correct offset
// Assertions: Hash is computed, offset is correct (18 bytes for IPv6), hash is deterministic
//
TEST_F(ToeplitzTest, ComputeAddrIPv6)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Create test IPv6 address
    QuicTestAddress TestAddr("2001:db8::1", 54321);
    ASSERT_EQ(QuicAddrGetFamily(&TestAddr.Addr), QUIC_ADDRESS_FAMILY_INET6);
    
    // Compute hash
    uint32_t Key = 0;
    uint32_t Offset = 0;
    CxPlatToeplitzHashComputeAddr(&ToeplitzHash, &TestAddr.Addr, &Key, &Offset);
    
    // Verify offset is correct for IPv6 (2 bytes port + 16 bytes IP = 18)
    ASSERT_EQ(Offset, 18u);
    
    // Verify hash is non-zero
    ASSERT_NE(Key, 0u);
    
    // Verify determinism
    uint32_t Key2 = 0;
    uint32_t Offset2 = 0;
    CxPlatToeplitzHashComputeAddr(&ToeplitzHash, &TestAddr.Addr, &Key2, &Offset2);
    ASSERT_EQ(Key, Key2);
    ASSERT_EQ(Offset, Offset2);
}

//
// Test: HashWithOffset
// Scenario: Tests CxPlatToeplitzHashCompute with non-zero offset parameter
// Method: Hash the same data at different offsets and verify different results
// Assertions: Same data at different offsets produces different hashes
//
TEST_F(ToeplitzTest, HashWithOffset)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Test data
    uint8_t TestData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    
    // Hash at offset 0
    uint32_t Hash0 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, sizeof(TestData), 0);
    
    // Hash at offset 4
    uint32_t Hash4 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, sizeof(TestData), 4);
    
    // Hash at offset 8
    uint32_t Hash8 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, sizeof(TestData), 8);
    
    // All three should be different
    ASSERT_NE(Hash0, Hash4);
    ASSERT_NE(Hash0, Hash8);
    ASSERT_NE(Hash4, Hash8);
    
    // All should be non-zero
    ASSERT_NE(Hash0, 0u);
    ASSERT_NE(Hash4, 0u);
    ASSERT_NE(Hash8, 0u);
}

//
// Test: XorCompositionProperty
// Scenario: Validates the XOR composition property of Toeplitz hash
// Method: Hash(A||B) should equal Hash(A, offset=0) XOR Hash(B, offset=len(A))
// Assertions: XOR composition property holds for concatenated inputs
//
TEST_F(ToeplitzTest, XorCompositionProperty)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Two separate inputs
    uint8_t Input1[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t Input2[] = {0x55, 0x66, 0x77, 0x88};
    
    // Concatenated input
    uint8_t InputCombined[8];
    CxPlatCopyMemory(InputCombined, Input1, sizeof(Input1));
    CxPlatCopyMemory(InputCombined + sizeof(Input1), Input2, sizeof(Input2));
    
    // Hash the combined input
    uint32_t HashCombined = CxPlatToeplitzHashCompute(&ToeplitzHash, InputCombined, sizeof(InputCombined), 0);
    
    // Hash the parts separately with appropriate offsets
    uint32_t Hash1 = CxPlatToeplitzHashCompute(&ToeplitzHash, Input1, sizeof(Input1), 0);
    uint32_t Hash2 = CxPlatToeplitzHashCompute(&ToeplitzHash, Input2, sizeof(Input2), sizeof(Input1));
    
    // The XOR of the parts should equal the hash of the combined input
    uint32_t HashXor = Hash1 ^ Hash2;
    ASSERT_EQ(HashCombined, HashXor);
}

//
// Test: ZeroLengthInput
// Scenario: Tests edge case of zero-length input
// Method: Hash empty input (length=0) at various offsets
// Assertions: Zero-length hash returns 0 (no bits to hash)
//
TEST_F(ToeplitzTest, ZeroLengthInput)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    uint8_t DummyData[] = {0x00};
    
    // Hash zero bytes at offset 0
    uint32_t Hash0 = CxPlatToeplitzHashCompute(&ToeplitzHash, DummyData, 0, 0);
    ASSERT_EQ(Hash0, 0u);
    
    // Hash zero bytes at offset 10
    uint32_t Hash10 = CxPlatToeplitzHashCompute(&ToeplitzHash, DummyData, 0, 10);
    ASSERT_EQ(Hash10, 0u);
    
    // All zero-length hashes should return 0
    uint32_t Hash20 = CxPlatToeplitzHashCompute(&ToeplitzHash, DummyData, 0, 20);
    ASSERT_EQ(Hash20, 0u);
}

//
// Test: MaximumOffset
// Scenario: Tests boundary condition with maximum valid offset
// Method: Hash data at the maximum allowed offset for the input size
// Assertions: Hash succeeds at boundary offset
//
TEST_F(ToeplitzTest, MaximumOffset)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Test with 4 bytes at maximum valid offset
    uint8_t TestData[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t MaxOffset = CXPLAT_TOEPLITZ_INPUT_SIZE_IP - sizeof(TestData);
    
    // This should succeed without assertion failure
    uint32_t Hash = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, sizeof(TestData), MaxOffset);
    
    // Hash should be non-zero for this non-zero input
    ASSERT_NE(Hash, 0u);
    
    // Verify determinism at max offset
    uint32_t Hash2 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, sizeof(TestData), MaxOffset);
    ASSERT_EQ(Hash, Hash2);
}

//
// Test: QuicInputSize
// Scenario: Tests initialization with CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC (38 bytes)
// Method: Initialize with QUIC input size and hash data
// Assertions: Initialization succeeds and hashing works with larger input size
//
TEST_F(ToeplitzTest, QuicInputSize)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_QUIC;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Hash 20 bytes (CID size) at offset 0
    uint8_t CidData[20];
    for (uint8_t i = 0; i < sizeof(CidData); i++) {
        CidData[i] = i;
    }
    
    uint32_t HashCid = CxPlatToeplitzHashCompute(&ToeplitzHash, CidData, sizeof(CidData), 0);
    ASSERT_NE(HashCid, 0u);
    
    // Hash 16 bytes (IPv6 address) at offset 20
    uint8_t IpData[16];
    for (uint8_t i = 0; i < sizeof(IpData); i++) {
        IpData[i] = 0xFF - i;
    }
    
    uint32_t HashIp = CxPlatToeplitzHashCompute(&ToeplitzHash, IpData, sizeof(IpData), 20);
    ASSERT_NE(HashIp, 0u);
    
    // Hash 2 bytes (port) at offset 36
    uint8_t PortData[] = {0x12, 0x34};
    uint32_t HashPort = CxPlatToeplitzHashCompute(&ToeplitzHash, PortData, sizeof(PortData), 36);
    ASSERT_NE(HashPort, 0u);
    
    // All three hashes should be different
    ASSERT_NE(HashCid, HashIp);
    ASSERT_NE(HashCid, HashPort);
    ASSERT_NE(HashIp, HashPort);
}

//
// Test: DeterminismCheck
// Scenario: Validates hash consistency across multiple invocations
// Method: Hash same input multiple times and verify all results match
// Assertions: Hash function is deterministic - same input always produces same output
//
TEST_F(ToeplitzTest, DeterminismCheck)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Test data
    uint8_t TestData[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    
    // Compute hash multiple times
    const int NumIterations = 10;
    uint32_t Hashes[NumIterations];
    
    for (int i = 0; i < NumIterations; i++) {
        Hashes[i] = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, sizeof(TestData), 0);
    }
    
    // All hashes should be identical
    for (int i = 1; i < NumIterations; i++) {
        ASSERT_EQ(Hashes[0], Hashes[i]);
    }
    
    // Hash should be non-zero
    ASSERT_NE(Hashes[0], 0u);
}

//
// Test: PartialInputHashing
// Scenario: Tests hashing partial inputs with varying HashInputLength parameter
// Method: Hash different lengths of the same input buffer
// Assertions: Different lengths produce different hashes, shorter inputs produce subsets of hash
//
TEST_F(ToeplitzTest, PartialInputHashing)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Test buffer
    uint8_t TestData[16];
    for (uint8_t i = 0; i < sizeof(TestData); i++) {
        TestData[i] = i * 17; // Some pattern
    }
    
    // Hash different lengths
    uint32_t Hash4 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, 4, 0);
    uint32_t Hash8 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, 8, 0);
    uint32_t Hash12 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, 12, 0);
    uint32_t Hash16 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData, 16, 0);
    
    // All should be different
    ASSERT_NE(Hash4, Hash8);
    ASSERT_NE(Hash4, Hash12);
    ASSERT_NE(Hash4, Hash16);
    ASSERT_NE(Hash8, Hash12);
    ASSERT_NE(Hash8, Hash16);
    ASSERT_NE(Hash12, Hash16);
    
    // Verify XOR composition: Hash4 XOR Hash(next 4 at offset 4) should equal Hash8
    uint32_t HashNext4 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestData + 4, 4, 4);
    ASSERT_EQ(Hash4 ^ HashNext4, Hash8);
}

//
// Test: SingleByteHashing
// Scenario: Tests hashing of single bytes at various positions
// Method: Hash individual bytes across the input range
// Assertions: Single byte hashes are unique and position-dependent
//
TEST_F(ToeplitzTest, SingleByteHashing)
{
    static const QuicBuffer KeyBuffer(HashKey);
    
    CXPLAT_TOEPLITZ_HASH ToeplitzHash{};
    CxPlatCopyMemory(ToeplitzHash.HashKey, KeyBuffer.Data, KeyBuffer.Length);
    ToeplitzHash.InputSize = CXPLAT_TOEPLITZ_INPUT_SIZE_IP;
    CxPlatToeplitzHashInitialize(&ToeplitzHash);
    
    // Hash same byte value at different offsets
    uint8_t TestByte[] = {0x42};
    
    uint32_t Hash0 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestByte, 1, 0);
    uint32_t Hash1 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestByte, 1, 1);
    uint32_t Hash2 = CxPlatToeplitzHashCompute(&ToeplitzHash, TestByte, 1, 2);
    
    // Same byte at different offsets should produce different hashes
    ASSERT_NE(Hash0, Hash1);
    ASSERT_NE(Hash0, Hash2);
    ASSERT_NE(Hash1, Hash2);
    
    // All should be non-zero
    ASSERT_NE(Hash0, 0u);
    ASSERT_NE(Hash1, 0u);
    ASSERT_NE(Hash2, 0u);
}
