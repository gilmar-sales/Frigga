#include "AnimationTimelineLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/UiScale.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    [[nodiscard]] std::string UniqueEventName(const std::vector<fra::AnimationEvent> &events)
    {
        for(int i = 1; i < 1000; ++i)
        {
            const std::string candidate = i == 1 ? "Event" : "Event " + std::to_string(i);
            const bool taken = std::ranges::any_of(events, [&](const fra::AnimationEvent &event) {
                return event.name == candidate;
            });
            if(!taken)
            {
                return candidate;
            }
        }
        return "Event";
    }
} // namespace

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
            const fg::ModelAsset *model = mAssets->FindModel(animator.modelSource);
            const fra::AnimationClip *clip = nullptr;
            std::string clipLabel = animator.clipName.empty() ? "(first clip)" : animator.clipName;

            if(model != nullptr && !model->clips.empty())
            {
                clip = &model->clips.front();
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
                clipLabel = clip->name;
                ImGui::Text("%s · %s", model->label.c_str(), clipLabel.c_str());
            }
            else
            {
                ImGui::TextUnformatted(clipLabel.c_str());
            }

            if(model == nullptr || clip == nullptr)
            {
                ImGui::TextDisabled("No clip loaded for this animator.");
                return;
            }

            const float duration = std::max(clip->duration, 0.001f);

            // Refresh the staging copy when switching clips; otherwise it
            // mirrors the last committed state.
            const std::string key = model->relativePath + '\n' + clip->name;
            if(key != mStagedKey)
            {
                mStagedKey      = key;
                mStaged         = clip->events;
                mSelectedEvent  = -1;
                mDraggingEvent  = -1;
                mRenameFor      = -2;
                mError.clear();
            }
            if(mSelectedEvent >= static_cast<int>(mStaged.size()))
            {
                mSelectedEvent = -1;
            }
            if(mSelectedEvent != mRenameFor && mSelectedEvent >= 0)
            {
                std::snprintf(mRenameBuf, sizeof(mRenameBuf), "%s",
                              mStaged[static_cast<std::size_t>(mSelectedEvent)].name.c_str());
                mRenameFor = mSelectedEvent;
            }

            drawEventTrack(animator, *clip, model->relativePath, duration);

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

            if(!mError.empty())
            {
                ImGui::TextColored(ImVec4 {1.0f, 0.4f, 0.4f, 1.0f}, "%s", mError.c_str());
            }
        });

    ImGui::End();
}

void AnimationTimelineLayer::drawEventTrack(fg::AnimatorComponent &animator,
                                            const fra::AnimationClip &clip,
                                            const std::string &modelPath, float duration)
{
    ImGui::PushID(clip.name.c_str());

    const float trackH  = EditorUiScale::S(30.0f);
    const float markerR = EditorUiScale::S(6.0f);

    float trackW = ImGui::GetContentRegionAvail().x;
    if(trackW < 50.0f)
    {
        trackW = 50.0f;
    }
    ImGui::InvisibleButton("##eventTrack", ImVec2 {trackW, trackH});
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    const float width = std::max(p1.x - p0.x, 1.0f);
    const float cy    = (p0.y + p1.y) * 0.5f;

    const auto timeToX = [&](float time) { return p0.x + (time / duration) * width; };
    const auto xToTime = [&](float x) {
        return std::clamp((x - p0.x) / width, 0.0f, 1.0f) * duration;
    };

    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p0, p1, IM_COL32(22, 24, 31, 255), 4.0f);
    draw->AddRect(p0, p1, IM_COL32(70, 75, 90, 255), 4.0f);
    for(int i = 1; i < 10; ++i)
    {
        const float gx = p0.x + (static_cast<float>(i) / 10.0f) * width;
        draw->AddLine(ImVec2 {gx, p0.y}, ImVec2 {gx, p1.y}, IM_COL32(255, 255, 255, 18));
    }

    const auto markerAt = [&](const ImVec2 &mouse) {
        for(std::size_t i = 0; i < mStaged.size(); ++i)
        {
            const float mx = timeToX(mStaged[i].timeSec);
            if(std::abs(mx - mouse.x) <= markerR + 4.0f && std::abs(cy - mouse.y) <= trackH * 0.5f)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    for(std::size_t i = 0; i < mStaged.size(); ++i)
    {
        const float mx       = timeToX(mStaged[i].timeSec);
        const bool selected  = static_cast<int>(i) == mSelectedEvent;
        const ImU32 color    = selected ? IM_COL32(255, 210, 90, 255) : IM_COL32(110, 220, 150, 255);
        const float r        = selected ? markerR + 1.5f : markerR;
        draw->AddQuadFilled(ImVec2 {mx, cy - r}, ImVec2 {mx + r, cy}, ImVec2 {mx, cy + r},
                            ImVec2 {mx - r, cy}, color);
        if(selected)
        {
            draw->AddQuad(ImVec2 {mx, cy - r}, ImVec2 {mx + r, cy}, ImVec2 {mx, cy + r},
                          ImVec2 {mx - r, cy}, IM_COL32(255, 255, 255, 255));
        }
    }

    const float playX = timeToX(std::clamp(animator.timeSec, 0.0f, duration));
    draw->AddLine(ImVec2 {playX, p0.y}, ImVec2 {playX, p1.y}, IM_COL32(255, 90, 90, 255), 2.0f);
    draw->AddTriangleFilled(ImVec2 {playX - 5.0f, p0.y}, ImVec2 {playX + 5.0f, p0.y},
                            ImVec2 {playX, p0.y + 7.0f}, IM_COL32(255, 90, 90, 255));

    const auto commit = [&](const char *action) {
        std::string error;
        if(mAssets->SetClipEvents(modelPath, clip.name, mStaged, &error))
        {
            mError.clear();
        }
        else
        {
            mError   = std::string(action) + ": " + error;
            mStaged  = clip.events;
            if(mSelectedEvent >= static_cast<int>(mStaged.size()))
            {
                mSelectedEvent = -1;
            }
        }
    };

    if(ImGui::IsItemHovered())
    {
        const int hovered = markerAt(ImGui::GetMousePos());
        if(hovered >= 0)
        {
            ImGui::SetTooltip("%s  ·  %.3fs", mStaged[static_cast<std::size_t>(hovered)].name.c_str(),
                              mStaged[static_cast<std::size_t>(hovered)].timeSec);
        }
        else
        {
            ImGui::SetTooltip("%.3fs — left-click to scrub, right-click to add an event",
                              xToTime(ImGui::GetMousePos().x));
        }
    }

    if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const int hit = markerAt(ImGui::GetMousePos());
        if(hit >= 0)
        {
            mSelectedEvent = hit;
            mDraggingEvent = hit;
        }
        else
        {
            animator.timeSec  = xToTime(ImGui::GetMousePos().x);
            animator.playing  = false;
            mSelectedEvent    = -1;
            mDraggingEvent    = -2;
            mRenameFor        = -2;
        }
    }
    if(mDraggingEvent >= 0)
    {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            mStaged[static_cast<std::size_t>(mDraggingEvent)].timeSec =
                xToTime(ImGui::GetMousePos().x);
        }
        else
        {
            fg::NormalizeClipEvents(mStaged, duration);
            commit("Move event");
            mDraggingEvent = -1;
        }
    }
    else if(mDraggingEvent == -2)
    {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            animator.timeSec = xToTime(ImGui::GetMousePos().x);
        }
        else
        {
            mDraggingEvent = -1;
        }
    }

    if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const int hit = markerAt(ImGui::GetMousePos());
        mSelectedEvent    = hit;
        mPendingEventTime = xToTime(ImGui::GetMousePos().x);
        mRenameFor        = -2;
        ImGui::OpenPopup("##ClipEventTrackMenu");
    }
    if(ImGui::BeginPopup("##ClipEventTrackMenu"))
    {
        if(mSelectedEvent >= 0 && mSelectedEvent < static_cast<int>(mStaged.size()))
        {
            const auto &event = mStaged[static_cast<std::size_t>(mSelectedEvent)];
            ImGui::TextDisabled("%s  ·  %.3fs", event.name.c_str(), event.timeSec);
            ImGui::Separator();
            if(ImGui::MenuItem("Rename"))
            {
                mFocusRename = true;
            }
            if(ImGui::MenuItem("Move to playhead"))
            {
                mStaged[static_cast<std::size_t>(mSelectedEvent)].timeSec =
                    std::clamp(animator.timeSec, 0.0f, duration);
                fg::NormalizeClipEvents(mStaged, duration);
                commit("Move event");
            }
            if(ImGui::MenuItem("Delete"))
            {
                mStaged.erase(mStaged.begin() + mSelectedEvent);
                mSelectedEvent = -1;
                mRenameFor     = -2;
                commit("Delete event");
            }
            ImGui::Separator();
        }
        if(ImGui::MenuItem("Add event here"))
        {
            mStaged.push_back(
                fra::AnimationEvent {.name = UniqueEventName(mStaged), .timeSec = mPendingEventTime});
            fg::NormalizeClipEvents(mStaged, duration);
            commit("Add event");
            // Select the event just added (sorted position).
            mSelectedEvent = 0;
            for(std::size_t i = 0; i < mStaged.size(); ++i)
            {
                if(std::abs(mStaged[i].timeSec - mPendingEventTime) < 0.0005f)
                {
                    mSelectedEvent = static_cast<int>(i);
                }
            }
            mRenameFor   = -2;
            mFocusRename = true;
        }
        ImGui::EndPopup();
    }

    ImGui::TextDisabled("0.00 s");
    ImGui::SameLine(trackW - EditorUiScale::S(52.0f));
    ImGui::TextDisabled("%.2f s", duration);

    if(mStaged.empty())
    {
        ImGui::TextDisabled("No events — right-click the track to add one.");
    }
    else
    {
        for(std::size_t i = 0; i < mStaged.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = static_cast<int>(i) == mSelectedEvent;
            char label[128];
            std::snprintf(label, sizeof(label), "%s  ·  %.3fs", mStaged[i].name.c_str(),
                          mStaged[i].timeSec);
            if(ImGui::Selectable(label, selected))
            {
                mSelectedEvent = static_cast<int>(i);
            }
            ImGui::PopID();
        }
    }

    if(mSelectedEvent >= 0 && mSelectedEvent < static_cast<int>(mStaged.size()))
    {
        auto &event = mStaged[static_cast<std::size_t>(mSelectedEvent)];
        ImGui::Separator();
        if(mFocusRename)
        {
            ImGui::SetKeyboardFocusHere();
            mFocusRename = false;
        }
        if(ImGui::InputText("Name", mRenameBuf, sizeof(mRenameBuf)))
        {
            event.name = mRenameBuf;
        }
        if(ImGui::IsItemDeactivatedAfterEdit())
        {
            if(event.name.empty())
            {
                event.name = UniqueEventName(mStaged);
                std::snprintf(mRenameBuf, sizeof(mRenameBuf), "%s", event.name.c_str());
            }
            commit("Rename event");
        }
        float eventTime = event.timeSec;
        if(ImGui::SliderFloat("Time", &eventTime, 0.0f, duration, "%.3f s"))
        {
            event.timeSec = eventTime;
        }
        if(ImGui::IsItemDeactivatedAfterEdit())
        {
            fg::NormalizeClipEvents(mStaged, duration);
            commit("Move event");
        }
        if(ImGui::Button("Delete event"))
        {
            mStaged.erase(mStaged.begin() + mSelectedEvent);
            mSelectedEvent = -1;
            mRenameFor     = -2;
            commit("Delete event");
        }
    }

    ImGui::PopID();
}
