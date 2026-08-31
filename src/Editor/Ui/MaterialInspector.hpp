#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"

#include <Freya/Freya.hpp>

#include <functional>
#include <imgui.h>
#include <optional>

namespace EditorMaterialUi
{
    enum class TextureSlot
    {
        Albedo,
        Normal,
        Roughness,
        Emissive,
        Metalness,
        Occlusion,
    };

    struct TextureSlotContext
    {
        skr::Arc<fg::AssetRegistry> assets;
        bool                          editingLocked = false;
        std::function<void(TextureSlot)> requestImport;
    };

    void DrawTextureSlot(const char *label, TextureSlot slot,
                         std::optional<std::uint32_t> &textureId, bool &changed,
                         const TextureSlotContext &ctx);

    bool DrawMaterialCreateInfo(fra::MaterialCreateInfo &info, bool editingLocked,
                                const TextureSlotContext &ctx);

    bool DrawMaterialAdvancedFields(fra::MaterialCreateInfo &info, bool editingLocked);

} // namespace EditorMaterialUi
