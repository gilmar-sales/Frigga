#pragma once

#include "Editor/Panels/ResourcesLayer.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freyr/Freyr.hpp>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace EditorAudioUi
{
    inline void DrawListenerFields(fg::AudioListenerComponent &listener, bool editingLocked)
    {
        ImGui::BeginDisabled(editingLocked);
        ImGui::Checkbox("Active", &listener.active);
        ImGui::EndDisabled();
    }

    inline std::string NormalizeAudioRef(std::string_view path)
    {
        std::string out {path};
        std::ranges::replace(out, '\\', '/');
        return out;
    }

    inline void DrawSourceFields(fg::AudioSourceComponent &source,
                                 const skr::Arc<fg::AssetRegistry> &assets,
                                 const skr::Arc<fg::AudioController> &controller, fr::Entity entity,
                                 bool editingLocked, bool showTransport)
    {
        ImGui::BeginDisabled(editingLocked);

        const auto events = assets->GetAllEventPaths();
        const auto &clips = assets->GetAudioClips();

        const char *preview = "(select event or clip)";
        if(!source.eventPath.empty())
        {
            preview = source.eventPath.c_str();
        }

        if(ImGui::BeginCombo("Event / Clip", preview))
        {
            if(!events.empty())
            {
                ImGui::SeparatorText("Bank events");
                for(const auto &eventPath : events)
                {
                    const bool selected = source.eventPath == eventPath;
                    if(ImGui::Selectable(eventPath.c_str(), selected))
                    {
                        source.eventPath = eventPath;
                    }
                }
            }

            if(!clips.empty())
            {
                ImGui::SeparatorText("Audio clips");
                for(const auto &clip : clips)
                {
                    const auto path = NormalizeAudioRef(clip.relativePath);
                    const bool selected = source.eventPath == path;
                    if(ImGui::Selectable(path.c_str(), selected))
                    {
                        source.eventPath = path;
                    }
                }
            }

            if(events.empty() && clips.empty())
            {
                ImGui::TextDisabled("Import a bank or clip in Resources.");
            }

            ImGui::EndCombo();
        }

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("FRIGGA_AUDIO_EVENT"))
            {
                source.eventPath =
                    NormalizeAudioRef(static_cast<const char *>(payload->Data));
            }
            if(const ImGuiPayload *payload =
                   ImGui::AcceptDragDropPayload(ResourcesLayer::kDragPayloadId))
            {
                const auto *drag =
                    static_cast<const ResourcesLayer::ResourceDragPayload *>(payload->Data);
                if(drag->kind == ResourcesLayer::EntryKind::AudioClip && drag->relativePath[0] != '\0')
                {
                    source.eventPath = NormalizeAudioRef(drag->relativePath);
                    (void)assets->LoadAudioClip(source.eventPath);
                }
            }
            ImGui::EndDragDropTarget();
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
