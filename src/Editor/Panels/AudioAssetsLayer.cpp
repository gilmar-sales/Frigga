#include "AudioAssetsLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cstdio>

namespace
{
    const SDL_DialogFileFilter kBankFilters[] = {
        {"Audio Banks", "audiobank.json;json"},
        {"All files", "*"},
    };
} // namespace

AudioAssetsLayer::AudioAssetsLayer(skr::Arc<fg::AssetRegistry> assets,
                                   skr::Arc<SelectionContext> selection,
                                   skr::Arc<fr::Registry> registry,
                                   skr::Arc<fg::SceneSimulationState> simulation,
                                   skr::Arc<fra::Window> window)
    : Layer("Audio Assets"), mAssets(std::move(assets)), mSelection(std::move(selection)),
      mRegistry(std::move(registry)), mSimulation(std::move(simulation)),
      mWindow(std::move(window))
{
}

void AudioAssetsLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    ImGui::InputTextWithHint("Filter", "event:/SFX/...", mFilter, sizeof(mFilter));

    if(!mSimulation->IsPlaying() && ImGui::Button("Import Bank"))
    {
        SDL_ShowOpenFileDialog(
            [](void *userdata, const char *const *filelist, int) {
                auto *self = static_cast<AudioAssetsLayer *>(userdata);
                if(filelist == nullptr || filelist[0] == nullptr)
                {
                    return;
                }
                (void)self->mAssets->ImportBank(filelist[0]);
            },
            this, static_cast<SDL_Window *>(mWindow->NativeWindow()), kBankFilters, 2, nullptr,
            false);
    }

    ImGui::Separator();

    const std::string filter = mFilter;
    for(const auto &bank : mAssets->GetBanks())
    {
        ImGui::PushID(bank.relativePath.c_str());
        if(ImGui::TreeNode(bank.label.c_str()))
        {
            ImGui::TextDisabled("%s", bank.relativePath.c_str());
            for(const auto &eventPath : bank.eventPaths)
            {
                if(!filter.empty() && eventPath.find(filter) == std::string::npos)
                {
                    continue;
                }

                ImGui::Selectable(eventPath.c_str());
                if(ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload("FRIGGA_AUDIO_EVENT", eventPath.c_str(),
                                              eventPath.size() + 1);
                    ImGui::TextUnformatted(eventPath.c_str());
                    ImGui::EndDragDropSource();
                }

                if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                   !mSimulation->IsPlaying())
                {
                    const fr::Entity selection = mSelection->Get();
                    if(selection != SelectionContext::Invalid)
                    {
                        if(!mRegistry->HasComponent<fg::AudioSourceComponent>(selection))
                        {
                            mRegistry->AddComponents(selection,
                                                     fg::AudioSourceComponent {.eventPath = eventPath});
                        }
                        else
                        {
                            mRegistry->TryGetComponents<fg::AudioSourceComponent>(
                                selection, [&](fg::AudioSourceComponent &source) {
                                    source.eventPath = eventPath;
                                });
                        }
                    }
                    else
                    {
                        const fr::Entity entity = mRegistry->CreateEntity(
                            fg::NameComponent {.name = "Audio Source"}, fg::TransformComponent {},
                            fg::AudioSourceComponent {.eventPath = eventPath});
                        mSelection->Select(entity);
                    }
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();
}
