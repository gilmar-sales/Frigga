#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Network/INetworkTransport.hpp>
#include <Frigga/Network/NetworkProtocol.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /// Snapshot client with interpolation. Inbound snapshots land in a jitter
    /// buffer; ApplySnapshots() renders `newest - interpolationDelay` by
    /// lerping between the two bracketing snapshots. Owned entities (local
    /// player) skip interpolation so input feels immediate.
    class NetworkClient
    {
      public:
        NetworkClient();
        ~NetworkClient() = default;

        void SetTransport(std::unique_ptr<IClientTransport> transport);

        bool Connect(const std::string &host, std::uint16_t port);
        void Disconnect();
        [[nodiscard]] bool IsConnected() const;

        /// Pump ENet, buffer snapshots, apply spawn/despawn inline and queue
        /// RPCs. `registry` spawns entities for Spawn messages.
        void Poll(fr::Registry &registry);
        /// Interpolate buffered snapshots onto entity transforms.
        /// `nowSeconds` is a monotonic client clock (e.g. accumulated dt).
        void ApplySnapshots(fr::Registry &registry, double nowSeconds);

        void SendInput(std::uint32_t netId, const std::vector<std::uint8_t> &payload);
        void SendRpc(std::uint32_t netId, std::uint32_t rpcId,
                     const std::vector<std::uint8_t> &payload);

        struct RpcMessage
        {
            std::uint32_t netId = 0;
            std::uint32_t rpcId = 0;
            std::vector<std::uint8_t> payload;
        };

        std::vector<RpcMessage> ConsumeRpcs();

        [[nodiscard]] std::optional<fr::Entity> FindEntity(std::uint32_t netId) const;
        [[nodiscard]] std::uint32_t GetLocalClientId() const
        {
            return mLocalClientId;
        }

        void SetInterpolationDelay(float seconds)
        {
            mInterpolationDelay = seconds;
        }

        [[nodiscard]] std::size_t BufferedSnapshots() const
        {
            return mSnapshots.size();
        }

      private:
        struct TimedSnapshot
        {
            double receiveTime = 0.0;
            NetSnapshot snapshot {};
        };

        void ensureTransport();
        void handlePacket(fr::Registry &registry, const NetPacket &packet, double nowSeconds);
        void applySpawn(fr::Registry &registry, NetReader &reader);
        void applyDespawn(fr::Registry &registry, NetReader &reader);

        std::unique_ptr<IClientTransport> mTransport;
        std::unordered_map<std::uint32_t, fr::Entity> mNetToEntity;
        std::deque<TimedSnapshot> mSnapshots;
        std::vector<RpcMessage> mRpcs;
        std::uint32_t mLocalClientId     = 0;
        float mInterpolationDelay        = kDefaultInterpolationDelay;
        double mClock                    = 0.0;
        bool mClockInitialized           = false;
    };

} // namespace FRIGGA_NAMESPACE
