#pragma once

#include <Freya/Freya.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    struct ModelAsset
    {
        std::string relativePath;
        std::string label;
        std::vector<std::uint32_t> meshIds;
        bool skinned = false;
        fra::Skeleton skeleton {};
        std::vector<fra::AnimationClip> clips;
    };

    struct TextureAsset
    {
        std::string relativePath;
        std::string label;
        std::uint32_t textureId = 0;
    };

    struct MaterialAsset
    {
        std::string name;
        std::uint32_t materialId = 0;
    };

    class AssetRegistry
    {
      public:
        AssetRegistry(const skr::Arc<fra::MeshPool> &meshPool,
                      const skr::Arc<fra::TexturePool> &texturePool,
                      const skr::Arc<fra::MaterialPool> &materialPool,
                      const skr::Arc<skr::Logger<AssetRegistry>> &logger);

        /// Headless catalog that assigns stable ids without Freya/Vulkan.
        struct CatalogTag
        {
        };

        static constexpr CatalogTag Catalog {};

        explicit AssetRegistry(CatalogTag);

        /// Copy `sourcePath` into Resources/Models (if needed) and load meshes.
        [[nodiscard]] std::optional<ModelAsset> ImportModel(const std::filesystem::path &sourcePath);

        /// Copy `sourcePath` into Resources/Textures (if needed) and load the texture.
        [[nodiscard]] std::optional<TextureAsset> ImportTexture(
            const std::filesystem::path &sourcePath);

        /// Load a model already stored under Resources/ (`relativePath` e.g. Models/car.glb).
        [[nodiscard]] std::optional<ModelAsset> LoadModel(
            const std::filesystem::path &relativePath);

        /// Load a texture already stored under Resources/.
        [[nodiscard]] std::optional<TextureAsset> LoadTexture(
            const std::filesystem::path &relativePath);

        [[nodiscard]] std::uint32_t CreateMaterial(const fra::MaterialCreateInfo &createInfo,
                                                   std::string name = "Material",
                                                   bool listInBrowser = true);

        [[nodiscard]] std::uint32_t DuplicateMaterial(std::uint32_t materialId,
                                                      std::string name = "Material");

        void UpdateMaterial(std::uint32_t materialId, const fra::MaterialCreateInfo &createInfo);

        [[nodiscard]] fra::MaterialCreateInfo GetMaterialCreateInfo(std::uint32_t materialId) const;

        [[nodiscard]] const std::vector<ModelAsset> &GetModels() const
        {
            return mModels;
        }

        /// Returns a stable pointer into the registry, or nullptr if not loaded.
        [[nodiscard]] const ModelAsset *FindModel(std::string_view relativePath) const;

        /// Loaded skinned models that expose at least one animation clip.
        [[nodiscard]] std::vector<const ModelAsset *> GetSkinnedModelsWithClips() const;

        [[nodiscard]] const std::vector<TextureAsset> &GetTextures() const
        {
            return mTextures;
        }

        [[nodiscard]] const std::vector<MaterialAsset> &GetMaterials() const
        {
            return mMaterials;
        }

        [[nodiscard]] bool TryFindModelByMeshId(std::uint32_t meshId, ModelAsset &outModel,
                                                std::uint32_t &outSubmeshIndex) const;

        [[nodiscard]] bool TryGetMeshId(std::string_view relativePath, std::uint32_t submeshIndex,
                                        std::uint32_t &outMeshId);

        [[nodiscard]] bool TryGetTexturePath(std::uint32_t textureId,
                                             std::string &outRelativePath) const;

        [[nodiscard]] bool TryGetTextureId(std::string_view relativePath,
                                           std::uint32_t &outTextureId);

        [[nodiscard]] static bool IsModelExtension(std::string_view extension);
        [[nodiscard]] static bool IsTextureExtension(std::string_view extension);

        [[nodiscard]] static std::filesystem::path ResourcesRoot();
        [[nodiscard]] static std::filesystem::path ToAbsoluteResourcePath(
            const std::filesystem::path &relativePath);

        /// Returns path relative to Resources/, or empty if outside the tree.
        [[nodiscard]] static std::filesystem::path MakeRelativeToResources(
            const std::filesystem::path &path);

      private:
        [[nodiscard]] std::filesystem::path copyIntoResources(
            const std::filesystem::path &sourcePath, std::string_view subdir) const;

        [[nodiscard]] std::optional<ModelAsset> loadModelAbsolute(
            const std::filesystem::path &absolutePath,
            const std::filesystem::path &relativePath);

        [[nodiscard]] std::optional<TextureAsset> loadTextureAbsolute(
            const std::filesystem::path &absolutePath,
            const std::filesystem::path &relativePath);

        [[nodiscard]] static std::string normalizeRelativeKey(const std::filesystem::path &relative);

        skr::Arc<fra::MeshPool> mMeshPool;
        skr::Arc<fra::TexturePool> mTexturePool;
        skr::Arc<fra::MaterialPool> mMaterialPool;
        skr::Arc<skr::Logger<AssetRegistry>> mLogger;

        std::vector<ModelAsset> mModels;
        std::vector<TextureAsset> mTextures;
        std::vector<MaterialAsset> mMaterials;

        std::unordered_map<std::string, std::size_t> mModelIndexByPath;
        std::unordered_map<std::string, std::size_t> mTextureIndexByPath;
        std::unordered_map<std::uint32_t, std::string> mTexturePathById;

        std::uint32_t mCatalogMeshSeq     = 1000;
        std::uint32_t mCatalogTextureSeq  = 1000;
        std::uint32_t mCatalogMaterialSeq = 1000;
    };

} // namespace FRIGGA_NAMESPACE
