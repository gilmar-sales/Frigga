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

class EditorApplication final: public fg::AbstractApplication
{
  public:
    EditorApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider)
        : AbstractApplication(serviceProvider),
          mRegistry(serviceProvider->GetService<fr::Registry>()),
          mSystemManager(serviceProvider->GetService<fr::SystemManager>()),
          mSimulation(serviceProvider->GetService<fg::SceneSimulationState>()),
          mInput(serviceProvider->GetService<fg::Input>())
    {
        PushLayer(mScope->GetServiceProvider()->GetService<HomeLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<MainLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<PreferencesLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<InputMapLayer>());

        EditorUiScale::Sync(mWindow->GetScale());

        mWindow->AddEventPollCallback([window = mWindow](SDL_Event event) {
            if(EditorUiScale::IsDisplayTopologyEvent(event))
            {
                EditorUiScale::Sync(window->GetScale());
            }
        });
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF("Resources/OpenSans.ttf", 18);

        static const ImWchar icons_ranges[] = {ICON_MIN_BTSP, ICON_MAX_BTSP, 0};
        ImFontConfig icons_config;
        icons_config.MergeMode  = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("Resources/BootstrapIconsFont.ttf", 16, &icons_config,
                                     icons_ranges);

        // Edit mode at startup: do not tick physics / gameplay until Play.
        syncSimulationPipeline();
    }

  protected:
    void RenderScene() override;
    void Update() override;

  private:
    void syncSimulationPipeline();

    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fr::SystemManager> mSystemManager;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fg::Input> mInput;
};
