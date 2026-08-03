#include "ArchetypesLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <imgui.h>

ArchetypesLayer::ArchetypesLayer(skr::Arc<fr::Registry> registry): mRegistry(std::move(registry)) {}

void ArchetypesLayer::onGui()
{
    const auto title = EditorDock::WindowId("Archetypes");
    ImGui::Begin(title.c_str());

    ImGui::End();
}
