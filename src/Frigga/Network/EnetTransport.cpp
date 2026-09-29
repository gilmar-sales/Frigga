#include <Frigga/Network/EnetTransport.hpp>

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <enet/enet.h>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        std::atomic<int> gEnetRefCount {0};
        std::mutex gEnetMutex;

        constexpr enet_uint32 kReliableFlag = ENET_PACKET_FLAG_RELIABLE;
        constexpr enet_uint32 kStateFlag    = ENET_PACKET_FLAG_UNSEQUENCED;

        ENetPacket *MakePacket(const std::vector<std::uint8_t> &bytes, enet_uint32 flags,
                               std::uint8_t channel)
        {
            (void)channel;
            return enet_packet_create(bytes.data(), bytes.size(), flags);
        }

        std::vector<std::uint8_t> PacketBytes(const ENetPacket *packet)
        {
            return std::vector<std::uint8_t>(packet->data, packet->data + packet->dataLength);
        }
    } // namespace

    bool EnetRuntime::EnsureInitialized()
    {
        std::lock_guard lock(gEnetMutex);
        if(gEnetRefCount++ == 0)
        {
            if(enet_initialize() != 0)
            {
                --gEnetRefCount;
                return false;
            }
        }
        return true;
    }

    void EnetRuntime::Release()
    {
        std::lock_guard lock(gEnetMutex);
        if(gEnetRefCount > 0 && --gEnetRefCount == 0)
        {
            enet_deinitialize();
        }
    }

    struct EnetServerTransport::Impl
    {
        ENetHost *host = nullptr;
        std::uint16_t port = 0;
        std::unordered_map<ENetPeer *, NetClientId> peerToClient;
        std::unordered_map<NetClientId, ENetPeer *> clientToPeer;
        NetClientId nextClientId = 1;
        ConnectCallback onConnect;
        DisconnectCallback onDisconnect;
        ReceiveCallback onReceive;
        bool initialized = false;
    };

    EnetServerTransport::EnetServerTransport(): mImpl(std::make_unique<Impl>())
    {
    }

    EnetServerTransport::~EnetServerTransport()
    {
        Stop();
    }

    bool EnetServerTransport::Start(std::uint16_t port, std::size_t maxClients)
    {
        if(mImpl->host != nullptr)
        {
            return true;
        }
        if(!EnetRuntime::EnsureInitialized())
        {
            return false;
        }
        mImpl->initialized = true;

        ENetAddress address {};
        address.host = ENET_HOST_ANY;
        address.port = port;

        const std::size_t clients = maxClients == 0 ? kMaxClients : maxClients;
        mImpl->host =
            enet_host_create(&address, clients, kNetChannelCount, 0, 0);
        if(mImpl->host == nullptr)
        {
            EnetRuntime::Release();
            mImpl->initialized = false;
            return false;
        }
        mImpl->port = port;
        return true;
    }

    void EnetServerTransport::Stop()
    {
        if(mImpl->host != nullptr)
        {
            enet_host_destroy(mImpl->host);
            mImpl->host = nullptr;
        }
        if(mImpl->initialized)
        {
            EnetRuntime::Release();
            mImpl->initialized = false;
        }
        mImpl->peerToClient.clear();
        mImpl->clientToPeer.clear();
    }

    bool EnetServerTransport::IsRunning() const
    {
        return mImpl->host != nullptr;
    }

    std::uint16_t EnetServerTransport::GetPort() const
    {
        return mImpl->port;
    }

    void EnetServerTransport::Poll()
    {
        if(mImpl->host == nullptr)
        {
            return;
        }
        ENetEvent event {};
        while(enet_host_service(mImpl->host, &event, 0) > 0)
        {
            switch(event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    const NetClientId id = mImpl->nextClientId++;
                    mImpl->peerToClient[event.peer] = id;
                    mImpl->clientToPeer[id]         = event.peer;
                    event.peer->data = reinterpret_cast<void *>(static_cast<std::uintptr_t>(id));
                    if(mImpl->onConnect)
                    {
                        mImpl->onConnect(id);
                    }
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    auto it = mImpl->peerToClient.find(event.peer);
                    if(it != mImpl->peerToClient.end())
                    {
                        const NetClientId id = it->second;
                        mImpl->peerToClient.erase(it);
                        mImpl->clientToPeer.erase(id);
                        event.peer->data = nullptr;
                        if(mImpl->onDisconnect)
                        {
                            mImpl->onDisconnect(id);
                        }
                    }
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    auto it = mImpl->peerToClient.find(event.peer);
                    const NetClientId from =
                        it != mImpl->peerToClient.end() ? it->second : NetClientId {0};
                    if(mImpl->onReceive && event.packet != nullptr)
                    {
                        mImpl->onReceive(
                            NetPacket {.fromClientId = from, .bytes = PacketBytes(event.packet)});
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }

    void EnetServerTransport::SendReliable(NetClientId to, const std::vector<std::uint8_t> &bytes)
    {
        if(mImpl->host == nullptr || bytes.empty())
        {
            return;
        }
        const auto it = mImpl->clientToPeer.find(to);
        if(it == mImpl->clientToPeer.end())
        {
            return;
        }
        if(ENetPacket *packet = MakePacket(bytes, kReliableFlag, kNetReliableChannel))
        {
            enet_peer_send(it->second, kNetReliableChannel, packet);
        }
    }

    void EnetServerTransport::SendState(NetClientId to, const std::vector<std::uint8_t> &bytes)
    {
        if(mImpl->host == nullptr || bytes.empty())
        {
            return;
        }
        const auto it = mImpl->clientToPeer.find(to);
        if(it == mImpl->clientToPeer.end())
        {
            return;
        }
        if(ENetPacket *packet = MakePacket(bytes, kStateFlag, kNetStateChannel))
        {
            enet_peer_send(it->second, kNetStateChannel, packet);
        }
    }

    void EnetServerTransport::BroadcastReliable(const std::vector<std::uint8_t> &bytes)
    {
        if(mImpl->host == nullptr || bytes.empty())
        {
            return;
        }
        if(ENetPacket *packet = MakePacket(bytes, kReliableFlag, kNetReliableChannel))
        {
            enet_host_broadcast(mImpl->host, kNetReliableChannel, packet);
        }
    }

    void EnetServerTransport::BroadcastState(const std::vector<std::uint8_t> &bytes)
    {
        if(mImpl->host == nullptr || bytes.empty())
        {
            return;
        }
        if(ENetPacket *packet = MakePacket(bytes, kStateFlag, kNetStateChannel))
        {
            enet_host_broadcast(mImpl->host, kNetStateChannel, packet);
        }
    }

    void EnetServerTransport::OnClientConnected(ConnectCallback callback)
    {
        mImpl->onConnect = std::move(callback);
    }

    void EnetServerTransport::OnClientDisconnected(DisconnectCallback callback)
    {
        mImpl->onDisconnect = std::move(callback);
    }

    void EnetServerTransport::OnReceive(ReceiveCallback callback)
    {
        mImpl->onReceive = std::move(callback);
    }

    struct EnetClientTransport::Impl
    {
        ENetHost *host = nullptr;
        ENetPeer *peer = nullptr;
        bool connected = false;
        bool initialized = false;
        ConnectCallback onConnect;
        DisconnectCallback onDisconnect;
        ReceiveCallback onReceive;
    };

    EnetClientTransport::EnetClientTransport(): mImpl(std::make_unique<Impl>())
    {
    }

    EnetClientTransport::~EnetClientTransport()
    {
        Disconnect();
    }

    bool EnetClientTransport::Connect(const std::string &host, std::uint16_t port)
    {
        Disconnect();
        if(!EnetRuntime::EnsureInitialized())
        {
            return false;
        }
        mImpl->initialized = true;

        mImpl->host = enet_host_create(nullptr, 1, kNetChannelCount, 0, 0);
        if(mImpl->host == nullptr)
        {
            EnetRuntime::Release();
            mImpl->initialized = false;
            return false;
        }

        ENetAddress address {};
        if(enet_address_set_host(&address, host.c_str()) != 0)
        {
            enet_host_destroy(mImpl->host);
            mImpl->host = nullptr;
            EnetRuntime::Release();
            mImpl->initialized = false;
            return false;
        }
        address.port = port;

        mImpl->peer = enet_host_connect(mImpl->host, &address, kNetChannelCount, 0);
        if(mImpl->peer == nullptr)
        {
            enet_host_destroy(mImpl->host);
            mImpl->host = nullptr;
            EnetRuntime::Release();
            mImpl->initialized = false;
            return false;
        }
        return true;
    }

    void EnetClientTransport::Disconnect()
    {
        if(mImpl->peer != nullptr)
        {
            enet_peer_disconnect_now(mImpl->peer, 0);
            mImpl->peer = nullptr;
        }
        if(mImpl->host != nullptr)
        {
            enet_host_destroy(mImpl->host);
            mImpl->host = nullptr;
        }
        if(mImpl->initialized)
        {
            EnetRuntime::Release();
            mImpl->initialized = false;
        }
        mImpl->connected = false;
    }

    bool EnetClientTransport::IsConnected() const
    {
        return mImpl->connected && mImpl->peer != nullptr;
    }

    void EnetClientTransport::Poll()
    {
        if(mImpl->host == nullptr)
        {
            return;
        }
        ENetEvent event {};
        while(enet_host_service(mImpl->host, &event, 0) > 0)
        {
            switch(event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                    mImpl->connected = true;
                    if(mImpl->onConnect)
                    {
                        mImpl->onConnect(NetClientId {0});
                    }
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    mImpl->connected = false;
                    mImpl->peer      = nullptr;
                    if(mImpl->onDisconnect)
                    {
                        mImpl->onDisconnect(NetClientId {0});
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    if(mImpl->onReceive && event.packet != nullptr)
                    {
                        mImpl->onReceive(
                            NetPacket {.fromClientId = 0, .bytes = PacketBytes(event.packet)});
                    }
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }

    void EnetClientTransport::SendReliable(NetClientId /*to*/,
                                           const std::vector<std::uint8_t> &bytes)
    {
        if(mImpl->peer == nullptr || bytes.empty())
        {
            return;
        }
        if(ENetPacket *packet = MakePacket(bytes, kReliableFlag, kNetReliableChannel))
        {
            enet_peer_send(mImpl->peer, kNetReliableChannel, packet);
        }
    }

    void EnetClientTransport::SendState(NetClientId /*to*/, const std::vector<std::uint8_t> &bytes)
    {
        if(mImpl->peer == nullptr || bytes.empty())
        {
            return;
        }
        if(ENetPacket *packet = MakePacket(bytes, kStateFlag, kNetStateChannel))
        {
            enet_peer_send(mImpl->peer, kNetStateChannel, packet);
        }
    }

    void EnetClientTransport::BroadcastReliable(const std::vector<std::uint8_t> &bytes)
    {
        SendReliable(0, bytes);
    }

    void EnetClientTransport::BroadcastState(const std::vector<std::uint8_t> &bytes)
    {
        SendState(0, bytes);
    }

    void EnetClientTransport::OnClientConnected(ConnectCallback callback)
    {
        mImpl->onConnect = std::move(callback);
    }

    void EnetClientTransport::OnClientDisconnected(DisconnectCallback callback)
    {
        mImpl->onDisconnect = std::move(callback);
    }

    void EnetClientTransport::OnReceive(ReceiveCallback callback)
    {
        mImpl->onReceive = std::move(callback);
    }

    std::unique_ptr<IServerTransport> MakeEnetServerTransport()
    {
        return std::make_unique<EnetServerTransport>();
    }

    std::unique_ptr<IClientTransport> MakeEnetClientTransport()
    {
        return std::make_unique<EnetClientTransport>();
    }

} // namespace FRIGGA_NAMESPACE
