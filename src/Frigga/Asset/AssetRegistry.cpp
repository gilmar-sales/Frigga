#include <Frigga/Asset/AssetRegistry.hpp>
#include <Frigga/Asset/FreyaHandles.hpp>

#include "Frigga/Audio/IAudioEngine.hpp"
#include "Frigga/Scene/PrefabCache.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <mutex>
#include <system_error>
#include <utility>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        std::string ToLower(std::string value)
        {
            std::ranges::transform(value, value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        std::filesystem::path GenericRelative(const std::filesystem::path &path)
        {
            return path.generic_string();
        }

        void BakeModelClips(ModelAsset &asset, float bakeHz)
        {
            asset.bakedClips.clear();
            if(!asset.skinned || asset.skeleton.JointCount() == 0 || asset.clips.empty())
            {
                return;
            }

            const float hz = std::max(bakeHz, 1.0f);
            asset.bakedClips.reserve(asset.clips.size());
            for(const auto &clip : asset.clips)
            {
                asset.bakedClips.push_back(fra::BakeClip(asset.skeleton, clip, hz));
            }
        }

        void HashCombine(std::uint64_t &seed, std::uint64_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        }

        void HashFloat(std::uint64_t &seed, float value)
        {
            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            HashCombine(seed, bits);
        }

        void HashTextureSlot(std::uint64_t &seed, const std::optional<fra::TextureHandle> &slot)
        {
            HashCombine(seed, slot && slot->IsValid() ? slot->Id() : 0u);
        }

        std::uint64_t HashMaterialCreateInfo(const fra::MaterialCreateInfo &info)
        {
            std::uint64_t seed = 0xcbf29ce484222325ull;
            HashTextureSlot(seed, info.albedo);
            HashTextureSlot(seed, info.normal);
            HashTextureSlot(seed, info.roughness);
            HashTextureSlot(seed, info.emissive);
            HashTextureSlot(seed, info.metalness);
            HashTextureSlot(seed, info.occlusion);
            HashFloat(seed, info.albedoFactor.x);
            HashFloat(seed, info.albedoFactor.y);
            HashFloat(seed, info.albedoFactor.z);
            HashFloat(seed, info.albedoFactor.w);
            HashFloat(seed, info.roughnessFactor);
            HashFloat(seed, info.metalnessFactor);
            HashFloat(seed, info.emissiveFactor.x);
            HashFloat(seed, info.emissiveFactor.y);
            HashFloat(seed, info.emissiveFactor.z);
            HashFloat(seed, info.aoFactor);
            HashFloat(seed, info.alphaCutoff);
            HashFloat(seed, info.clearcoat);
            HashFloat(seed, info.clearcoatRoughness);
            HashFloat(seed, info.transmission);
            HashFloat(seed, info.ior);
            HashCombine(seed, static_cast<std::uint64_t>(info.alphaMode));
            HashCombine(seed, info.unlit ? 1u : 0u);
            HashCombine(seed, info.doubleSided ? 1u : 0u);
            HashCombine(seed, info.receiveShadows ? 1u : 0u);
            HashCombine(seed, info.packedMetallicRoughness ? 1u : 0u);
            HashCombine(seed, info.techniqueId);
            return seed;
        }
    } // namespace

    const fra::BakedClip *ModelAsset::FindBakedClip(std::string_view clipName) const
    {
        if(clipName.empty())
        {
            return bakedClips.empty() ? nullptr : &bakedClips.front();
        }

        for(std::size_t i = 0; i < clips.size() && i < bakedClips.size(); ++i)
        {
            if(clips[i].name == clipName)
            {
                return &bakedClips[i];
            }
        }

        for(std::size_t i = 0; i < clips.size() && i < bakedClips.size(); ++i)
        {
            if(clips[i].name.find(clipName) != std::string::npos)
            {
                return &bakedClips[i];
            }
        }

        return bakedClips.empty() ? nullptr : &bakedClips.front();
    }

    const fra::BakedClip *ModelAsset::BakedClipFor(const fra::AnimationClip &clip) const
    {
        for(std::size_t i = 0; i < clips.size() && i < bakedClips.size(); ++i)
        {
            if(&clips[i] == &clip)
            {
                return &bakedClips[i];
            }
        }
        return nullptr;
    }

    AssetRegistry::AssetRegistry(const skr::Arc<fra::MeshPool> &meshPool,
                                 const skr::Arc<fra::TexturePool> &texturePool,
                                 const skr::Arc<fra::MaterialPool> &materialPool,
                                 const skr::Arc<skr::Logger<AssetRegistry>> &logger)
        : mMeshPool(meshPool), mTexturePool(texturePool), mMaterialPool(materialPool),
          mLogger(logger)
    {
        WarmFonts();
    }

    AssetRegistry::AssetRegistry(CatalogTag) {}

    namespace
    {
        std::filesystem::path &MutableResourcesRoot()
        {
            static std::filesystem::path root = AssetRegistry::EngineResourcesRoot();
            return root;
        }
    } // namespace

    std::filesystem::path AssetRegistry::EngineResourcesRoot()
    {
        return {"Resources"};
    }

    std::filesystem::path AssetRegistry::ResourcesRoot()
    {
        return MutableResourcesRoot();
    }

    void AssetRegistry::SetResourcesRoot(std::filesystem::path root)
    {
        if(root.empty())
        {
            MutableResourcesRoot() = EngineResourcesRoot();
        }
        else
        {
            MutableResourcesRoot() = std::move(root);
        }
        PrefabCache::Instance().Clear();
    }

    void AssetRegistry::ResetResourcesRoot()
    {
        SetResourcesRoot({});
    }

    void AssetRegistry::ClearCatalog()
    {
        mModels.clear();
        mTextures.clear();
        mMaterials.clear();
        mFonts.clear();
        mBanks.clear();
        mAudioClips.clear();
        mModelIndexByPath.clear();
        mTextureIndexByPath.clear();
        mFontIndexByPath.clear();
        mBankIndexByPath.clear();
        mAudioClipIndexByPath.clear();
        mTexturePathById.clear();
        {
            std::lock_guard lock(mMaterialCacheMutex);
            mSharedMaterialByHash.clear();
            mSharedMaterialIds.clear();
        }
        PrefabCache::Instance().Clear();
    }

    std::string AssetRegistry::assetIdFor(const std::filesystem::path &relativePath,
                                          std::string_view type)
    {
        const auto root = ResourcesRoot();
        if(!mManifestLoaded || mManifest.Root() != root)
        {
            std::string error;
            if(!mManifest.Load(root, &error) && mLogger)
            {
                mLogger->LogWarning("Unable to load asset manifest '{}': {}", root.string(), error);
            }
            mManifestLoaded = true;
        }
        const auto key = normalizeRelativeKey(relativePath);
        const auto id = mManifest.RecordImport(key, type, ResourcesRoot() / key);
        std::string error;
        if(!mManifest.Save(root, &error) && mLogger)
        {
            mLogger->LogWarning("Unable to save asset manifest '{}': {}", root.string(), error);
        }
        return id;
    }

    void AssetRegistry::SetAudioEngine(const skr::Arc<IAudioEngine> &audioEngine)
    {
        mAudioEngine = audioEngine;
    }

    std::filesystem::path AssetRegistry::ToAbsoluteResourcePath(
        const std::filesystem::path &relativePath)
    {
        return ResourcesRoot() / relativePath;
    }

    std::filesystem::path AssetRegistry::MakeRelativeToResources(const std::filesystem::path &path)
    {
        std::error_code ec;
        const auto absolute = std::filesystem::weakly_canonical(path, ec);
        if(ec)
        {
            return {};
        }

        const auto root = std::filesystem::weakly_canonical(ResourcesRoot(), ec);
        if(ec || root.empty())
        {
            return {};
        }

        const auto relative = std::filesystem::relative(absolute, root, ec);
        // Use generic_string(): path::native() is wchar_t on Windows MinGW.
        if(ec || relative.empty() || relative.generic_string().starts_with(".."))
        {
            return {};
        }

        return GenericRelative(relative);
    }

    std::string AssetRegistry::normalizeRelativeKey(const std::filesystem::path &relative)
    {
        return GenericRelative(relative).generic_string();
    }

    bool AssetRegistry::IsModelExtension(std::string_view extension)
    {
        const auto ext = ToLower(std::string(extension));
        return ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj" ||
               ext == ".dae" || ext == ".3ds" || ext == ".blend";
    }

    bool AssetRegistry::IsTextureExtension(std::string_view extension)
    {
        const auto ext = ToLower(std::string(extension));
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
               ext == ".bmp" || ext == ".hdr" || ext == ".webp";
    }

    bool AssetRegistry::IsFontExtension(std::string_view extension)
    {
        const auto ext = ToLower(std::string(extension));
        return ext == ".ttf" || ext == ".otf";
    }

    bool AssetRegistry::IsPrefabExtension(std::string_view extension)
    {
        const auto ext = ToLower(std::string(extension));
        return ext == ".prefab";
    }

    bool AssetRegistry::IsBankExtension(std::string_view extension)
    {
        const auto ext = ToLower(std::string(extension));
        // Prefer IsBankFilename: path.extension() for *.audiobank.json is ".json".
        return ext == ".audiobank.json";
    }

    bool AssetRegistry::IsBankFilename(std::string_view filename)
    {
        return ToLower(std::string(filename)).ends_with(".audiobank.json");
    }

    bool AssetRegistry::IsAudioClipExtension(std::string_view extension)
    {
        const auto ext = ToLower(std::string(extension));
        return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
    }

    std::filesystem::path AssetRegistry::copyIntoResources(const std::filesystem::path &sourcePath,
                                                           std::string_view subdir) const
    {
        const auto relativeUnderResources = MakeRelativeToResources(sourcePath);
        if(!relativeUnderResources.empty())
        {
            return relativeUnderResources;
        }

        const auto destDir = ResourcesRoot() / subdir;
        std::error_code ec;
        std::filesystem::create_directories(destDir, ec);

        auto filename = sourcePath.filename();
        if(filename.empty())
        {
            filename = "asset";
        }

        auto destination = destDir / filename;
        if(std::filesystem::exists(destination) &&
           !std::filesystem::equivalent(sourcePath, destination, ec))
        {
            const auto stem = destination.stem().string();
            const auto ext  = destination.extension().string();
            for(int i = 1; i < 1000; ++i)
            {
                const auto candidate = destDir / std::format("{} ({}){}", stem, i, ext);
                if(!std::filesystem::exists(candidate))
                {
                    destination = candidate;
                    break;
                }
            }
        }

        std::filesystem::copy_file(sourcePath, destination,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if(ec)
        {
            if(mLogger)
            {
                mLogger->LogError("Failed to copy '{}' to '{}': {}", sourcePath.string(),
                                  destination.string(), ec.message());
            }
            return {};
        }

        // Companion .mtl for Wavefront OBJ (best-effort).
        if(ToLower(sourcePath.extension().string()) == ".obj")
        {
            auto mtlSource = sourcePath;
            mtlSource.replace_extension(".mtl");
            if(std::filesystem::is_regular_file(mtlSource))
            {
                auto mtlDest = destination;
                mtlDest.replace_extension(".mtl");
                std::filesystem::copy_file(mtlSource, mtlDest,
                                           std::filesystem::copy_options::overwrite_existing, ec);
            }
        }

        return GenericRelative(std::filesystem::path {subdir} / destination.filename());
    }

    std::optional<ModelAsset> AssetRegistry::loadModelAbsolute(
        const std::filesystem::path &absolutePath, const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mModelIndexByPath.find(key); it != mModelIndexByPath.end())
        {
            return mModels[it->second];
        }

        const auto baseLabel = relativePath.stem().string();
        ModelAsset asset {.assetId      = assetIdFor(key, "model"),
                          .relativePath = key,
                          .label        = relativePath.filename().string()};

        const auto appendSubmeshes =
            [&](const std::vector<fra::ModelSubmesh> &parts) {
                for(std::size_t i = 0; i < parts.size(); ++i)
                {
                    const auto meshId     = FromHandle(parts[i].mesh);
                    const auto materialId = FromHandle(parts[i].material);
                    asset.submeshes.push_back(ModelSubmeshAsset {
                        .meshId     = meshId,
                        .materialId = materialId,
                    });
                    if(materialId != 0)
                    {
                        const auto matName =
                            parts.size() == 1
                                ? std::format("{} Material", baseLabel)
                                : std::format("{} Material {}", baseLabel, i);
                        catalogMaterialIfNew(materialId, matName);
                    }
                }
            };

        if(mMeshPool == nullptr)
        {
            asset.submeshes.push_back(
                ModelSubmeshAsset {.meshId = ++mCatalogMeshSeq, .materialId = 0});
        }
        else
        {
            // Prefer the skinned loader when the file contains a skeleton.
            auto skinned = mMeshPool->CreateSkinnedModelFromFile(absolutePath.string());
            if(skinned.skeleton.JointCount() > 0 && !skinned.submeshes.empty())
            {
                asset.skinned  = true;
                appendSubmeshes(skinned.submeshes);
                asset.skeleton = std::move(skinned.skeleton);
                asset.clips    = std::move(skinned.clips);
                for(auto &clip : asset.clips)
                {
                    fra::EnsureDefaultFootstepEvents(clip);
                }
                BakeModelClips(asset, 30.0f);
            }
            else
            {
                appendSubmeshes(mMeshPool->CreateModelFromFile(absolutePath.string()));
            }

            if(asset.submeshes.empty())
            {
                if(mLogger)
                {
                    mLogger->LogError("Failed to import model '{}'", absolutePath.string());
                }
                return std::nullopt;
            }
        }

        mModelIndexByPath.emplace(key, mModels.size());
        mModels.push_back(std::move(asset));

        if(mLogger)
        {
            const auto &stored = mModels.back();
            if(stored.skinned)
            {
                mLogger->LogInformation(
                    "Imported skinned model '{}' ({} submeshes, {} joints, {} clips)", key,
                    stored.submeshes.size(), stored.skeleton.JointCount(), stored.clips.size());
            }
            else
            {
                mLogger->LogInformation("Imported model '{}' ({} submeshes)", key,
                                        stored.submeshes.size());
            }
        }

        return mModels.back();
    }

    std::optional<TextureAsset> AssetRegistry::loadTextureAbsolute(
        const std::filesystem::path &absolutePath, const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mTextureIndexByPath.find(key); it != mTextureIndexByPath.end())
        {
            return mTextures[it->second];
        }

        TextureAsset asset {.assetId      = assetIdFor(key, "texture"),
                            .relativePath = key,
                            .label        = relativePath.filename().string()};

        if(mTexturePool == nullptr)
        {
            asset.textureId = ++mCatalogTextureSeq;
        }
        else
        {
            const auto loaded = mTexturePool->CreateTextureFromFile(absolutePath.string());
            if(!loaded)
            {
                if(mLogger)
                {
                    mLogger->LogError("Failed to import texture '{}'", absolutePath.string());
                }
                return std::nullopt;
            }
            asset.textureId = FromHandle(*loaded);
        }

        mTextureIndexByPath.emplace(key, mTextures.size());
        mTexturePathById[asset.textureId] = key;
        mTextures.push_back(asset);

        if(mLogger)
        {
            mLogger->LogInformation("Imported texture '{}' (id {})", key, asset.textureId);
        }

        return asset;
    }

    std::optional<ModelAsset> AssetRegistry::ImportModel(const std::filesystem::path &sourcePath)
    {
        if(!std::filesystem::is_regular_file(sourcePath))
        {
            if(mLogger)
            {
                mLogger->LogError("Model path is not a file: {}", sourcePath.string());
            }
            return std::nullopt;
        }

        if(!IsModelExtension(sourcePath.extension().string()))
        {
            if(mLogger)
            {
                mLogger->LogWarning("Unrecognized model extension '{}'",
                                    sourcePath.extension().string());
            }
        }

        const auto relative = copyIntoResources(sourcePath, "Models");
        if(relative.empty())
        {
            return std::nullopt;
        }

        return loadModelAbsolute(ToAbsoluteResourcePath(relative), relative);
    }

    std::optional<TextureAsset> AssetRegistry::ImportTexture(const std::filesystem::path &sourcePath)
    {
        if(!std::filesystem::is_regular_file(sourcePath))
        {
            if(mLogger)
            {
                mLogger->LogError("Texture path is not a file: {}", sourcePath.string());
            }
            return std::nullopt;
        }

        if(!IsTextureExtension(sourcePath.extension().string()))
        {
            if(mLogger)
            {
                mLogger->LogWarning("Unrecognized texture extension '{}'",
                                    sourcePath.extension().string());
            }
        }

        const auto relative = copyIntoResources(sourcePath, "Textures");
        if(relative.empty())
        {
            return std::nullopt;
        }

        return loadTextureAbsolute(ToAbsoluteResourcePath(relative), relative);
    }

    std::optional<ModelAsset> AssetRegistry::LoadModel(const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mModelIndexByPath.find(key); it != mModelIndexByPath.end())
        {
            return mModels[it->second];
        }

        const auto absolute = ToAbsoluteResourcePath(key);
        if(!std::filesystem::is_regular_file(absolute))
        {
            if(mLogger)
            {
                mLogger->LogError("Model resource not found: {}", absolute.string());
            }
            return std::nullopt;
        }

        return loadModelAbsolute(absolute, key);
    }

    std::optional<TextureAsset> AssetRegistry::LoadTexture(const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mTextureIndexByPath.find(key); it != mTextureIndexByPath.end())
        {
            return mTextures[it->second];
        }

        const auto absolute = ToAbsoluteResourcePath(key);
        if(!std::filesystem::is_regular_file(absolute))
        {
            if(mLogger)
            {
                mLogger->LogError("Texture resource not found: {}", absolute.string());
            }
            return std::nullopt;
        }

        return loadTextureAbsolute(absolute, key);
    }

    bool AssetRegistry::loadFontAbsolute(const std::filesystem::path &absolutePath,
                                         const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(mFontIndexByPath.contains(key))
        {
            return true;
        }

        if(mTexturePool == nullptr)
        {
            return false;
        }

        auto atlas = fra::FontAtlas::Create(*mTexturePool, absolutePath.string());
        if(!atlas.Valid())
        {
            if(mLogger)
            {
                mLogger->LogError("Failed to load font '{}'", absolutePath.string());
            }
            return false;
        }

        FontAsset asset {.assetId      = assetIdFor(key, "font"),
                         .relativePath = key,
                         .label        = relativePath.filename().string(),
                         .atlas        = std::make_unique<fra::FontAtlas>(std::move(atlas))};

        mFontIndexByPath.emplace(key, mFonts.size());
        mFonts.push_back(std::move(asset));

        if(mLogger)
        {
            mLogger->LogInformation("Loaded font '{}'", key);
        }

        return true;
    }

    bool AssetRegistry::LoadFont(const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(mFontIndexByPath.contains(key))
        {
            return true;
        }

        const auto absolute = ToAbsoluteResourcePath(key);
        if(!std::filesystem::is_regular_file(absolute))
        {
            if(mLogger)
            {
                mLogger->LogWarning("Font resource not found: {}", absolute.string());
            }
            return false;
        }

        if(!IsFontExtension(absolute.extension().string()) && mLogger)
        {
            mLogger->LogWarning("Unrecognized font extension '{}'",
                                absolute.extension().string());
        }

        return loadFontAbsolute(absolute, key);
    }

    void AssetRegistry::WarmFonts()
    {
        if(mTexturePool == nullptr)
        {
            return;
        }

        // Default billboard-text font shipped with the engine Resources tree.
        (void)LoadFont("Fonts/OpenSans.ttf");
        (void)LoadFont("Fonts/NotoSans-Regular.ttf");

        const auto fontsDir = ResourcesRoot() / "Fonts";
        std::error_code ec;
        if(!std::filesystem::is_directory(fontsDir, ec))
        {
            return;
        }

        for(const auto &entry : std::filesystem::directory_iterator(fontsDir, ec))
        {
            if(ec || !entry.is_regular_file())
            {
                continue;
            }
            if(!IsFontExtension(entry.path().extension().string()))
            {
                continue;
            }
            const auto relative = MakeRelativeToResources(entry.path());
            if(!relative.empty())
            {
                (void)LoadFont(relative);
            }
        }
    }

    const fra::FontAtlas *AssetRegistry::FindFont(std::string_view relativePath) const
    {
        const auto key = normalizeRelativeKey(relativePath);
        const auto it  = mFontIndexByPath.find(key);
        if(it == mFontIndexByPath.end())
        {
            return nullptr;
        }
        const auto &asset = mFonts[it->second];
        if(!asset.atlas || !asset.atlas->Valid())
        {
            return nullptr;
        }
        return asset.atlas.get();
    }

    std::optional<BankAsset> AssetRegistry::loadBankAbsolute(
        const std::filesystem::path &absolutePath, const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mBankIndexByPath.find(key); it != mBankIndexByPath.end())
        {
            return mBanks[it->second];
        }

        BankAsset asset {.assetId      = assetIdFor(key, "audio-bank"),
                         .relativePath = key,
                         .label        = relativePath.filename().string()};

        if(mAudioEngine != nullptr)
        {
            if(!mAudioEngine->IsInitialized())
            {
                (void)mAudioEngine->Initialize();
            }
            std::vector<std::string> events;
            if(!mAudioEngine->LoadBank(absolutePath, events))
            {
                if(mLogger)
                {
                    mLogger->LogError("Failed to load audio bank '{}'", absolutePath.string());
                }
                return std::nullopt;
            }
            asset.eventPaths = std::move(events);
        }

        mBankIndexByPath.emplace(key, mBanks.size());
        mBanks.push_back(std::move(asset));

        if(mLogger)
        {
            const auto &stored = mBanks.back();
            mLogger->LogInformation("Loaded bank '{}' ({} events)", key, stored.eventPaths.size());
        }

        return mBanks.back();
    }

    std::optional<AudioClipAsset> AssetRegistry::loadAudioClipAbsolute(
        const std::filesystem::path &absolutePath, const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mAudioClipIndexByPath.find(key); it != mAudioClipIndexByPath.end())
        {
            return mAudioClips[it->second];
        }

        AudioClipAsset asset {.assetId      = assetIdFor(key, "audio-clip"),
                              .relativePath = key,
                              .label        = relativePath.filename().string()};

        if(mAudioEngine != nullptr)
        {
            if(!mAudioEngine->IsInitialized())
            {
                (void)mAudioEngine->Initialize();
            }
            if(const auto waveform = mAudioEngine->DecodeWaveform(absolutePath))
            {
                asset.durationSec = waveform->durationSec;
            }
        }

        mAudioClipIndexByPath.emplace(key, mAudioClips.size());
        mAudioClips.push_back(std::move(asset));

        if(mLogger)
        {
            mLogger->LogInformation("Loaded audio clip '{}'", key);
        }

        return mAudioClips.back();
    }

    std::optional<BankAsset> AssetRegistry::ImportBank(const std::filesystem::path &sourcePath)
    {
        if(!std::filesystem::is_regular_file(sourcePath))
        {
            if(mLogger)
            {
                mLogger->LogError("Bank path is not a file: {}", sourcePath.string());
            }
            return std::nullopt;
        }

        const auto relative = copyIntoResources(sourcePath, "Audio/Banks");
        if(relative.empty())
        {
            return std::nullopt;
        }

        return loadBankAbsolute(ToAbsoluteResourcePath(relative), relative);
    }

    std::optional<BankAsset> AssetRegistry::LoadBank(const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mBankIndexByPath.find(key); it != mBankIndexByPath.end())
        {
            return mBanks[it->second];
        }

        const auto absolute = ToAbsoluteResourcePath(key);
        if(!std::filesystem::is_regular_file(absolute))
        {
            if(mLogger)
            {
                mLogger->LogError("Bank resource not found: {}", absolute.string());
            }
            return std::nullopt;
        }

        return loadBankAbsolute(absolute, key);
    }

    std::optional<AudioClipAsset> AssetRegistry::ImportAudioClip(
        const std::filesystem::path &sourcePath)
    {
        if(!std::filesystem::is_regular_file(sourcePath))
        {
            if(mLogger)
            {
                mLogger->LogError("Audio clip path is not a file: {}", sourcePath.string());
            }
            return std::nullopt;
        }

        const auto relative = copyIntoResources(sourcePath, "Audio/Clips");
        if(relative.empty())
        {
            return std::nullopt;
        }

        return loadAudioClipAbsolute(ToAbsoluteResourcePath(relative), relative);
    }

    std::optional<AudioClipAsset> AssetRegistry::LoadAudioClip(
        const std::filesystem::path &relativePath)
    {
        const auto key = normalizeRelativeKey(relativePath);
        if(const auto it = mAudioClipIndexByPath.find(key); it != mAudioClipIndexByPath.end())
        {
            return mAudioClips[it->second];
        }

        const auto absolute = ToAbsoluteResourcePath(key);
        if(!std::filesystem::is_regular_file(absolute))
        {
            if(mLogger)
            {
                mLogger->LogError("Audio clip resource not found: {}", absolute.string());
            }
            return std::nullopt;
        }

        return loadAudioClipAbsolute(absolute, key);
    }

    const BankAsset *AssetRegistry::FindBank(std::string_view relativePath) const
    {
        const auto key = normalizeRelativeKey(relativePath);
        const auto it  = mBankIndexByPath.find(key);
        if(it == mBankIndexByPath.end())
        {
            return nullptr;
        }
        return &mBanks[it->second];
    }

    const AudioClipAsset *AssetRegistry::FindAudioClip(std::string_view relativePath) const
    {
        const auto key = normalizeRelativeKey(relativePath);
        const auto it  = mAudioClipIndexByPath.find(key);
        if(it == mAudioClipIndexByPath.end())
        {
            return nullptr;
        }
        return &mAudioClips[it->second];
    }

    std::vector<std::string> AssetRegistry::GetAllEventPaths() const
    {
        std::vector<std::string> events;
        for(const auto &bank : mBanks)
        {
            events.insert(events.end(), bank.eventPaths.begin(), bank.eventPaths.end());
        }
        std::ranges::sort(events);
        events.erase(std::unique(events.begin(), events.end()), events.end());
        return events;
    }

    std::uint32_t AssetRegistry::CreateMaterial(const fra::MaterialCreateInfo &createInfo,
                                                std::string name, bool listInBrowser)
    {
        std::uint32_t materialId = 0;
        if(mMaterialPool == nullptr)
        {
            materialId = ++mCatalogMaterialSeq;
        }
        else
        {
            materialId = FromHandle(mMaterialPool->Create(createInfo));
        }

        if(listInBrowser)
        {
            if(name.empty())
            {
                name = std::format("Material {}", materialId);
            }
            mMaterials.push_back(MaterialAsset {.name = std::move(name), .materialId = materialId});
        }
        return materialId;
    }

    void AssetRegistry::forgetSharedMaterialLocked(std::uint32_t materialId)
    {
        mSharedMaterialIds.erase(materialId);
        for(auto it = mSharedMaterialByHash.begin(); it != mSharedMaterialByHash.end();)
        {
            if(it->second == materialId)
            {
                it = mSharedMaterialByHash.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::uint32_t AssetRegistry::GetOrCreateSharedMaterial(const fra::MaterialCreateInfo &createInfo)
    {
        const auto hash = HashMaterialCreateInfo(createInfo);

        std::lock_guard lock(mMaterialCacheMutex);
        if(const auto it = mSharedMaterialByHash.find(hash); it != mSharedMaterialByHash.end())
        {
            return it->second;
        }

        const auto materialId = CreateMaterial(createInfo, {}, false);
        mSharedMaterialByHash.emplace(hash, materialId);
        mSharedMaterialIds.insert(materialId);
        return materialId;
    }

    bool AssetRegistry::IsSharedMaterial(std::uint32_t materialId) const
    {
        std::lock_guard lock(mMaterialCacheMutex);
        return mSharedMaterialIds.contains(materialId);
    }

    std::uint32_t AssetRegistry::DuplicateMaterial(std::uint32_t materialId, std::string name)
    {
        return CreateMaterial(GetMaterialCreateInfo(materialId), std::move(name));
    }

    void AssetRegistry::UpdateMaterial(std::uint32_t materialId,
                                       const fra::MaterialCreateInfo &createInfo)
    {
        {
            std::lock_guard lock(mMaterialCacheMutex);
            forgetSharedMaterialLocked(materialId);
        }

        if(mMaterialPool == nullptr)
        {
            return;
        }
        mMaterialPool->Update(AsMaterialHandle(materialId), createInfo);
    }

    fra::MaterialCreateInfo AssetRegistry::GetMaterialCreateInfo(std::uint32_t materialId) const
    {
        if(mMaterialPool == nullptr)
        {
            return {};
        }
        return mMaterialPool->GetCreateInfo(AsMaterialHandle(materialId));
    }

    void AssetRegistry::catalogMaterialIfNew(std::uint32_t materialId, std::string name)
    {
        if(materialId == 0)
        {
            return;
        }
        for(const auto &existing : mMaterials)
        {
            if(existing.materialId == materialId)
            {
                return;
            }
        }
        mMaterials.push_back(MaterialAsset {.name = std::move(name), .materialId = materialId});
    }

    bool AssetRegistry::TryFindModelByMeshId(std::uint32_t meshId, ModelAsset &outModel,
                                             std::uint32_t &outSubmeshIndex) const
    {
        for(const auto &model : mModels)
        {
            for(std::size_t i = 0; i < model.submeshes.size(); ++i)
            {
                if(model.submeshes[i].meshId == meshId)
                {
                    outModel         = model;
                    outSubmeshIndex  = static_cast<std::uint32_t>(i);
                    return true;
                }
            }
        }
        return false;
    }

    bool AssetRegistry::TryGetMeshId(std::string_view relativePath, std::uint32_t submeshIndex,
                                     std::uint32_t &outMeshId)
    {
        const auto model = LoadModel(relativePath);
        if(!model || submeshIndex >= model->submeshes.size())
        {
            return false;
        }
        outMeshId = model->submeshes[submeshIndex].meshId;
        return true;
    }

    bool AssetRegistry::TryGetTexturePath(std::uint32_t textureId,
                                          std::string &outRelativePath) const
    {
        const auto it = mTexturePathById.find(textureId);
        if(it == mTexturePathById.end())
        {
            return false;
        }
        outRelativePath = it->second;
        return true;
    }

    bool AssetRegistry::TryGetTextureId(std::string_view relativePath, std::uint32_t &outTextureId)
    {
        const auto texture = LoadTexture(relativePath);
        if(!texture)
        {
            return false;
        }
        outTextureId = texture->textureId;
        return true;
    }

    const ModelAsset *AssetRegistry::FindModel(std::string_view relativePath) const
    {
        const auto key = normalizeRelativeKey(relativePath);
        const auto it  = mModelIndexByPath.find(key);
        if(it == mModelIndexByPath.end())
        {
            return nullptr;
        }
        return &mModels[it->second];
    }

    std::vector<const ModelAsset *> AssetRegistry::GetSkinnedModelsWithClips() const
    {
        std::vector<const ModelAsset *> result;
        for(const auto &model : mModels)
        {
            if(model.skinned && !model.clips.empty())
            {
                result.push_back(&model);
            }
        }
        return result;
    }

} // namespace FRIGGA_NAMESPACE
