#include <Frigga/Network/NetworkClient.hpp>

#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/Network/EnetTransport.hpp>
#include <Frigga/Network/NetworkComponents.hpp>

#include <glm/gtc/quaternion.hpp>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        NetTransformPose LerpPose(const NetTransformPose &a, const NetTransformPose &b, float t)
        {
            NetTransformPose out;
            out.position = a.position + (b.position - a.position) * t;
            out.rotation = glm::slerp(a.rotation, b.rotation, t);
            out.scale    = a.scale + (b.scale - a.scale) * t;
            return out;
        }
    } // namespace

    NetworkClient::NetworkClient()
    {
        ensureTransport();
    }

    void NetworkClient::SetTransport(std::unique_ptr<IClientTransport> transport)
    {
        mTransport = std::move(transport);
        if(mTransport)
        {
            mTransport->OnReceive([this](const NetPacket &packet) {
                // handlePacket needs the registry; buffer through Poll() instead.
                // The callback below is replaced in Poll(); this default only
                // keeps the transport wired when no Poll is running.
                (void)packet;
            });
        }
    }

    void NetworkClient::ensureTransport()
    {
        if(!mTransport)
        {
            mTransport = MakeEnetClientTransport();
        }
    }

    bool NetworkClient::Connect(const std::string &host, std::uint16_t port)
    {
        ensureTransport();
        mSnapshots.clear();
        mRpcs.clear();
        mNetToEntity.clear();
        mClockInitialized = false;
        return mTransport->Connect(host, port);
    }

    void NetworkClient::Disconnect()
    {
        if(mTransport)
        {
            mTransport->Disconnect();
        }
        mSnapshots.clear();
        mNetToEntity.clear();
    }

    bool NetworkClient::IsConnected() const
    {
        return mTransport && mTransport->IsConnected();
    }

    void NetworkClient::Poll(fr::Registry &registry)
    {
        if(!mTransport)
        {
            return;
        }
        // Capture receive time per packet so interpolation stays smooth even
        // when several snapshots arrive in one Poll burst.
        mTransport->OnReceive([this, &registry](const NetPacket &packet) {
            handlePacket(registry, packet, mClock);
        });
        mTransport->Poll();
    }

    void NetworkClient::handlePacket(fr::Registry &registry, const NetPacket &packet,
                                     double nowSeconds)
    {
        if(packet.bytes.empty())
        {
            return;
        }
        NetReader reader(packet.bytes);
        std::uint8_t type = 0;
        // Peek without consuming for dispatch: reader starts at 0.
        if(!reader.ReadU8(type))
        {
            return;
        }
        if(type == static_cast<std::uint8_t>(NetMessageType::Snapshot))
        {
            // Re-parse from the start (DecodeSnapshot expects the type byte).
            NetReader full(packet.bytes);
            NetSnapshot snapshot {};
            if(!DecodeSnapshot(full, snapshot))
            {
                return;
            }
            if(!mSnapshots.empty() && snapshot.tick <= mSnapshots.back().snapshot.tick)
            {
                return; // stale / duplicate
            }
            mSnapshots.push_back(TimedSnapshot {.receiveTime = nowSeconds,
                                                .snapshot     = std::move(snapshot)});
            while(mSnapshots.size() > kMaxSnapshotsBuffered)
            {
                mSnapshots.pop_front();
            }
        }
        else if(type == static_cast<std::uint8_t>(NetMessageType::Spawn))
        {
            NetReader full(packet.bytes);
            applySpawn(registry, full);
        }
        else if(type == static_cast<std::uint8_t>(NetMessageType::Despawn))
        {
            NetReader full(packet.bytes);
            applyDespawn(registry, full);
        }
        else if(type == static_cast<std::uint8_t>(NetMessageType::Rpc))
        {
            NetReader full(packet.bytes);
            std::uint8_t rpcType = 0;
            std::uint32_t netId = 0;
            std::uint32_t rpcId = 0;
            std::uint16_t size  = 0;
            if(!full.ReadU8(rpcType) || !full.ReadU32(netId) || !full.ReadU32(rpcId) ||
               !full.ReadU16(size))
            {
                return;
            }
            RpcMessage rpc {.netId = netId, .rpcId = rpcId};
            rpc.payload.resize(size);
            if(size > 0 && !full.ReadBytes(rpc.payload.data(), size))
            {
                return;
            }
            mRpcs.push_back(std::move(rpc));
        }
    }

    void NetworkClient::applySpawn(fr::Registry &registry, NetReader &reader)
    {
        std::uint32_t netId  = 0;
        std::uint32_t owner  = 0;
        NetTransformPose pose {};
        std::string prefab;
        if(!DecodeSpawn(reader, netId, owner, pose, prefab))
        {
            return;
        }
        if(mNetToEntity.contains(netId))
        {
            return;
        }
        const fr::Entity entity = registry.CreateEntity();
        registry.AddComponent(entity, NetworkIdentity {.netId         = netId,
                                                      .ownerClientId = owner,
                                                      .prefab        = prefab});
        registry.AddComponent(entity, Replicated {});
        registry.AddComponent(entity, TransformComponent {.position = pose.position,
                                                          .scale    = pose.scale,
                                                          .rotation = pose.rotation});
        mNetToEntity[netId] = entity;
    }

    void NetworkClient::applyDespawn(fr::Registry &registry, NetReader &reader)
    {
        std::uint8_t type = 0;
        std::uint32_t netId = 0;
        if(!reader.ReadU8(type) || !reader.ReadU32(netId))
        {
            return;
        }
        const auto it = mNetToEntity.find(netId);
        if(it == mNetToEntity.end())
        {
            return;
        }
        if(registry.IsAlive(it->second))
        {
            registry.DestroyEntity(it->second);
        }
        mNetToEntity.erase(it);
    }

    void NetworkClient::ApplySnapshots(fr::Registry &registry, double nowSeconds)
    {
        mClock = nowSeconds;
        if(mSnapshots.empty())
        {
            return;
        }
        // Drop snapshots older than the interpolation horizon, keeping one
        // behind for the lerp lower bound.
        const double renderTime = nowSeconds - mInterpolationDelay;
        while(mSnapshots.size() >= 3 && mSnapshots[1].receiveTime <= renderTime)
        {
            mSnapshots.pop_front();
        }
        if(mSnapshots.size() == 1)
        {
            const auto &snapshot = mSnapshots.front().snapshot;
            for(const auto &entry : snapshot.entries)
            {
                const auto it = mNetToEntity.find(entry.netId);
                if(it == mNetToEntity.end() || !registry.IsAlive(it->second))
                {
                    continue;
                }
                registry.TryGetComponents<TransformComponent>(
                    it->second, [&](TransformComponent &transform) {
                        transform.position = entry.pose.position;
                        transform.rotation = entry.pose.rotation;
                        transform.scale    = entry.pose.scale;
                    });
            }
            return;
        }

        const TimedSnapshot *a     = nullptr;
        const TimedSnapshot *b     = nullptr;
        for(std::size_t i = 0; i + 1 < mSnapshots.size(); ++i)
        {
            if(mSnapshots[i].receiveTime <= renderTime &&
               renderTime <= mSnapshots[i + 1].receiveTime)
            {
                a = &mSnapshots[i];
                b = &mSnapshots[i + 1];
                break;
            }
        }
        if(a == nullptr || b == nullptr)
        {
            // Render time outside the buffer: clamp to newest.
            a = &mSnapshots[mSnapshots.size() - 2];
            b = &mSnapshots.back();
        }
        const double span = b->receiveTime - a->receiveTime;
        const float t =
            span > 1e-6 ? static_cast<float>((renderTime - a->receiveTime) / span) : 1.0f;
        const float clamped = std::clamp(t, 0.0f, 1.0f);

        std::unordered_map<std::uint32_t, const NetSnapshotEntry *> bIndex;
        bIndex.reserve(b->snapshot.entries.size());
        for(const auto &entry : b->snapshot.entries)
        {
            bIndex[entry.netId] = &entry;
        }
        for(const auto &entryA : a->snapshot.entries)
        {
            const auto itB = bIndex.find(entryA.netId);
            if(itB == bIndex.end())
            {
                continue;
            }
            const auto itEntity = mNetToEntity.find(entryA.netId);
            if(itEntity == mNetToEntity.end() || !registry.IsAlive(itEntity->second))
            {
                continue;
            }
            // Owned entities are client-predicted; never snap them back to the
            // interpolated ghost (server corrects via RPC/input ack instead).
            bool skip = false;
            registry.TryGetComponents<NetworkOwner>(
                itEntity->second, [&](const NetworkOwner &owner) { skip = owner.isLocal; });
            if(skip)
            {
                continue;
            }
            const NetTransformPose pose = LerpPose(entryA.pose, itB->second->pose, clamped);
            registry.TryGetComponents<TransformComponent>(
                itEntity->second, [&](TransformComponent &transform) {
                    transform.position = pose.position;
                    transform.rotation = pose.rotation;
                    transform.scale    = pose.scale;
                });
        }
    }

    void NetworkClient::SendInput(std::uint32_t netId, const std::vector<std::uint8_t> &payload)
    {
        NetWriter writer;
        writer.WriteU8(static_cast<std::uint8_t>(NetMessageType::Input));
        writer.WriteU32(0); // server fills tick on receipt ordering
        writer.WriteU32(netId);
        writer.WriteU16(static_cast<std::uint16_t>(payload.size()));
        writer.WriteBytes(payload.data(), payload.size());
        if(mTransport)
        {
            mTransport->SendState(0, writer.Data());
        }
    }

    void NetworkClient::SendRpc(std::uint32_t netId, std::uint32_t rpcId,
                                const std::vector<std::uint8_t> &payload)
    {
        NetWriter writer;
        writer.WriteU8(static_cast<std::uint8_t>(NetMessageType::Rpc));
        writer.WriteU32(netId);
        writer.WriteU32(rpcId);
        writer.WriteU16(static_cast<std::uint16_t>(payload.size()));
        writer.WriteBytes(payload.data(), payload.size());
        if(mTransport)
        {
            mTransport->SendReliable(0, writer.Data());
        }
    }

    std::vector<NetworkClient::RpcMessage> NetworkClient::ConsumeRpcs()
    {
        std::vector<RpcMessage> out;
        out.swap(mRpcs);
        return out;
    }

    std::optional<fr::Entity> NetworkClient::FindEntity(std::uint32_t netId) const
    {
        if(const auto it = mNetToEntity.find(netId); it != mNetToEntity.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

} // namespace FRIGGA_NAMESPACE
