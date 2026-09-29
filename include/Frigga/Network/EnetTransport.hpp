#pragma once

#include <Frigga/Macro.hpp>
#include <Frigga/Network/INetworkTransport.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace FRIGGA_NAMESPACE
{

    /// ENet-backed server transport. Created by NetworkServer::Start; the
    /// class itself is public so gameplay code can inject a fake in tests.
    class EnetServerTransport final: public IServerTransport
    {
      public:
        EnetServerTransport();
        ~EnetServerTransport() override;

        EnetServerTransport(const EnetServerTransport &)            = delete;
        EnetServerTransport &operator=(const EnetServerTransport &) = delete;

        bool Start(std::uint16_t port, std::size_t maxClients) override;
        void Stop() override;
        [[nodiscard]] bool IsRunning() const override;
        [[nodiscard]] std::uint16_t GetPort() const override;

        void Poll() override;
        void SendReliable(NetClientId to, const std::vector<std::uint8_t> &bytes) override;
        void SendState(NetClientId to, const std::vector<std::uint8_t> &bytes) override;
        void BroadcastReliable(const std::vector<std::uint8_t> &bytes) override;
        void BroadcastState(const std::vector<std::uint8_t> &bytes) override;

        void OnClientConnected(ConnectCallback callback) override;
        void OnClientDisconnected(DisconnectCallback callback) override;
        void OnReceive(ReceiveCallback callback) override;

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    class EnetClientTransport final: public IClientTransport
    {
      public:
        EnetClientTransport();
        ~EnetClientTransport() override;

        EnetClientTransport(const EnetClientTransport &)            = delete;
        EnetClientTransport &operator=(const EnetClientTransport &) = delete;

        bool Connect(const std::string &host, std::uint16_t port) override;
        void Disconnect() override;
        [[nodiscard]] bool IsConnected() const override;

        void Poll() override;
        void SendReliable(NetClientId to, const std::vector<std::uint8_t> &bytes) override;
        void SendState(NetClientId to, const std::vector<std::uint8_t> &bytes) override;
        void BroadcastReliable(const std::vector<std::uint8_t> &bytes) override;
        void BroadcastState(const std::vector<std::uint8_t> &bytes) override;

        void OnClientConnected(ConnectCallback callback) override;
        void OnClientDisconnected(DisconnectCallback callback) override;
        void OnReceive(ReceiveCallback callback) override;

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

    /// Process-wide ENet init/deinit with ref-counting. Transports call it
    /// internally; exposed for tests that only need enet_initialize().
    class EnetRuntime
    {
      public:
        static bool EnsureInitialized();
        static void Release();
    };

    std::unique_ptr<IServerTransport> MakeEnetServerTransport();
    std::unique_ptr<IClientTransport> MakeEnetClientTransport();

} // namespace FRIGGA_NAMESPACE
