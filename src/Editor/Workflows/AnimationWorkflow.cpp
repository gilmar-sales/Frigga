#include "AnimationWorkflow.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Panels/AnimationClipsLayer.hpp"
#include "Editor/Panels/AnimationPreviewLayer.hpp"
#include "Editor/Panels/AnimationTimelineLayer.hpp"
#include "Editor/Panels/AnimatorPanelLayer.hpp"
#include "Editor/Panels/PlaceholderLayer.hpp"

#include <imgui_internal.h>

AnimationWorkflow::AnimationWorkflow(skr::Arc<HierarchyLayer> hierarchy,
                                     skr::Arc<fg::AssetRegistry> assets,
                                     skr::Arc<SelectionContext> selection,
                                     skr::Arc<fr::Registry> registry,
                                     skr::Arc<fg::SceneSimulationState> simulation,
                                     skr::Arc<fra::Renderer> renderer,
                                     skr::Arc<fra::MeshPool> meshPool, skr::Arc<fg::Scene> scene,
                                     skr::Arc<EditorPreferences> preferences)
    : Workflow("Animation",
               {
                   hierarchy,
                   skr::MakeArc<AnimationPreviewLayer>(renderer, registry, meshPool, selection,
                                                       scene, preferences),
                   skr::MakeArc<AnimationTimelineLayer>(assets, selection, registry),
                   skr::MakeArc<AnimatorPanelLayer>(assets, selection, registry, simulation),
                   skr::MakeArc<PlaceholderLayer>(
                       "Curve Editor", "Edit animation curves, keys, and tangents."),
                   skr::MakeArc<AnimationClipsLayer>(assets, selection, registry, simulation),
               })
{
}

void AnimationWorkflow::buildDefaultDockLayout(ImGuiID dockspaceId)
{
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID mainId   = dockspaceId;
    ImGuiID leftId   = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.16f, nullptr, &mainId);
    ImGuiID rightId  = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.22f, nullptr, &mainId);
    ImGuiID bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.28f, nullptr, &mainId);

    const auto hierarchy  = EditorDock::WindowId("Hierarchy");
    const auto components = EditorDock::WindowId("Components");
    const auto preview    = EditorDock::WindowId("Preview");
    const auto timeline   = EditorDock::WindowId("Timeline");
    const auto animator   = EditorDock::WindowId("Animator");
    const auto curves     = EditorDock::WindowId("Curve Editor");
    const auto clips      = EditorDock::WindowId("Animation Clips");

    ImGui::DockBuilderDockWindow(hierarchy.c_str(), leftId);
    ImGui::DockBuilderDockWindow(components.c_str(), rightId);
    ImGui::DockBuilderDockWindow(animator.c_str(), rightId);
    ImGui::DockBuilderDockWindow(preview.c_str(), mainId);
    ImGui::DockBuilderDockWindow(timeline.c_str(), bottomId);
    ImGui::DockBuilderDockWindow(clips.c_str(), bottomId);
    ImGui::DockBuilderDockWindow(curves.c_str(), bottomId);

    ImGui::DockBuilderFinish(dockspaceId);
}
