#include "EcsWorkflow.hpp"

#include "Editor/DockLayout.hpp"

#include <imgui_internal.h>

void EcsWorkflow::buildDefaultDockLayout(ImGuiID dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID mainId   = dockspaceId;
    ImGuiID leftId   = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.18f, nullptr, &mainId);
    ImGuiID rightId  = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.22f, nullptr, &mainId);
    ImGuiID bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.28f, nullptr, &mainId);

    const auto hierarchy  = EditorDock::WindowId("Hierarchy");
    const auto components = EditorDock::WindowId("Components");
    const auto archetypes = EditorDock::WindowId("Archetypes");
    const auto resources  = EditorDock::WindowId("Resources");

    ImGui::DockBuilderDockWindow(hierarchy.c_str(), leftId);
    ImGui::DockBuilderDockWindow(components.c_str(), rightId);
    ImGui::DockBuilderDockWindow(archetypes.c_str(), mainId);
    ImGui::DockBuilderDockWindow(resources.c_str(), bottomId);

    ImGui::DockBuilderFinish(dockspaceId);
}
