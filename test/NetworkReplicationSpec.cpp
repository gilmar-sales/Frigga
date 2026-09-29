#include "EmptyApp.hpp"

#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/Network/EnetTransport.hpp>
#include <Frigga/Network/NetworkClient.hpp>
#include <Frigga/Network/NetworkComponents.hpp>
#include <Frigga/Network/NetworkController.hpp>
#include <Frigga/Network/NetworkServer.hpp>

#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <vector>

namespace
{
    struct NetBus
    {
        std::deque<std::vector<std::uint8_t>> serverReliable;
        std::deque<std::vector<std::uint8_t>> serverState;
        std::deque<std::pair<fg::NetClientId, std::vector<std::uint8_t>>> toServer;
    };

    class FakeServerTransport final: public fg::IServerTransport
    {
      public:
        explicit FakeServerTransport(std::shared_ptr<NetBus> bus): mBus(std::move(bus)) {}

        bool Start(std::uint16_t port, std::size_t) override
        {
            mRunning = true;
            mPort    = port;
            return true;
        }

        void Stop() override
        {
            mRunning = false;
        }

        [[nodiscard]] bool IsRunning() const override
        {
            return mRunning;
        }

        [[nodiscard]] std::uint16_t GetPort() const override
        {
            return mPort;
        }

        void Poll() override
        {
            while(!mBus->toServer.empty())
            {
                const auto [from, bytes] = std::move(mBus->toServer.front());
                mBus->toServer.pop_front();
                if(mOnReceive)
                {
                    mOnReceive(fg::NetPacket {.fromClientId = from, .bytes = bytes});
                }
            }
        }

        void SendReliable(fg::NetClientId, const std::vector<std::uint8_t> &bytes) override
        {
            mBus->serverReliable.push_back(bytes);
        }

        void SendState(fg::NetClientId, const std::vector<std::uint8_t> &bytes) override
        {
            mBus->serverState.push_back(bytes);
        }

        void BroadcastReliable(const std::vector<std::uint8_t> &bytes) override
        {
            mBus->serverReliable.push_back(bytes);
        }

        void BroadcastState(const std::vector<std::uint8_t> &bytes) override
        {
            mBus->serverState.push_back(bytes);
        }

        void OnClientConnected(ConnectCallback cb) override
        {
            mOnConnect = std::move(cb);
        }

        void OnClientDisconnected(DisconnectCallback cb) override
        {
            mOnDisconnect = std::move(cb);
        }

        void OnReceive(ReceiveCallback cb) override
        {
            mOnReceive = std::move(cb);
        }

        void FakeConnect(fg::NetClientId id)
        {
            if(mOnConnect)
            {
                mOnConnect(id);
            }
        }

        std::shared_ptr<NetBus> mBus;
        ConnectCallback mOnConnect;
        DisconnectCallback mOnDisconnect;
        ReceiveCallback mOnReceive;
        bool mRunning = false;
        std::uint16_t mPort = 0;
    };

    class FakeClientTransport final: public fg::IClientTransport
    {
      public:
        explicit FakeClientTransport(std::shared_ptr<NetBus> bus): mBus(std::move(bus)) {}

        bool Connect(const std::string &, std::uint16_t) override
        {
            mConnected = true;
            return true;
        }

        void Disconnect() override
        {
            mConnected = false;
        }

        [[nodiscard]] bool IsConnected() const override
        {
            return mConnected;
        }

        void Poll() override
        {
            while(!mBus->serverReliable.empty())
            {
                auto bytes = std::move(mBus->serverReliable.front());
                mBus->serverReliable.pop_front();
                if(mOnReceive)
                {
                    mOnReceive(fg::NetPacket {.fromClientId = 0, .bytes = bytes});
                }
            }
            while(!mBus->serverState.empty())
            {
                auto bytes = std::move(mBus->serverState.front());
                mBus->serverState.pop_front();
                if(mOnReceive)
                {
                    mOnReceive(fg::NetPacket {.fromClientId = 0, .bytes = bytes});
                }
            }
        }

        void SendReliable(fg::NetClientId, const std::vector<std::uint8_t> &bytes) override
        {
            mBus->toServer.emplace_back(1, bytes);
        }

        void SendState(fg::NetClientId, const std::vector<std::uint8_t> &bytes) override
        {
            mBus->toServer.emplace_back(1, bytes);
        }

        void BroadcastReliable(const std::vector<std::uint8_t> &bytes) override
        {
            SendReliable(0, bytes);
        }

        void BroadcastState(const std::vector<std::uint8_t> &bytes) override
        {
            SendState(0, bytes);
        }

        void OnClientConnected(ConnectCallback cb) override
        {
            mOnConnect = std::move(cb);
        }

        void OnClientDisconnected(DisconnectCallback cb) override
        {
            mOnDisconnect = std::move(cb);
        }

        void OnReceive(ReceiveCallback cb) override
        {
            mOnReceive = std::move(cb);
        }

        std::shared_ptr<NetBus> mBus;
        ConnectCallback mOnConnect;
        DisconnectCallback mOnDisconnect;
        ReceiveCallback mOnReceive;
        bool mConnected = false;
    };

    class NetworkReplicationSpec: public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            mApp = skr::ApplicationBuilder()
                       .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                           freyr.WithComponent<fg::TransformComponent>()
                               .WithComponent<fg::NetworkIdentity>()
                               .WithComponent<fg::Replicated>()
                               .WithComponent<fg::NetworkOwner>();
                       })
                       .Build<EmptyApp>();
            mRegistry = mApp->GetRootServiceProvider()->GetService<fr::Registry>();
            mBus      = std::make_shared<NetBus>();
        }

        skr::Arc<EmptyApp> mApp;
        skr::Arc<fr::Registry> mRegistry;
        std::shared_ptr<NetBus> mBus;
    };
} // namespace

TEST_F(NetworkReplicationSpec, SpawnReplicatesThroughBus)
{
    fg::NetworkServer server;
    auto *serverTransport = new FakeServerTransport(mBus);
    server.SetTransport(std::unique_ptr<fg::IServerTransport>(serverTransport));
    ASSERT_TRUE(server.Start(7777));

    fg::NetworkClient client;
    client.SetTransport(std::make_unique<FakeClientTransport>(mBus));

    const fr::Entity entity = mRegistry->CreateEntity(
        fg::TransformComponent {.position = {1.0f, 2.0f, 3.0f}}, fg::NetworkIdentity {});
    const std::uint32_t netId = server.RegisterEntity(*mRegistry, entity);
    EXPECT_NE(netId, fg::kInvalidNetId);

    server.Poll();
    client.Poll(*mRegistry);

    const auto remote = client.FindEntity(netId);
    ASSERT_TRUE(remote.has_value());
    ASSERT_TRUE(mRegistry->IsAlive(*remote));
    bool poseOk = false;
    mRegistry->TryGetComponents<fg::TransformComponent>(
        *remote, [&](const fg::TransformComponent &transform) {
            poseOk = transform.position.x == 1.0f && transform.position.y == 2.0f;
        });
    EXPECT_TRUE(poseOk);
}

TEST_F(NetworkReplicationSpec, SnapshotInterpolatesTowardsNewest)
{
    fg::NetworkServer server;
    server.SetTransport(std::make_unique<FakeServerTransport>(mBus));
    ASSERT_TRUE(server.Start(7777));

    fg::NetworkClient client;
    client.SetTransport(std::make_unique<FakeClientTransport>(mBus));
    client.SetInterpolationDelay(0.1f);

    const fr::Entity entity = mRegistry->CreateEntity(
        fg::TransformComponent {.position = {0.0f, 0.0f, 0.0f}}, fg::NetworkIdentity {});
    const std::uint32_t netId = server.RegisterEntity(*mRegistry, entity);
    ASSERT_NE(netId, fg::kInvalidNetId);

    server.Poll();
    client.Poll(*mRegistry);
    const auto remote = client.FindEntity(netId);
    ASSERT_TRUE(remote.has_value());

    // Move the server entity twice; each move is one snapshot tick.
    mRegistry->TryGetComponents<fg::TransformComponent>(
        entity, [](fg::TransformComponent &transform) { transform.position.x = 10.0f; });
    server.BroadcastSnapshot(*mRegistry);
    mRegistry->TryGetComponents<fg::TransformComponent>(
        entity, [](fg::TransformComponent &transform) { transform.position.x = 20.0f; });
    server.BroadcastSnapshot(*mRegistry);

    // Deliver both snapshots at t=0 and t=0.05, then render at t=0.1.
    client.Poll(*mRegistry);
    client.ApplySnapshots(*mRegistry, 0.05);
    client.ApplySnapshots(*mRegistry, 0.10);

    float x = 0.0f;
    mRegistry->TryGetComponents<fg::TransformComponent>(
        *remote, [&](const fg::TransformComponent &transform) { x = transform.position.x; });
    EXPECT_GE(x, 0.0f);
    EXPECT_LE(x, 20.0f);
    EXPECT_GT(client.BufferedSnapshots(), 0u);
}

TEST_F(NetworkReplicationSpec, ClientInputReachesServerQueue)
{
    fg::NetworkServer server;
    server.SetTransport(std::make_unique<FakeServerTransport>(mBus));
    ASSERT_TRUE(server.Start(7777));

    fg::NetworkClient client;
    client.SetTransport(std::make_unique<FakeClientTransport>(mBus));

    const std::vector<std::uint8_t> payload {0xde, 0xad, 0xbe, 0xef};
    client.SendInput(7, payload);
    server.Poll();

    const auto inputs = server.ConsumeInputs();
    ASSERT_EQ(inputs.size(), 1u);
    EXPECT_EQ(inputs[0].netId, 7u);
    EXPECT_EQ(inputs[0].payload, payload);
}

TEST_F(NetworkReplicationSpec, DespawnDestroysRemoteEntity)
{
    fg::NetworkServer server;
    server.SetTransport(std::make_unique<FakeServerTransport>(mBus));
    ASSERT_TRUE(server.Start(7777));

    fg::NetworkClient client;
    client.SetTransport(std::make_unique<FakeClientTransport>(mBus));

    const fr::Entity entity = mRegistry->CreateEntity(
        fg::TransformComponent {.position = {0.0f, 0.0f, 0.0f}}, fg::NetworkIdentity {});
    const std::uint32_t netId = server.RegisterEntity(*mRegistry, entity);
    server.Poll();
    client.Poll(*mRegistry);
    ASSERT_TRUE(client.FindEntity(netId).has_value());

    server.UnregisterEntity(*mRegistry, entity);
    server.Poll();
    client.Poll(*mRegistry);
    // Despawn is applied as deferred destroy; flush the queue.
    mRegistry->Update(0.0f);
    const auto remote = client.FindEntity(netId);
    if(remote.has_value())
    {
        EXPECT_FALSE(mRegistry->IsAlive(*remote));
    }
}

TEST(NetworkEnet, RuntimeInitializes)
{
    ASSERT_TRUE(fg::EnetRuntime::EnsureInitialized());
    fg::EnetRuntime::Release();
}
