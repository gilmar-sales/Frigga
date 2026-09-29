#include <Frigga/Network/NetworkProtocol.hpp>

#include <gtest/gtest.h>

TEST(NetworkProtocol, WriterReaderRoundTrip)
{
    fg::NetWriter writer;
    writer.WriteU8(0xab);
    writer.WriteU16(0xcdef);
    writer.WriteU32(0x12345678);
    writer.WriteF32(1.5f);
    writer.WriteVec3(glm::vec3(1.0f, 2.0f, 3.0f));
    writer.WriteQuat(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    writer.WriteString("hello");

    fg::NetReader reader(writer.Data());
    std::uint8_t u8 = 0;
    std::uint16_t u16 = 0;
    std::uint32_t u32 = 0;
    float f = 0.0f;
    glm::vec3 v {};
    glm::quat q {1.0f, 0.0f, 0.0f, 0.0f};
    std::string s;
    ASSERT_TRUE(reader.ReadU8(u8));
    EXPECT_EQ(u8, 0xab);
    ASSERT_TRUE(reader.ReadU16(u16));
    EXPECT_EQ(u16, 0xcdef);
    ASSERT_TRUE(reader.ReadU32(u32));
    EXPECT_EQ(u32, 0x12345678u);
    ASSERT_TRUE(reader.ReadF32(f));
    EXPECT_FLOAT_EQ(f, 1.5f);
    ASSERT_TRUE(reader.ReadVec3(v));
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
    ASSERT_TRUE(reader.ReadQuat(q));
    EXPECT_FLOAT_EQ(q.w, 1.0f);
    ASSERT_TRUE(reader.ReadString(s));
    EXPECT_EQ(s, "hello");
    EXPECT_EQ(reader.Remaining(), 0u);
}

TEST(NetworkProtocol, ReaderRejectsTruncated)
{
    std::vector<std::uint8_t> bytes {0x01, 0x02};
    fg::NetReader reader(bytes);
    std::uint32_t u32 = 0;
    EXPECT_FALSE(reader.ReadU32(u32));
    std::string s;
    EXPECT_FALSE(reader.ReadString(s));
}

TEST(NetworkProtocol, SnapshotEncodeDecode)
{
    fg::NetSnapshot snapshot {.tick = 42};
    snapshot.entries.push_back(fg::NetSnapshotEntry {
        .netId = 7,
        .pose  = {.position = {1.0f, 2.0f, 3.0f},
                  .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                  .scale    = {1.0f, 1.0f, 1.0f}},
    });
    snapshot.entries.push_back(fg::NetSnapshotEntry {
        .netId = 9,
        .pose  = {.position = {-4.0f, 0.5f, 8.0f},
                  .rotation = glm::quat(0.7071f, 0.0f, 0.7071f, 0.0f),
                  .scale    = {2.0f, 2.0f, 2.0f}},
    });

    fg::NetWriter writer;
    fg::EncodeSnapshot(snapshot, writer);

    fg::NetReader reader(writer.Data());
    fg::NetSnapshot decoded {};
    ASSERT_TRUE(fg::DecodeSnapshot(reader, decoded));
    EXPECT_EQ(decoded.tick, 42u);
    ASSERT_EQ(decoded.entries.size(), 2u);
    EXPECT_EQ(decoded.entries[0].netId, 7u);
    EXPECT_FLOAT_EQ(decoded.entries[0].pose.position.x, 1.0f);
    EXPECT_EQ(decoded.entries[1].netId, 9u);
    EXPECT_FLOAT_EQ(decoded.entries[1].pose.position.x, -4.0f);
}

TEST(NetworkProtocol, SnapshotRejectsWrongType)
{
    fg::NetWriter writer;
    writer.WriteU8(static_cast<std::uint8_t>(fg::NetMessageType::Input));
    writer.WriteU32(1);
    fg::NetReader reader(writer.Data());
    fg::NetSnapshot snapshot {};
    EXPECT_FALSE(fg::DecodeSnapshot(reader, snapshot));
}

TEST(NetworkProtocol, SpawnEncodeDecode)
{
    fg::NetTransformPose pose {.position = {5.0f, 6.0f, 7.0f},
                               .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                               .scale    = {1.0f, 2.0f, 3.0f}};
    fg::NetWriter writer;
    fg::EncodeSpawn(11, 3, pose, "player.json", writer);

    fg::NetReader reader(writer.Data());
    std::uint32_t netId = 0;
    std::uint32_t owner = 0;
    fg::NetTransformPose decoded {};
    std::string prefab;
    ASSERT_TRUE(fg::DecodeSpawn(reader, netId, owner, decoded, prefab));
    EXPECT_EQ(netId, 11u);
    EXPECT_EQ(owner, 3u);
    EXPECT_FLOAT_EQ(decoded.position.y, 6.0f);
    EXPECT_EQ(prefab, "player.json");
}
