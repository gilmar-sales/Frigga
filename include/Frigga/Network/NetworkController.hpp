#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Network/NetworkClient.hpp>
#include <Frigga/Network/NetworkServer.hpp>

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /// Gameplay-facing networking API. Mirrors AudioController: it only sets
    /// component intents and session state, while NetworkServerSystem /
    /// NetworkClientSystem own the transports and tick them.
    class NetworkController
    {
      public:
        NetworkController(const skr::Arc<fr::Registry> &registry,
                          const skr::Arc<NetworkServer> &server,
                          const skr::Arc<NetworkClient> &client);

        bool Host(std::uint16_t port, std::size_t maxClients = kMaxClients);
        void StopHost();
        [[nodiscard]] bool IsHosting() const;

        bool Join(const std::string &host, std::uint16_t port);
        void Leave();
        [[nodiscard]] bool IsConnected() const;

        /// Tag an entity for replication (server) or mark-and-send (client
        /// owned). Returns the stable netId, or kInvalidNetId on failure.
        std::uint32_t Replicate(fr::Entity entity,
                                std::uint32_t ownerClientId = kServerClientId);
        void Unreplicate(fr::Entity entity);

        void SendInput(std::uint32_t netId, const std::vector<std::uint8_t> &payload);
        void SendRpc(std::uint32_t netId, std::uint32_t rpcId,
                     const std::vector<std::uint8_t> &payload);

        [[nodiscard]] bool IsServer() const
        {
            return IsHosting();
        }

      private:
        skr::Arc<fr::Registry> mRegistry;
        skr::Arc<NetworkServer> mServer;
        skr::Arc<NetworkClient> mClient;
    };

} // namespace FRIGGA_NAMESPACE
