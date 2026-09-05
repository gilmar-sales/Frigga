#pragma once

#include "BoostrapIconsFont.hpp"
#include "UiScale.hpp"

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

#include <cstdio>
class EditorApplication final: public fg::AbstractApplication
{
  public:
    EditorApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider)
        : AbstractApplication(serviceProvider),
          mRegistry(serviceProvider->GetService<fr::Registry>()),
          mSystemManager(serviceProvider->GetService<fr::SystemManager>()),
          mSimulation(serviceProvider->GetService<fg::SceneSimulationState>()),
          mInput(serviceProvider->GetService<fg::Input>()),
          mMcp(serviceProvider->GetService<ProjectSession>(), serviceProvider->GetService<fg::Scene>(),
               mSimulation, serviceProvider->GetService<skr::Logger<EditorMcpService>>())
    {
        PushLayer(mScope->GetServiceProvider()->GetService<HomeLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<MainLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<PreferencesLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<InputMapLayer>());

        EditorUiScale::Sync(mWindow->GetScale());

        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("Resources/Fonts/OpenSans.ttf", 18);

        static const ImWchar icons_ranges[] = {ICON_MIN_BTSP, ICON_MAX_BTSP, 0};
        ImFontConfig icons_config;
        icons_config.MergeMode  = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("Resources/Fonts/BootstrapIconsFont.ttf", 16,
                                     &icons_config, icons_ranges);

        // Edit mode at startup: only Render (animation preview + draw) ticks.
        syncPlayPipelines();
        std::string mcpError;
        if(!mMcp.Start(mcpError))
        {
            std::fprintf(stderr, "Unable to start Editor MCP service: %s\n", mcpError.c_str());
        }
    }

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
