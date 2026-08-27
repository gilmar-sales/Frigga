#include "ShadingWorkflow.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Panels/MaterialPropertiesLayer.hpp"
#include "Editor/Panels/MaterialsLayer.hpp"
#include "Editor/Panels/PlaceholderLayer.hpp"
#include "Editor/Panels/ShadingPreviewLayer.hpp"

#include <imgui_internal.h>

ShadingWorkflow::ShadingWorkflow(skr::Arc<fg::AssetRegistry> assets,
                                 skr::Arc<fg::PrimitiveMeshFactory> primitives,
                                 skr::Arc<MaterialSelectionContext> materialSelection,
                                 skr::Arc<fra::Renderer> renderer,
                                 skr::Arc<fra::MeshPool> meshPool, skr::Arc<fr::Registry> registry,
                                 skr::Arc<fg::Scene> scene, skr::Arc<EditorPreferences> preferences,
                                 skr::Arc<fg::SceneSimulationState> simulation,
                                 skr::Arc<fra::Window> window)
    : Workflow("Shading",
               {
                   skr::MakeArc<MaterialsLayer>(assets, primitives, materialSelection, simulation),
                   skr::MakeArc<PlaceholderLayer>(
                       "Shader Graph", "Node graph for authoring shaders."),
                   skr::MakeArc<MaterialPropertiesLayer>(assets, primitives, materialSelection,
                                                         simulation, window),
                   skr::MakeArc<ShadingPreviewLayer>(renderer, registry, meshPool, primitives,
                                                     materialSelection, scene, preferences,
                                                     simulation),
               })
{
}

void ShadingWorkflow::buildDefaultDockLayout(ImGuiID dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID mainId   = dockspaceId;
    ImGuiID leftId   = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.18f, nullptr, &mainId);
    ImGuiID rightId  = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.24f, nullptr, &mainId);
    ImGuiID bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.32f, nullptr, &mainId);

    const auto materials  = EditorDock::WindowId("Materials");
    const auto graph      = EditorDock::WindowId("Shader Graph");
    const auto properties = EditorDock::WindowId("Material Properties");
    const auto preview    = EditorDock::WindowId("Preview");

    ImGui::DockBuilderDockWindow(materials.c_str(), leftId);
    ImGui::DockBuilderDockWindow(graph.c_str(), mainId);
    ImGui::DockBuilderDockWindow(properties.c_str(), rightId);
    ImGui::DockBuilderDockWindow(preview.c_str(), bottomId);

    ImGui::DockBuilderFinish(dockspaceId);
}
