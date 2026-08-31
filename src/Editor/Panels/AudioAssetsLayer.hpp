#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freya/Core/Window.hpp>
#include <Frigga/Core/Layer.hpp>
#include <Freyr/Freyr.hpp>

class AudioAssetsLayer: public fg::Layer
{
  public:
    AudioAssetsLayer(skr::Arc<fg::AssetRegistry> assets, skr::Arc<SelectionContext> selection,
                       skr::Arc<fr::Registry> registry,
                       skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window);
    ~AudioAssetsLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fra::Window> mWindow;
    char mFilter[128] = {};
};
