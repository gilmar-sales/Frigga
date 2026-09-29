#pragma once

#include <Frigga/Macro.hpp>

// Forward declarations for asset catalog types. Headers that only hold
// `skr::Arc<AssetRegistry>` or take `const ModelAsset &` should include this
// instead of AssetRegistry.hpp, so catalog API changes do not rebuild them.
namespace FRIGGA_NAMESPACE
{

    class AssetRegistry;
    struct ModelAsset;
    struct TextureAsset;
    struct MaterialAsset;
    struct FontAsset;
    struct BankAsset;
    struct AudioClipAsset;

} // namespace FRIGGA_NAMESPACE
