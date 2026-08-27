#pragma once

#include "Editor/MaterialSelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Core/Layer.hpp>

class MaterialsLayer: public fg::Layer
{
  public:
    MaterialsLayer(skr::Arc<fg::AssetRegistry> assets,
                   skr::Arc<fg::PrimitiveMeshFactory> primitives,
                   skr::Arc<MaterialSelectionContext> materialSelection,
                   skr::Arc<fg::SceneSimulationState> simulation);
    ~MaterialsLayer() override = default;

    void onGui() override;

  private:
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<MaterialSelectionContext> mMaterialSelection;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    char mFilter[128] = {};
};
