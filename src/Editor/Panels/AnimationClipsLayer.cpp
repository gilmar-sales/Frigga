#include "AnimationClipsLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"

AnimationClipsLayer::AnimationClipsLayer(skr::Arc<fg::AssetRegistry> assets,
                                         skr::Arc<SelectionContext> selection,
                                         skr::Arc<fr::Registry> registry,
                                         skr::Arc<fg::SceneSimulationState> simulation)
    : Layer("Animation Clips"), mAssets(std::move(assets)), mSelection(std::move(selection)),
      mRegistry(std::move(registry)), mSimulation(std::move(simulation))
{
}

void AnimationClipsLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    const auto skinned = mAssets->GetSkinnedModelsWithClips();
    if(skinned.empty())
    {
        ImGui::TextWrapped(
            "No skinned models with clips loaded. Import a glTF/FBX with bones "
            "(e.g. Models/Fox.glb) from Resources.");
        ImGui::End();
        return;
    }

    const fr::Entity selection = mSelection->Get();
    const bool canAssign =
        selection != SelectionContext::Invalid && !mSimulation->IsPlaying() &&
        mRegistry->HasComponent<fg::AnimatorComponent>(selection);

    for(const auto *model : skinned)
    {
        if(!ImGui::TreeNodeEx(model->relativePath.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            continue;
        }

        ImGui::TextDisabled("%u joints · %zu clips", model->skeleton.JointCount(),
                            model->clips.size());

        for(const auto &clip : model->clips)
        {
            ImGui::PushID(clip.name.c_str());
            ImGui::BulletText("%s (%.2fs)", clip.name.c_str(), clip.duration);
            if(canAssign)
            {
                ImGui::SameLine();
                if(ImGui::SmallButton("Assign"))
                {
                    mRegistry->TryGetComponents<fg::AnimatorComponent>(
                        selection, [&](fg::AnimatorComponent &animator) {
                            animator.modelSource = model->relativePath;
                            animator.clipName    = clip.name;
                            animator.timeSec     = 0.0f;
                        });
                }
            }
            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    if(selection != SelectionContext::Invalid &&
       !mRegistry->HasComponent<fg::AnimatorComponent>(selection))
    {
        ImGui::Separator();
        ImGui::TextDisabled("Select an entity with an Animator to assign clips.");
    }

    ImGui::End();
}
