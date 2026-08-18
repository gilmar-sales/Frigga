#include <Frigga/Net/Protocol.hpp>

#include <gtest/gtest.h>

TEST(Protocol, HashIsStable)
{
    EXPECT_EQ(fg::NetHashName("Fire"), fg::NetHashName("Fire"));
    EXPECT_NE(fg::NetHashName("Fire"), fg::NetHashName("Jump"));
}

TEST(Protocol, FrameRoundTrip)
{
    const std::uint8_t payloadBytes[] = {1, 2, 3, 4};
    const auto encoded =
        fg::NetEncodeFrame(fg::NetMessageType::Command, std::span {payloadBytes});
    std::vector<std::uint8_t> buffer = encoded;
    fg::NetFrame frame;
    ASSERT_EQ(fg::NetTryExtractFrame(buffer, frame), fg::NetExtractStatus::Ok);
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(frame.type, fg::NetMessageType::Command);
    ASSERT_EQ(frame.payload.size(), 4u);
    EXPECT_EQ(frame.payload[0], 1);
}

TEST(Protocol, ExtractNeedMoreThenOk)
{
    const auto encoded = fg::NetEncodeFrame(fg::NetMessageType::Ping, {});
    std::vector<std::uint8_t> buffer(encoded.begin(), encoded.begin() + 2);
    fg::NetFrame frame;
    EXPECT_EQ(fg::NetTryExtractFrame(buffer, frame), fg::NetExtractStatus::NeedMore);
    buffer.insert(buffer.end(), encoded.begin() + 2, encoded.end());
    EXPECT_EQ(fg::NetTryExtractFrame(buffer, frame), fg::NetExtractStatus::Ok);
    EXPECT_EQ(frame.type, fg::NetMessageType::Ping);
}

TEST(Protocol, RejectOversizedFrame)
{
    std::vector<std::uint8_t> buffer;
    fg::NetWriteU32(buffer, fg::kNetMaxFramePayload + 3);
    fg::NetWriteU16(buffer, 1);
    fg::NetFrame frame;
    EXPECT_EQ(fg::NetTryExtractFrame(buffer, frame), fg::NetExtractStatus::Invalid);
}

TEST(Protocol, HelloWelcomeNamedSnapshot)
{
    const auto hello = fg::NetEncodeHello("Gilmar");
    std::uint16_t version = 0;
    std::string name;
    ASSERT_TRUE(fg::NetDecodeHello(hello, version, name));
    EXPECT_EQ(version, fg::kNetProtocolVersion);
    EXPECT_EQ(name, "Gilmar");

    const auto welcome = fg::NetEncodeWelcome(7, 42);
    std::uint32_t peer = 0;
    std::uint32_t tick = 0;
    ASSERT_TRUE(fg::NetDecodeWelcome(welcome, peer, tick, version));
    EXPECT_EQ(peer, 7u);
    EXPECT_EQ(tick, 42u);

    const std::uint8_t body[] = {9, 8};
    const auto named = fg::NetEncodeNamed("Fire", std::span {body});
    std::uint32_t hash = 0;
    std::span<const std::uint8_t> inner;
    ASSERT_TRUE(fg::NetDecodeNamed(named, hash, inner));
    EXPECT_EQ(hash, fg::NetHashName("Fire"));
    ASSERT_EQ(inner.size(), 2u);
    EXPECT_EQ(inner[0], 9);

    fg::NetSnapshotEntity entity {.netId = 3, .px = 1.5f, .py = 2.0f, .pz = -4.0f, .qw = 1.0f};
    const auto snap = fg::NetEncodeSnapshot(11, std::span {&entity, 1});
    std::vector<fg::NetSnapshotEntity> decoded;
    ASSERT_TRUE(fg::NetDecodeSnapshot(snap, tick, decoded));
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].netId, 3u);
    EXPECT_NEAR(decoded[0].px, 1.5f, 1e-5f);
    EXPECT_NEAR(decoded[0].qw, 1.0f, 1e-5f);
    EXPECT_EQ(tick, 11u);
}
