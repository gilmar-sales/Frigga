#include <Frigga/Network/NetworkController.hpp>

#include <Frigga/Network/NetworkComponents.hpp>

namespace FRIGGA_NAMESPACE
{

    NetworkController::NetworkController(const skr::Arc<fr::Registry> &registry,
                                         const skr::Arc<NetworkServer> &server,
                                         const skr::Arc<NetworkClient> &client)
        : mRegistry(registry), mServer(server), mClient(client)
    {
    }

    bool NetworkController::Host(std::uint16_t port, std::size_t maxClients)
    {
        if(mClient && mClient->IsConnected())
        {
            mClient->Disconnect();
        }
        return mServer && mServer->Start(port, maxClients);
    }

    void NetworkController::StopHost()
    {
        if(mServer)
        {
            mServer->Stop();
        }
    }

    bool NetworkController::IsHosting() const
    {
        return mServer && mServer->IsHosting();
    }

    bool NetworkController::Join(const std::string &host, std::uint16_t port)
    {
        if(mServer && mServer->IsHosting())
        {
            mServer->Stop();
        }
        return mClient && mClient->Connect(host, port);
    }

    void NetworkController::Leave()
    {
        if(mClient)
        {
            mClient->Disconnect();
        }
    }

    bool NetworkController::IsConnected() const
    {
        return mClient && mClient->IsConnected();
    }

    std::uint32_t NetworkController::Replicate(fr::Entity entity, std::uint32_t ownerClientId)
    {
        if(!mRegistry || !mRegistry->IsAlive(entity))
        {
            return kInvalidNetId;
        }
        if(IsHosting() && mServer)
        {
            return mServer->RegisterEntity(*mRegistry, entity, ownerClientId);
        }
        // Client-side: tag only. The server assigns the authoritative netId
        // on Spawn; owned entities additionally get NetworkOwner for
        // prediction exclusion during interpolation.
        if(!mRegistry->HasComponent<NetworkIdentity>(entity))
        {
            mRegistry->AddComponent(entity, NetworkIdentity {.ownerClientId = ownerClientId});
        }
        if(!mRegistry->HasComponent<Replicated>(entity))
        {
            mRegistry->AddComponent(entity, Replicated {});
        }
        std::uint32_t netId = kInvalidNetId;
        mRegistry->TryGetComponents<NetworkIdentity>(
            entity, [&](const NetworkIdentity &identity) { netId = identity.netId; });
        return netId;
    }

    void NetworkController::Unreplicate(fr::Entity entity)
    {
        if(!mRegistry || !mRegistry->IsAlive(entity))
        {
            return;
        }
        if(IsHosting() && mServer)
        {
            mServer->UnregisterEntity(*mRegistry, entity);
        }
        if(mRegistry->HasComponent<Replicated>(entity))
        {
            mRegistry->RemoveComponent<Replicated>(entity);
        }
    }

    void NetworkController::SendInput(std::uint32_t netId, const std::vector<std::uint8_t> &payload)
    {
        if(mClient)
        {
            mClient->SendInput(netId, payload);
        }
    }

    void NetworkController::SendRpc(std::uint32_t netId, std::uint32_t rpcId,
                                    const std::vector<std::uint8_t> &payload)
    {
        if(IsHosting() && mServer)
        {
            mServer->BroadcastRpc(netId, rpcId, payload);
        }
        else if(mClient)
        {
            mClient->SendRpc(netId, rpcId, payload);
        }
    }

} // namespace FRIGGA_NAMESPACE
