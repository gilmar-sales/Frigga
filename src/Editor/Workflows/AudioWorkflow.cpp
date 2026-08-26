#include "AudioWorkflow.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Panels/AudioAssetsLayer.hpp"
#include "Editor/Panels/AudioInspectorLayer.hpp"
#include "Editor/Panels/AudioMixerLayer.hpp"
#include "Editor/Panels/AudioWaveformLayer.hpp"

#include <imgui_internal.h>

AudioWorkflow::AudioWorkflow(skr::Arc<HierarchyLayer> hierarchy,
                             skr::Arc<fg::AssetRegistry> assets,
                             skr::Arc<SelectionContext> selection, skr::Arc<fr::Registry> registry,
                             skr::Arc<fg::SceneSimulationState> simulation,
                             skr::Arc<fg::IAudioEngine> audioEngine,
                             skr::Arc<fg::AudioController> controller,
                             skr::Arc<fra::Window> window)
    : Workflow("Audio",
               {
                   hierarchy,
                   skr::MakeArc<AudioMixerLayer>(audioEngine),
                   skr::MakeArc<AudioWaveformLayer>(assets, audioEngine, controller),
                   skr::MakeArc<AudioAssetsLayer>(assets, selection, registry, simulation, window),
                   skr::MakeArc<AudioInspectorLayer>(assets, selection, registry, simulation,
                                                     controller),
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
