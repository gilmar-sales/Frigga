#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"

#include <Freya/Asset/AnimationClip.hpp>

#include <Freyr/Freyr.hpp>
#include <Frigga/Core/Layer.hpp>

#include <string>
#include <vector>

class AnimationTimelineLayer: public fg::Layer
{
  public:
    AnimationTimelineLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                           skr::Arc<fr::Registry> registry);
    ~AnimationTimelineLayer() override = default;

    void onGui() override;

  private:
    void drawEventTrack(fg::AnimatorComponent &animator, const fra::AnimationClip &clip,
                        const std::string &modelPath, float duration);

    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fr::Registry> mRegistry;

    /// Staged event copy so drags/scrubs preview live; persisted on release.
    std::string mStagedKey;
    std::vector<fra::AnimationEvent> mStaged;
    int mSelectedEvent      = -1;
    int mDraggingEvent      = -1;
    float mPendingEventTime = 0.0f;
    char mRenameBuf[128]    = {};
    int mRenameFor          = -2;
    bool mFocusRename       = false;
    std::string mError;
};
