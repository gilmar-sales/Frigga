#include <Frigga/Network/NetworkServer.hpp>

#include <Frigga/ECS/Components/TransformComponent.hpp>
#include <Frigga/Network/EnetTransport.hpp>

namespace FRIGGA_NAMESPACE
{

    NetworkServer::NetworkServer()
    {
        ensureTransport();
    }

    void NetworkServer::SetTransport(std::unique_ptr<IServerTransport> transport)
    {
        mTransport = std::move(transport);
        if(mTransport)
        {
            mTransport->OnClientConnected(
                [this](NetClientId id) { mPendingConnections.push_back(id); });
            mTransport->OnClientDisconnected(
                [this](NetClientId id) { mPendingDisconnections.push_back(id); });
            mTransport->OnReceive([this](const NetPacket &packet) { handlePacket(packet); });
        }
    }

    void NetworkServer::ensureTransport()
    {
        if(!mTransport)
        {
            SetTransport(MakeEnetServerTransport());
        }
    }

    bool NetworkServer::Start(std::uint16_t port, std::size_t maxClients)
    {
        ensureTransport();
        mClients.clear();
        mInputs.clear();
        mRpcs.clear();
        mPendingConnections.clear();
        mPendingDisconnections.clear();
        mJoiners.clear();
        mTick          = 0;
        mSnapshotAccum = 0.0f;
        return mTransport->Start(port, maxClients == 0 ? kMaxClients : maxClients);
    }

    void NetworkServer::Stop()
    {
        if(mTransport)
        {
            mTransport->Stop();
        }
        mClients.clear();
        mNetToEntity.clear();
        mEntityToNet.clear();
    }

    bool NetworkServer::IsHosting() const
    {
        return mTransport && mTransport->IsRunning();
    }

    std::uint32_t NetworkServer::RegisterEntity(fr::Registry &registry, fr::Entity entity,
                                                std::uint32_t ownerClientId)
    {
        if(!registry.IsAlive(entity))
        {
            return kInvalidNetId;
        }
        if(!registry.HasComponent<NetworkIdentity>(entity))
        {
            registry.AddComponent(entity, NetworkIdentity {});
        }
        const std::uint32_t netId = mNextNetId++;
        registry.TryGetComponents<NetworkIdentity>(entity, [&](NetworkIdentity &identity) {
            identity.netId         = netId;
            identity.ownerClientId = ownerClientId;
        });
        if(!registry.HasComponent<Replicated>(entity))
        {
            registry.AddComponent(entity, Replicated {});
        }
        mNetToEntity[netId] = entity;
        mEntityToNet[entity] = netId;

        // Reliable spawn so late joiners and packet loss never miss births.
        NetWriter writer;
        registry.TryGetComponents<NetworkIdentity>(
            entity, [&](const NetworkIdentity &identity) { encodeSpawn(registry, entity, identity, writer); });
        if(mTransport && mTransport->IsRunning())
        {
            mTransport->BroadcastReliable(writer.Data());
        }
        return netId;
    }

    void NetworkServer::UnregisterEntity(fr::Registry &registry, fr::Entity entity)
    {
        const auto it = mEntityToNet.find(entity);
        if(it == mEntityToNet.end())
        {
            return;
        }
        const std::uint32_t netId = it->second;
        mEntityToNet.erase(it);
        mNetToEntity.erase(netId);

        NetWriter writer;
        writer.WriteU8(static_cast<std::uint8_t>(NetMessageType::Despawn));
        writer.WriteU32(netId);
        if(mTransport && mTransport->IsRunning())
        {
            mTransport->BroadcastReliable(writer.Data());
        }
        registry.TryGetComponents<NetworkIdentity>(
            entity, [](NetworkIdentity &identity) { identity.netId = kInvalidNetId; });
    }

    void NetworkServer::Poll()
    {
        if(!IsHosting())
        {
            return;
        }
        // Promote transport-level connects into the client roster first so a
        // snapshot sent in the same tick already reaches the newcomer.
        // Joiners are remembered in mJoiners; the next BroadcastSnapshot
        // re-sends every Spawn reliably to them.
        mTransport->Poll();
        for(const auto id : mPendingConnections)
        {
            if(std::find(mClients.begin(), mClients.end(), id) == mClients.end())
            {
                mClients.push_back(id);
            }
            if(std::find(mJoiners.begin(), mJoiners.end(), id) == mJoiners.end())
            {
                mJoiners.push_back(id);
            }
        }
        mPendingConnections.clear();
        for(const auto id : mPendingDisconnections)
        {
            std::erase(mClients, id);
            std::erase(mJoiners, id);
        }
        mPendingDisconnections.clear();
    }

    void NetworkServer::encodeSpawn(fr::Registry &registry, fr::Entity entity,
                                    const NetworkIdentity &identity, NetWriter &out) const
    {
        NetTransformPose pose {};
        registry.TryGetComponents<TransformComponent>(
            entity, [&](const TransformComponent &transform) {
                pose.position = transform.position;
                pose.rotation = transform.rotation;
                pose.scale    = transform.scale;
            });
        EncodeSpawn(identity.netId, identity.ownerClientId, pose, identity.prefab, out);
    }

    void NetworkServer::rebuildEntityIndex(fr::Registry &registry)
    {
        // Drop dead entities; adopt newly tagged ones lazily is done by the
        // controller, so here we only garbage-collect.
        std::vector<std::uint32_t> dead;
        for(const auto &[netId, entity] : mNetToEntity)
        {
            if(!registry.IsAlive(entity))
            {
                dead.push_back(netId);
                continue;
            }
            std::uint32_t current = kInvalidNetId;
            registry.TryGetComponents<NetworkIdentity>(
                entity, [&](const NetworkIdentity &identity) { current = identity.netId; });
            if(current != netId)
            {
                dead.push_back(netId);
            }
        }
        for(const auto netId : dead)
        {
            if(const auto it = mNetToEntity.find(netId); it != mNetToEntity.end())
            {
                mEntityToNet.erase(it->second);
                mNetToEntity.erase(it);
            }
        }
    }

    void NetworkServer::BroadcastSnapshot(fr::Registry &registry)
    {
        if(!IsHosting())
        {
            return;
        }
        rebuildEntityIndex(registry);

        // Catch joiners up: re-send every Spawn reliably before the snapshot.
        if(!mJoiners.empty())
        {
            for(const auto joiner : mJoiners)
            {
                for(const auto &[netId, entity] : mNetToEntity)
                {
                    if(!registry.IsAlive(entity))
                    {
                        continue;
                    }
                    NetWriter spawn;
                    registry.TryGetComponents<NetworkIdentity>(
                        entity, [&](const NetworkIdentity &identity) {
                            if(identity.netId == netId)
                            {
                                encodeSpawn(registry, entity, identity, spawn);
                            }
                        });
                    if(spawn.Size() > 0)
                    {
                        mTransport->SendReliable(joiner, spawn.Data());
                    }
                }
            }
            mJoiners.clear();
        }

        // Read-only query: Iterate() copies components without dirtying
        // change ticks (unlike Mutation::Each, which marks everything read).
        NetSnapshot snapshot {.tick = ++mTick};
        for(auto &[entity, identity, transform] :
            registry.CreateQuery()->Iterate<NetworkIdentity, TransformComponent>())
        {
            if(identity.netId == kInvalidNetId)
            {
                continue;
            }
            if(!registry.HasComponent<Replicated>(entity))
            {
                continue;
            }
            snapshot.entries.push_back(NetSnapshotEntry {
                .netId = identity.netId,
                .pose  = {.position = transform.position,
                          .rotation = transform.rotation,
                          .scale    = transform.scale},
            });
        }

        NetWriter writer;
        EncodeSnapshot(snapshot, writer);
        mTransport->BroadcastState(writer.Data());
        mSnapshotAccum = 0.0f;
    }

    bool NetworkServer::ShouldSendSnapshot(float deltaTime)
    {
        if(!IsHosting())
        {
            return false;
        }
        mSnapshotAccum += deltaTime;
        return mSnapshotAccum >= 1.0f / mSnapshotHz;
    }

    void NetworkServer::SendRpc(NetClientId to, std::uint32_t netId, std::uint32_t rpcId,
                                const std::vector<std::uint8_t> &payload)
    {
        NetWriter writer;
        writer.WriteU8(static_cast<std::uint8_t>(NetMessageType::Rpc));
        writer.WriteU32(netId);
        writer.WriteU32(rpcId);
        writer.WriteU16(static_cast<std::uint16_t>(payload.size()));
        writer.WriteBytes(payload.data(), payload.size());
        if(mTransport && mTransport->IsRunning())
        {
            mTransport->SendReliable(to, writer.Data());
        }
    }

    void NetworkServer::BroadcastRpc(std::uint32_t netId, std::uint32_t rpcId,
                                     const std::vector<std::uint8_t> &payload)
    {
        NetWriter writer;
        writer.WriteU8(static_cast<std::uint8_t>(NetMessageType::Rpc));
        writer.WriteU32(netId);
        writer.WriteU32(rpcId);
        writer.WriteU16(static_cast<std::uint16_t>(payload.size()));
        writer.WriteBytes(payload.data(), payload.size());
        if(mTransport && mTransport->IsRunning())
        {
            mTransport->BroadcastReliable(writer.Data());
        }
    }

    void NetworkServer::handlePacket(const NetPacket &packet)
    {
        if(packet.bytes.empty())
        {
            return;
        }
        NetReader reader(packet.bytes);
        std::uint8_t type = 0;
        if(!reader.ReadU8(type))
        {
            return;
        }
        if(type == static_cast<std::uint8_t>(NetMessageType::Input))
        {
            NetInputMessage input {.clientId = packet.fromClientId};
            std::uint16_t size = 0;
            if(!reader.ReadU32(input.tick) || !reader.ReadU32(input.netId) ||
               !reader.ReadU16(size))
            {
                return;
            }
            input.payload.resize(size);
            if(size > 0 && !reader.ReadBytes(input.payload.data(), size))
            {
                return;
            }
            mInputs.push_back(std::move(input));
        }
        else if(type == static_cast<std::uint8_t>(NetMessageType::Rpc))
        {
            NetRpcMessage rpc {.from = packet.fromClientId};
            std::uint16_t size = 0;
            if(!reader.ReadU32(rpc.netId) || !reader.ReadU32(rpc.rpcId) ||
               !reader.ReadU16(size))
            {
                return;
            }
            rpc.payload.resize(size);
            if(size > 0 && !reader.ReadBytes(rpc.payload.data(), size))
            {
                return;
            }
            mRpcs.push_back(std::move(rpc));
        }
    }

    std::optional<fr::Entity> NetworkServer::FindEntity(std::uint32_t netId) const
    {
        if(const auto it = mNetToEntity.find(netId); it != mNetToEntity.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    std::vector<NetInputMessage> NetworkServer::ConsumeInputs()
    {
        std::vector<NetInputMessage> out;
        out.swap(mInputs);
        return out;
    }

    std::vector<NetRpcMessage> NetworkServer::ConsumeRpcs()
    {
        std::vector<NetRpcMessage> out;
        out.swap(mRpcs);
        return out;
    }

    std::vector<NetClientId> NetworkServer::ConsumeConnections()
    {
        std::vector<NetClientId> out = mClients;
        return out;
    }

    std::vector<NetClientId> NetworkServer::ConsumeDisconnections()
    {
        return {};
    }

    std::vector<NetClientId> NetworkServer::GetClients() const
    {
        return mClients;
    }

} // namespace FRIGGA_NAMESPACE
