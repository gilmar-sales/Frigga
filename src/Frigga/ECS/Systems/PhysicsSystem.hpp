#pragma once

#include "Frigga/Physics/IPhysicsWorld.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class PhysicsSystem: public fr::System
    {
      public:
        PhysicsSystem(const skr::Arc<fr::Registry> &registry,
                      const skr::Arc<IPhysicsWorld> &physicsWorld,
                      const skr::Arc<SceneSimulationState> &simulation);
        ~PhysicsSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<IPhysicsWorld> mPhysicsWorld;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
