#pragma once

#include "Frigga/Net/Protocol.hpp"

#include <Freyr/Freyr.hpp>
#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trantor
{
    class EventLoopThread;
    class TcpServer;
    class TcpClient;
    class TcpConnection;
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
} // namespace trantor

namespace FRIGGA_NAMESPACE
{

    class Network
    {
      public:
        using CommandHandler = std::function<void(std::uint32_t peerId,
                                                  std::span<const std::uint8_t> payload)>;
        using EventHandler   = std::function<void(std::span<const std::uint8_t> payload)>;

        explicit Network(const skr::Arc<skr::Logger<Network>> &logger);
        ~Network();

        Network(const Network &)            = delete;
        Network &operator=(const Network &) = delete;

        /// Listen-server (Editor Play) or dedicated host.
        bool Host(std::uint16_t port, bool dedicated = false);
        bool Connect(std::string_view host, std::uint16_t port,
                     std::string_view playerName = "Player");
        void Disconnect();

        [[nodiscard]] NetRole Role() const;
        [[nodiscard]] bool IsAuthority() const;
        [[nodiscard]] bool IsDedicated() const;
        [[nodiscard]] bool IsSessionActive() const;
        [[nodiscard]] std::uint32_t LocalPeerId() const;
        [[nodiscard]] std::uint32_t Tick() const;
        [[nodiscard]] std::uint16_t Port() const;
        [[nodiscard]] std::size_t PeerCount() const;
        [[nodiscard]] const std::string &LastError() const;

        void SendCommand(std::string_view name, std::span<const std::uint8_t> payload = {});
        void BroadcastEvent(std::string_view name, std::span<const std::uint8_t> payload = {});
        void SendEvent(std::uint32_t peerId, std::string_view name,
                       std::span<const std::uint8_t> payload = {});

        void OnCommand(std::string name, CommandHandler handler);
        void OnEvent(std::string name, EventHandler handler);

        /// Drain IO inbox, dispatch commands/events, apply snapshots (game thread).
        void Receive(fr::Registry &registry);
        /// Authority: snapshot NetworkTransform. Clients: no-op.
        void Send(fr::Registry &registry);

      private:
        struct Incoming
        {
            std::uint32_t             peerId = 0;
            NetMessageType            type   = NetMessageType::Hello;
            std::vector<std::uint8_t> payload;
        };

        struct ConnectionState;

        void ensureLoop();
        void postIncoming(std::uint32_t peerId, const NetFrame &frame);
        void sendBytes(const trantor::TcpConnectionPtr &conn, std::vector<std::uint8_t> bytes);
        void sendToPeer(std::uint32_t peerId, NetMessageType type,
                        std::span<const std::uint8_t> payload);
        void broadcast(NetMessageType type, std::span<const std::uint8_t> payload,
                       std::uint32_t exceptPeer = 0);
        void handleHello(const trantor::TcpConnectionPtr &conn, std::span<const std::uint8_t> payload);
        void bindConnection(const trantor::TcpConnectionPtr &conn, std::uint32_t peerId);
        void onServerConnection(const trantor::TcpConnectionPtr &conn);
        void onClientConnection(const trantor::TcpConnectionPtr &conn);
        void onMessage(const trantor::TcpConnectionPtr &conn, std::vector<std::uint8_t> chunk);
        void applySnapshot(fr::Registry &registry, std::span<const std::uint8_t> payload);
        void dispatchNamed(bool isCommand, std::uint32_t peerId,
                           std::span<const std::uint8_t> payload);

        skr::Arc<skr::Logger<Network>> mLogger;
        std::unique_ptr<trantor::EventLoopThread> mLoopThread;
        std::unique_ptr<trantor::TcpServer> mServer;
        std::shared_ptr<trantor::TcpClient> mClient;

        mutable std::mutex mMutex;
        std::vector<Incoming> mInbox;
        std::unordered_map<std::uint32_t, trantor::TcpConnectionPtr> mPeers;
        std::unordered_map<std::uint32_t, CommandHandler> mCommandHandlers;
        std::unordered_map<std::uint32_t, EventHandler> mEventHandlers;
        std::unordered_map<std::uint32_t, fr::Entity> mEntitiesByNetId;

        NetRole       mRole        = NetRole::Offline;
        std::uint32_t mLocalPeerId = 0;
        std::uint32_t mNextPeerId  = kNetHostPeerId + 1;
        std::uint32_t mNextNetId   = 1;
        std::uint32_t mTick        = 0;
        std::uint16_t mPort        = 0;
        std::string   mLastError;
        std::string   mPlayerName {"Player"};
        bool          mLoopStarted = false;
    };

} // namespace FRIGGA_NAMESPACE
