#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"

#include <Frigga/Core/Layer.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ResourcesLayer: public fg::Layer
{
  public:
    explicit ResourcesLayer(skr::Arc<fg::PrimitiveMeshFactory> primitives);
    ~ResourcesLayer() override = default;

    void onGui() override;

  private:
    enum class EntryKind
    {
        Primitive,
        File,
    };

    struct AssetEntry
    {
        EntryKind kind = EntryKind::File;
        std::string label;
        std::filesystem::path relativePath;
        fg::PrimitiveType primitive = fg::PrimitiveType::Cube;
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
    [[nodiscard]] bool isSelected(const AssetEntry &entry) const;
    void selectEntry(const AssetEntry &entry);
    [[nodiscard]] bool passesFilter(std::string_view text) const;

    skr::Arc<fg::PrimitiveMeshFactory> mPrimitives;
    AssetFolder mRoot {};
    bool mNeedsRefresh = true;
    std::string mFilter;
    std::optional<AssetEntry> mSelected;
};
