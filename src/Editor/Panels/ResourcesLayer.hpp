#pragma once

#include "Editor/SelectionContext.hpp"
#include "Frigga/Asset/AssetRegistry.hpp"
#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/Scene/Scene.hpp"
#include "Frigga/Scene/SceneSimulationState.hpp"

#include <Frigga/Core/Layer.hpp>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ResourcesLayer: public fg::Layer
{
  public:
    enum class EntryKind
    {
        Primitive,
        Model,
        Texture,
        Material,
        Prefab,
        File,
    };

    struct ResourceDragPayload
    {
        EntryKind kind                    = EntryKind::File;
        fg::PrimitiveType primitive       = fg::PrimitiveType::Cube;
        char relativePath[256]            = {};
        std::uint32_t materialId          = 0;
        std::uint32_t meshId              = 0;
    };

    static constexpr const char *kDragPayloadId = "FRIGGA_RESOURCE";

    enum class ViewMode : std::uint8_t
    {
        List = 0,
        Grid,
        Details,
    };

    ResourcesLayer(skr::Arc<fg::PrimitiveMeshFactory> primitives,
                   skr::Arc<fg::AssetRegistry> assets, skr::Arc<fr::Registry> registry,
                   skr::Arc<fg::Scene> scene, skr::Arc<SelectionContext> selection,
                   skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window);
    ~ResourcesLayer() override = default;

    void onUpdate() override;
    void onGui() override;

    void MarkDirty();
    [[nodiscard]] bool InstantiatePrefab(const std::filesystem::path &relativePath,
                                         fr::Entity parent = static_cast<fr::Entity>(-1));
    bool HandleDrop(const ResourceDragPayload &payload, fr::Entity parent);

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

  private:
    enum class PendingImport
    {
        None,
        Model,
        Texture,
    };

    void refresh();
    void scanDirectory(const std::filesystem::path &absoluteDir,
                       const std::filesystem::path &relativeDir, AssetFolder &folder);
    void ensurePrefabsFolder();
    [[nodiscard]] const AssetFolder *currentFolder() const;
    void enterFolder(std::string_view name);
    void goUp();
    void drawToolbar();
    void drawBreadcrumbs();
    void drawBrowser();
    void drawListView(const AssetFolder &folder);
    void drawGridView(const AssetFolder &folder);
    void drawDetailsView(const AssetFolder &folder);
    void drawSearchResults();
    bool drawFolderRow(const AssetFolder &folder, ViewMode mode);
    bool drawEntryRow(const AssetEntry &entry, ViewMode mode);
    void drawInspector();
    void handleEntryActivation(const AssetEntry &entry);
    [[nodiscard]] bool isSelected(const AssetEntry &entry) const;
    void selectEntry(const AssetEntry &entry);
    void selectFolder(const AssetFolder &folder);
    [[nodiscard]] bool passesFilter(std::string_view text) const;
    void beginDrag(const AssetEntry &entry) const;
    void collectMatches(const AssetFolder &folder, std::vector<const AssetEntry *> &out) const;

    void requestImportModel();
    void requestImportTexture();
    void processPendingImports();
    void spawnSelected();
    void spawnPrimitive(fg::PrimitiveType type);
    void spawnModel(const std::filesystem::path &relativePath);
    void spawnMesh(std::uint32_t meshId, const std::string &name);
    void spawnPrefab(const std::filesystem::path &relativePath);
    void createMaterialAsset();
    void assignMaterialToSelection(std::uint32_t materialId);

    static void onImportModelDialog(void *userdata, const char *const *filelist, int filter);
    static void onImportTextureDialog(void *userdata, const char *const *filelist, int filter);

    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    skr::Arc<fg::AssetRegistry> mAssets;
    skr::Arc<fr::Registry> mRegistry;
    skr::Arc<fg::Scene> mScene;
    skr::Arc<SelectionContext> mSelection;
    skr::Arc<fg::SceneSimulationState> mSimulation;
    skr::Arc<fra::Window> mWindow;

    AssetFolder mRoot {};
    bool mNeedsRefresh = true;
    std::string mFilter;
    std::optional<AssetEntry> mSelected;
    std::optional<std::string> mSelectedFolder;
    std::vector<std::string> mFolderPath;
    ViewMode mViewMode = ViewMode::List;
    std::string mStatus;

    std::mutex mDialogMutex;
    PendingImport mPendingImport = PendingImport::None;
    std::optional<std::filesystem::path> mPendingPath;
};
