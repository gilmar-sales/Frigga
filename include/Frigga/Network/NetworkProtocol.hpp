#pragma once

#include <Frigga/Macro.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE
{

    /// ENet channels. Channel 0 = reliable (spawn/despawn/RPC/session),
    /// channel 1 = unreliable-sequenced (snapshots/inputs).
    inline constexpr std::uint8_t kNetReliableChannel   = 0;
    inline constexpr std::uint8_t kNetStateChannel      = 1;
    inline constexpr std::uint8_t kNetChannelCount      = 2;

    inline constexpr std::uint16_t kDefaultServerPort   = 7777;
    inline constexpr std::uint32_t kMaxClients          = 32;
    inline constexpr float         kDefaultSnapshotHz  = 20.0f;
    inline constexpr std::size_t   kMaxSnapshotsBuffered = 64;
    /// Client renders this many seconds behind the newest snapshot.
    inline constexpr float kDefaultInterpolationDelay = 0.1f;

    enum class NetMessageType : std::uint8_t
    {
        Spawn    = 1,
        Despawn  = 2,
        Snapshot = 3,
        Input    = 4,
        Rpc      = 5,
    };

    /// Little-endian binary writer. Payloads stay small (transform snapshots),
    /// so a vector-backed writer with bounds-checked reads is enough.
    class NetWriter
    {
      public:
        void WriteU8(std::uint8_t value)
        {
            mData.push_back(value);
        }

        void WriteU16(std::uint16_t value)
        {
            mData.push_back(static_cast<std::uint8_t>(value & 0xff));
            mData.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        }

        void WriteU32(std::uint32_t value)
        {
            for(int i = 0; i < 4; ++i)
            {
                mData.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xff));
            }
        }

        void WriteF32(float value)
        {
            std::uint32_t bits = 0;
            static_assert(sizeof(float) == sizeof(bits));
            std::memcpy(&bits, &value, sizeof(float));
            WriteU32(bits);
        }

        void WriteVec3(const glm::vec3 &value)
        {
            WriteF32(value.x);
            WriteF32(value.y);
            WriteF32(value.z);
        }

        void WriteQuat(const glm::quat &value)
        {
            WriteF32(value.x);
            WriteF32(value.y);
            WriteF32(value.z);
            WriteF32(value.w);
        }

        void WriteBytes(const void *data, std::size_t size)
        {
            const auto *bytes = static_cast<const std::uint8_t *>(data);
            mData.insert(mData.end(), bytes, bytes + size);
        }

        void WriteString(std::string_view text)
        {
            WriteU16(static_cast<std::uint16_t>(text.size()));
            WriteBytes(text.data(), text.size());
        }

        [[nodiscard]] const std::vector<std::uint8_t> &Data() const
        {
            return mData;
        }

        [[nodiscard]] std::size_t Size() const
        {
            return mData.size();
        }

        void Clear()
        {
            mData.clear();
        }

      private:
        std::vector<std::uint8_t> mData;
    };

    class NetReader
    {
      public:
        NetReader(const std::uint8_t *data, std::size_t size): mData(data), mSize(size)
        {
        }

        explicit NetReader(const std::vector<std::uint8_t> &data)
            : mData(data.data()), mSize(data.size())
        {
        }

        [[nodiscard]] bool ReadU8(std::uint8_t &out)
        {
            if(mOffset + 1 > mSize)
            {
                return false;
            }
            out = mData[mOffset++];
            return true;
        }

        [[nodiscard]] bool ReadU16(std::uint16_t &out)
        {
            if(mOffset + 2 > mSize)
            {
                return false;
            }
            out = static_cast<std::uint16_t>(mData[mOffset]) |
                  (static_cast<std::uint16_t>(mData[mOffset + 1]) << 8);
            mOffset += 2;
            return true;
        }

        [[nodiscard]] bool ReadU32(std::uint32_t &out)
        {
            if(mOffset + 4 > mSize)
            {
                return false;
            }
            out = 0;
            for(int i = 0; i < 4; ++i)
            {
                out |= static_cast<std::uint32_t>(mData[mOffset + i]) << (8 * i);
            }
            mOffset += 4;
            return true;
        }

        [[nodiscard]] bool ReadF32(float &out)
        {
            std::uint32_t bits = 0;
            if(!ReadU32(bits))
            {
                return false;
            }
            std::memcpy(&out, &bits, sizeof(float));
            return true;
        }

        [[nodiscard]] bool ReadVec3(glm::vec3 &out)
        {
            return ReadF32(out.x) && ReadF32(out.y) && ReadF32(out.z);
        }

        [[nodiscard]] bool ReadQuat(glm::quat &out)
        {
            // Stored as x,y,z,w.
            return ReadF32(out.x) && ReadF32(out.y) && ReadF32(out.z) && ReadF32(out.w);
        }

        [[nodiscard]] bool ReadBytes(void *out, std::size_t size)
        {
            if(mOffset + size > mSize)
            {
                return false;
            }
            std::memcpy(out, mData + mOffset, size);
            mOffset += size;
            return true;
        }

        [[nodiscard]] bool ReadString(std::string &out)
        {
            std::uint16_t length = 0;
            if(!ReadU16(length))
            {
                return false;
            }
            if(mOffset + length > mSize)
            {
                return false;
            }
            out.assign(reinterpret_cast<const char *>(mData + mOffset), length);
            mOffset += length;
            return true;
        }

        [[nodiscard]] std::size_t Remaining() const
        {
            return mSize - mOffset;
        }

        [[nodiscard]] std::size_t Offset() const
        {
            return mOffset;
        }

      private:
        const std::uint8_t *mData = nullptr;
        std::size_t mSize         = 0;
        std::size_t mOffset       = 0;
    };

    struct NetTransformPose
    {
        glm::vec3 position {};
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale {1.0f, 1.0f, 1.0f};
    };

    struct NetSnapshotEntry
    {
        std::uint32_t netId = 0;
        NetTransformPose pose {};
    };

    struct NetSnapshot
    {
        std::uint32_t tick = 0;
        std::vector<NetSnapshotEntry> entries;
    };

    void EncodeSnapshot(const NetSnapshot &snapshot, NetWriter &out);
    [[nodiscard]] bool DecodeSnapshot(NetReader &reader, NetSnapshot &out);

    void EncodeSpawn(std::uint32_t netId, std::uint32_t ownerClientId,
                     const NetTransformPose &pose, std::string_view prefab, NetWriter &out);
    [[nodiscard]] bool DecodeSpawn(NetReader &reader, std::uint32_t &netId,
                                   std::uint32_t &ownerClientId, NetTransformPose &pose,
                                   std::string &prefab);

} // namespace FRIGGA_NAMESPACE
