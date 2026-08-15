#pragma once

#include "Frigga/Input/Input.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    class ThirdPersonCameraSystem: public fr::System
    {
      public:
        ThirdPersonCameraSystem(const skr::Arc<fr::Registry> &registry,
                                const skr::Arc<Input> &input,
                                const skr::Arc<SceneSimulationState> &simulation);
        ~ThirdPersonCameraSystem() override = default;

        void Update(float deltaTime) override;

      private:
        skr::Arc<Input> mInput;
        skr::Arc<SceneSimulationState> mSimulation;
    };

} // namespace FRIGGA_NAMESPACE
