#pragma once

#include "Editor/MaterialSelectionContext.hpp"
#include "Editor/Ui/MaterialInspector.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Freya/Core/Window.hpp>
#include <Frigga/Core/Layer.hpp>

#include <filesystem>
#include <mutex>
#include <optional>

class MaterialPropertiesLayer: public fg::Layer
{
  public:
    MaterialPropertiesLayer(skr::Arc<fg::AssetRegistry> assets,
                            skr::Arc<fg::PrimitiveMeshFactory> primitives,
                            skr::Arc<MaterialSelectionContext> materialSelection,
                            skr::Arc<fg::SceneSimulationState> simulation,
                            skr::Arc<fra::Window> window);
    ~MaterialPropertiesLayer() override = default;

    void onUpdate() override;
    void onGui() override;

  private:
    void requestTextureImport(EditorMaterialUi::TextureSlot slot);
    void processPendingTextureImport();
    static void onTextureDialog(void *userdata, const char *const *filelist, int filter);

    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<MaterialSelectionContext> mMaterialSelection;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fra::Window> mWindow;

    std::mutex mDialogMutex;
    EditorMaterialUi::TextureSlot mPendingTextureSlot = EditorMaterialUi::TextureSlot::Albedo;
    bool mHasPendingTextureSlot                       = false;
    std::optional<std::filesystem::path> mPendingTexturePath;
};
