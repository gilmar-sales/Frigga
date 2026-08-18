#include "EmptyApp.hpp"

#include <Frigga/Net/Network.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace
{
    bool WaitUntil(const std::function<bool()> &pred, int timeoutMs = 2000)
    {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while(std::chrono::steady_clock::now() < deadline)
        {
            if(pred())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return pred();
    }
} // namespace

TEST(Network, HostConnectHandshakeAndCommand)
{
    auto app = skr::ApplicationBuilder()
                   .WithExtension<fr::FreyrExtension>([](fr::FreyrExtension &freyr) {
                       freyr.WithPipeline(
                           [](fr::PipelineBuilder &pipeline) { pipeline.WithName("Main"); });
                   })
                   .Build<EmptyApp>();
    auto registry = app->GetRootServiceProvider()->GetService<fr::Registry>();
    ASSERT_TRUE(registry);

    fg::Network host {{}};
    ASSERT_TRUE(host.Host(0, true));
    ASSERT_NE(host.Port(), 0);

    fg::Network client {{}};
    ASSERT_TRUE(client.Connect("127.0.0.1", host.Port(), "Tester"));
    ASSERT_TRUE(WaitUntil([&] { return client.LocalPeerId() != 0; }));
    EXPECT_EQ(client.Role(), fg::NetRole::Client);
    EXPECT_TRUE(host.IsAuthority());
    EXPECT_TRUE(host.IsDedicated());
    ASSERT_TRUE(WaitUntil([&] { return host.PeerCount() >= 1; }));

    std::uint32_t gotPeer = 0;
    std::vector<std::uint8_t> gotPayload;
    host.OnCommand("Fire", [&](std::uint32_t peer, std::span<const std::uint8_t> payload) {
        gotPeer = peer;
        gotPayload.assign(payload.begin(), payload.end());
    });

    const std::uint8_t shot[] = {1, 2, 3};
    client.SendCommand("Fire", std::span {shot});
    ASSERT_TRUE(WaitUntil([&] {
        host.Receive(*registry);
        return gotPeer != 0;
    }));
    EXPECT_EQ(gotPeer, client.LocalPeerId());
    ASSERT_EQ(gotPayload.size(), 3u);
    EXPECT_EQ(gotPayload[0], 1);
    EXPECT_EQ(gotPayload[2], 3);
}

TEST(Network, HostDisconnectReturnsOffline)
{
    fg::Network host {{}};
    ASSERT_TRUE(host.Host(0, true));
    host.Disconnect();
    EXPECT_EQ(host.Role(), fg::NetRole::Offline);
    EXPECT_FALSE(host.IsSessionActive());
}
