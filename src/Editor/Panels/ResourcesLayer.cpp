#include "ResourcesLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <imgui.h>

void ResourcesLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    ImGui::Begin(title.c_str());
    ImGui::End();
}
