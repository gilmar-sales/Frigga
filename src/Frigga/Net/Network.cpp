#include "Frigga/Net/Network.hpp"

#include "Frigga/ECS/Components/NetworkIdentity.hpp"
#include "Frigga/ECS/Components/NetworkTransform.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <trantor/net/EventLoop.h>
#include <trantor/net/EventLoopThread.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/TcpClient.h>
#include <trantor/net/TcpConnection.h>
#include <trantor/net/TcpServer.h>
#include <trantor/utils/MsgBuffer.h>

#include <utility>
#include <future>

namespace FRIGGA_NAMESPACE
{
    struct Network::ConnectionState
    {
        std::uint32_t             peerId = 0;
        std::vector<std::uint8_t> recv;
        bool                      welcomed = false;
    };

    Network::Network(const skr::Arc<skr::Logger<Network>> &logger) : mLogger(logger) {}

    Network::~Network()
    {
        Disconnect();
        if(mLoopThread)
        {
            if(auto *loop = mLoopThread->getLoop())
            {
                loop->quit();
            }
        }
    }

    void Network::ensureLoop()
    {
        if(mLoopStarted)
        {
            return;
        }
        mLoopThread = std::make_unique<trantor::EventLoopThread>("FriggaNet");
        mLoopThread->run();
        mLoopStarted = true;
    }

    NetRole Network::Role() const
    {
        std::lock_guard lock(mMutex);
        return mRole;
    }

    bool Network::IsAuthority() const
    {
        const auto role = Role();
        return role == NetRole::Host || role == NetRole::Dedicated;
    }

    bool Network::IsDedicated() const
    {
        return Role() == NetRole::Dedicated;
    }

    bool Network::IsSessionActive() const
    {
        return Role() != NetRole::Offline;
    }

    std::uint32_t Network::LocalPeerId() const
    {
        std::lock_guard lock(mMutex);
        return mLocalPeerId;
    }

    std::uint32_t Network::Tick() const
    {
        std::lock_guard lock(mMutex);
        return mTick;
    }

    std::uint16_t Network::Port() const
    {
        std::lock_guard lock(mMutex);
        return mPort;
    }

    std::size_t Network::PeerCount() const
    {
        std::lock_guard lock(mMutex);
        return mPeers.size();
    }

    const std::string &Network::LastError() const
    {
        return mLastError;
    }

    bool Network::Host(std::uint16_t port, bool dedicated)
    {
        Disconnect();
        ensureLoop();
        auto *loop = mLoopThread->getLoop();
        if(!loop)
        {
            mLastError = "Net event loop failed to start";
            return false;
        }

        {
            std::lock_guard lock(mMutex);
            mRole        = dedicated ? NetRole::Dedicated : NetRole::Host;
            mLocalPeerId = kNetHostPeerId;
            mPort        = port;
            mNextPeerId  = kNetHostPeerId + 1;
            mTick        = 0;
            mLastError.clear();
        }

        std::promise<bool> started;
        loop->queueInLoop([this, loop, port, &started]() {
            try
            {
                mServer = std::make_unique<trantor::TcpServer>(
                    loop, trantor::InetAddress(port), "FriggaHost");
                mServer->setConnectionCallback(
                    [this](const trantor::TcpConnectionPtr &conn) { onServerConnection(conn); });
                mServer->setRecvMessageCallback(
                    [this](const trantor::TcpConnectionPtr &conn, trantor::MsgBuffer *buf) {
                        std::vector<std::uint8_t> chunk(buf->peek(),
                                                        buf->peek() + buf->readableBytes());
                        buf->retrieveAll();
                        onMessage(conn, std::move(chunk));
                    });
                mServer->start();
                {
                    std::lock_guard lock(mMutex);
                    mPort = mServer->address().toPort();
                }
                started.set_value(true);
            }
            catch(...)
            {
                started.set_value(false);
            }
        });
        if(!started.get_future().get())
        {
            std::lock_guard lock(mMutex);
            mRole        = NetRole::Offline;
            mLocalPeerId = 0;
            mLastError   = "Failed to bind TCP port";
            return false;
        }

        if(mLogger)
        {
            mLogger->LogInformation("Network host on port {} ({})", port,
                                    dedicated ? "dedicated" : "listen-server");
        }
        return true;
    }

    bool Network::Connect(std::string_view host, std::uint16_t port, std::string_view playerName)
    {
        Disconnect();
        ensureLoop();
        auto *loop = mLoopThread->getLoop();
        if(!loop)
        {
            mLastError = "Net event loop failed to start";
            return false;
        }

        mPlayerName = std::string(playerName);
        {
            std::lock_guard lock(mMutex);
            mRole        = NetRole::Client;
            mLocalPeerId = 0;
            mPort        = port;
            mLastError.clear();
        }

        const std::string ip(host);
        std::promise<bool> started;
        loop->queueInLoop([this, loop, ip, port, &started]() {
            mClient = std::make_shared<trantor::TcpClient>(
                loop, trantor::InetAddress(ip, port), "FriggaClient");
            mClient->setConnectionCallback(
                [this](const trantor::TcpConnectionPtr &conn) { onClientConnection(conn); });
            mClient->setMessageCallback(
                [this](const trantor::TcpConnectionPtr &conn, trantor::MsgBuffer *buf) {
                    std::vector<std::uint8_t> chunk(buf->peek(),
                                                    buf->peek() + buf->readableBytes());
                    buf->retrieveAll();
                    onMessage(conn, std::move(chunk));
                });
            mClient->connect();
            started.set_value(true);
        });
        started.get_future().wait();

        if(mLogger)
        {
            mLogger->LogInformation("Network connecting to {}:{}", ip, port);
        }
        return true;
    }

    void Network::Disconnect()
    {
        auto *loop = mLoopThread ? mLoopThread->getLoop() : nullptr;
        if(loop)
        {
            std::promise<void> done;
            loop->queueInLoop([this, &done]() {
                if(mClient)
                {
                    mClient->disconnect();
                    mClient.reset();
                }
                if(mServer)
                {
                    mServer->stop();
                    mServer.reset();
                }
                done.set_value();
            });
            done.get_future().wait();
        }

        std::lock_guard lock(mMutex);
        mPeers.clear();
        mInbox.clear();
        mEntitiesByNetId.clear();
        mRole        = NetRole::Offline;
        mLocalPeerId = 0;
        mPort        = 0;
        mTick        = 0;
    }

    void Network::SendCommand(std::string_view name, std::span<const std::uint8_t> payload)
    {
        if(Role() != NetRole::Client)
        {
            return;
        }
        const auto named = NetEncodeNamed(name, payload);
        sendToPeer(kNetHostPeerId, NetMessageType::Command, named);
    }

    void Network::BroadcastEvent(std::string_view name, std::span<const std::uint8_t> payload)
    {
        if(!IsAuthority())
        {
            return;
        }
        const auto named = NetEncodeNamed(name, payload);
        broadcast(NetMessageType::Event, named);
    }

    void Network::SendEvent(std::uint32_t peerId, std::string_view name,
                            std::span<const std::uint8_t> payload)
    {
        if(!IsAuthority())
        {
            return;
        }
        const auto named = NetEncodeNamed(name, payload);
        sendToPeer(peerId, NetMessageType::Event, named);
    }

    void Network::OnCommand(std::string name, CommandHandler handler)
    {
        std::lock_guard lock(mMutex);
        mCommandHandlers[NetHashName(name)] = std::move(handler);
    }

    void Network::OnEvent(std::string name, EventHandler handler)
    {
        std::lock_guard lock(mMutex);
        mEventHandlers[NetHashName(name)] = std::move(handler);
    }

    void Network::postIncoming(std::uint32_t peerId, const NetFrame &frame)
    {
        std::lock_guard lock(mMutex);
        mInbox.push_back(Incoming {.peerId = peerId, .type = frame.type, .payload = frame.payload});
    }

    void Network::sendBytes(const trantor::TcpConnectionPtr &conn, std::vector<std::uint8_t> bytes)
    {
        if(!conn || !conn->connected() || bytes.empty())
        {
            return;
        }
        auto *loop = conn->getLoop();
        loop->queueInLoop([conn, bytes = std::move(bytes)]() {
            if(conn->connected())
            {
                conn->send(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            }
        });
    }

    void Network::sendToPeer(std::uint32_t peerId, NetMessageType type,
                             std::span<const std::uint8_t> payload)
    {
        trantor::TcpConnectionPtr conn;
        {
            std::lock_guard lock(mMutex);
            if(mRole == NetRole::Client && mClient)
            {
                conn = mClient->connection();
            }
            else
            {
                const auto it = mPeers.find(peerId);
                if(it != mPeers.end())
                {
                    conn = it->second;
                }
            }
        }
        sendBytes(conn, NetEncodeFrame(type, payload));
    }

    void Network::broadcast(NetMessageType type, std::span<const std::uint8_t> payload,
                            std::uint32_t exceptPeer)
    {
        std::vector<trantor::TcpConnectionPtr> conns;
        {
            std::lock_guard lock(mMutex);
            conns.reserve(mPeers.size());
            for(const auto &[id, conn] : mPeers)
            {
                if(id != exceptPeer)
                {
                    conns.push_back(conn);
                }
            }
        }
        const auto bytes = NetEncodeFrame(type, payload);
        for(const auto &conn : conns)
        {
            sendBytes(conn, bytes);
        }
    }

    void Network::bindConnection(const trantor::TcpConnectionPtr &conn, std::uint32_t peerId)
    {
        auto state = conn->getContext<ConnectionState>();
        if(!state)
        {
            state = std::make_shared<ConnectionState>();
            conn->setContext(state);
        }
        state->peerId    = peerId;
        state->welcomed  = true;
        conn->setTcpNoDelay(true);
        std::lock_guard lock(mMutex);
        mPeers[peerId] = conn;
    }

    void Network::onServerConnection(const trantor::TcpConnectionPtr &conn)
    {
        if(conn->connected())
        {
            auto state = std::make_shared<ConnectionState>();
            conn->setContext(state);
            conn->setTcpNoDelay(true);
            if(mLogger)
            {
                mLogger->LogInformation("Peer connected from {}", conn->peerAddr().toIpPort());
            }
            return;
        }

        auto state = conn->getContext<ConnectionState>();
        const auto peerId = state ? state->peerId : 0;
        {
            std::lock_guard lock(mMutex);
            if(peerId != 0)
            {
                mPeers.erase(peerId);
            }
        }
        if(mLogger)
        {
            mLogger->LogInformation("Peer {} disconnected", peerId);
        }
    }

    void Network::onClientConnection(const trantor::TcpConnectionPtr &conn)
    {
        if(conn->connected())
        {
            auto state = std::make_shared<ConnectionState>();
            conn->setContext(state);
            conn->setTcpNoDelay(true);
            const auto hello = NetEncodeHello(mPlayerName);
            sendBytes(conn, NetEncodeFrame(NetMessageType::Hello, hello));
            return;
        }

        std::lock_guard lock(mMutex);
        mPeers.clear();
        if(mRole == NetRole::Client)
        {
            mRole        = NetRole::Offline;
            mLocalPeerId = 0;
            mLastError   = "Disconnected from host";
        }
    }

    void Network::handleHello(const trantor::TcpConnectionPtr &conn,
                              std::span<const std::uint8_t> payload)
    {
        std::uint16_t version = 0;
        std::string name;
        if(!NetDecodeHello(payload, version, name) || version != kNetProtocolVersion)
        {
            conn->forceClose();
            return;
        }

        std::uint32_t peerId = 0;
        std::uint32_t tick   = 0;
        {
            std::lock_guard lock(mMutex);
            peerId = mNextPeerId++;
            tick   = mTick;
        }
        bindConnection(conn, peerId);
        const auto welcome = NetEncodeWelcome(peerId, tick);
        sendBytes(conn, NetEncodeFrame(NetMessageType::Welcome, welcome));
        if(mLogger)
        {
            mLogger->LogInformation("Welcomed peer {} ({})", peerId, name);
        }
    }

    void Network::onMessage(const trantor::TcpConnectionPtr &conn, std::vector<std::uint8_t> chunk)
    {
        auto state = conn->getContext<ConnectionState>();
        if(!state)
        {
            return;
        }
        state->recv.insert(state->recv.end(), chunk.begin(), chunk.end());

        for(;;)
        {
            NetFrame frame;
            const auto status = NetTryExtractFrame(state->recv, frame);
            if(status == NetExtractStatus::NeedMore)
            {
                break;
            }
            if(status == NetExtractStatus::Invalid)
            {
                conn->forceClose();
                return;
            }

            if(frame.type == NetMessageType::Hello)
            {
                handleHello(conn, frame.payload);
                continue;
            }
            if(frame.type == NetMessageType::Welcome)
            {
                std::uint32_t peerId = 0;
                std::uint32_t tick   = 0;
                std::uint16_t version = 0;
                if(!NetDecodeWelcome(frame.payload, peerId, tick, version) ||
                   version != kNetProtocolVersion)
                {
                    conn->forceClose();
                    return;
                }
                bindConnection(conn, kNetHostPeerId);
                std::lock_guard lock(mMutex);
                mLocalPeerId = peerId;
                mTick        = tick;
                continue;
            }
            if(frame.type == NetMessageType::Ping)
            {
                sendBytes(conn, NetEncodeFrame(NetMessageType::Pong, {}));
                continue;
            }
            postIncoming(state->peerId, frame);
        }
    }

    void Network::dispatchNamed(bool isCommand, std::uint32_t peerId,
                                std::span<const std::uint8_t> payload)
    {
        std::uint32_t nameHash = 0;
        std::span<const std::uint8_t> inner;
        if(!NetDecodeNamed(payload, nameHash, inner))
        {
            return;
        }

        CommandHandler command;
        EventHandler event;
        {
            std::lock_guard lock(mMutex);
            if(isCommand)
            {
                const auto it = mCommandHandlers.find(nameHash);
                if(it != mCommandHandlers.end())
                {
                    command = it->second;
                }
            }
            else
            {
                const auto it = mEventHandlers.find(nameHash);
                if(it != mEventHandlers.end())
                {
                    event = it->second;
                }
            }
        }
        if(command)
        {
            command(peerId, inner);
        }
        if(event)
        {
            event(inner);
        }
    }

    void Network::applySnapshot(fr::Registry &registry, std::span<const std::uint8_t> payload)
    {
        std::uint32_t tick = 0;
        std::vector<NetSnapshotEntity> entities;
        if(!NetDecodeSnapshot(payload, tick, entities))
        {
            return;
        }
        {
            std::lock_guard lock(mMutex);
            mTick = tick;
        }

        for(const auto &snap : entities)
        {
            fr::Entity entity {};
            bool exists = false;
            {
                std::lock_guard lock(mMutex);
                const auto it = mEntitiesByNetId.find(snap.netId);
                if(it != mEntitiesByNetId.end())
                {
                    entity = it->second;
                    exists = true;
                }
            }

            if(!exists)
            {
                entity = registry.CreateEntity(
                    NetworkIdentity {.netId = snap.netId},
                    NetworkTransform {},
                    TransformComponent {
                        .position = {snap.px, snap.py, snap.pz},
                        .scale    = {1.0f, 1.0f, 1.0f},
                        .rotation = {snap.qw, snap.qx, snap.qy, snap.qz},
                    });
                std::lock_guard lock(mMutex);
                mEntitiesByNetId[snap.netId] = entity;
                continue;
            }

            registry.TryGetComponents<TransformComponent>(
                entity, [&](TransformComponent &transform) {
                    transform.position = {snap.px, snap.py, snap.pz};
                    transform.rotation = {snap.qw, snap.qx, snap.qy, snap.qz};
                });
        }
    }

    void Network::Receive(fr::Registry &registry)
    {
        std::vector<Incoming> batch;
        {
            std::lock_guard lock(mMutex);
            batch.swap(mInbox);
        }
        for(const auto &msg : batch)
        {
            switch(msg.type)
            {
            case NetMessageType::Command:
                if(IsAuthority())
                {
                    dispatchNamed(true, msg.peerId, msg.payload);
                }
                break;
            case NetMessageType::Event:
                if(!IsAuthority())
                {
                    dispatchNamed(false, msg.peerId, msg.payload);
                }
                break;
            case NetMessageType::Snapshot:
                if(!IsAuthority())
                {
                    applySnapshot(registry, msg.payload);
                }
                break;
            default:
                break;
            }
        }
    }

    void Network::Send(fr::Registry &registry)
    {
        if(!IsAuthority())
        {
            return;
        }

        std::vector<NetSnapshotEntity> entities;
        std::unordered_map<std::uint32_t, fr::Entity> live;
        registry.CreateMutation()->Each<NetworkIdentity, NetworkTransform, TransformComponent>(
            [&](fr::Entity entity, NetworkIdentity &identity, NetworkTransform &,
                TransformComponent &transform) {
                if(identity.netId == 0)
                {
                    identity.netId = mNextNetId++;
                }
                live[identity.netId] = entity;
                entities.push_back(NetSnapshotEntity {
                    .netId = identity.netId,
                    .px    = transform.position.x,
                    .py    = transform.position.y,
                    .pz    = transform.position.z,
                    .qx    = transform.rotation.x,
                    .qy    = transform.rotation.y,
                    .qz    = transform.rotation.z,
                    .qw    = transform.rotation.w,
                });
            });
        {
            std::lock_guard lock(mMutex);
            mEntitiesByNetId = std::move(live);
        }

        {
            std::lock_guard lock(mMutex);
            ++mTick;
        }
        if(entities.empty() && PeerCount() == 0)
        {
            return;
        }
        const auto payload = NetEncodeSnapshot(Tick(), entities);
        broadcast(NetMessageType::Snapshot, payload);
    }

} // namespace FRIGGA_NAMESPACE
