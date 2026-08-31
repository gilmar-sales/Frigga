#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Frigga.hpp>

class AnimationClipsLayer: public fg::Layer
{
  public:
    AnimationClipsLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                        skr::Arc<fr::Registry> registry,
                        skr::Arc<fg::SceneSimulationState> simulation);
    ~AnimationClipsLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::SceneSimulationState> mSimulation;
};
