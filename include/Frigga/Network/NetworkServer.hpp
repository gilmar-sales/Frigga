#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Network/INetworkTransport.hpp>
#include <Frigga/Network/NetworkComponents.hpp>
#include <Frigga/Network/NetworkProtocol.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct NetInputMessage
    {
        NetClientId clientId = 0;
        std::uint32_t tick   = 0;
        std::uint32_t netId  = 0;
        std::vector<std::uint8_t> payload;
    };

    struct NetRpcMessage
    {
        NetClientId from = 0;
        std::uint32_t netId = 0;
        std::uint32_t rpcId = 0;
        std::vector<std::uint8_t> payload;
    };

    /// Authoritative server. Owns netId allocation, the entity<->netId map,
    /// snapshot broadcast and the inbound input/RPC queues. I/O happens in
    /// Poll()/BroadcastSnapshot(), both driven by NetworkServerSystem at the
    /// Simulation rate; gameplay code talks through NetworkController.
    class NetworkServer
    {
      public:
        NetworkServer();
        ~NetworkServer() = default;

        /// Inject a fake transport (tests). When null, ENet is used.
        void SetTransport(std::unique_ptr<IServerTransport> transport);

        bool Start(std::uint16_t port, std::size_t maxClients = kMaxClients);
        void Stop();
        [[nodiscard]] bool IsHosting() const;

        /// Allocate a netId for an entity and emit a Spawn message.
        /// The entity must already carry NetworkIdentity; Replicated is added.
        std::uint32_t RegisterEntity(fr::Registry &registry, fr::Entity entity,
                                     std::uint32_t ownerClientId = kServerClientId);
        void UnregisterEntity(fr::Registry &registry, fr::Entity entity);

        /// Pump ENet events and route inbound packets into queues.
        void Poll();
        /// Build a snapshot from every replicated entity and broadcast it
        /// over the state channel. Call at snapshotHz (default 20 Hz).
        void BroadcastSnapshot(fr::Registry &registry);

        void SendRpc(NetClientId to, std::uint32_t netId, std::uint32_t rpcId,
                     const std::vector<std::uint8_t> &payload);
        void BroadcastRpc(std::uint32_t netId, std::uint32_t rpcId,
                          const std::vector<std::uint8_t> &payload);

        [[nodiscard]] std::optional<fr::Entity> FindEntity(std::uint32_t netId) const;
        [[nodiscard]] std::uint32_t GetTick() const
        {
            return mTick;
        }

        std::vector<NetInputMessage> ConsumeInputs();
        std::vector<NetRpcMessage> ConsumeRpcs();
        std::vector<NetClientId> ConsumeConnections();
        std::vector<NetClientId> ConsumeDisconnections();

        [[nodiscard]] std::vector<NetClientId> GetClients() const;

        void SetSnapshotRate(float hz)
        {
            mSnapshotHz = hz > 0.0f ? hz : kDefaultSnapshotHz;
        }

        [[nodiscard]] float GetSnapshotRate() const
        {
            return mSnapshotHz;
        }

        /// Time since the last broadcast; the system accumulates deltaTime
        /// and calls BroadcastSnapshot when ShouldSendSnapshot elapses.
        [[nodiscard]] bool ShouldSendSnapshot(float deltaTime);

      private:
        void ensureTransport();
        void handlePacket(const NetPacket &packet);
        void rebuildEntityIndex(fr::Registry &registry);
        void encodeSpawn(fr::Registry &registry, fr::Entity entity,
                         const NetworkIdentity &identity, NetWriter &out) const;

        std::unique_ptr<IServerTransport> mTransport;
        std::unordered_map<std::uint32_t, fr::Entity> mNetToEntity;
        std::unordered_map<fr::Entity, std::uint32_t> mEntityToNet;
        std::vector<NetClientId> mClients;
        std::vector<NetClientId> mJoiners;
        std::vector<NetInputMessage> mInputs;
        std::vector<NetRpcMessage> mRpcs;
        std::vector<NetClientId> mPendingConnections;
        std::vector<NetClientId> mPendingDisconnections;
        std::uint32_t mNextNetId = 1;
        std::uint32_t mTick      = 0;
        float mSnapshotHz        = kDefaultSnapshotHz;
        float mSnapshotAccum     = 0.0f;
    };

} // namespace FRIGGA_NAMESPACE
