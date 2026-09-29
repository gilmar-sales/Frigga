#include <Frigga/Network/NetworkProtocol.hpp>

namespace FRIGGA_NAMESPACE
{

    void EncodeSnapshot(const NetSnapshot &snapshot, NetWriter &out)
    {
        out.WriteU8(static_cast<std::uint8_t>(NetMessageType::Snapshot));
        out.WriteU32(snapshot.tick);
        out.WriteU16(static_cast<std::uint16_t>(snapshot.entries.size()));
        for(const auto &entry : snapshot.entries)
        {
            out.WriteU32(entry.netId);
            out.WriteVec3(entry.pose.position);
            out.WriteQuat(entry.pose.rotation);
            out.WriteVec3(entry.pose.scale);
        }
    }

    bool DecodeSnapshot(NetReader &reader, NetSnapshot &out)
    {
        std::uint8_t type = 0;
        if(!reader.ReadU8(type) || type != static_cast<std::uint8_t>(NetMessageType::Snapshot))
        {
            return false;
        }
        std::uint16_t count = 0;
        if(!reader.ReadU32(out.tick) || !reader.ReadU16(count))
        {
            return false;
        }
        out.entries.clear();
        out.entries.reserve(count);
        for(std::uint16_t i = 0; i < count; ++i)
        {
            NetSnapshotEntry entry {};
            if(!reader.ReadU32(entry.netId) || !reader.ReadVec3(entry.pose.position) ||
               !reader.ReadQuat(entry.pose.rotation) || !reader.ReadVec3(entry.pose.scale))
            {
                return false;
            }
            // Guard against NaN smuggling through the wire.
            const auto finite = [](float v) { return v == v && v != 1.0f / 0.0f && v != -1.0f / 0.0f; };
            if(!finite(entry.pose.position.x) || !finite(entry.pose.position.y) ||
               !finite(entry.pose.position.z))
            {
                return false;
            }
            out.entries.push_back(entry);
        }
        return true;
    }

    void EncodeSpawn(std::uint32_t netId, std::uint32_t ownerClientId,
                     const NetTransformPose &pose, std::string_view prefab, NetWriter &out)
    {
        out.WriteU8(static_cast<std::uint8_t>(NetMessageType::Spawn));
        out.WriteU32(netId);
        out.WriteU32(ownerClientId);
        out.WriteVec3(pose.position);
        out.WriteQuat(pose.rotation);
        out.WriteVec3(pose.scale);
        out.WriteString(prefab);
    }

    bool DecodeSpawn(NetReader &reader, std::uint32_t &netId, std::uint32_t &ownerClientId,
                     NetTransformPose &pose, std::string &prefab)
    {
        std::uint8_t type = 0;
        if(!reader.ReadU8(type) || type != static_cast<std::uint8_t>(NetMessageType::Spawn))
        {
            return false;
        }
        return reader.ReadU32(netId) && reader.ReadU32(ownerClientId) &&
               reader.ReadVec3(pose.position) && reader.ReadQuat(pose.rotation) &&
               reader.ReadVec3(pose.scale) && reader.ReadString(prefab);
    }

} // namespace FRIGGA_NAMESPACE
