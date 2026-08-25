#include "AudioInspectorLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"

#include <cstdio>

AudioInspectorLayer::AudioInspectorLayer(skr::Arc<fg::AssetRegistry> assets,
                                         skr::Arc<SelectionContext> selection,
                                         skr::Arc<fr::Registry> registry,
                                         skr::Arc<fg::SceneSimulationState> simulation,
                                         skr::Arc<fg::AudioController> controller)
    : Layer("Audio Inspector"), mAssets(std::move(assets)), mSelection(std::move(selection)),
      mRegistry(std::move(registry)), mSimulation(std::move(simulation)),
      mController(std::move(controller))
{
}

void AudioInspectorLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    const fr::Entity selection = mSelection->Get();
    if(selection == SelectionContext::Invalid)
    {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    const bool hasSource   = mRegistry->HasComponent<fg::AudioSourceComponent>(selection);
    const bool hasListener = mRegistry->HasComponent<fg::AudioListenerComponent>(selection);

    if(!hasSource && !hasListener)
    {
        ImGui::TextWrapped("Selected entity has no audio components.");
        ImGui::BeginDisabled(mSimulation->IsPlaying());
        if(ImGui::Button("Add Audio Source"))
        {
            mRegistry->AddComponents(selection, fg::AudioSourceComponent {});
        }
        if(ImGui::Button("Add Audio Listener"))
        {
            mRegistry->AddComponents(selection, fg::AudioListenerComponent {});
        }
        ImGui::EndDisabled();
        ImGui::End();
        return;
    }

    if(hasListener)
    {
        mRegistry->TryGetComponents<fg::AudioListenerComponent>(
            selection, [&](fg::AudioListenerComponent &listener) {
                ImGui::SeparatorText("Listener");
                ImGui::BeginDisabled(mSimulation->IsPlaying());
                ImGui::Checkbox("Active", &listener.active);
                ImGui::EndDisabled();
            });
    }

    if(hasSource)
    {
        mRegistry->TryGetComponents<fg::AudioSourceComponent>(
            selection, [&](fg::AudioSourceComponent &source) {
                ImGui::SeparatorText("Source");
                ImGui::BeginDisabled(mSimulation->IsPlaying());

                const auto events = mAssets->GetAllEventPaths();
                if(ImGui::BeginCombo(
                       "Event", source.eventPath.empty() ? "(select event)" : source.eventPath.c_str()))
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
                    ImGui::DragFloat("Min Distance", &source.minDistance, 0.1f, 0.1f, 100.0f,
                                     "%.1f");
                    ImGui::DragFloat("Max Distance", &source.maxDistance, 0.5f, 1.0f, 500.0f,
                                     "%.1f");
                }

                ImGui::EndDisabled();

                if(!source.eventPath.empty())
                {
                    if(ImGui::Button("Preview"))
                    {
                        (void)mController->PreviewEvent(source.eventPath, source.volume);
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Play"))
                    {
                        (void)mController->Play(selection, source.eventPath);
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Stop"))
                    {
                        mController->Stop(selection);
                    }
                }
            });
    }

    ImGui::End();
}
