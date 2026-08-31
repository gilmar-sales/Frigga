#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Freyr/Freyr.hpp>

class AnimationTimelineLayer: public fg::Layer
{
  public:
    AnimationTimelineLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                           skr::Arc<fr::Registry> registry);
    ~AnimationTimelineLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fr::Registry> mRegistry;
};
