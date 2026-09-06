#pragma once

#include <Freyr/Freyr.hpp>
#include <Frigga/Frigga.hpp>
#include <Frigga/Input/Input.hpp>
#include <Frigga/Scene/SceneSimulationState.hpp>

#include "HomeLayer.hpp"
#include "MainLayer.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/InputMapLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"
#include "Mcp/EditorMcpService.hpp"
#include "Project/ProjectSession.hpp"

class EditorApplication final: public fg::AbstractApplication
{
  public:
    EditorApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider);

  protected:
    void RenderScene() override;
    void Update() override;
    void OnAfterGuiLayout() override;
    [[nodiscard]] bool ShouldBootstrapViewportFallback() const override;

  private:
    void syncPlayPipelines();

    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fr::SystemManager> mSystemManager;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fg::Input> mInput;
    EditorMcpService mMcp;
};
