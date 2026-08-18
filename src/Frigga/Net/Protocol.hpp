#pragma once

#include "Frigga/Macro.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    inline constexpr std::uint16_t kNetProtocolVersion = 1;
    inline constexpr std::uint32_t kNetMaxFramePayload = 64u * 1024u;
    inline constexpr std::uint32_t kNetHostPeerId      = 1;

    enum class NetMessageType : std::uint16_t
    {
        Hello    = 1,
        Welcome  = 2,
        Ping     = 3,
        Pong     = 4,
        Command  = 5,
        Event    = 6,
        Snapshot = 7,
    };

    enum class NetRole : std::uint8_t
    {
        Offline = 0,
        Host,
        Client,
        Dedicated,
    };

    struct NetFrame
    {
        NetMessageType            type = NetMessageType::Hello;
        std::vector<std::uint8_t> payload;
    };

    enum class NetExtractStatus : std::uint8_t
    {
        NeedMore = 0,
        Ok,
        Invalid,
    };

    [[nodiscard]] std::uint32_t NetHashName(std::string_view name);

    void NetWriteU16(std::vector<std::uint8_t> &out, std::uint16_t value);
    void NetWriteU32(std::vector<std::uint8_t> &out, std::uint32_t value);
    void NetWriteF32(std::vector<std::uint8_t> &out, float value);
    void NetWriteString(std::vector<std::uint8_t> &out, std::string_view value);

    [[nodiscard]] bool NetReadU16(std::span<const std::uint8_t> bytes, std::size_t &offset,
                                  std::uint16_t &value);
    [[nodiscard]] bool NetReadU32(std::span<const std::uint8_t> bytes, std::size_t &offset,
                                  std::uint32_t &value);
    [[nodiscard]] bool NetReadF32(std::span<const std::uint8_t> bytes, std::size_t &offset,
                                  float &value);
    [[nodiscard]] bool NetReadString(std::span<const std::uint8_t> bytes, std::size_t &offset,
                                     std::string &value);

    [[nodiscard]] std::vector<std::uint8_t> NetEncodeFrame(NetMessageType type,
                                                           std::span<const std::uint8_t> payload);

    /// Consume one complete frame from the front of @p buffer. On Ok, the frame
    /// bytes are erased. On NeedMore, buffer is unchanged. On Invalid, caller
    /// should drop the connection.
    [[nodiscard]] NetExtractStatus NetTryExtractFrame(std::vector<std::uint8_t> &buffer,
                                                      NetFrame &out);

    [[nodiscard]] std::vector<std::uint8_t> NetEncodeHello(std::string_view playerName);
    [[nodiscard]] bool NetDecodeHello(std::span<const std::uint8_t> payload,
                                      std::uint16_t &protocolVersion, std::string &playerName);

    [[nodiscard]] std::vector<std::uint8_t> NetEncodeWelcome(std::uint32_t peerId,
                                                             std::uint32_t serverTick);
    [[nodiscard]] bool NetDecodeWelcome(std::span<const std::uint8_t> payload, std::uint32_t &peerId,
                                        std::uint32_t &serverTick, std::uint16_t &protocolVersion);

    [[nodiscard]] std::vector<std::uint8_t> NetEncodeNamed(std::string_view name,
                                                           std::span<const std::uint8_t> payload);
    [[nodiscard]] bool NetDecodeNamed(std::span<const std::uint8_t> bytes, std::uint32_t &nameHash,
                                      std::span<const std::uint8_t> &payload);

    struct NetSnapshotEntity
    {
        std::uint32_t netId = 0;
        float         px = 0.0f;
        float         py = 0.0f;
        float         pz = 0.0f;
        float         qx = 0.0f;
        float         qy = 0.0f;
        float         qz = 0.0f;
        float         qw = 1.0f;
    };

    [[nodiscard]] std::vector<std::uint8_t> NetEncodeSnapshot(
        std::uint32_t tick, std::span<const NetSnapshotEntity> entities);
    [[nodiscard]] bool NetDecodeSnapshot(std::span<const std::uint8_t> payload, std::uint32_t &tick,
                                         std::vector<NetSnapshotEntity> &entities);

} // namespace FRIGGA_NAMESPACE
