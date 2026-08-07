#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Core/Layer.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ResourcesLayer: public fg::Layer
{
  public:
    ResourcesLayer(skr::Arc<fg::PrimitiveMeshFactory> primitives,
                   skr::Arc<fg::AssetRegistry> assets, skr::Arc<fr::Registry> registry,
                   skr::Arc<SelectionContext> selection,
                   skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window);
    ~ResourcesLayer() override = default;

    void onUpdate() override;
    void onGui() override;

  private:
    enum class EntryKind
    {
        Primitive,
        Model,
        Texture,
        Material,
        File,
    };

    enum class PendingImport
    {
        None,
        Model,
        Texture,
    };

    struct AssetEntry
    {
        EntryKind kind = EntryKind::File;
        std::string label;
        std::filesystem::path relativePath;
        fg::PrimitiveType primitive = fg::PrimitiveType::Cube;
        std::uint32_t materialId    = 0;
        std::uint32_t meshId        = 0;
        std::uint32_t textureId     = 0;
        std::uint32_t submeshIndex  = 0;
    };

    struct AssetFolder
    {
        std::string name;
        std::vector<AssetEntry> entries;
        std::vector<AssetFolder> children;
    };

    void refresh();
    void scanDirectory(const std::filesystem::path &absoluteDir,
                       const std::filesystem::path &relativeDir, AssetFolder &folder);
    void drawFolder(const AssetFolder &folder);
    void drawEntry(const AssetEntry &entry);
    void drawInspector();
    void drawToolbar();
    [[nodiscard]] bool isSelected(const AssetEntry &entry) const;
    void selectEntry(const AssetEntry &entry);
    [[nodiscard]] bool passesFilter(std::string_view text) const;

    void requestImportModel();
    void requestImportTexture();
    void processPendingImports();
    void spawnSelected();
    void spawnPrimitive(fg::PrimitiveType type);
    void spawnModel(const std::filesystem::path &relativePath);
    void spawnMesh(std::uint32_t meshId, const std::string &name);
    void createMaterialAsset();
    void assignMaterialToSelection(std::uint32_t materialId);

    static void onImportModelDialog(void *userdata, const char *const *filelist, int filter);
    static void onImportTextureDialog(void *userdata, const char *const *filelist, int filter);

    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fra::Window> mWindow;

    AssetFolder mRoot {};
    bool mNeedsRefresh = true;
    std::string mFilter;
    std::optional<AssetEntry> mSelected;
    std::string mStatus;

    std::mutex mDialogMutex;
    PendingImport mPendingImport = PendingImport::None;
    std::optional<std::filesystem::path> mPendingPath;
};
