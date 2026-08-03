#include "AudioWorkflow.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Panels/PlaceholderLayer.hpp"

#include <imgui_internal.h>

AudioWorkflow::AudioWorkflow(skr::Arc<HierarchyLayer> hierarchy)
    : Workflow("Audio",
               {
                   hierarchy,
                   skr::MakeArc<PlaceholderLayer>(
                       "Mixer", "Mix buses, groups, and master levels."),
                   skr::MakeArc<PlaceholderLayer>(
                       "Waveform", "Preview and trim audio waveforms."),
                   skr::MakeArc<PlaceholderLayer>(
                       "Audio Assets", "Browse clips, music, and sound banks."),
                   skr::MakeArc<PlaceholderLayer>(
                       "Audio Inspector", "Source parameters, attenuation, and spatialize."),
               })
{
}

void AudioWorkflow::buildDefaultDockLayout(ImGuiID dockspaceId)
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
    const auto mixer      = EditorDock::WindowId("Mixer");
    const auto waveform   = EditorDock::WindowId("Waveform");
    const auto assets     = EditorDock::WindowId("Audio Assets");
    const auto inspector  = EditorDock::WindowId("Audio Inspector");

    ImGui::DockBuilderDockWindow(hierarchy.c_str(), leftId);
    ImGui::DockBuilderDockWindow(components.c_str(), rightId);
    ImGui::DockBuilderDockWindow(inspector.c_str(), rightId);
    ImGui::DockBuilderDockWindow(mixer.c_str(), mainId);
    ImGui::DockBuilderDockWindow(waveform.c_str(), bottomId);
    ImGui::DockBuilderDockWindow(assets.c_str(), bottomId);

    ImGui::DockBuilderFinish(dockspaceId);
}
