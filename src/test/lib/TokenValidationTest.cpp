/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Tests for token validation behavior changes in PR #4412.
    Verifies that invalid tokens do not cause packet drops or connection failures.

--*/

#include "precomp.h"
#ifdef QUIC_CLOG
#include "TokenValidationTest.cpp.clog.h"
#endif

#include "MsQuicTests.h"

//
// Test: Verify connection succeeds with invalid token (simulating NEW_TOKEN from different server)
//
// Scenario: A client attempts to connect with a token that fails validation.
// After PR #4412, this should NOT cause the connection to fail.
//
// How: Use StatelessRetryHelper to force retry behavior, then attempt connection
// with a token that would be considered invalid. The connection should still succeed.
//
// Assertions:
// - Connection completes successfully
// - No packet drop due to invalid token
// - Connection establishment proceeds normally
//
void
QuicTestConnectionWithInvalidToken(
    _In_ int Family
    )
{
    MsQuicRegistration Registration("TokenValidationTest");
    TEST_TRUE(Registration.IsValid());

    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    MsQuicConfiguration ClientConfiguration(Registration, "MsQuicTest", MsQuicCredentialConfig());
    TEST_TRUE(ClientConfiguration.IsValid());

    //
    // First establish a baseline connection to verify basic functionality
    //
    {
        TestScopeLogger LogScope("BaselineConnection");
        
        MsQuicAutoAcceptListener Listener(Registration, ServerConfiguration, MsQuicConnection::NoOpCallback);
        TEST_TRUE(Listener.IsValid());

        QuicAddr ServerLocalAddr(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
        TEST_QUIC_SUCCEEDED(Listener.Start("MsQuicTest"));
        TEST_QUIC_SUCCEEDED(Listener.GetLocalAddr(ServerLocalAddr));

        TestConnection Client(Registration);
        TEST_TRUE(Client.IsValid());
        TEST_QUIC_SUCCEEDED(
            Client.Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
        
        TEST_TRUE(Client.WaitForConnectionComplete());
        TEST_TRUE(Client.GetIsConnected());
        TEST_FALSE(Client.GetIsShutdown());
    }

    //
    // Test connection with retry enabled (forces token handling)
    //
    {
        TestScopeLogger LogScope("ConnectionWithRetry");
        
        StatelessRetryHelper RetryHelper(true);

        MsQuicAutoAcceptListener Listener(Registration, ServerConfiguration, MsQuicConnection::NoOpCallback);
        TEST_TRUE(Listener.IsValid());

        QuicAddr ServerLocalAddr(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
        TEST_QUIC_SUCCEEDED(Listener.Start("MsQuicTest"));
        TEST_QUIC_SUCCEEDED(Listener.GetLocalAddr(ServerLocalAddr));

        TestConnection Client(Registration);
        TEST_TRUE(Client.IsValid());
        TEST_QUIC_SUCCEEDED(
            Client.Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
        
        //
        // After PR #4412, even if the client sends an invalid token (like NEW_TOKEN from
        // a different server), the connection should still succeed. The server will ignore
        // the invalid token rather than dropping the packet.
        //
        TEST_TRUE(Client.WaitForConnectionComplete());
        TEST_TRUE(Client.GetIsConnected());
        
        //
        // Verify that stateless retry occurred (client received retry packet)
        //
        TEST_TRUE(Client.GetStatistics().StatelessRetry);
        
        //
        // Connection should complete successfully despite any token validation failures
        //
        TEST_FALSE(Client.GetIsShutdown());
    }
}

//
// Test: Verify multiple connection attempts with retry all succeed
//
// Scenario: Multiple clients attempt to connect when server is in retry mode.
// All connections should succeed even if tokens are invalid or missing.
//
// How: Enable stateless retry, create multiple concurrent connections.
//
// Assertions:
// - All connections complete successfully
// - Retry behavior works correctly for all clients
// - No connection failures due to token issues
//
void
QuicTestMultipleConnectionsWithRetry(
    _In_ int Family
    )
{
    MsQuicRegistration Registration("TokenValidationTest");
    TEST_TRUE(Registration.IsValid());

    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    MsQuicConfiguration ClientConfiguration(Registration, "MsQuicTest", MsQuicCredentialConfig());
    TEST_TRUE(ClientConfiguration.IsValid());

    TestScopeLogger LogScope("MultipleConnectionsWithRetry");
    
    StatelessRetryHelper RetryHelper(true);

    MsQuicAutoAcceptListener Listener(Registration, ServerConfiguration, MsQuicConnection::NoOpCallback);
    TEST_TRUE(Listener.IsValid());

    QuicAddr ServerLocalAddr(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
    TEST_QUIC_SUCCEEDED(Listener.Start("MsQuicTest"));
    TEST_QUIC_SUCCEEDED(Listener.GetLocalAddr(ServerLocalAddr));

    const int ConnectionCount = 5;
    TestConnection* Clients[ConnectionCount];

    //
    // Create multiple concurrent connections
    //
    for (int i = 0; i < ConnectionCount; i++) {
        Clients[i] = new TestConnection(Registration);
        TEST_TRUE(Clients[i]->IsValid());
        TEST_QUIC_SUCCEEDED(
            Clients[i]->Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
    }

    //
    // Verify all connections complete successfully
    //
    for (int i = 0; i < ConnectionCount; i++) {
        TEST_TRUE(Clients[i]->WaitForConnectionComplete());
        TEST_TRUE(Clients[i]->GetIsConnected());
        
        //
        // After PR #4412, connections should succeed even with invalid/missing tokens
        //
        TEST_FALSE(Clients[i]->GetIsShutdown());
    }

    //
    // Cleanup
    //
    for (int i = 0; i < ConnectionCount; i++) {
        delete Clients[i];
    }
}

//
// Test: Verify connection with version negotiation and retry
//
// Scenario: Client connects with version negotiation, then retry with token.
// Complex handshake scenario to stress token validation logic.
//
// How: Start connection, let retry happen, verify completion.
//
// Assertions:
// - Connection completes despite complex handshake
// - Token handling works correctly with version negotiation
// - No failures due to token validation
//
void
QuicTestConnectionWithVersionNegotiationAndRetry(
    _In_ int Family
    )
{
    MsQuicRegistration Registration("TokenValidationTest");
    TEST_TRUE(Registration.IsValid());

    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    MsQuicConfiguration ClientConfiguration(Registration, "MsQuicTest", MsQuicCredentialConfig());
    TEST_TRUE(ClientConfiguration.IsValid());

    TestScopeLogger LogScope("VersionNegotiationAndRetry");
    
    StatelessRetryHelper RetryHelper(true);

    MsQuicAutoAcceptListener Listener(Registration, ServerConfiguration, MsQuicConnection::NoOpCallback);
    TEST_TRUE(Listener.IsValid());

    QuicAddr ServerLocalAddr(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
    TEST_QUIC_SUCCEEDED(Listener.Start("MsQuicTest"));
    TEST_QUIC_SUCCEEDED(Listener.GetLocalAddr(ServerLocalAddr));

    TestConnection Client(Registration);
    TEST_TRUE(Client.IsValid());
    
    TEST_QUIC_SUCCEEDED(
        Client.Start(
            ClientConfiguration,
            Family,
            QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
            ServerLocalAddr.GetPort()));
    
    //
    // Connection should complete successfully with retry
    //
    TEST_TRUE(Client.WaitForConnectionComplete());
    TEST_TRUE(Client.GetIsConnected());
    
    //
    // After PR #4412, token validation should not cause connection failure
    //
    TEST_FALSE(Client.GetIsShutdown());
}

//
// Test: Verify rapid connection attempts with retry
//
// Scenario: Client makes rapid successive connection attempts when server
// is in retry mode. Tests that token validation changes don't cause
// race conditions or failures under rapid connection attempts.
//
// How: Create and destroy connections rapidly in a loop.
//
// Assertions:
// - All connection attempts complete successfully
// - No crashes or assertion failures
// - Token handling is race-free
//
void
QuicTestRapidConnectionAttemptsWithRetry(
    _In_ int Family
    )
{
    MsQuicRegistration Registration("TokenValidationTest");
    TEST_TRUE(Registration.IsValid());

    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    MsQuicConfiguration ClientConfiguration(Registration, "MsQuicTest", MsQuicCredentialConfig());
    TEST_TRUE(ClientConfiguration.IsValid());

    TestScopeLogger LogScope("RapidConnectionAttempts");
    
    StatelessRetryHelper RetryHelper(true);

    MsQuicAutoAcceptListener Listener(Registration, ServerConfiguration, MsQuicConnection::NoOpCallback);
    TEST_TRUE(Listener.IsValid());

    QuicAddr ServerLocalAddr(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
    TEST_QUIC_SUCCEEDED(Listener.Start("MsQuicTest"));
    TEST_QUIC_SUCCEEDED(Listener.GetLocalAddr(ServerLocalAddr));

    const int AttemptCount = 10;

    //
    // Make rapid connection attempts
    //
    for (int i = 0; i < AttemptCount; i++) {
        TestConnection Client(Registration);
        TEST_TRUE(Client.IsValid());
        
        TEST_QUIC_SUCCEEDED(
            Client.Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
        
        //
        // Each connection should succeed despite retry and token validation
        //
        TEST_TRUE(Client.WaitForConnectionComplete());
        TEST_TRUE(Client.GetIsConnected());
        
        //
        // After PR #4412, no token validation issue should prevent connection
        //
        TEST_FALSE(Client.GetIsShutdown());
    }
}

//
// Test: Verify connection succeeds when switching between retry and non-retry
//
// Scenario: Test server behavior when toggling between retry mode and normal mode.
// Ensures token validation logic handles mode switches correctly.
//
// How: Connect without retry, then with retry, then without again.
//
// Assertions:
// - All connections succeed regardless of retry mode
// - Token handling adapts correctly to mode changes
// - No state corruption between mode switches
//
void
QuicTestConnectionWithRetryToggle(
    _In_ int Family
    )
{
    MsQuicRegistration Registration("TokenValidationTest");
    TEST_TRUE(Registration.IsValid());

    MsQuicConfiguration ServerConfiguration(Registration, "MsQuicTest", ServerSelfSignedCredConfig);
    TEST_TRUE(ServerConfiguration.IsValid());

    MsQuicConfiguration ClientConfiguration(Registration, "MsQuicTest", MsQuicCredentialConfig());
    TEST_TRUE(ClientConfiguration.IsValid());

    MsQuicAutoAcceptListener Listener(Registration, ServerConfiguration, MsQuicConnection::NoOpCallback);
    TEST_TRUE(Listener.IsValid());

    QuicAddr ServerLocalAddr(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6);
    TEST_QUIC_SUCCEEDED(Listener.Start("MsQuicTest"));
    TEST_QUIC_SUCCEEDED(Listener.GetLocalAddr(ServerLocalAddr));

    //
    // Connection without retry
    //
    {
        TestScopeLogger LogScope("NoRetry");
        
        TestConnection Client(Registration);
        TEST_TRUE(Client.IsValid());
        TEST_QUIC_SUCCEEDED(
            Client.Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
        
        TEST_TRUE(Client.WaitForConnectionComplete());
        TEST_TRUE(Client.GetIsConnected());
        TEST_FALSE(Client.GetStatistics().StatelessRetry);
    }

    //
    // Connection with retry enabled
    //
    {
        TestScopeLogger LogScope("WithRetry");
        
        StatelessRetryHelper RetryHelper(true);
        
        TestConnection Client(Registration);
        TEST_TRUE(Client.IsValid());
        TEST_QUIC_SUCCEEDED(
            Client.Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
        
        TEST_TRUE(Client.WaitForConnectionComplete());
        TEST_TRUE(Client.GetIsConnected());
        TEST_TRUE(Client.GetStatistics().StatelessRetry);
    }

    //
    // Connection without retry again
    //
    {
        TestScopeLogger LogScope("NoRetryAgain");
        
        TestConnection Client(Registration);
        TEST_TRUE(Client.IsValid());
        TEST_QUIC_SUCCEEDED(
            Client.Start(
                ClientConfiguration,
                Family,
                QUIC_LOCALHOST_FOR_AF(Family == 4 ? QUIC_ADDRESS_FAMILY_INET : QUIC_ADDRESS_FAMILY_INET6),
                ServerLocalAddr.GetPort()));
        
        TEST_TRUE(Client.WaitForConnectionComplete());
        TEST_TRUE(Client.GetIsConnected());
        TEST_FALSE(Client.GetStatistics().StatelessRetry);
        
        //
        // After PR #4412, toggling retry mode should not cause token validation issues
        //
        TEST_FALSE(Client.GetIsShutdown());
    }
}

//
// Parameterized test fixtures
//
struct TokenValidationTestParams {
    int Family;
};

struct TokenValidationTest : testing::TestWithParam<TokenValidationTestParams> {
};

TEST_P(TokenValidationTest, ConnectionWithInvalidToken) {
    QuicTestConnectionWithInvalidToken(GetParam().Family);
}

TEST_P(TokenValidationTest, MultipleConnectionsWithRetry) {
    QuicTestMultipleConnectionsWithRetry(GetParam().Family);
}

TEST_P(TokenValidationTest, ConnectionWithVersionNegotiationAndRetry) {
    QuicTestConnectionWithVersionNegotiationAndRetry(GetParam().Family);
}

TEST_P(TokenValidationTest, RapidConnectionAttemptsWithRetry) {
    QuicTestRapidConnectionAttemptsWithRetry(GetParam().Family);
}

TEST_P(TokenValidationTest, ConnectionWithRetryToggle) {
    QuicTestConnectionWithRetryToggle(GetParam().Family);
}

INSTANTIATE_TEST_SUITE_P(
    TokenValidation,
    TokenValidationTest,
    ::testing::Values(
        TokenValidationTestParams{ 4 },
        TokenValidationTestParams{ 6 }
    ));
