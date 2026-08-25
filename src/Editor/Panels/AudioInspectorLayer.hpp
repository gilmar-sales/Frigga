#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Audio/AudioController.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Core/Layer.hpp>
#include <Freyr/Freyr.hpp>

class AudioInspectorLayer: public fg::Layer
{
  public:
    AudioInspectorLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                        skr::Arc<fr::Registry> registry,
                        skr::Arc<fg::SceneSimulationState> simulation,
                        skr::Arc<fg::AudioController> controller);
    ~AudioInspectorLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fg::AudioController> mController;
};
