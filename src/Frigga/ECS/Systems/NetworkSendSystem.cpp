#include "NetworkSendSystem.hpp"

namespace FRIGGA_NAMESPACE
{

    NetworkSendSystem::NetworkSendSystem(const skr::Arc<fr::Registry> &registry,
                                         const skr::Arc<Network> &network,
                                         const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mNetwork(network), mSimulation(simulation)
    {
    }

    void NetworkSendSystem::Update(float)
    {
        if(!mNetwork || !mSimulation || !mSimulation->IsPlaying())
        {
            return;
        }
        mNetwork->Send(*mRegistry);
    }

} // namespace FRIGGA_NAMESPACE
