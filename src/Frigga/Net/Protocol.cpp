#include "Frigga/Net/Protocol.hpp"

#include <cstring>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kFnvOffset = 2166136261u;
        constexpr std::uint32_t kFnvPrime  = 16777619u;
        constexpr std::uint32_t kHeaderSize = 6; // u32 size + u16 type
    } // namespace

    std::uint32_t NetHashName(std::string_view name)
    {
        std::uint32_t hash = kFnvOffset;
        for(const unsigned char ch : name)
        {
            hash ^= ch;
            hash *= kFnvPrime;
        }
        return hash;
    }

    void NetWriteU16(std::vector<std::uint8_t> &out, std::uint16_t value)
    {
        out.push_back(static_cast<std::uint8_t>(value));
        out.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    void NetWriteU32(std::vector<std::uint8_t> &out, std::uint32_t value)
    {
        out.push_back(static_cast<std::uint8_t>(value));
        out.push_back(static_cast<std::uint8_t>(value >> 8));
        out.push_back(static_cast<std::uint8_t>(value >> 16));
        out.push_back(static_cast<std::uint8_t>(value >> 24));
    }

    void NetWriteF32(std::vector<std::uint8_t> &out, float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        NetWriteU32(out, bits);
    }

    void NetWriteString(std::vector<std::uint8_t> &out, std::string_view value)
    {
        const auto size = static_cast<std::uint16_t>(
            value.size() > 0xffff ? 0xffff : value.size());
        NetWriteU16(out, size);
        out.insert(out.end(), value.begin(), value.begin() + size);
    }

    bool NetReadU16(std::span<const std::uint8_t> bytes, std::size_t &offset, std::uint16_t &value)
    {
        if(offset + 2 > bytes.size())
        {
            return false;
        }
        value = static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
        offset += 2;
        return true;
    }

    bool NetReadU32(std::span<const std::uint8_t> bytes, std::size_t &offset, std::uint32_t &value)
    {
        if(offset + 4 > bytes.size())
        {
            return false;
        }
        value = static_cast<std::uint32_t>(bytes[offset]) |
                (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
        offset += 4;
        return true;
    }

    bool NetReadF32(std::span<const std::uint8_t> bytes, std::size_t &offset, float &value)
    {
        std::uint32_t bits = 0;
        if(!NetReadU32(bytes, offset, bits))
        {
            return false;
        }
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool NetReadString(std::span<const std::uint8_t> bytes, std::size_t &offset, std::string &value)
    {
        std::uint16_t size = 0;
        if(!NetReadU16(bytes, offset, size))
        {
            return false;
        }
        if(offset + size > bytes.size())
        {
            return false;
        }
        value.assign(reinterpret_cast<const char *>(bytes.data() + offset), size);
        offset += size;
        return true;
    }

    std::vector<std::uint8_t> NetEncodeFrame(NetMessageType type,
                                             std::span<const std::uint8_t> payload)
    {
        std::vector<std::uint8_t> out;
        out.reserve(kHeaderSize + payload.size());
        const auto inner = static_cast<std::uint32_t>(2 + payload.size());
        NetWriteU32(out, inner);
        NetWriteU16(out, static_cast<std::uint16_t>(type));
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    NetExtractStatus NetTryExtractFrame(std::vector<std::uint8_t> &buffer, NetFrame &out)
    {
        if(buffer.size() < 4)
        {
            return NetExtractStatus::NeedMore;
        }
        std::size_t offset = 0;
        std::uint32_t inner = 0;
        if(!NetReadU32(buffer, offset, inner))
        {
            return NetExtractStatus::Invalid;
        }
        if(inner < 2 || inner > kNetMaxFramePayload + 2)
        {
            return NetExtractStatus::Invalid;
        }
        if(buffer.size() < 4 + inner)
        {
            return NetExtractStatus::NeedMore;
        }
        std::uint16_t type = 0;
        if(!NetReadU16(buffer, offset, type))
        {
            return NetExtractStatus::Invalid;
        }
        out.type = static_cast<NetMessageType>(type);
        out.payload.assign(buffer.begin() + static_cast<std::ptrdiff_t>(offset),
                           buffer.begin() + static_cast<std::ptrdiff_t>(4 + inner));
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(4 + inner));
        return NetExtractStatus::Ok;
    }

    std::vector<std::uint8_t> NetEncodeHello(std::string_view playerName)
    {
        std::vector<std::uint8_t> payload;
        NetWriteU16(payload, kNetProtocolVersion);
        NetWriteString(payload, playerName);
        return payload;
    }

    bool NetDecodeHello(std::span<const std::uint8_t> payload, std::uint16_t &protocolVersion,
                        std::string &playerName)
    {
        std::size_t offset = 0;
        return NetReadU16(payload, offset, protocolVersion) &&
               NetReadString(payload, offset, playerName);
    }

    std::vector<std::uint8_t> NetEncodeWelcome(std::uint32_t peerId, std::uint32_t serverTick)
    {
        std::vector<std::uint8_t> payload;
        NetWriteU32(payload, peerId);
        NetWriteU32(payload, serverTick);
        NetWriteU16(payload, kNetProtocolVersion);
        return payload;
    }

    bool NetDecodeWelcome(std::span<const std::uint8_t> payload, std::uint32_t &peerId,
                          std::uint32_t &serverTick, std::uint16_t &protocolVersion)
    {
        std::size_t offset = 0;
        return NetReadU32(payload, offset, peerId) && NetReadU32(payload, offset, serverTick) &&
               NetReadU16(payload, offset, protocolVersion);
    }

    std::vector<std::uint8_t> NetEncodeNamed(std::string_view name,
                                             std::span<const std::uint8_t> payload)
    {
        std::vector<std::uint8_t> out;
        NetWriteU32(out, NetHashName(name));
        NetWriteU16(out, static_cast<std::uint16_t>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    bool NetDecodeNamed(std::span<const std::uint8_t> bytes, std::uint32_t &nameHash,
                        std::span<const std::uint8_t> &payload)
    {
        std::size_t offset = 0;
        std::uint16_t size = 0;
        if(!NetReadU32(bytes, offset, nameHash) || !NetReadU16(bytes, offset, size))
        {
            return false;
        }
        if(offset + size > bytes.size())
        {
            return false;
        }
        payload = bytes.subspan(offset, size);
        return true;
    }

    std::vector<std::uint8_t> NetEncodeSnapshot(std::uint32_t tick,
                                                std::span<const NetSnapshotEntity> entities)
    {
        std::vector<std::uint8_t> payload;
        NetWriteU32(payload, tick);
        NetWriteU16(payload, static_cast<std::uint16_t>(entities.size()));
        for(const auto &entity : entities)
        {
            NetWriteU32(payload, entity.netId);
            NetWriteF32(payload, entity.px);
            NetWriteF32(payload, entity.py);
            NetWriteF32(payload, entity.pz);
            NetWriteF32(payload, entity.qx);
            NetWriteF32(payload, entity.qy);
            NetWriteF32(payload, entity.qz);
            NetWriteF32(payload, entity.qw);
        }
        return payload;
    }

    bool NetDecodeSnapshot(std::span<const std::uint8_t> payload, std::uint32_t &tick,
                           std::vector<NetSnapshotEntity> &entities)
    {
        std::size_t offset = 0;
        std::uint16_t count = 0;
        if(!NetReadU32(payload, offset, tick) || !NetReadU16(payload, offset, count))
        {
            return false;
        }
        entities.clear();
        entities.reserve(count);
        for(std::uint16_t i = 0; i < count; ++i)
        {
            NetSnapshotEntity entity {};
            if(!NetReadU32(payload, offset, entity.netId) ||
               !NetReadF32(payload, offset, entity.px) || !NetReadF32(payload, offset, entity.py) ||
               !NetReadF32(payload, offset, entity.pz) || !NetReadF32(payload, offset, entity.qx) ||
               !NetReadF32(payload, offset, entity.qy) || !NetReadF32(payload, offset, entity.qz) ||
               !NetReadF32(payload, offset, entity.qw))
            {
                return false;
            }
            entities.push_back(entity);
        }
        return true;
    }

} // namespace FRIGGA_NAMESPACE
