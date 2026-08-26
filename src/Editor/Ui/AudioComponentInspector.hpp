#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>
#include <imgui.h>

namespace EditorAudioUi
{
    inline void DrawListenerFields(fg::AudioListenerComponent &listener, bool editingLocked)
    {
        ImGui::BeginDisabled(editingLocked);
        ImGui::Checkbox("Active", &listener.active);
        ImGui::EndDisabled();
    }

    inline void DrawSourceFields(fg::AudioSourceComponent &source,
                                 const skr::Arc<fg::AssetRegistry> &assets,
                                 const skr::Arc<fg::AudioController> &controller, fr::Entity entity,
                                 bool editingLocked, bool showTransport)
    {
        ImGui::BeginDisabled(editingLocked);

        const auto events = assets->GetAllEventPaths();
        if(ImGui::BeginCombo("Event", source.eventPath.empty() ? "(select event)"
                                                               : source.eventPath.c_str()))
        {
            for(const auto &eventPath : events)
            {
                const bool selected = source.eventPath == eventPath;
                if(ImGui::Selectable(eventPath.c_str(), selected))
                {
                    source.eventPath = eventPath;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::DragFloat("Volume", &source.volume, 0.01f, 0.0f, 2.0f, "%.2f");
        ImGui::DragFloat("Pitch", &source.pitch, 0.01f, 0.1f, 4.0f, "%.2f");
        ImGui::Checkbox("Play On Awake", &source.playOnAwake);
        ImGui::Checkbox("Loop", &source.loop);
        ImGui::Checkbox("3D", &source.is3D);
        if(source.is3D)
        {
            ImGui::DragFloat("Min Distance", &source.minDistance, 0.1f, 0.1f, 100.0f, "%.1f");
            ImGui::DragFloat("Max Distance", &source.maxDistance, 0.5f, 1.0f, 500.0f, "%.1f");
        }

        ImGui::EndDisabled();

        if(!showTransport || source.eventPath.empty() || !controller)
        {
            return;
        }

        if(ImGui::Button("Preview"))
        {
            (void)controller->PreviewEvent(source.eventPath, source.volume);
        }
        ImGui::SameLine();
        if(ImGui::Button("Play"))
        {
            (void)controller->Play(entity, source.eventPath);
        }
        ImGui::SameLine();
        if(ImGui::Button("Stop"))
        {
            controller->Stop(entity);
            controller->StopPreview();
        }
    }
} // namespace EditorAudioUi
