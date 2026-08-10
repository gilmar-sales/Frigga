#pragma once

#include "BoostrapIconsFont.hpp"

#include <Frigga/Frigga.hpp>

#include "HomeLayer.hpp"
#include "MainLayer.hpp"
#include "Panels/HierarchyLayer.hpp"
#include "Panels/PreferencesLayer.hpp"
#include "Panels/ResourcesLayer.hpp"

class EditorApplication final: public fg::AbstractApplication
{
  public:
    EditorApplication(const skr::Arc<skr::ServiceProvider> &serviceProvider)
        : AbstractApplication(serviceProvider), mRegistry(serviceProvider->GetService<fr::Registry>())
    {
        PushLayer(mScope->GetServiceProvider()->GetService<HomeLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<MainLayer>());
        PushLayer(mScope->GetServiceProvider()->GetService<PreferencesLayer>());

        ImGuiIO &io        = ImGui::GetIO();
        io.FontGlobalScale = mWindow->GetScale();

        mWindow->AddEventPollCallback([window = mWindow](SDL_Event event) {
            if(event.window.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
            {
                ImGuiIO &io        = ImGui::GetIO();
                io.FontGlobalScale = window->GetScale();
            }
        });
        io.Fonts->AddFontFromFileTTF("Resources/OpenSans.ttf", 18);

        static const ImWchar icons_ranges[] = {ICON_MIN_BTSP, ICON_MAX_BTSP, 0};
        ImFontConfig icons_config;
        icons_config.MergeMode  = true;
        icons_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("Resources/BootstrapIconsFont.ttf", 16, &icons_config,
                                     icons_ranges);
    }

  protected:
    void RenderScene() override;
    void Update() override;

  private:
    skr::Arc<fr::Registry> mRegistry;
};
