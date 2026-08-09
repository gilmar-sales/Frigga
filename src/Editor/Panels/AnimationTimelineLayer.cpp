#include "AnimationTimelineLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"

#include <algorithm>

AnimationTimelineLayer::AnimationTimelineLayer(skr::Arc<fg::AssetRegistry> assets,
                                               skr::Arc<SelectionContext> selection,
                                               skr::Arc<fr::Registry> registry)
    : Layer("Timeline"), mAssets(std::move(assets)), mSelection(std::move(selection)),
      mRegistry(std::move(registry))
{
}

void AnimationTimelineLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    const fr::Entity selection = mSelection->Get();
    if(selection == SelectionContext::Invalid ||
       !mRegistry->HasComponent<fg::AnimatorComponent>(selection))
    {
        ImGui::TextDisabled("Select an animated entity to scrub its clip.");
        ImGui::End();
        return;
    }

    mRegistry->TryGetComponents<fg::AnimatorComponent>(
        selection, [&](fg::AnimatorComponent &animator) {
            float duration = 1.0f;
            std::string clipLabel = animator.clipName.empty() ? "(first clip)" : animator.clipName;

            if(const auto *model = mAssets->FindModel(animator.modelSource);
               model != nullptr && !model->clips.empty())
            {
                const fra::AnimationClip *clip = &model->clips.front();
                for(const auto &candidate : model->clips)
                {
                    if(!animator.clipName.empty() &&
                       (candidate.name == animator.clipName ||
                        candidate.name.find(animator.clipName) != std::string::npos))
                    {
                        clip = &candidate;
                        break;
                    }
                }
                duration  = std::max(clip->duration, 0.001f);
                clipLabel = clip->name;

                ImGui::Text("%s · %s", model->label.c_str(), clipLabel.c_str());

                if(!clip->events.empty() && ImGui::TreeNode("Events"))
                {
                    for(const auto &event : clip->events)
                    {
                        ImGui::BulletText("%.3fs  %s", event.timeSec, event.name.c_str());
                    }
                    ImGui::TreePop();
                }
            }
            else
            {
                ImGui::TextUnformatted(clipLabel.c_str());
            }

            const bool wasPlaying = animator.playing;
            if(ImGui::SliderFloat("##timeline", &animator.timeSec, 0.0f, duration, "%.3f s"))
            {
                // Scrubbing pauses automatic advance until the user hits play again.
                animator.playing = false;
            }
            if(ImGui::Button(animator.playing ? "Pause" : "Play"))
            {
                animator.playing = !animator.playing;
            }
            ImGui::SameLine();
            if(ImGui::Button("Restart"))
            {
                animator.timeSec = 0.0f;
                animator.playing = true;
            }
            if(wasPlaying != animator.playing)
            {
                // no-op placeholder for future transport sync
            }
        });

    ImGui::End();
}
