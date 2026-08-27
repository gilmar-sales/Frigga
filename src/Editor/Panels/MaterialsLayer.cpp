#include "MaterialsLayer.hpp"

#include "Editor/DockLayout.hpp"

#include <format>

namespace
{
    struct MaterialDragPayload
    {
        std::uint32_t materialId = 0;
    };

    constexpr const char *kMaterialDragPayloadId = "FRIGGA_MATERIAL";
} // namespace

MaterialsLayer::MaterialsLayer(skr::Arc<fg::AssetRegistry> assets,
                               skr::Arc<fg::PrimitiveMeshFactory> primitives,
                               skr::Arc<MaterialSelectionContext> materialSelection,
                               skr::Arc<fg::SceneSimulationState> simulation)
    : Layer("Materials"), mAssets(std::move(assets)), mPrimitives(std::move(primitives)),
      mMaterialSelection(std::move(materialSelection)), mSimulation(std::move(simulation))
{
}

void MaterialsLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    ImGui::InputTextWithHint("Filter", "material name...", mFilter, sizeof(mFilter));

    const bool locked = mSimulation->IsPlaying();
    ImGui::BeginDisabled(locked);
    if(ImGui::Button("New Material"))
    {
        const auto source = mPrimitives->GetMaterialCreateInfo(mPrimitives->GetDefaultMaterial());
        const auto name   = std::format("Material {}", mAssets->GetMaterials().size() + 1);
        const auto id     = mAssets->CreateMaterial(source, name);
        mMaterialSelection->Select(id);
    }
    ImGui::SameLine();
    if(ImGui::Button("Duplicate") && mMaterialSelection->HasSelection())
    {
        const auto id =
            mAssets->DuplicateMaterial(mMaterialSelection->Get(), "Duplicated Material");
        mMaterialSelection->Select(id);
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    const std::string filter = mFilter;
    const auto defaultMaterial = mPrimitives->GetDefaultMaterial();

    const auto drawEntry = [&](const char *label, std::uint32_t materialId) {
        if(!filter.empty() && std::string_view {label}.find(filter) == std::string::npos)
        {
            return;
        }

        ImGui::PushID(static_cast<int>(materialId));
        const bool selected =
            mMaterialSelection->HasSelection() && mMaterialSelection->Get() == materialId;
        if(ImGui::Selectable(label, selected))
        {
            mMaterialSelection->Select(materialId);
        }

        if(ImGui::BeginDragDropSource())
        {
            MaterialDragPayload payload {.materialId = materialId};
            ImGui::SetDragDropPayload(kMaterialDragPayloadId, &payload, sizeof(payload));
            ImGui::TextUnformatted(label);
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    };

    drawEntry("Default Material", defaultMaterial);
    for(const auto &material : mAssets->GetMaterials())
    {
        drawEntry(material.name.c_str(), material.materialId);
    }

    ImGui::End();
}
