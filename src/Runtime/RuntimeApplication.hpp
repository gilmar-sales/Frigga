#pragma once

#include "RuntimeProject.hpp"

#include <Frigga/Core/AbstractApplication.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Module/GameplayModuleHost.hpp>
#include <Frigga/Scene/Scene.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include <Freyr/Core/SystemManager.hpp>

class RuntimeApplication final: public fg::AbstractApplication
{
  public:
    RuntimeApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider);

  protected:
    void RenderScene() override;
    [[nodiscard]] bool ShouldBootstrapViewportFallback() const override
    {
        // The standalone game presents directly to the swapchain. The editor
        // is the only consumer that needs an offscreen viewport target.
        return false;
    }

  private:
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fr::SystemManager> mSystemManager;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<fg::Input> mInput;
    skr::Arc<fg::SceneSimulationState> mSimulation;
};
