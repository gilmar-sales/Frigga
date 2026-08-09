#include "AnimatorPanelLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"

#include <cstdio>
#include <cstring>

AnimatorPanelLayer::AnimatorPanelLayer(skr::Arc<fg::AssetRegistry> assets,
                                       skr::Arc<SelectionContext> selection,
                                       skr::Arc<fr::Registry> registry,
                                       skr::Arc<fg::SceneSimulationState> simulation)
    : Layer("Animator"), mAssets(std::move(assets)), mSelection(std::move(selection)),
      mRegistry(std::move(registry)), mSimulation(std::move(simulation))
{
}

void AnimatorPanelLayer::onGui()
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

    if(!mRegistry->HasComponent<fg::AnimatorComponent>(selection))
    {
        ImGui::TextWrapped("Selected entity has no Animator component.");
        if(!mSimulation->IsPlaying() && ImGui::Button("Add Animator"))
        {
            fg::ModelAsset model {};
            std::uint32_t submesh = 0;
            std::string source;
            mRegistry->TryGetComponents<fg::MeshComponent>(
                selection, [&](fg::MeshComponent &mesh) {
                    if(mAssets->TryFindModelByMeshId(mesh.meshId, model, submesh) &&
                       model.skinned)
                    {
                        source = model.relativePath;
                    }
                });
            mRegistry->AddComponents(selection,
                                     fg::AnimatorComponent {.modelSource = std::move(source)});
        }
        ImGui::End();
        return;
    }

    mRegistry->TryGetComponents<fg::AnimatorComponent>(
        selection, [&](fg::AnimatorComponent &animator) {
            ImGui::BeginDisabled(mSimulation->IsPlaying());

            char sourceBuf[256];
            std::snprintf(sourceBuf, sizeof(sourceBuf), "%s", animator.modelSource.c_str());
            if(ImGui::InputText("Model", sourceBuf, sizeof(sourceBuf)))
            {
                animator.modelSource = sourceBuf;
            }

            if(ImGui::BeginCombo("Clip",
                                 animator.clipName.empty() ? "(first clip)"
                                                           : animator.clipName.c_str()))
            {
                const auto *model = mAssets->FindModel(animator.modelSource);
                if(model == nullptr && !animator.modelSource.empty())
                {
                    (void)mAssets->LoadModel(animator.modelSource);
                    model = mAssets->FindModel(animator.modelSource);
                }

                if(ImGui::Selectable("(first clip)", animator.clipName.empty()))
                {
                    animator.clipName.clear();
                    animator.timeSec = 0.0f;
                }

                if(model != nullptr)
                {
                    for(const auto &clip : model->clips)
                    {
                        const bool selected = animator.clipName == clip.name;
                        if(ImGui::Selectable(clip.name.c_str(), selected))
                        {
                            animator.clipName = clip.name;
                            animator.timeSec  = 0.0f;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::DragFloat("Speed", &animator.speed, 0.01f, 0.0f, 8.0f, "%.2f");
            ImGui::Checkbox("Playing", &animator.playing);
            ImGui::Checkbox("Loop", &animator.loop);
            ImGui::Checkbox("Use GPU", &animator.useGpu);
            ImGui::Checkbox("Preview in Edit", &animator.previewInEdit);

            float duration = 0.0f;
            if(const auto *model = mAssets->FindModel(animator.modelSource);
               model != nullptr && !model->clips.empty())
            {
                for(const auto &clip : model->clips)
                {
                    if(animator.clipName.empty() || clip.name == animator.clipName ||
                       clip.name.find(animator.clipName) != std::string::npos)
                    {
                        duration = clip.duration;
                        break;
                    }
                }
                if(duration <= 0.0f)
                {
                    duration = model->clips.front().duration;
                }
            }

            if(duration > 0.0f)
            {
                ImGui::SliderFloat("Time", &animator.timeSec, 0.0f, duration, "%.3f s");
            }
            else
            {
                ImGui::DragFloat("Time", &animator.timeSec, 0.01f, 0.0f, 100.0f, "%.3f s");
            }

            ImGui::TextDisabled("Bone palette: offset %u · count %u", animator.boneOffset,
                                animator.boneCount);
            ImGui::EndDisabled();
        });

    ImGui::End();
}
