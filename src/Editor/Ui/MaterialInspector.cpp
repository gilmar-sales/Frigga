#include "MaterialInspector.hpp"

#include <algorithm>
#include <format>

namespace EditorMaterialUi
{
    void DrawTextureSlot(const char *label, TextureSlot slot,
                         std::optional<std::uint32_t> &textureId, bool &changed,
                         const TextureSlotContext &ctx)
    {
        std::string pathLabel = "(none)";
        if(textureId)
        {
            std::string path;
            if(ctx.assets->TryGetTexturePath(*textureId, path))
            {
                pathLabel = path;
            }
            else
            {
                pathLabel = std::format("id {}", *textureId);
            }
        }

        ImGui::PushID(label);
        ImGui::Text("%s", label);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", pathLabel.c_str());
        ImGui::BeginDisabled(ctx.editingLocked);
        if(ImGui::Button("Import..."))
        {
            if(ctx.requestImport)
            {
                ctx.requestImport(slot);
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!textureId.has_value());
        if(ImGui::Button("Clear"))
        {
            textureId.reset();
            changed = true;
        }
        ImGui::EndDisabled();

        if(ImGui::BeginCombo("##pick", "From library..."))
        {
            for(const auto &texture : ctx.assets->GetTextures())
            {
                const bool selected = textureId && *textureId == texture.textureId;
                if(ImGui::Selectable(texture.relativePath.c_str(), selected))
                {
                    textureId = texture.textureId;
                    changed   = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    bool DrawMaterialCreateInfo(fra::MaterialCreateInfo &info, bool editingLocked,
                                const TextureSlotContext &ctx)
    {
        bool changed = false;

        ImGui::BeginDisabled(editingLocked);
        changed |= ImGui::ColorEdit4("Albedo", &info.albedoFactor.x);
        changed |= ImGui::DragFloat("Roughness", &info.roughnessFactor, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Metalness", &info.metalnessFactor, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::ColorEdit3("Emissive", &info.emissiveFactor.x);
        changed |= ImGui::DragFloat("AO Factor", &info.aoFactor, 0.01f, 0.0f, 1.0f);

        int alphaMode = static_cast<int>(info.alphaMode);
        if(ImGui::Combo("Alpha Mode", &alphaMode, "Opaque\0Mask\0Blend\0"))
        {
            info.alphaMode = static_cast<fra::AlphaMode>(std::clamp(alphaMode, 0, 2));
            changed        = true;
        }
        if(info.alphaMode == fra::AlphaMode::Mask)
        {
            changed |= ImGui::DragFloat("Alpha Cutoff", &info.alphaCutoff, 0.01f, 0.0f, 1.0f);
        }

        ImGui::SeparatorText("Maps");
        DrawTextureSlot("Albedo Map", TextureSlot::Albedo, info.albedo, changed, ctx);
        DrawTextureSlot("Normal Map", TextureSlot::Normal, info.normal, changed, ctx);
        DrawTextureSlot("Roughness Map", TextureSlot::Roughness, info.roughness, changed, ctx);
        DrawTextureSlot("Metalness Map", TextureSlot::Metalness, info.metalness, changed, ctx);
        DrawTextureSlot("Emissive Map", TextureSlot::Emissive, info.emissive, changed, ctx);
        DrawTextureSlot("Occlusion Map", TextureSlot::Occlusion, info.occlusion, changed, ctx);

        changed |= DrawMaterialAdvancedFields(info, editingLocked);
        ImGui::EndDisabled();

        return changed;
    }

    bool DrawMaterialAdvancedFields(fra::MaterialCreateInfo &info, bool editingLocked)
    {
        bool changed = false;

        ImGui::SeparatorText("Advanced");
        ImGui::BeginDisabled(editingLocked);
        changed |= ImGui::DragFloat("Clearcoat", &info.clearcoat, 0.01f, 0.0f, 1.0f);
        changed |=
            ImGui::DragFloat("Clearcoat Roughness", &info.clearcoatRoughness, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("Transmission", &info.transmission, 0.01f, 0.0f, 1.0f);
        changed |= ImGui::DragFloat("IOR", &info.ior, 0.01f, 1.0f, 2.5f);
        changed |= ImGui::Checkbox("Unlit", &info.unlit);
        changed |= ImGui::Checkbox("Double Sided", &info.doubleSided);
        changed |= ImGui::Checkbox("Receive Shadows", &info.receiveShadows);
        changed |= ImGui::Checkbox("Packed MR", &info.packedMetallicRoughness);
        ImGui::BeginDisabled(true);
        ImGui::InputScalar("Technique ID", ImGuiDataType_U32, &info.techniqueId);
        ImGui::EndDisabled();
        ImGui::EndDisabled();

        return changed;
    }

} // namespace EditorMaterialUi
