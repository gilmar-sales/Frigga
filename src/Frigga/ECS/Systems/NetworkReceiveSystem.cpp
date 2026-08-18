#include "NetworkReceiveSystem.hpp"

namespace FRIGGA_NAMESPACE
{

    NetworkReceiveSystem::NetworkReceiveSystem(const skr::Arc<fr::Registry> &registry,
                                               const skr::Arc<Network> &network,
                                               const skr::Arc<SceneSimulationState> &simulation)
        : System(registry), mNetwork(network), mSimulation(simulation)
    {
    }

    void NetworkReceiveSystem::Update(float)
    {
        if(!mNetwork || !mSimulation || !mSimulation->IsPlaying())
        {
            return;
        }
        mNetwork->Receive(*mRegistry);
    }

} // namespace FRIGGA_NAMESPACE
