#include "AudioInspectorLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Ui/AudioComponentInspector.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

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
    const bool editingLocked = mSimulation->IsPlaying();

    if(!hasSource && !hasListener)
    {
        ImGui::TextWrapped("Selected entity has no audio components.");
        ImGui::BeginDisabled(editingLocked);
        if(ImGui::Button("Add Audio Source"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(selection))
            {
                mRegistry->AddComponents(selection, fg::TransformComponent {});
            }
            mRegistry->AddComponents(selection, fg::AudioSourceComponent {});
        }
        if(ImGui::Button("Add Audio Listener"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(selection))
            {
                mRegistry->AddComponents(selection, fg::TransformComponent {});
            }
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
                EditorAudioUi::DrawListenerFields(listener, editingLocked);
            });
    }

    if(hasSource)
    {
        mRegistry->TryGetComponents<fg::AudioSourceComponent>(
            selection, [&](fg::AudioSourceComponent &source) {
                ImGui::SeparatorText("Source");
                EditorAudioUi::DrawSourceFields(source, mAssets, mController, selection,
                                                editingLocked, true);
            });
    }

    ImGui::End();
}
