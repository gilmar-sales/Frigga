#include <Frigga/ECS/Systems/NetworkSystems.hpp>

namespace FRIGGA_NAMESPACE
{

    NetworkServerSystem::NetworkServerSystem(const skr::Arc<fr::Registry> &registry,
                                             const skr::Arc<NetworkServer> &server,
                                             const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mServer(server), mSimulation(simulation)
    {
    }

    void NetworkServerSystem::Update(float deltaTime)
    {
        if(!mServer || !mServer->IsHosting())
        {
            return;
        }
        if(mSimulation && !mSimulation->IsPlaying())
        {
            // Keep the socket pump alive in Edit so connects are visible,
            // but never advance the simulation tick.
            mServer->Poll();
            return;
        }
        mServer->Poll();
        if(mServer->ShouldSendSnapshot(deltaTime))
        {
            mServer->BroadcastSnapshot(*mRegistry);
        }
    }

    NetworkClientSystem::NetworkClientSystem(const skr::Arc<fr::Registry> &registry,
                                             const skr::Arc<NetworkClient> &client,
                                             const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mClient(client), mSimulation(simulation)
    {
    }

    void NetworkClientSystem::Update(float deltaTime)
    {
        if(!mClient)
        {
            return;
        }
        mClock += deltaTime;
        mClient->Poll(*mRegistry);
        if(mSimulation && !mSimulation->IsPlaying())
        {
            return;
        }
        mClient->ApplySnapshots(*mRegistry, mClock);
    }

} // namespace FRIGGA_NAMESPACE
