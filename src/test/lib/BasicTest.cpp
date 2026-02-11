/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Basic MsQuic API Functionality.

--*/

#include "precomp.h"
#ifdef QUIC_CLOG
#include "BasicTest.cpp.clog.h"
#endif

#ifdef QUIC_API_ENABLE_PREVIEW_FEATURES

namespace {

struct RegistrationCloseContext {
    CxPlatEvent Event;
};

_Function_class_(QUIC_REGISTRATION_CLOSE_CALLBACK)
void
QUIC_API RegistrationCloseCallback(
    _In_opt_ void* Context
    )
{
    RegistrationCloseContext* CloseContext = (RegistrationCloseContext*)Context;
    CloseContext->Event.Set();
}

}

#endif

void QuicTestRegistrationOpenClose()
{
    //
    // Open and syncrhonous close
    //
    {
        MsQuicRegistration Registration;
        TEST_TRUE(Registration.IsValid());
    }

#ifdef QUIC_API_ENABLE_PREVIEW_FEATURES
    //
    // Open and asyncrhonous close
    //
    {
        MsQuicRegistration Registration;
        TEST_TRUE(Registration.IsValid());

        RegistrationCloseContext Context{};
        Registration.CloseAsync(RegistrationCloseCallback, &Context);
        Context.Event.WaitForever();
    }
#endif
}

_Function_class_(NEW_CONNECTION_CALLBACK)
static
bool
QUIC_API
ListenerDoNothingCallback(
    _In_ TestListener* /* Listener */,
    _In_ HQUIC /* ConnectionHandle */
    )
{
    TEST_FAILURE("This callback should never be called!");
    return false;
}

void QuicTestCreateListener()
{
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, nullptr);
        TEST_TRUE(Listener.IsValid());
    }

    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());
    }
}

void QuicTestStartListener()
{
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());
    MsQuicAlpn Alpn("MsQuicTest");
    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());
        TEST_QUIC_SUCCEEDED(Listener.Start(Alpn, Alpn.Length()));
    }

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());
        QuicAddr LocalAddress(QUIC_ADDRESS_FAMILY_UNSPEC);
        TEST_QUIC_SUCCEEDED(Listener.Start(Alpn, Alpn.Length(), &LocalAddress.SockAddr));
    }
}

void QuicTestStartListenerMultiAlpns()
{
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());
    MsQuicAlpn Alpn("MsQuicTest1", "MsQuicTest2");
    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());
        TEST_QUIC_SUCCEEDED(Listener.Start(Alpn, Alpn.Length()));
    }

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());
        QuicAddr LocalAddress(QUIC_ADDRESS_FAMILY_UNSPEC);
        TEST_QUIC_SUCCEEDED(Listener.Start(Alpn, Alpn.Length(), &LocalAddress.SockAddr));
    }
}

void QuicTestStartListenerImplicit(const FamilyArgs& Params)
{
    const int Family = Params.Family;
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());
    MsQuicAlpn Alpn("MsQuicTest");
    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());

        QuicAddr LocalAddress(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
        TEST_QUIC_SUCCEEDED(Listener.Start(Alpn, Alpn.Length(), &LocalAddress.SockAddr));
    }
}

void QuicTestStartTwoListeners()
{
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());
    MsQuicAlpn Alpn1("MsQuicTest");
    MsQuicConfiguration ServerConfiguration1(Registration, Alpn1, ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration1.IsValid());
    MsQuicAlpn Alpn2("MsQuicTest2");
    MsQuicConfiguration ServerConfiguration2(Registration, Alpn2, ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration2.IsValid());

    {
        TestListener Listener1(Registration, ListenerDoNothingCallback, ServerConfiguration1);
        TEST_TRUE(Listener1.IsValid());
        TEST_QUIC_SUCCEEDED(Listener1.Start(Alpn1, Alpn1.Length()));

        QuicAddr LocalAddress;
        TEST_QUIC_SUCCEEDED(Listener1.GetLocalAddr(LocalAddress));

        TestListener Listener2(Registration, ListenerDoNothingCallback, ServerConfiguration2);
        TEST_TRUE(Listener2.IsValid());
        TEST_QUIC_SUCCEEDED(Listener2.Start(Alpn2, Alpn2.Length(), &LocalAddress.SockAddr));
    }
}

void QuicTestStartTwoListenersSameALPN()
{
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());
    MsQuicAlpn Alpn1("MsQuicTest");
    MsQuicConfiguration ServerConfiguration1(Registration, Alpn1, ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration1.IsValid());
    MsQuicAlpn Alpn2("MsQuicTest", "MsQuicTest2");
    MsQuicConfiguration ServerConfiguration2(Registration, Alpn2, ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration2.IsValid());

    {
        //
        // Both try to listen on the same, single ALPN
        //
        TestListener Listener1(Registration, ListenerDoNothingCallback, ServerConfiguration1);
        TEST_TRUE(Listener1.IsValid());
        TEST_QUIC_SUCCEEDED(Listener1.Start(Alpn1, Alpn1.Length()));

        QuicAddr LocalAddress;
        TEST_QUIC_SUCCEEDED(Listener1.GetLocalAddr(LocalAddress));

        TestListener Listener2(Registration, ListenerDoNothingCallback, ServerConfiguration1);
        TEST_TRUE(Listener2.IsValid());
        TEST_QUIC_STATUS(
            QUIC_STATUS_ALPN_IN_USE,
            Listener2.Start(Alpn1, Alpn1.Length(), &LocalAddress.SockAddr));
    }

    {
        //
        // First listener on two ALPNs and second overlaps one of those.
        //
        TestListener Listener1(Registration, ListenerDoNothingCallback, ServerConfiguration2);
        TEST_TRUE(Listener1.IsValid());
        TEST_QUIC_SUCCEEDED(Listener1.Start(Alpn2, Alpn2.Length()));

        QuicAddr LocalAddress;
        TEST_QUIC_SUCCEEDED(Listener1.GetLocalAddr(LocalAddress));

        TestListener Listener2(Registration, ListenerDoNothingCallback, ServerConfiguration1);
        TEST_TRUE(Listener2.IsValid());
        TEST_QUIC_STATUS(
            QUIC_STATUS_ALPN_IN_USE,
            Listener2.Start(Alpn1, Alpn1.Length(), &LocalAddress.SockAddr));
    }

    {
        //
        // First listener on one ALPN and second with two (one that overlaps).
        //
        TestListener Listener1(Registration, ListenerDoNothingCallback, ServerConfiguration1);
        TEST_TRUE(Listener1.IsValid());
        TEST_QUIC_SUCCEEDED(Listener1.Start(Alpn1, Alpn1.Length()));

        QuicAddr LocalAddress;
        TEST_QUIC_SUCCEEDED(Listener1.GetLocalAddr(LocalAddress));

        TestListener Listener2(Registration, ListenerDoNothingCallback, ServerConfiguration2);
        TEST_TRUE(Listener2.IsValid());
        TEST_QUIC_STATUS(
            QUIC_STATUS_ALPN_IN_USE,
            Listener2.Start(Alpn2, Alpn2.Length(), &LocalAddress.SockAddr));
    }
}

void QuicTestStartListenerExplicit(const FamilyArgs& Params)
{
    const int Family = Params.Family;
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());
    MsQuicAlpn Alpn("MsQuicTest");
    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    {
        TestListener Listener(Registration, ListenerDoNothingCallback, ServerConfiguration);
        TEST_TRUE(Listener.IsValid());

        QUIC_ADDRESS_FAMILY QuicAddrFamily = (Family == 4) ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6;
        QuicAddr LocalAddress(QuicAddr(QuicAddrFamily, true), TestUdpPortBase);
        if (UseDuoNic) {
            QuicAddrSetToDuoNic(&LocalAddress.SockAddr);
        }
        QUIC_STATUS Status = QUIC_STATUS_ADDRESS_IN_USE;
        while (Status == QUIC_STATUS_ADDRESS_IN_USE) {
            LocalAddress.IncrementPort();
            Status = Listener.Start(Alpn, Alpn.Length(), &LocalAddress.SockAddr);
        }
        TEST_QUIC_SUCCEEDED(Status);
    }
}

void QuicTestCreateConnection()
{
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());

    {
        TestConnection Connection(Registration);
        TEST_TRUE(Connection.IsValid());
    }
}

void QuicTestBindConnectionImplicit(const FamilyArgs& Params)
{
    const int Family = Params.Family;
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());

    {
        TestConnection Connection(Registration);
        TEST_TRUE(Connection.IsValid());

        QuicAddr LocalAddress(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
        TEST_QUIC_SUCCEEDED(Connection.SetLocalAddr(LocalAddress));
    }
}

void QuicTestBindConnectionExplicit(const FamilyArgs& Params)
{
    const int Family = Params.Family;
    MsQuicRegistration Registration;
    TEST_TRUE(Registration.IsValid());

    {
        TestConnection Connection(Registration);
        TEST_TRUE(Connection.IsValid());

        QUIC_ADDRESS_FAMILY QuicAddrFamily = (Family == 4) ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6;
        QuicAddr LocalAddress(QuicAddr(QuicAddrFamily, true), TestUdpPortBase);
        if (UseDuoNic) {
            QuicAddrSetToDuoNic(&LocalAddress.SockAddr);
        }
        QUIC_STATUS Status = QUIC_STATUS_ADDRESS_IN_USE;
        while (Status == QUIC_STATUS_ADDRESS_IN_USE) {
            LocalAddress.IncrementPort();
            Status = Connection.SetLocalAddr(LocalAddress);
        }
        TEST_QUIC_SUCCEEDED(Status);
    }
}

void QuicTestAddrFunctions(const FamilyArgs& Params)
{
    const int Family = Params.Family;
    QUIC_ADDR SockAddr;
    QUIC_ADDRESS_FAMILY QuicAddrFamily = (Family == 4) ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6;

    // initialize the struct to 0xFF to ensure any code issues are caught by the following tests
    memset(&SockAddr, 0xFF, sizeof(SockAddr));

    QuicAddrSetFamily(&SockAddr, QuicAddrFamily);
    TEST_TRUE(QuicAddrGetFamily(&SockAddr) == QuicAddrFamily);

    QuicAddrSetToLoopback(&SockAddr);

    if (QuicAddrFamily == QUIC_ADDRESS_FAMILY_INET) {
        TEST_TRUE((SockAddr.Ipv4.sin_addr.s_addr & 0x00FFFF00UL) == 0);
    } else {
        for (unsigned long i = 0; i < sizeof(SockAddr.Ipv6.sin6_addr) - 1; i++) {
            TEST_TRUE(SockAddr.Ipv6.sin6_addr.s6_addr[i] == 0);
        }
    }

    TEST_TRUE(QuicAddrGetFamily(&SockAddr) == QuicAddrFamily);
}

//
// QUIC Range Tests
//

//
// Scenario: Basic lifecycle - Initialize and uninitialize an empty range
// Tests QuicRangeInitialize and QuicRangeUninitialize with default settings
// Assertions: Range is properly initialized with expected default values
//
void QuicTestRangeInitUninit()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Verify initialization postconditions
    TEST_EQUAL(Range.UsedLength, 0);
    TEST_EQUAL(Range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_EQUAL(Range.MaxAllocSize, QUIC_RANGE_NO_MAX_ALLOC_SIZE);
    TEST_TRUE(Range.SubRanges == Range.PreAllocSubRanges);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add single value to empty range
// Tests QuicRangeAddValue with a single value insertion
// Assertions: Value is added, range size is 1, subrange contains correct Low and Count
//
void QuicTestRangeAddSingleValue()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add a single value
    TEST_TRUE(QuicRangeAddValue(&Range, 100));
    
    // Verify the range now contains one subrange with the value
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 100);
    TEST_EQUAL(Sub->Count, 1);
    
    // Verify we can query the value
    uint64_t Count, Value;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 100, &Count, &IsLast));
    TEST_EQUAL(Count, 1);
    TEST_TRUE(IsLast);
    
    TEST_TRUE(QuicRangeGetMinSafe(&Range, &Value));
    TEST_EQUAL(Value, 100);
    TEST_TRUE(QuicRangeGetMaxSafe(&Range, &Value));
    TEST_EQUAL(Value, 100);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add multiple non-overlapping values in ascending order
// Tests QuicRangeAddValue with sequential insertions that don't overlap
// Assertions: Each value creates a separate subrange, all values are retrievable
//
void QuicTestRangeAddMultipleAscending()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add non-overlapping values: 10, 20, 30
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    
    // Should have 3 separate subranges
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    // Verify each subrange
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 10);
    TEST_EQUAL(Sub0->Count, 1);
    
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    TEST_EQUAL(Sub1->Low, 20);
    TEST_EQUAL(Sub1->Count, 1);
    
    QUIC_SUBRANGE* Sub2 = QuicRangeGet(&Range, 2);
    TEST_EQUAL(Sub2->Low, 30);
    TEST_EQUAL(Sub2->Count, 1);
    
    // Verify min/max
    TEST_EQUAL(QuicRangeGetMin(&Range), 10);
    TEST_EQUAL(QuicRangeGetMax(&Range), 30);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add adjacent values that should merge into a single subrange
// Tests QuicRangeAddValue with values that are consecutive (e.g., 5, 6, 7)
// Assertions: Adjacent values are merged into one contiguous subrange via QuicRangeCompact
//
void QuicTestRangeAddAdjacentMerge()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add adjacent values: 5, 6, 7
    TEST_TRUE(QuicRangeAddValue(&Range, 5));
    TEST_TRUE(QuicRangeAddValue(&Range, 6));
    TEST_TRUE(QuicRangeAddValue(&Range, 7));
    
    // Should merge into a single subrange [5, 7] with Count=3
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 5);
    TEST_EQUAL(Sub->Count, 3);
    
    // Verify all values are present
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 5, &Count, &IsLast));
    TEST_EQUAL(Count, 3);
    TEST_TRUE(QuicRangeGetRange(&Range, 6, &Count, &IsLast));
    TEST_EQUAL(Count, 2);
    TEST_TRUE(QuicRangeGetRange(&Range, 7, &Count, &IsLast));
    TEST_EQUAL(Count, 1);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add a contiguous range using QuicRangeAddRange
// Tests QuicRangeAddRange to insert multiple consecutive values at once
// Assertions: Range is added as single subrange, RangeUpdated flag is set correctly
//
void QuicTestRangeAddContiguousRange()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [100, 109] (10 values)
    BOOLEAN RangeUpdated = FALSE;
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, 100, 10, &RangeUpdated);
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_TRUE(RangeUpdated);
    
    // Should be a single subrange
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    TEST_EQUAL(Sub->Low, 100);
    TEST_EQUAL(Sub->Count, 10);
    
    // Verify boundaries
    TEST_EQUAL(QuicRangeGetMin(&Range), 100);
    TEST_EQUAL(QuicRangeGetMax(&Range), 109);
    
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, 100, &Count, &IsLast));
    TEST_EQUAL(Count, 10);
    TEST_TRUE(QuicRangeGetRange(&Range, 105, &Count, &IsLast));
    TEST_EQUAL(Count, 5);
    TEST_TRUE(QuicRangeGetRange(&Range, 109, &Count, &IsLast));
    TEST_EQUAL(Count, 1);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add overlapping range that extends existing subrange
// Tests QuicRangeAddRange when new range overlaps and extends an existing subrange
// Assertions: Ranges are merged, RangeUpdated is TRUE, final subrange covers union
//
void QuicTestRangeAddOverlappingRange()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add initial range [10, 19]
    BOOLEAN RangeUpdated = FALSE;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 10, &RangeUpdated), nullptr);
    TEST_TRUE(RangeUpdated);
    
    // Add overlapping range [15, 24] - overlaps and extends
    RangeUpdated = FALSE;
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, 15, 10, &RangeUpdated);
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_TRUE(RangeUpdated);
    
    // Should merge into single subrange [10, 24]
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    TEST_EQUAL(Sub->Low, 10);
    TEST_EQUAL(Sub->Count, 15);
    TEST_EQUAL(QuicRangeGetMin(&Range), 10);
    TEST_EQUAL(QuicRangeGetMax(&Range), 24);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add range that subsumes multiple existing subranges
// Tests QuicRangeAddRange when new range covers several existing non-contiguous subranges
// Assertions: All overlapped subranges are merged into one, old subranges are removed
//
void QuicTestRangeAddSubsumesMultiple()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add three separate subranges
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 5, &RangeUpdated), nullptr);  // [10, 14]
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 20, 5, &RangeUpdated), nullptr);  // [20, 24]
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 5, &RangeUpdated), nullptr);  // [30, 34]
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    // Add range [5, 39] that subsumes all three
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, 5, 35, &RangeUpdated);
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_TRUE(RangeUpdated);
    
    // Should now be a single subrange
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    TEST_EQUAL(Sub->Low, 5);
    TEST_EQUAL(Sub->Count, 35);
    TEST_EQUAL(QuicRangeGetMin(&Range), 5);
    TEST_EQUAL(QuicRangeGetMax(&Range), 39);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove a value from the middle of a subrange (split operation)
// Tests QuicRangeRemoveRange when removal splits one subrange into two
// Assertions: Original subrange is split, two new subranges exist with correct boundaries
//
void QuicTestRangeRemoveMiddleSplit()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [10, 19]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 10, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    
    // Remove middle value [14, 15] - should split into [10, 13] and [16, 19]
    TEST_TRUE(QuicRangeRemoveRange(&Range, 14, 2));
    
    // Should now have 2 subranges
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 10);
    TEST_EQUAL(Sub0->Count, 4);  // [10, 13]
    
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    TEST_EQUAL(Sub1->Low, 16);
    TEST_EQUAL(Sub1->Count, 4);  // [16, 19]
    
    // Verify removed values are not present
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_FALSE(QuicRangeGetRange(&Range, 14, &Count, &IsLast));
    TEST_FALSE(QuicRangeGetRange(&Range, 15, &Count, &IsLast));
    
    // Verify remaining values
    TEST_TRUE(QuicRangeGetRange(&Range, 13, &Count, &IsLast));
    TEST_TRUE(QuicRangeGetRange(&Range, 16, &Count, &IsLast));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove range from the left edge of a subrange
// Tests QuicRangeRemoveRange when removal truncates the start of a subrange
// Assertions: Subrange Low is updated, Count is reduced, total size unchanged
//
void QuicTestRangeRemoveLeftEdge()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [50, 59]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 50, 10, &RangeUpdated), nullptr);
    
    // Remove left edge [50, 52]
    TEST_TRUE(QuicRangeRemoveRange(&Range, 50, 3));
    
    // Should still be 1 subrange, now [53, 59]
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 53);
    TEST_EQUAL(Sub->Count, 7);
    TEST_EQUAL(QuicRangeGetMin(&Range), 53);
    TEST_EQUAL(QuicRangeGetMax(&Range), 59);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove range from the right edge of a subrange
// Tests QuicRangeRemoveRange when removal truncates the end of a subrange
// Assertions: Subrange Low unchanged, Count is reduced
//
void QuicTestRangeRemoveRightEdge()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [100, 109]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 100, 10, &RangeUpdated), nullptr);
    
    // Remove right edge [107, 109]
    TEST_TRUE(QuicRangeRemoveRange(&Range, 107, 3));
    
    // Should still be 1 subrange, now [100, 106]
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 100);
    TEST_EQUAL(Sub->Count, 7);
    TEST_EQUAL(QuicRangeGetMin(&Range), 100);
    TEST_EQUAL(QuicRangeGetMax(&Range), 106);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove entire subrange
// Tests QuicRangeRemoveRange when removal exactly matches a subrange
// Assertions: Subrange is completely removed, size decreases
//
void QuicTestRangeRemoveFull()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add three subranges
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 20, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 5, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    // Remove the middle subrange completely [20, 24]
    TEST_TRUE(QuicRangeRemoveRange(&Range, 20, 5));
    
    // Should now have 2 subranges
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 10);
    TEST_EQUAL(Sub0->Count, 5);
    
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    TEST_EQUAL(Sub1->Low, 30);
    TEST_EQUAL(Sub1->Count, 5);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove non-existent range (no-op)
// Tests QuicRangeRemoveRange when the range to remove doesn't overlap with any existing subranges
// Assertions: Operation succeeds (returns TRUE), range unchanged
//
void QuicTestRangeRemoveNonExistent()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [100, 109]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 100, 10, &RangeUpdated), nullptr);
    
    // Remove non-existent range [50, 59]
    TEST_TRUE(QuicRangeRemoveRange(&Range, 50, 10));
    
    // Range should be unchanged
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 100);
    TEST_EQUAL(Sub->Count, 10);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: SetMin at subrange boundary
// Tests QuicRangeSetMin when the new minimum exactly matches a subrange Low value
// Assertions: Subranges below the minimum are removed, boundary subrange remains intact
//
void QuicTestRangeSetMinAtBoundary()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add three subranges: [10, 14], [20, 24], [30, 34]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 20, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 5, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    // Set minimum to 20 (start of second subrange)
    QuicRangeSetMin(&Range, 20);
    
    // First subrange should be removed
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    TEST_EQUAL(QuicRangeGetMin(&Range), 20);
    
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 20);
    TEST_EQUAL(Sub0->Count, 5);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: SetMin in middle of subrange
// Tests QuicRangeSetMin when the new minimum falls within a subrange
// Assertions: Subrange is truncated from the left, Low and Count are adjusted
//
void QuicTestRangeSetMinInMiddle()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [100, 119]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 100, 20, &RangeUpdated), nullptr);
    
    // Set minimum to 110 (middle of subrange)
    QuicRangeSetMin(&Range, 110);
    
    // Subrange should be truncated to [110, 119]
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 110);
    TEST_EQUAL(Sub->Count, 10);
    TEST_EQUAL(QuicRangeGetMin(&Range), 110);
    TEST_EQUAL(QuicRangeGetMax(&Range), 119);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: SetMin above all values (clears range)
// Tests QuicRangeSetMin when the new minimum is greater than all existing values
// Assertions: All subranges are removed, range becomes empty
//
void QuicTestRangeSetMinAboveAll()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add ranges
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 10, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 10, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    // Set minimum above all values
    QuicRangeSetMin(&Range, 1000);
    
    // Range should now be empty
    TEST_EQUAL(QuicRangeSize(&Range), 0);
    
    uint64_t Value;
    TEST_FALSE(QuicRangeGetMinSafe(&Range, &Value));
    TEST_FALSE(QuicRangeGetMaxSafe(&Range, &Value));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: SetMin below all values (no change)
// Tests QuicRangeSetMin when the new minimum is less than all existing values
// Assertions: Range is unchanged
//
void QuicTestRangeSetMinBelowAll()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add range [100, 109]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 100, 10, &RangeUpdated), nullptr);
    
    // Set minimum below all values
    QuicRangeSetMin(&Range, 50);
    
    // Range should be unchanged
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 100);
    TEST_EQUAL(Sub->Count, 10);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Reset range to empty state
// Tests QuicRangeReset after adding values
// Assertions: UsedLength becomes 0, allocation unchanged, subsequent queries return empty
//
void QuicTestRangeReset()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add several values
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 10, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 10, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    uint32_t AllocLengthBefore = Range.AllocLength;
    
    // Reset the range
    QuicRangeReset(&Range);
    
    // Verify reset postconditions
    TEST_EQUAL(Range.UsedLength, 0);
    TEST_EQUAL(Range.AllocLength, AllocLengthBefore);  // Allocation unchanged
    TEST_EQUAL(QuicRangeSize(&Range), 0);
    
    uint64_t Value;
    TEST_FALSE(QuicRangeGetMinSafe(&Range, &Value));
    TEST_FALSE(QuicRangeGetMaxSafe(&Range, &Value));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Query operations on empty range
// Tests QuicRangeGetMinSafe, QuicRangeGetMaxSafe, and QuicRangeGetRange on an empty range
// Assertions: Safe APIs return FALSE, no crashes occur
//
void QuicTestRangeEmptyQueries()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Verify empty range queries
    TEST_EQUAL(QuicRangeSize(&Range), 0);
    
    uint64_t Value;
    TEST_FALSE(QuicRangeGetMinSafe(&Range, &Value));
    TEST_FALSE(QuicRangeGetMaxSafe(&Range, &Value));
    
    uint64_t Count;
    BOOLEAN IsLast;
    TEST_FALSE(QuicRangeGetRange(&Range, 100, &Count, &IsLast));
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add duplicate value (no-op)
// Tests QuicRangeAddValue when value already exists in the range
// Assertions: Operation succeeds, RangeUpdated is FALSE, range unchanged
//
void QuicTestRangeAddDuplicate()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add value 50
    TEST_TRUE(QuicRangeAddValue(&Range, 50));
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    
    // Add same value again
    BOOLEAN RangeUpdated = TRUE;  // Set to TRUE to verify it gets set to FALSE
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, 50, 1, &RangeUpdated);
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_FALSE(RangeUpdated);  // Should be FALSE because value was already present
    
    // Range should be unchanged
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    TEST_EQUAL(Sub->Low, 50);
    TEST_EQUAL(Sub->Count, 1);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Compact range with adjacent subranges
// Tests QuicRangeCompact when subranges are adjacent (e.g., [10,14] and [15,19])
// Assertions: Adjacent subranges are merged into one contiguous subrange
//
void QuicTestRangeCompactAdjacent()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Manually create adjacent subranges by adding values with gaps, then filling
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 5, &RangeUpdated), nullptr);   // [10, 14]
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 15, 5, &RangeUpdated), nullptr);   // [15, 19]
    
    // Should already be merged by QuicRangeAddRange's internal compact call
    // But explicitly test compact
    QuicRangeCompact(&Range);
    
    // Should be merged into single subrange [10, 19]
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub->Low, 10);
    TEST_EQUAL(Sub->Count, 10);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Grow allocation beyond initial size
// Tests that adding enough subranges triggers reallocation from PreAllocSubRanges to heap
// Assertions: AllocLength increases, SubRanges pointer changes, all data preserved
//
void QuicTestRangeGrowAllocation()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Initial allocation is 8 subranges
    TEST_EQUAL(Range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_TRUE(Range.SubRanges == Range.PreAllocSubRanges);
    
    // Add 9 non-adjacent values to force growth (need more than 8 subranges)
    BOOLEAN RangeUpdated;
    for (uint32_t i = 0; i < 10; i++) {
        TEST_NOT_EQUAL(QuicRangeAddRange(&Range, i * 10, 1, &RangeUpdated), nullptr);
    }
    
    // Should have grown beyond initial allocation
    TEST_EQUAL(QuicRangeSize(&Range), 10);
    TEST_TRUE(Range.AllocLength > QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_TRUE(Range.SubRanges != Range.PreAllocSubRanges);  // Now on heap
    
    // Verify all values are still present
    for (uint32_t i = 0; i < 10; i++) {
        QUIC_SUBRANGE* Sub = QuicRangeGet(&Range, i);
        TEST_EQUAL(Sub->Low, i * 10);
        TEST_EQUAL(Sub->Count, 1);
    }
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Shrink allocation after removing many subranges
// Tests QuicRangeShrink is triggered when usage falls below threshold after removals
// Assertions: AllocLength decreases, all remaining data preserved
//
void QuicTestRangeShrinkAllocation()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add many subranges to grow allocation (32 subranges)
    BOOLEAN RangeUpdated;
    for (uint32_t i = 0; i < 32; i++) {
        TEST_NOT_EQUAL(QuicRangeAddRange(&Range, i * 10, 1, &RangeUpdated), nullptr);
    }
    TEST_EQUAL(QuicRangeSize(&Range), 32);
    
    uint32_t AllocAfterGrow = Range.AllocLength;
    TEST_TRUE(AllocAfterGrow >= 32);
    
    // Remove most subranges (keep only 2) to trigger shrink
    for (uint32_t i = 2; i < 32; i++) {
        TEST_TRUE(QuicRangeRemoveRange(&Range, i * 10, 1));
    }
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    // Should have shrunk (threshold: used < alloc / 4 and alloc >= 4 * initial)
    // With 2 used and alloc >= 32, should shrink
    TEST_TRUE(Range.AllocLength < AllocAfterGrow);
    
    // Verify remaining data
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 0);
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    TEST_EQUAL(Sub1->Low, 10);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Shrink back to PreAllocSubRanges
// Tests QuicRangeShrink when shrinking back to QUIC_RANGE_INITIAL_SUB_COUNT uses PreAlloc buffer
// Assertions: SubRanges points to PreAllocSubRanges, heap memory is freed
//
void QuicTestRangeShrinkToPreAlloc()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Grow to heap
    BOOLEAN RangeUpdated;
    for (uint32_t i = 0; i < 16; i++) {
        TEST_NOT_EQUAL(QuicRangeAddRange(&Range, i * 10, 1, &RangeUpdated), nullptr);
    }
    TEST_TRUE(Range.SubRanges != Range.PreAllocSubRanges);
    
    // Remove down to 2 subranges and trigger shrink to initial size
    for (uint32_t i = 2; i < 16; i++) {
        TEST_TRUE(QuicRangeRemoveRange(&Range, i * 10, 1));
    }
    
    // Manually shrink to initial size
    TEST_TRUE(QuicRangeShrink(&Range, QUIC_RANGE_INITIAL_SUB_COUNT));
    
    // Should now use PreAllocSubRanges
    TEST_EQUAL(Range.AllocLength, QUIC_RANGE_INITIAL_SUB_COUNT);
    TEST_TRUE(Range.SubRanges == Range.PreAllocSubRanges);
    
    // Verify data preserved
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Max capacity limit
// Tests that adding subranges respects MaxAllocSize limit
// Assertions: Growth stops at or near MaxAllocSize, with oldest values possibly aged out
//
void QuicTestRangeMaxCapacity()
{
    QUIC_RANGE Range;
    // Set a small MaxAllocSize to test capacity limit (e.g., 16 subranges max)
    QuicRangeInitialize(sizeof(QUIC_SUBRANGE) * 16, &Range);
    
    // Try to add 20 non-adjacent subranges
    BOOLEAN RangeUpdated;
    uint32_t SuccessCount = 0;
    for (uint32_t i = 0; i < 20; i++) {
        if (QuicRangeAddRange(&Range, i * 100, 1, &RangeUpdated) != nullptr) {
            SuccessCount++;
        }
    }
    
    // Should not exceed 16 subranges allocation
    TEST_TRUE(Range.AllocLength <= 16);
    
    // All additions may succeed due to aging out old values when capacity is reached
    // The actual size should be at most the allocation limit
    TEST_TRUE(QuicRangeSize(&Range) <= 16);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Add values in descending order
// Tests QuicRangeAddValue with values inserted in reverse order
// Assertions: Values are inserted at correct positions, range maintains sorted order
//
void QuicTestRangeAddDescending()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add values in descending order: 30, 20, 10
    TEST_TRUE(QuicRangeAddValue(&Range, 30));
    TEST_TRUE(QuicRangeAddValue(&Range, 20));
    TEST_TRUE(QuicRangeAddValue(&Range, 10));
    
    // Should maintain sorted order
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 10);
    
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    TEST_EQUAL(Sub1->Low, 20);
    
    QUIC_SUBRANGE* Sub2 = QuicRangeGet(&Range, 2);
    TEST_EQUAL(Sub2->Low, 30);
    
    TEST_EQUAL(QuicRangeGetMin(&Range), 10);
    TEST_EQUAL(QuicRangeGetMax(&Range), 30);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: GetRange with IsLastRange flag
// Tests QuicRangeGetRange returns correct IsLastRange flag for last subrange
// Assertions: IsLastRange is TRUE for last subrange, FALSE for others
//
void QuicTestRangeGetRangeLastFlag()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add two subranges
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 20, 5, &RangeUpdated), nullptr);
    
    uint64_t Count;
    BOOLEAN IsLast;
    
    // Query first subrange
    TEST_TRUE(QuicRangeGetRange(&Range, 10, &Count, &IsLast));
    TEST_EQUAL(Count, 5);
    TEST_FALSE(IsLast);  // Not the last subrange
    
    // Query second (last) subrange
    TEST_TRUE(QuicRangeGetRange(&Range, 20, &Count, &IsLast));
    TEST_EQUAL(Count, 5);
    TEST_TRUE(IsLast);  // Is the last subrange
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Large Count value
// Tests QuicRangeAddRange with a large Count value
// Assertions: Large range is added correctly, boundaries are correct
//
void QuicTestRangeLargeCount()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add a large range [1000, 1000000]
    uint64_t Low = 1000;
    uint64_t Count = 1000000;
    BOOLEAN RangeUpdated;
    QUIC_SUBRANGE* Sub = QuicRangeAddRange(&Range, Low, Count, &RangeUpdated);
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_TRUE(RangeUpdated);
    
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    TEST_EQUAL(Sub->Low, Low);
    TEST_EQUAL(Sub->Count, Count);
    TEST_EQUAL(QuicRangeGetMin(&Range), Low);
    TEST_EQUAL(QuicRangeGetMax(&Range), Low + Count - 1);
    
    // Verify GetRange at boundaries
    uint64_t RetCount;
    BOOLEAN IsLast;
    TEST_TRUE(QuicRangeGetRange(&Range, Low, &RetCount, &IsLast));
    TEST_EQUAL(RetCount, Count);
    TEST_TRUE(QuicRangeGetRange(&Range, Low + Count - 1, &RetCount, &IsLast));
    TEST_EQUAL(RetCount, 1);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Compact with no changes needed
// Tests QuicRangeCompact when range is already optimal (no adjacent/overlapping subranges)
// Assertions: Range is unchanged after compact
//
void QuicTestRangeCompactNoOp()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add well-separated subranges
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 2, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 20, 2, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 2, &RangeUpdated), nullptr);
    
    uint32_t SizeBefore = QuicRangeSize(&Range);
    
    // Compact should have no effect (already optimal)
    QuicRangeCompact(&Range);
    
    TEST_EQUAL(QuicRangeSize(&Range), SizeBefore);
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: QuicRangeGetSafe with valid and invalid indices
// Tests QuicRangeGetSafe returns correct pointer or NULL based on index validity
// Assertions: Valid indices return non-NULL, invalid indices return NULL
//
void QuicTestRangeGetSafe()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add one subrange
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 100, 10, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 1);
    
    // Valid index
    QUIC_SUBRANGE* Sub = QuicRangeGetSafe(&Range, 0);
    TEST_NOT_EQUAL(Sub, nullptr);
    TEST_EQUAL(Sub->Low, 100);
    
    // Invalid indices
    TEST_EQUAL(QuicRangeGetSafe(&Range, 1), nullptr);
    TEST_EQUAL(QuicRangeGetSafe(&Range, 100), nullptr);
    
    QuicRangeUninitialize(&Range);
}

//
// Scenario: Remove range spanning multiple subranges
// Tests QuicRangeRemoveRange when removal overlaps multiple non-contiguous subranges
// Assertions: All overlapping parts are removed, partial overlaps are truncated
//
void QuicTestRangeRemoveSpanningMultiple()
{
    QUIC_RANGE Range;
    QuicRangeInitialize(QUIC_RANGE_NO_MAX_ALLOC_SIZE, &Range);
    
    // Add three subranges: [10,14], [20,24], [30,34]
    BOOLEAN RangeUpdated;
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 10, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 20, 5, &RangeUpdated), nullptr);
    TEST_NOT_EQUAL(QuicRangeAddRange(&Range, 30, 5, &RangeUpdated), nullptr);
    TEST_EQUAL(QuicRangeSize(&Range), 3);
    
    // Remove [12, 32] - overlaps all three subranges
    TEST_TRUE(QuicRangeRemoveRange(&Range, 12, 21));
    
    // Should have 2 subranges left: [10,11] and [33,34]
    TEST_EQUAL(QuicRangeSize(&Range), 2);
    
    QUIC_SUBRANGE* Sub0 = QuicRangeGet(&Range, 0);
    TEST_EQUAL(Sub0->Low, 10);
    TEST_EQUAL(Sub0->Count, 2);  // [10, 11]
    
    QUIC_SUBRANGE* Sub1 = QuicRangeGet(&Range, 1);
    TEST_EQUAL(Sub1->Low, 33);
    TEST_EQUAL(Sub1->Count, 2);  // [33, 34]
    
    QuicRangeUninitialize(&Range);
}
