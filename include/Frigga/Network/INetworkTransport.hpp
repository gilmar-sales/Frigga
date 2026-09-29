#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Network/NetworkProtocol.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    using NetClientId = std::uint32_t;

    struct NetPacket
    {
        NetClientId fromClientId = 0; // 0 = server (on client-side receive)
        std::vector<std::uint8_t> bytes;
    };

    /// Transport-agnostic event sink. ENet and the in-memory fake used by
    /// tests both feed these; NetworkServer/NetworkClient own the logic.
    class INetworkTransport
    {
      public:
        virtual ~INetworkTransport() = default;

        virtual void Poll() = 0;

        /// Reliable ordered send (spawn/despawn/RPC/session control).
        virtual void SendReliable(NetClientId to, const std::vector<std::uint8_t> &bytes) = 0;
        /// Unreliable sequenced send (snapshots/inputs). May drop/reorder.
        virtual void SendState(NetClientId to, const std::vector<std::uint8_t> &bytes) = 0;
        virtual void BroadcastReliable(const std::vector<std::uint8_t> &bytes)         = 0;
        virtual void BroadcastState(const std::vector<std::uint8_t> &bytes)            = 0;

        using ConnectCallback    = std::function<void(NetClientId)>;
        using DisconnectCallback = std::function<void(NetClientId)>;
        using ReceiveCallback    = std::function<void(const NetPacket &)>;

        virtual void OnClientConnected(ConnectCallback callback)       = 0;
        virtual void OnClientDisconnected(DisconnectCallback callback) = 0;
        virtual void OnReceive(ReceiveCallback callback)               = 0;
    };

    class IServerTransport: public INetworkTransport
    {
      public:
        virtual bool Start(std::uint16_t port, std::size_t maxClients) = 0;
        virtual void Stop()                                            = 0;
        [[nodiscard]] virtual bool IsRunning() const                   = 0;
        [[nodiscard]] virtual std::uint16_t GetPort() const            = 0;
    };

    class IClientTransport: public INetworkTransport
    {
      public:
        virtual bool Connect(const std::string &host, std::uint16_t port) = 0;
        virtual void Disconnect()                                         = 0;
        [[nodiscard]] virtual bool IsConnected() const                    = 0;
    };

} // namespace FRIGGA_NAMESPACE
