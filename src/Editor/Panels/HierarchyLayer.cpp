#include "HierarchyLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/Components/UserDataComponent.hpp"

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <format>
#include <imgui.h>
#include <sstream>

namespace
{
    const SDL_DialogFileFilter kTextureFilters[] = {
        {"Images", "png;jpg;jpeg;tga;bmp;hdr;webp"},
        {"All files", "*"},
    };

    glm::quat LookDown()
    {
        return glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f}, glm::vec3 {0.0f, 0.0f, 1.0f});
    }

    void DrawNamedProperty(fg::NamedProperty &property)
    {
        ImGui::PushID(property.name.c_str());
        auto &value = property.value;
        switch(value.kind)
        {
        case fg::PropertyKind::Bool:
            ImGui::Checkbox(property.name.c_str(), &value.boolValue);
            break;
        case fg::PropertyKind::Int64:
        {
            int asInt = static_cast<int>(value.intValue);
            if(ImGui::InputInt(property.name.c_str(), &asInt))
            {
                value.intValue = asInt;
            }
            break;
        }
        case fg::PropertyKind::Float:
            ImGui::DragFloat(property.name.c_str(), &value.floatValue, 0.01f);
            break;
        case fg::PropertyKind::String:
        {
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%s", value.stringValue.c_str());
            if(ImGui::InputText(property.name.c_str(), buffer, sizeof(buffer)))
            {
                value.stringValue = buffer;
            }
            break;
        }
        case fg::PropertyKind::Vec2:
            ImGui::DragFloat2(property.name.c_str(), &value.vec2Value[0], 0.01f);
            break;
        case fg::PropertyKind::Vec3:
            ImGui::DragFloat3(property.name.c_str(), &value.vec3Value[0], 0.01f);
            break;
        case fg::PropertyKind::Vec4:
            ImGui::DragFloat4(property.name.c_str(), &value.vec4Value[0], 0.01f);
            break;
        }
        ImGui::PopID();
    }
} // namespace

HierarchyLayer::HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::PrimitiveMeshFactory> primitives,
                               skr::Arc<fg::AssetRegistry> assets,
                               skr::Arc<SelectionContext> selection,
                               skr::Arc<fg::SceneSimulationState> simulation,
                               skr::Arc<fra::Window> window,
                               skr::Arc<fg::UserComponentRegistry> userComponents)
    : mRegistry(std::move(registry)), mScene(std::move(scene)),
      mPrimitives(std::move(primitives)), mAssets(std::move(assets)),
      mSelection(std::move(selection)), mSimulation(std::move(simulation)),
      mWindow(std::move(window)), mUserComponents(std::move(userComponents)),
      nodeToRename(SelectionContext::Invalid)
{
}

const char *HierarchyLayer::getLightDisplayName(fra::LightType type)
{
    switch(type)
    {
        case fra::LightType::Point:
            return "Point Light";
        case fra::LightType::Directional:
            return "Directional Light";
        case fra::LightType::Spot:
            return "Spot Light";
        case fra::LightType::Area:
            return "Area Light";
    }
    return "Light";
}

const char *HierarchyLayer::getLightIcon(fra::LightType type)
{
    switch(type)
    {
        case fra::LightType::Point:
            return ICON_BTSP_BRIGHTNESSHIGH;
        case fra::LightType::Directional:
            return ICON_BTSP_SUN;
        case fra::LightType::Spot:
            return ICON_BTSP_LIGHT;
        case fra::LightType::Area:
            return ICON_BTSP_BOUNDINGBOX;
    }
    return ICON_BTSP_LIGHT;
}

fg::LightComponent HierarchyLayer::makeDefaultLight(fra::LightType type)
{
    fg::LightComponent light {.type = type};
    switch(type)
    {
        case fra::LightType::Point:
            light.intensity = 15.0f;
            light.radius    = 30.0f;
            break;
        case fra::LightType::Directional:
            light.intensity   = 0.8f;
            light.castShadows = true;
            break;
        case fra::LightType::Spot:
            light.intensity         = 12.0f;
            light.radius            = 40.0f;
            light.innerAngleDegrees = 18.0f;
            light.outerAngleDegrees = 28.0f;
            break;
        case fra::LightType::Area:
            light.intensity  = 3.0f;
            light.color      = {1.0f, 0.95f, 0.9f};
            light.halfWidth  = 0.8f;
            light.halfHeight = 0.8f;
            break;
    }
    return light;
}

fg::TransformComponent HierarchyLayer::makeDefaultLightTransform(fra::LightType type)
{
    switch(type)
    {
        case fra::LightType::Point:
            return fg::TransformComponent {.position = {2.0f, 3.0f, 2.0f},
                                           .scale    = {1.0f, 1.0f, 1.0f},
                                           .rotation = LookDown()};
        case fra::LightType::Directional:
        {
            const glm::vec3 dir = glm::normalize(glm::vec3 {-0.4f, -1.0f, -0.3f});
            return fg::TransformComponent {
                .position = {0.0f, 4.0f, 0.0f},
                .scale    = {1.0f, 1.0f, 1.0f},
                .rotation = glm::quatLookAt(dir, glm::vec3 {0.0f, 1.0f, 0.0f})};
        }
        case fra::LightType::Spot:
        {
            const glm::vec3 position {2.0f, 4.0f, 2.0f};
            const glm::vec3 dir = glm::normalize(glm::vec3 {0.0f, 0.0f, 0.0f} - position);
            return fg::TransformComponent {
                .position = position,
                .scale    = {1.0f, 1.0f, 1.0f},
                .rotation = glm::quatLookAt(dir, glm::vec3 {0.0f, 1.0f, 0.0f})};
        }
        case fra::LightType::Area:
            return fg::TransformComponent {.position = {0.0f, 3.5f, 0.0f},
                                           .scale    = {1.0f, 1.0f, 1.0f},
                                           .rotation = LookDown()};
    }
    return fg::TransformComponent {.position = {0.0f, 3.0f, 0.0f},
                                   .scale    = {1.0f, 1.0f, 1.0f},
                                   .rotation = LookDown()};
}

fg::RigidBodyComponent HierarchyLayer::makeDefaultRigidBody(fr::Entity entity) const
{
    fg::RigidBodyComponent rigidBody {};
    mRegistry->TryGetComponents<fg::MeshComponent>(entity, [&](fg::MeshComponent &mesh) {
        fg::PrimitiveType primitive = fg::PrimitiveType::Cube;
        if(!mPrimitives->TryFindPrimitive(mesh.meshId, primitive))
        {
            return;
        }

        switch(primitive)
        {
        case fg::PrimitiveType::Cube:
            rigidBody.shape = fg::ColliderShape::Box;
            break;
        case fg::PrimitiveType::Sphere:
            rigidBody.shape  = fg::ColliderShape::Sphere;
            rigidBody.radius = 0.5f;
            break;
        case fg::PrimitiveType::Capsule:
            rigidBody.shape  = fg::ColliderShape::Capsule;
            rigidBody.radius = 0.5f;
            rigidBody.height = 1.0f;
            break;
        case fg::PrimitiveType::Cylinder:
        case fg::PrimitiveType::Cone:
        case fg::PrimitiveType::Plane:
        case fg::PrimitiveType::Quad:
        case fg::PrimitiveType::Count:
            // Use mesh hull so scale matches the rendered primitive (Plane is 10x10, not 1x1).
            rigidBody.shape = fg::ColliderShape::Mesh;
            break;
        }
    });
    return rigidBody;
}

const char *HierarchyLayer::resolveEntityIcon(fr::Entity entity) const
{
    const char *icon = nullptr;
    mRegistry->TryGetComponents<fg::LightComponent>(entity, [&](fg::LightComponent &light) {
        icon = getLightIcon(light.type);
    });
    if(icon != nullptr)
    {
        return icon;
    }

    if(mRegistry->HasComponent<fg::CameraComponent>(entity))
    {
        return ICON_BTSP_CAMERAVIDEO;
    }
    if(mRegistry->HasComponent<fg::MeshComponent>(entity))
    {
        return ICON_BTSP_BOX;
    }
    if(mRegistry->HasComponent<fg::RigidBodyComponent>(entity))
    {
        return ICON_BTSP_BOUNDINGBOX;
    }
    return "";
}

void HierarchyLayer::createEmptyEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    mRegistry->CreateEntity(
        [](auto entity, fg::NameComponent &name) {
            std::stringstream entity_name;
            entity_name << "Empty(" << entity << ")";
            name.name = entity_name.str();
        },
        fg::NameComponent {});
}

void HierarchyLayer::createPrimitiveEntity(fg::PrimitiveType type)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto *displayName = fg::PrimitiveMeshFactory::GetDisplayName(type);
    const auto meshId       = mPrimitives->GetMesh(type);
    const auto materialId   = mPrimitives->GetDefaultMaterial();

    mRegistry->CreateEntity(fg::NameComponent {.name = displayName}, fg::TransformComponent {},
                            fg::MeshComponent {.meshId = meshId},
                            fg::MaterialComponent {.materialId = materialId});
}

void HierarchyLayer::createCameraEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    mRegistry->CreateEntity(fg::NameComponent {.name = "Camera"},
                            fg::TransformComponent {.position = {0.0f, 1.5f, -5.0f}},
                            fg::CameraComponent {});
}

void HierarchyLayer::createLightEntity(fra::LightType type)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    mRegistry->CreateEntity(fg::NameComponent {.name = getLightDisplayName(type)},
                            makeDefaultLightTransform(type), makeDefaultLight(type));
}

void HierarchyLayer::addRigidBodyToSelection()
{
    if(!mSelection->HasSelection() || mSimulation->IsPlaying())
    {
        return;
    }

    const auto entity = mSelection->Get();
    if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::TransformComponent {});
    }
    if(!mRegistry->HasComponent<fg::RigidBodyComponent>(entity))
    {
        mRegistry->AddComponents(entity, makeDefaultRigidBody(entity));
    }
}

void HierarchyLayer::addLightToSelection(fra::LightType type)
{
    if(!mSelection->HasSelection() || mSimulation->IsPlaying())
    {
        return;
    }

    addLightToEntity(mSelection->Get(), type);
}

void HierarchyLayer::addUserComponentToSelection(std::string_view typeId)
{
    if(!mSelection->HasSelection() || mSimulation->IsPlaying())
    {
        return;
    }
    addUserComponentToEntity(mSelection->Get(), typeId);
}

void HierarchyLayer::addUserComponentToEntity(fr::Entity entity, std::string_view typeId)
{
    if(mSimulation->IsPlaying() || typeId.empty() || !mUserComponents)
    {
        return;
    }

    const auto desc = mUserComponents->Find(typeId);
    if(!desc)
    {
        return;
    }

    auto makeInstance = [&]() -> fg::UserComponentInstance {
        if(!desc->defaultInstance.typeId.empty())
        {
            return desc->defaultInstance;
        }
        if(desc->makeDefault)
        {
            return desc->makeDefault();
        }
        return fg::UserComponentInstance {.typeId = std::string(typeId)};
    };

    if(!mRegistry->HasComponent<fg::UserDataComponent>(entity))
    {
        fg::UserDataComponent data {};
        data.instances.push_back(makeInstance());
        mRegistry->AddComponents(entity, std::move(data));
        // Freyr defers archetype migrations; flush before inspector reads the bag.
        mRegistry->ExecuteTasks();
        return;
    }

    mRegistry->TryGetComponents<fg::UserDataComponent>(entity, [&](fg::UserDataComponent &data) {
        if(fg::FindUserComponent(data, typeId))
        {
            return;
        }
        data.instances.push_back(makeInstance());
    });
}

void HierarchyLayer::drawGameplayAddComponentMenu(fr::Entity entity)
{
    if(!mUserComponents)
    {
        return;
    }

    const auto types = mUserComponents->GetTypes();
    if(types.empty())
    {
        return;
    }

    ImGui::Separator();
    if(!ImGui::BeginMenu("Gameplay"))
    {
        return;
    }

    for(const auto &type : types)
    {
        bool alreadyHas = false;
        if(mRegistry->HasComponent<fg::UserDataComponent>(entity))
        {
            mRegistry->TryGetComponents<fg::UserDataComponent>(
                entity, [&](fg::UserDataComponent &data) {
                    alreadyHas = fg::FindUserComponent(data, type.typeId) != nullptr;
                });
        }

        ImGui::BeginDisabled(alreadyHas);
        if(ImGui::MenuItem(type.displayName.c_str()))
        {
            addUserComponentToEntity(entity, type.typeId);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndMenu();
}

void HierarchyLayer::addLightToEntity(fr::Entity entity, fra::LightType type)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
    {
        mRegistry->AddComponents(entity, makeDefaultLightTransform(type));
    }
    if(!mRegistry->HasComponent<fg::LightComponent>(entity))
    {
        mRegistry->AddComponents(entity, makeDefaultLight(type));
    }
    else
    {
        mRegistry->TryGetComponents<fg::LightComponent>(
            entity, [type](fg::LightComponent &light) { light.type = type; });
    }
}

bool HierarchyLayer::isEntityLocked(fr::Entity entity) const
{
    if(mScene->IsMainCamera(entity))
    {
        return true;
    }

    bool locked = false;
    mRegistry->TryGetComponents<fg::CameraComponent>(entity, [&locked](fg::CameraComponent &camera) {
        locked = camera.locked;
    });
    return locked;
}

void HierarchyLayer::setPrimaryCamera(fr::Entity entity)
{
    mRegistry->CreateMutation()->Each<fg::CameraComponent>(
        [entity](auto candidate, fg::CameraComponent &camera) {
            camera.primary = (candidate == entity);
        });
}

void HierarchyLayer::drawTextureSlot(const char *label, PendingTextureSlot slot,
                                     std::optional<std::uint32_t> &textureId, bool &changed)
{
    std::string pathLabel = "(none)";
    if(textureId)
    {
        std::string path;
        if(mAssets->TryGetTexturePath(*textureId, path))
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
    if(ImGui::Button("Import..."))
    {
        requestTextureForSlot(slot);
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
        for(const auto &texture : mAssets->GetTextures())
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
    ImGui::PopID();
}

void HierarchyLayer::requestTextureForSlot(PendingTextureSlot slot)
{
    {
        std::lock_guard lock(mDialogMutex);
        mPendingTextureSlot   = slot;
        mPendingTextureEntity = mSelection->Get();
        mPendingTexturePath.reset();
    }

    SDL_ShowOpenFileDialog(onTextureDialog, this, mWindow->Get(), kTextureFilters,
                           static_cast<int>(std::size(kTextureFilters)), nullptr, false);
}

void HierarchyLayer::onTextureDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<HierarchyLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        std::lock_guard lock(self->mDialogMutex);
        self->mPendingTextureSlot = PendingTextureSlot::None;
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingTexturePath = filelist[0];
}

void HierarchyLayer::processPendingTextureImport()
{
    PendingTextureSlot slot = PendingTextureSlot::None;
    std::filesystem::path path;
    fr::Entity entity = SelectionContext::Invalid;
    {
        std::lock_guard lock(mDialogMutex);
        if(mPendingTextureSlot == PendingTextureSlot::None || !mPendingTexturePath)
        {
            return;
        }
        slot                  = mPendingTextureSlot;
        path                  = *mPendingTexturePath;
        entity                = mPendingTextureEntity;
        mPendingTextureSlot   = PendingTextureSlot::None;
        mPendingTexturePath.reset();
        mPendingTextureEntity = SelectionContext::Invalid;
    }

    if(entity == SelectionContext::Invalid || mSimulation->IsPlaying())
    {
        return;
    }

    const auto texture = mAssets->ImportTexture(path);
    if(!texture)
    {
        return;
    }

    mRegistry->TryGetComponents<fg::MaterialComponent>(entity, [&](fg::MaterialComponent &material) {
        if(material.materialId == mPrimitives->GetDefaultMaterial())
        {
            material.materialId =
                mAssets->DuplicateMaterial(material.materialId, "Unique Material");
        }

        auto info = mPrimitives->GetMaterialCreateInfo(material.materialId);
        switch(slot)
        {
        case PendingTextureSlot::Albedo:
            info.albedo = texture->textureId;
            break;
        case PendingTextureSlot::Normal:
            info.normal = texture->textureId;
            break;
        case PendingTextureSlot::Roughness:
            info.roughness = texture->textureId;
            break;
        case PendingTextureSlot::Emissive:
            info.emissive = texture->textureId;
            break;
        case PendingTextureSlot::Metalness:
            info.metalness = texture->textureId;
            break;
        case PendingTextureSlot::None:
            break;
        }
        mPrimitives->UpdateMaterial(material.materialId, info);
    });
}

void HierarchyLayer::onUpdate()
{
    processPendingTextureImport();
}

void HierarchyLayer::onGui()
{
    const auto hierarchyTitle  = EditorDock::WindowId("Hierarchy");
    const auto componentsTitle = EditorDock::WindowId("Components");

    ImGui::Begin(hierarchyTitle.c_str());

    if(ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
    {
        mSelection->Clear();
    }
    else if(ImGui::BeginPopupContextWindow("##HierarchyContext", 1))
    {
        ImGui::BeginDisabled(mSimulation->IsPlaying());
        if(ImGui::MenuItem("Empty entity"))
        {
            createEmptyEntity();
        }

        if(ImGui::BeginMenu("3D Object"))
        {
            using fg::PrimitiveType;
            for(auto type: {PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Capsule,
                            PrimitiveType::Cylinder, PrimitiveType::Cone, PrimitiveType::Plane,
                            PrimitiveType::Quad})
            {
                if(ImGui::MenuItem(fg::PrimitiveMeshFactory::GetDisplayName(type)))
                {
                    createPrimitiveEntity(type);
                }
            }
            ImGui::EndMenu();
        }

        if(ImGui::MenuItem("Camera"))
        {
            createCameraEntity();
        }

        if(ImGui::BeginMenu(ICON_BTSP_LIGHT " Light"))
        {
            for(auto type: {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot,
                            fra::LightType::Area})
            {
                const auto label =
                    std::format("{} {}", getLightIcon(type), getLightDisplayName(type));
                if(ImGui::MenuItem(label.c_str()))
                {
                    createLightEntity(type);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndDisabled();
        if(mSimulation->IsPlaying() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Stop Play mode to create entities");
        }

        ImGui::EndPopup();
    }

    mRegistry->CreateMutation()->Each<fg::NameComponent>(
        [this](auto entity, fg::NameComponent &name) { drawEntityNode(entity, name); });

    ImGui::End();

    ImGui::Begin(componentsTitle.c_str());

    if(mSelection->HasSelection()) drawComponents();

    ImGui::End();
}

void HierarchyLayer::drawEntityNode(fr::Entity entity, fg::NameComponent &name)
{
    const bool renaming = nodeToRename == entity;

    ImGuiTreeNodeFlags flags =
        ((mSelection->Get() == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
    if(renaming)
    {
        flags |= ImGuiTreeNodeFlags_AllowOverlap;
    }

    const char *icon = resolveEntityIcon(entity);
    bool        opened = false;
    if(renaming)
    {
        // Keep the icon in the row; the name is drawn by the overlay InputText.
        opened = icon[0] != '\0'
                     ? ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)),
                                        flags, "%s", icon)
                     : ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)),
                                        flags, "");
    }
    else
    {
        opened = icon[0] != '\0'
                     ? ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)),
                                        flags, "%s %s", icon, name.name.c_str())
                     : ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)),
                                        flags, "%s", name.name.c_str());
    }

    const ImVec2 rowMin       = ImGui::GetItemRectMin();
    const ImVec2 rowMax       = ImGui::GetItemRectMax();
    const ImVec2 backupCursor = ImGui::GetCursorScreenPos();

    const std::string popUpId  = std::format("##PopUp{}", entity);
    const std::string renameId = std::format("##Rename{}", entity);
    if(ImGui::BeginPopupContextItem(popUpId.data()))
    {
        const bool locked     = isEntityLocked(entity);
        const bool playLocked = mSimulation->IsPlaying();

        if(ImGui::MenuItem("Delete", nullptr, false, !locked && !playLocked))
        {
            mRegistry->DestroyEntity(entity);
            if(mSelection->Get() == entity) mSelection->Clear();
        }
        else if((locked || playLocked) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(playLocked ? "Stop Play mode to delete entities"
                                         : "Main Camera cannot be removed");
        }

        if(ImGui::MenuItem("Rename...", nullptr, false, !playLocked)) nodeToRename = entity;

        ImGui::BeginDisabled(playLocked);
        if(ImGui::MenuItem("Add transform"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
        }

        if(ImGui::MenuItem("Add mesh"))
        {
            if(!mRegistry->HasComponent<fg::MeshComponent>(entity))
            {
                mRegistry->AddComponents(
                    entity, fg::MeshComponent {.meshId = mPrimitives->GetMesh(fg::PrimitiveType::Cube)});
            }
            if(!mRegistry->HasComponent<fg::MaterialComponent>(entity))
            {
                mRegistry->AddComponents(
                    entity,
                    fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
            }
        }

        if(ImGui::MenuItem("Add material"))
        {
            if(!mRegistry->HasComponent<fg::MaterialComponent>(entity))
            {
                mRegistry->AddComponents(
                    entity,
                    fg::MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});
            }
        }

        if(ImGui::MenuItem("Add camera"))
        {
            if(!mRegistry->HasComponent<fg::CameraComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::CameraComponent {});
            }
        }

        if(ImGui::BeginMenu(ICON_BTSP_LIGHT " Add light"))
        {
            for(auto type: {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot,
                            fra::LightType::Area})
            {
                const auto label =
                    std::format("{} {}", getLightIcon(type), getLightDisplayName(type));
                if(ImGui::MenuItem(label.c_str()))
                {
                    addLightToEntity(entity, type);
                }
            }
            ImGui::EndMenu();
        }

        if(ImGui::MenuItem("Add rigid body"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::RigidBodyComponent>(entity))
            {
                mRegistry->AddComponents(entity, makeDefaultRigidBody(entity));
            }
        }

        if(ImGui::MenuItem("Add animator"))
        {
            if(!mRegistry->HasComponent<fg::AnimatorComponent>(entity))
            {
                fg::AnimatorComponent animator {};
                mRegistry->TryGetComponents<fg::MeshComponent>(entity, [&](fg::MeshComponent &mesh) {
                    fg::ModelAsset model {};
                    std::uint32_t submesh = 0;
                    if(mAssets->TryFindModelByMeshId(mesh.meshId, model, submesh) && model.skinned)
                    {
                        animator.modelSource = model.relativePath;
                    }
                });
                mRegistry->AddComponents(entity, std::move(animator));
            }
        }

        drawGameplayAddComponentMenu(entity);
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    if(!renaming && (ImGui::IsItemClicked() || ImGui::IsItemClicked(1)))
    {
        mSelection->Select(entity);
    }

    if(!renaming && !mSimulation->IsPlaying() && ImGui::IsItemHovered() &&
       ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        nodeToRename = entity;
    }

    if(renaming)
    {
        static char buffer[65] {};
        static fr::Entity bufferEntity = SelectionContext::Invalid;
        static bool       needsFocus   = false;
        if(bufferEntity != entity)
        {
            bufferEntity = entity;
            needsFocus   = true;
            buffer[0]    = '\0';
            mRegistry->TryGetComponents<fg::NameComponent>(
                entity, [](fg::NameComponent &name) {
                    std::strncpy(buffer, name.name.c_str(), sizeof(buffer) - 1);
                    buffer[sizeof(buffer) - 1] = '\0';
                });
        }

        float labelX = rowMin.x + ImGui::GetTreeNodeToLabelSpacing();
        if(icon[0] != '\0')
        {
            labelX += ImGui::CalcTextSize(icon).x + ImGui::CalcTextSize(" ").x;
        }

        const float rowHeight = rowMax.y - rowMin.y;
        const float framePadY =
            std::max(0.0f, (rowHeight - ImGui::GetFontSize()) * 0.5f);

        ImGui::SetCursorScreenPos(ImVec2(labelX, rowMin.y));
        ImGui::PushItemWidth(std::max(24.0f, rowMax.x - labelX));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, framePadY));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        // Blend with the selected tree-row background.
        const ImVec4 header =
            ImGui::GetStyleColorVec4(mSelection->Get() == entity ? ImGuiCol_Header
                                                                : ImGuiCol_FrameBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, header);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, header);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, header);

        if(needsFocus)
        {
            ImGui::SetKeyboardFocusHere();
            needsFocus = false;
        }

        const bool committed = ImGui::InputText(
            renameId.data(), buffer, sizeof(buffer),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();
        ImGui::SetCursorScreenPos(backupCursor);

        if(committed)
        {
            mRegistry->TryGetComponents<fg::NameComponent>(
                entity, [](fg::NameComponent &name) { name.name = buffer; });
            nodeToRename = SelectionContext::Invalid;
            bufferEntity = SelectionContext::Invalid;
        }
        else if(ImGui::IsItemDeactivated())
        {
            nodeToRename = SelectionContext::Invalid;
            bufferEntity = SelectionContext::Invalid;
        }
    }

    if(opened) ImGui::TreePop();
}

void HierarchyLayer::drawComponents()
{
    const fr::Entity selection = mSelection->Get();

    mRegistry->TryGetComponents<fg::TransformComponent>(
        selection, [](fg::TransformComponent &transform) {
            static glm::vec3 Rotation;

            if(ImGui::CollapsingHeader("Transform Component", nullptr, ImGuiWindowFlags_ChildWindow))
            {
                ImGui::DragFloat3("##Position", &transform.position[0], 0.1f);

                Rotation = glm::degrees(glm::eulerAngles(glm::normalize(transform.rotation)));

                if(ImGui::DragFloat3("##Rotation", &Rotation[0], 0.1f))
                {
                    if(Rotation.y > 90.f)
                    {
                        Rotation.x += 180;
                        Rotation.x += 180;
                    }
                    if(Rotation.x > 180)
                    {
                        Rotation.x = 360 - Rotation.x;
                    }

                    auto radVec = glm::radians(Rotation);

                    transform.rotation = glm::quat(radVec);
                }
                ImGui::DragFloat3("##Scale", &transform.scale[0], 0.1f);
            }
        });

    mRegistry->TryGetComponents<fg::CameraComponent>(
        selection, [this, selection](fg::CameraComponent &camera) {
            if(ImGui::CollapsingHeader("Camera Component", nullptr, ImGuiWindowFlags_ChildWindow))
            {
                ImGui::DragFloat("FOV", &camera.fovDegrees, 0.1f, 1.0f, 179.0f);
                ImGui::DragFloat("Near", &camera.nearPlane, 0.01f, 0.001f, 100.0f);
                ImGui::DragFloat("Far", &camera.farPlane, 1.0f, 1.0f, 10000.0f);

                bool primary = camera.primary;
                ImGui::BeginDisabled(camera.locked && camera.primary);
                if(ImGui::Checkbox("Primary", &primary))
                {
                    if(primary)
                    {
                        setPrimaryCamera(selection);
                    }
                    else if(!camera.locked)
                    {
                        camera.primary = false;
                    }
                }
                ImGui::EndDisabled();

                if(camera.locked)
                {
                    ImGui::TextDisabled("Locked (Main Camera)");
                }
            }
        });

    mRegistry->TryGetComponents<fg::LightComponent>(
        selection, [this, selection](fg::LightComponent &light) {
            bool open = true;
            if(ImGui::CollapsingHeader("Light Component", &open, ImGuiWindowFlags_ChildWindow))
            {
                int typeIndex = static_cast<int>(light.type);
                if(ImGui::Combo("Type", &typeIndex, "Point\0Directional\0Spot\0Area\0"))
                {
                    light.type = static_cast<fra::LightType>(typeIndex);
                }

                ImGui::ColorEdit3("Color", &light.color[0]);
                ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 1000.0f);
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Relative to IBL / exposure (Dir ~0.5–1, Area ~2–4, Point ~10–20)");
                }

                if(light.type == fra::LightType::Point || light.type == fra::LightType::Spot)
                {
                    ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.1f, 1000.0f);
                }

                if(light.type == fra::LightType::Spot)
                {
                    ImGui::DragFloat("Inner Angle", &light.innerAngleDegrees, 0.1f, 0.0f, 89.0f);
                    ImGui::DragFloat("Outer Angle", &light.outerAngleDegrees, 0.1f, 0.0f, 89.0f);
                    if(light.outerAngleDegrees < light.innerAngleDegrees)
                    {
                        light.outerAngleDegrees = light.innerAngleDegrees;
                    }
                }

                if(light.type == fra::LightType::Area)
                {
                    ImGui::DragFloat("Half Width", &light.halfWidth, 0.01f, 0.01f, 100.0f);
                    ImGui::DragFloat("Half Height", &light.halfHeight, 0.01f, 0.01f, 100.0f);
                }

                ImGui::Checkbox("Cast Shadows", &light.castShadows);
            }

            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::LightComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::MeshComponent>(selection, [this](fg::MeshComponent &mesh) {
        if(ImGui::CollapsingHeader("Mesh Component", nullptr, ImGuiWindowFlags_ChildWindow))
        {
            fg::PrimitiveType currentPrimitive = fg::PrimitiveType::Cube;
            const bool isPrimitive =
                mPrimitives->TryFindPrimitive(mesh.meshId, currentPrimitive);

            fg::ModelAsset currentModel {};
            std::uint32_t currentSubmesh = 0;
            const bool isImported =
                !isPrimitive &&
                mAssets->TryFindModelByMeshId(mesh.meshId, currentModel, currentSubmesh);

            std::string preview = "Unknown";
            if(isPrimitive)
            {
                preview = fg::PrimitiveMeshFactory::GetDisplayName(currentPrimitive);
            }
            else if(isImported)
            {
                preview = currentModel.meshIds.size() == 1
                              ? currentModel.label
                              : std::format("{} [{}]", currentModel.label, currentSubmesh);
            }

            if(ImGui::BeginCombo("Mesh", preview.c_str()))
            {
                if(ImGui::BeginMenu("Primitives"))
                {
                    for(std::uint8_t i = 0;
                        i < static_cast<std::uint8_t>(fg::PrimitiveType::Count); ++i)
                    {
                        const auto type     = static_cast<fg::PrimitiveType>(i);
                        const bool selected = isPrimitive && type == currentPrimitive;
                        if(ImGui::Selectable(fg::PrimitiveMeshFactory::GetDisplayName(type),
                                             selected))
                        {
                            mesh.meshId = mPrimitives->GetMesh(type);
                        }
                        if(selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndMenu();
                }

                if(ImGui::BeginMenu("Imported"))
                {
                    if(mAssets->GetModels().empty())
                    {
                        ImGui::TextDisabled("Import models in Resources");
                    }
                    for(const auto &model : mAssets->GetModels())
                    {
                        for(std::size_t i = 0; i < model.meshIds.size(); ++i)
                        {
                            const auto label =
                                model.meshIds.size() == 1
                                    ? model.label
                                    : std::format("{} [{}]", model.label, i);
                            const bool selected =
                                isImported && model.relativePath == currentModel.relativePath &&
                                static_cast<std::uint32_t>(i) == currentSubmesh;
                            if(ImGui::Selectable(label.c_str(), selected))
                            {
                                mesh.meshId = model.meshIds[i];
                            }
                            if(selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Cast Shadows", &mesh.castShadows);

            if(!isPrimitive)
            {
                ImGui::TextDisabled("Mesh ID: %u", mesh.meshId);
                if(isImported)
                {
                    ImGui::TextWrapped("Source: Resources/%s", currentModel.relativePath.c_str());
                }
            }
        }
    });

    mRegistry->TryGetComponents<fg::MaterialComponent>(
        selection, [this, selection](fg::MaterialComponent &material) {
            bool open = true;
            if(ImGui::CollapsingHeader("Material Component", &open, ImGuiWindowFlags_ChildWindow))
            {
                ImGui::BeginDisabled(mSimulation->IsPlaying());

                const auto defaultMaterial = mPrimitives->GetDefaultMaterial();
                const bool isDefault       = material.materialId == defaultMaterial;

                ImGui::Text("Material ID: %u%s", material.materialId,
                            isDefault ? " (Default, shared)" : "");
                if(isDefault)
                {
                    ImGui::TextDisabled("Make Unique before editing factors/maps.");
                }

                if(ImGui::Button("Assign Default"))
                {
                    material.materialId = defaultMaterial;
                }
                ImGui::SameLine();
                if(ImGui::Button("Make Unique"))
                {
                    material.materialId =
                        mAssets->DuplicateMaterial(material.materialId, "Unique Material");
                }

                if(!mAssets->GetMaterials().empty() && ImGui::BeginCombo("Library", "Assign..."))
                {
                    for(const auto &asset : mAssets->GetMaterials())
                    {
                        const bool selected = asset.materialId == material.materialId;
                        if(ImGui::Selectable(asset.name.c_str(), selected))
                        {
                            material.materialId = asset.materialId;
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::BeginDisabled(isDefault);
                auto info    = mPrimitives->GetMaterialCreateInfo(material.materialId);
                bool changed = false;
                changed |= ImGui::ColorEdit4("Albedo", &info.albedoFactor.x);
                changed |=
                    ImGui::DragFloat("Roughness", &info.roughnessFactor, 0.01f, 0.0f, 1.0f);
                changed |=
                    ImGui::DragFloat("Metalness", &info.metalnessFactor, 0.01f, 0.0f, 1.0f);
                changed |= ImGui::ColorEdit3("Emissive", &info.emissiveFactor.x);

                int alphaMode = static_cast<int>(info.alphaMode);
                if(ImGui::Combo("Alpha Mode", &alphaMode, "Opaque\0Mask\0Blend\0"))
                {
                    info.alphaMode = static_cast<fra::AlphaMode>(std::clamp(alphaMode, 0, 2));
                    changed        = true;
                }
                if(info.alphaMode == fra::AlphaMode::Mask)
                {
                    changed |=
                        ImGui::DragFloat("Alpha Cutoff", &info.alphaCutoff, 0.01f, 0.0f, 1.0f);
                }

                ImGui::SeparatorText("Maps");
                drawTextureSlot("Albedo Map", PendingTextureSlot::Albedo, info.albedo, changed);
                drawTextureSlot("Normal Map", PendingTextureSlot::Normal, info.normal, changed);
                drawTextureSlot("Roughness Map", PendingTextureSlot::Roughness, info.roughness,
                                changed);
                drawTextureSlot("Metalness Map", PendingTextureSlot::Metalness, info.metalness,
                                changed);
                drawTextureSlot("Emissive Map", PendingTextureSlot::Emissive, info.emissive,
                                changed);

                if(changed)
                {
                    mPrimitives->UpdateMaterial(material.materialId, info);
                }
                ImGui::EndDisabled();

                ImGui::EndDisabled();
            }

            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::MaterialComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::RigidBodyComponent>(
        selection, [this](fg::RigidBodyComponent &rigidBody) {
            if(ImGui::CollapsingHeader("Rigid Body Component", nullptr,
                                       ImGuiWindowFlags_ChildWindow))
            {
                ImGui::BeginDisabled(mSimulation->IsPlaying());
                int motion = static_cast<int>(rigidBody.motion);
                if(ImGui::Combo("Motion", &motion, "Static\0Kinematic\0Dynamic\0"))
                {
                    rigidBody.motion = static_cast<fg::BodyMotionType>(motion);
                    if(rigidBody.motion == fg::BodyMotionType::Static)
                    {
                        rigidBody.collisionLayer = 0;
                    }
                    else if(rigidBody.collisionLayer == 0)
                    {
                        rigidBody.collisionLayer = 1;
                    }
                }

                int shape = static_cast<int>(rigidBody.shape);
                if(ImGui::Combo("Collider", &shape, "Box\0Sphere\0Capsule\0Mesh\0"))
                {
                    rigidBody.shape = static_cast<fg::ColliderShape>(shape);
                }

                switch(rigidBody.shape)
                {
                case fg::ColliderShape::Box:
                    ImGui::DragFloat3("Half Extents", &rigidBody.halfExtents[0], 0.01f, 0.001f,
                                      1000.0f);
                    break;
                case fg::ColliderShape::Sphere:
                    ImGui::DragFloat("Radius", &rigidBody.radius, 0.01f, 0.001f, 1000.0f);
                    break;
                case fg::ColliderShape::Capsule:
                    ImGui::DragFloat("Radius", &rigidBody.radius, 0.01f, 0.001f, 1000.0f);
                    ImGui::DragFloat("Height", &rigidBody.height, 0.01f, 0.001f, 1000.0f);
                    break;
                case fg::ColliderShape::Mesh:
                    ImGui::TextDisabled("Convex hull from MeshComponent primitive");
                    break;
                }

                ImGui::BeginDisabled(rigidBody.motion == fg::BodyMotionType::Static);
                ImGui::DragFloat("Mass", &rigidBody.mass, 0.01f, 0.001f, 100000.0f);
                ImGui::EndDisabled();
                ImGui::DragFloat("Friction", &rigidBody.friction, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Restitution", &rigidBody.restitution, 0.01f, 0.0f, 1.0f);

                int layer = rigidBody.collisionLayer;
                if(ImGui::SliderInt("Collision Layer", &layer, 0, 15))
                {
                    rigidBody.collisionLayer = static_cast<std::uint8_t>(layer);
                }
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "Object layer 0..15. Static bodies are remapped to layer 0 on Play.");
                }

                ImGui::Text("Collide With Layers");
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "Collision is mutual: both layers must include each other in their masks.");
                }
                for(int bit = 0; bit < 16; ++bit)
                {
                    if(bit % 8 != 0)
                    {
                        ImGui::SameLine();
                    }
                    bool value = (rigidBody.collideWithLayers & (1u << bit)) != 0;
                    ImGui::PushID(bit);
                    const auto label = std::format("{}", bit);
                    if(ImGui::Checkbox(label.c_str(), &value))
                    {
                        if(value)
                        {
                            rigidBody.collideWithLayers |= static_cast<std::uint16_t>(1u << bit);
                        }
                        else
                        {
                            rigidBody.collideWithLayers &=
                                static_cast<std::uint16_t>(~(1u << bit));
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::TextDisabled("Mask: 0x%04X", rigidBody.collideWithLayers);
                ImGui::EndDisabled();

                if(rigidBody.body.IsValid())
                {
                    ImGui::TextDisabled("Body ID: %u", rigidBody.body.id);
                }
                else if(mSimulation->IsPlaying())
                {
                    ImGui::TextDisabled("Collider edits apply after Stop");
                }
            }
        });

    mRegistry->TryGetComponents<fg::AnimatorComponent>(
        selection, [this, selection](fg::AnimatorComponent &animator) {
            bool open = true;
            if(ImGui::CollapsingHeader("Animator Component", &open, ImGuiWindowFlags_ChildWindow))
            {
                ImGui::BeginDisabled(mSimulation->IsPlaying());

                char sourceBuf[256];
                std::snprintf(sourceBuf, sizeof(sourceBuf), "%s", animator.modelSource.c_str());
                if(ImGui::InputText("Model Source", sourceBuf, sizeof(sourceBuf)))
                {
                    animator.modelSource = sourceBuf;
                }

                if(ImGui::BeginCombo("Clip", animator.clipName.empty() ? "(first clip)"
                                                                       : animator.clipName.c_str()))
                {
                    if(ImGui::Selectable("(first clip)", animator.clipName.empty()))
                    {
                        animator.clipName.clear();
                        animator.timeSec = 0.0f;
                    }

                    if(const auto *model = mAssets->FindModel(animator.modelSource);
                       model != nullptr)
                    {
                        for(const auto &clip : model->clips)
                        {
                            const bool selected = animator.clipName == clip.name;
                            if(ImGui::Selectable(clip.name.c_str(), selected))
                            {
                                animator.clipName = clip.name;
                                animator.timeSec  = 0.0f;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::DragFloat("Speed", &animator.speed, 0.01f, 0.0f, 8.0f, "%.2f");
                ImGui::Checkbox("Playing", &animator.playing);
                ImGui::SameLine();
                ImGui::Checkbox("Loop", &animator.loop);
                ImGui::Checkbox("Use GPU", &animator.useGpu);
                ImGui::SameLine();
                ImGui::Checkbox("Preview in Edit", &animator.previewInEdit);
                ImGui::TextDisabled("Bones: offset %u count %u", animator.boneOffset,
                                    animator.boneCount);
                ImGui::EndDisabled();
            }

            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::AnimatorComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::UserDataComponent>(
        selection, [this, selection](fg::UserDataComponent &data) {
            for(std::size_t i = 0; i < data.instances.size();)
            {
                auto &instance = data.instances[i];
                std::string header = instance.typeId;
                if(mUserComponents)
                {
                    if(const auto desc = mUserComponents->Find(instance.typeId))
                    {
                        header = desc->displayName;
                    }
                }

                ImGui::PushID(static_cast<int>(i));
                bool open = true;
                if(ImGui::CollapsingHeader(header.c_str(), &open, ImGuiWindowFlags_ChildWindow))
                {
                    ImGui::BeginDisabled(mSimulation->IsPlaying());
                    for(auto &property : instance.properties)
                    {
                        DrawNamedProperty(property);
                    }
                    ImGui::EndDisabled();
                }

                if(!open && !mSimulation->IsPlaying())
                {
                    data.instances.erase(data.instances.begin() +
                                         static_cast<std::ptrdiff_t>(i));
                    ImGui::PopID();
                    if(data.instances.empty())
                    {
                        mRegistry->RemoveComponent<fg::UserDataComponent>(selection);
                        mRegistry->ExecuteTasks();
                        return;
                    }
                    continue;
                }

                ImGui::PopID();
                ++i;
            }
        });
}
