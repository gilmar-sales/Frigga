#include "EditorApp.hpp"

#include "BootstrapIconsFont.h"
#include "MainLayer.hpp"
#include "Panels/Hierarchy.hpp"
#include "Panels/Preferences.hpp"
#include "Viewports/Game.hpp"
#include "Viewports/Scene.hpp"
#include "Workflows/ECS.hpp"
#include "Workflows/GamePlay.hpp"

EditorApp::EditorApp(): fg::Application("Frigga Editor", 1280, 720)
{
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("Resources/OpenSans.ttf", 16);

    static const ImWchar icons_ranges[] = {ICON_MIN_BTSP, ICON_MAX_BTSP, 0};
    ImFontConfig icons_config;
    icons_config.MergeMode  = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("Resources/BootstrapIconsFont.ttf", 16, &icons_config, icons_ranges);

    pushLayer(new MainLayer(m_scene));
    pushLayer(new PreferencesLayer());
};

fg::Application *fg::CreateApplication()
{
    return new EditorApp();
}
