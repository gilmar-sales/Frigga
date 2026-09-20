#pragma once

#include "Frigga/Asset/AssetRegistry.hpp"

#include <imgui.h>
#include <string>

namespace EditorFontUi
{
    /// Combo over warmed FontAssets. Writes FontAsset.assetId into @p fontId.
    inline bool DrawFontCombo(const char *label, std::string &fontId,
                              const skr::Arc<fg::AssetRegistry> &assets, bool editingLocked)
    {
        if(!assets)
        {
            ImGui::TextDisabled("%s (no assets)", label);
            return false;
        }

        const fg::FontAsset *current = assets->FindFontAsset(fontId);
        const char *preview = "(none)";
        std::string previewOwned;
        if(current)
        {
            previewOwned = current->label.empty() ? current->relativePath : current->label;
            preview      = previewOwned.c_str();
        }
        else if(!fontId.empty())
        {
            previewOwned = fontId;
            preview      = previewOwned.c_str();
        }

        bool changed = false;
        ImGui::BeginDisabled(editingLocked);
        ImGui::PushID(label);
        if(ImGui::BeginCombo(label, preview))
        {
            for(const auto &font : assets->GetFonts())
            {
                if(!font.atlas || !font.atlas->Valid())
                {
                    continue;
                }
                const bool selected = font.assetId == fontId;
                const auto itemLabel =
                    font.label.empty() ? font.relativePath.c_str() : font.label.c_str();
                if(ImGui::Selectable(itemLabel, selected))
                {
                    fontId  = font.assetId;
                    changed = true;
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        ImGui::EndDisabled();
        return changed;
    }
} // namespace EditorFontUi
