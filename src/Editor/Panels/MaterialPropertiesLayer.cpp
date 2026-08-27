#include "MaterialPropertiesLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Editor/Ui/MaterialInspector.hpp"

#include <SDL3/SDL_dialog.h>

namespace
{
    const SDL_DialogFileFilter kTextureFilters[] = {
        {"Images", "png;jpg;jpeg;tga;bmp;hdr;exr"},
        {"All files", "*"},
    };
} // namespace

MaterialPropertiesLayer::MaterialPropertiesLayer(
    skr::Arc<fg::AssetRegistry> assets, skr::Arc<fg::PrimitiveMeshFactory> primitives,
    skr::Arc<MaterialSelectionContext> materialSelection,
    skr::Arc<fg::SceneSimulationState> simulation, skr::Arc<fra::Window> window)
    : Layer("Material Properties"), mAssets(std::move(assets)), mPrimitives(std::move(primitives)),
      mMaterialSelection(std::move(materialSelection)), mSimulation(std::move(simulation)),
      mWindow(std::move(window))
{
}

void MaterialPropertiesLayer::onUpdate()
{
    processPendingTextureImport();
}

void MaterialPropertiesLayer::onGui()
{
    const auto title = EditorDock::WindowId(getName().c_str());
    if(!ImGui::Begin(title.c_str()))
    {
        ImGui::End();
        return;
    }

    if(!mMaterialSelection->HasSelection())
    {
        ImGui::TextDisabled("Select a material in the Materials panel.");
        ImGui::End();
        return;
    }

    const auto materialId      = mMaterialSelection->Get();
    const auto defaultMaterial = mPrimitives->GetDefaultMaterial();
    const bool isDefault       = materialId == defaultMaterial;
    const bool locked          = mSimulation->IsPlaying();

    ImGui::Text("Material ID: %u%s", materialId, isDefault ? " (Default, shared)" : "");
    if(isDefault)
    {
        ImGui::TextDisabled("Duplicate before editing factors/maps.");
    }

    ImGui::BeginDisabled(locked);
    if(isDefault && ImGui::Button("Duplicate"))
    {
        const auto id = mAssets->DuplicateMaterial(materialId, "Unique Material");
        mMaterialSelection->Select(id);
    }
    ImGui::EndDisabled();

    if(mMaterialSelection->Get() != materialId)
    {
        ImGui::End();
        return;
    }

    auto info = mPrimitives->GetMaterialCreateInfo(mMaterialSelection->Get());

    EditorMaterialUi::TextureSlotContext textureCtx {
        .assets         = mAssets,
        .editingLocked  = locked || isDefault,
        .requestImport  = [this](EditorMaterialUi::TextureSlot slot) {
            requestTextureImport(slot);
        },
    };

    ImGui::BeginDisabled(locked || isDefault);
    if(EditorMaterialUi::DrawMaterialCreateInfo(info, locked || isDefault, textureCtx))
    {
        mAssets->UpdateMaterial(mMaterialSelection->Get(), info);
    }
    ImGui::EndDisabled();

    ImGui::End();
}

void MaterialPropertiesLayer::requestTextureImport(EditorMaterialUi::TextureSlot slot)
{
    {
        std::lock_guard lock(mDialogMutex);
        mPendingTextureSlot    = slot;
        mHasPendingTextureSlot = true;
        mPendingTexturePath.reset();
    }

    SDL_ShowOpenFileDialog(onTextureDialog, this,
                           static_cast<SDL_Window *>(mWindow->NativeWindow()), kTextureFilters,
                           static_cast<int>(std::size(kTextureFilters)), nullptr, false);
}

void MaterialPropertiesLayer::onTextureDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<MaterialPropertiesLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        std::lock_guard lock(self->mDialogMutex);
        self->mHasPendingTextureSlot = false;
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingTexturePath = filelist[0];
}

void MaterialPropertiesLayer::processPendingTextureImport()
{
    EditorMaterialUi::TextureSlot slot = EditorMaterialUi::TextureSlot::Albedo;
    std::filesystem::path path;
    {
        std::lock_guard lock(mDialogMutex);
        if(!mHasPendingTextureSlot || !mPendingTexturePath)
        {
            return;
        }
        slot                     = mPendingTextureSlot;
        path                     = *mPendingTexturePath;
        mHasPendingTextureSlot   = false;
        mPendingTexturePath.reset();
    }

    if(mSimulation->IsPlaying() || !mMaterialSelection->HasSelection())
    {
        return;
    }

    const auto texture = mAssets->ImportTexture(path);
    if(!texture)
    {
        return;
    }

    const auto materialId = mMaterialSelection->Get();
    if(materialId == mPrimitives->GetDefaultMaterial())
    {
        mMaterialSelection->Select(
            mAssets->DuplicateMaterial(materialId, "Unique Material"));
    }

    auto info = mPrimitives->GetMaterialCreateInfo(mMaterialSelection->Get());
    switch(slot)
    {
    case EditorMaterialUi::TextureSlot::Albedo:
        info.albedo = texture->textureId;
        break;
    case EditorMaterialUi::TextureSlot::Normal:
        info.normal = texture->textureId;
        break;
    case EditorMaterialUi::TextureSlot::Roughness:
        info.roughness = texture->textureId;
        break;
    case EditorMaterialUi::TextureSlot::Emissive:
        info.emissive = texture->textureId;
        break;
    case EditorMaterialUi::TextureSlot::Metalness:
        info.metalness = texture->textureId;
        break;
    case EditorMaterialUi::TextureSlot::Occlusion:
        info.occlusion = texture->textureId;
        break;
    }
    mAssets->UpdateMaterial(mMaterialSelection->Get(), info);
}
