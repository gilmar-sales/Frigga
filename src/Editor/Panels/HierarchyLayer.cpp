#include "HierarchyLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Editor/Panels/ResourcesLayer.hpp"
#include "Editor/Ui/AudioComponentInspector.hpp"
#include "Editor/Ui/ComponentClipboard.hpp"
#include "Editor/UiScale.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/BillboardComponent.hpp"
#include "Frigga/ECS/Components/BillboardTextComponent.hpp"
#include "Frigga/ECS/Components/FullscreenEffectComponent.hpp"
#include "Frigga/ECS/Components/HealthBarComponent.hpp"
#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/ParticleEmitterComponent.hpp"
#include "Frigga/ECS/Components/PrefabComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/ECS/Components/UserDataComponent.hpp"
#include "Frigga/Module/FriComponentInspector.hpp"
#include "Frigga/Module/GameplayTypeIds.hpp"
#include "Frigga/Scene/Prefab.hpp"

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <format>
#include <map>
#include <sstream>
#include <system_error>
#include <imgui.h>
#include <unordered_set>
#include <vector>

namespace
{
    const SDL_DialogFileFilter kTextureFilters[] = {
        {"Images", "png;jpg;jpeg;tga;bmp;hdr;webp"},
        {"All files", "*"},
    };

    const SDL_DialogFileFilter kPrefabFilters[] = {
        {"Prefabs", "prefab"},
        {"All files", "*"},
    };

    constexpr std::string_view kThirdPersonCameraTypeId = fg::kThirdPersonCameraTypeId;
    constexpr std::string_view kCharacterControllerTypeId = fg::kCharacterControllerTypeId;

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
                               skr::Arc<fg::UserComponentRegistry> userComponents,
                               skr::Arc<ResourcesLayer> resources,
                               skr::Arc<fg::AudioController> audioController)
    : mRegistry(std::move(registry)), mScene(std::move(scene)),
      mPrimitives(std::move(primitives)), mAssets(std::move(assets)),
      mSelection(std::move(selection)), mSimulation(std::move(simulation)),
      mWindow(std::move(window)), mUserComponents(std::move(userComponents)),
      mResources(std::move(resources)), mAudioController(std::move(audioController)),
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
    if(mRegistry->HasComponent<fg::AudioListenerComponent>(entity))
    {
        return ICON_BTSP_MIC;
    }
    if(mRegistry->HasComponent<fg::AudioSourceComponent>(entity))
    {
        return ICON_BTSP_VOLUMEUP;
    }
    if(mRegistry->HasComponent<fg::FullscreenEffectComponent>(entity))
    {
        return ICON_BTSP_LAYERS;
    }
    if(mRegistry->HasComponent<fg::ParticleEmitterComponent>(entity))
    {
        return ICON_BTSP_STAR;
    }
    if(mRegistry->HasComponent<fg::BillboardComponent>(entity) ||
       mRegistry->HasComponent<fg::BillboardTextComponent>(entity) ||
       mRegistry->HasComponent<fg::HealthBarComponent>(entity))
    {
        return ICON_BTSP_IMAGE;
    }
    if(mRegistry->HasComponent<fg::PrefabComponent>(entity))
    {
        return ICON_BTSP_COLLECTION;
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

void HierarchyLayer::parentNewEntity(fr::Entity entity)
{
    if(!mSelection->HasSelection())
    {
        return;
    }
    const auto parent = mSelection->Get();
    if(parent == entity)
    {
        return;
    }
    mRegistry->ExecuteTasks();
    fg::TransformUtil::SetParent(*mRegistry, entity, parent, false);
}

void HierarchyLayer::createPrefabFromSelection()
{
    if(mSimulation->IsPlaying() || !mSelection->HasSelection())
    {
        return;
    }
    requestSavePrefabDialog(mSelection->Get());
}

void HierarchyLayer::requestSavePrefabDialog(fr::Entity entity)
{
    std::string name = "Prefab";
    mRegistry->TryGetComponents<fg::NameComponent>(
        entity, [&](fg::NameComponent &component) { name = component.name; });
    const auto stem = fg::Prefab::SanitizeFileStem(name);
    const auto path = (fg::Prefab::DefaultDirectory() / stem).replace_extension(".prefab");

    {
        std::lock_guard lock(mDialogMutex);
        mPendingPrefabEntity     = entity;
        mDialogDefaultLocation   = path.string();
    }

    std::error_code ec;
    std::filesystem::create_directories(fg::Prefab::DefaultDirectory(), ec);
    SDL_ShowSaveFileDialog(onPrefabSaveDialog, this,
                           static_cast<SDL_Window *>(mWindow->NativeWindow()), kPrefabFilters,
                           static_cast<int>(std::size(kPrefabFilters)),
                           mDialogDefaultLocation.c_str());
}

void HierarchyLayer::onPrefabSaveDialog(void *userdata, const char *const *filelist, int)
{
    auto *self = static_cast<HierarchyLayer *>(userdata);
    if(filelist == nullptr || filelist[0] == nullptr)
    {
        return;
    }

    std::lock_guard lock(self->mDialogMutex);
    self->mPendingPrefabPath = filelist[0];
}

void HierarchyLayer::processPendingPrefabSave()
{
    std::filesystem::path path;
    fr::Entity entity = SelectionContext::Invalid;
    {
        std::lock_guard lock(mDialogMutex);
        if(!mPendingPrefabPath)
        {
            return;
        }
        path                 = *mPendingPrefabPath;
        entity               = mPendingPrefabEntity;
        mPendingPrefabPath.reset();
        mPendingPrefabEntity = SelectionContext::Invalid;
    }

    if(entity == SelectionContext::Invalid)
    {
        return;
    }
    if(path.extension().empty())
    {
        path.replace_extension(".prefab");
    }

    if(fg::Prefab::Save(*mScene, entity, path))
    {
        if(mResources)
        {
            mResources->MarkDirty();
        }
        if(!mRegistry->HasComponent<fg::PrefabComponent>(entity))
        {
            auto relative = fg::AssetRegistry::MakeRelativeToResources(path);
            if(relative.empty())
            {
                relative = path.generic_string();
            }
            mRegistry->AddComponents(entity,
                                     fg::PrefabComponent {.source = relative.generic_string()});
        }
        else
        {
            auto relative = fg::AssetRegistry::MakeRelativeToResources(path);
            mRegistry->TryGetComponents<fg::PrefabComponent>(
                entity, [&](fg::PrefabComponent &prefab) {
                    prefab.source = relative.empty() ? path.generic_string()
                                                     : relative.generic_string();
                });
        }
    }
}

void HierarchyLayer::createEmptyEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    mRegistry->CreateEntity(
        [this](auto entity, fg::NameComponent &name) {
            std::stringstream entity_name;
            entity_name << "Empty(" << entity << ")";
            name.name = entity_name.str();
            parentNewEntity(entity);
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

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = displayName}, fg::TransformComponent {},
        fg::MeshComponent {.meshId = meshId},
        fg::MaterialComponent {.materialId = materialId});
    parentNewEntity(entity);
}

void HierarchyLayer::createCameraEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Camera"},
        fg::TransformComponent {.position = {0.0f, 1.5f, -5.0f}}, fg::CameraComponent {});
    parentNewEntity(entity);
}

void HierarchyLayer::createAudioSourceEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto entity = mRegistry->CreateEntity(fg::NameComponent {.name = "Audio Source"},
                                                fg::TransformComponent {},
                                                fg::AudioSourceComponent {});
    parentNewEntity(entity);
}

void HierarchyLayer::createAudioListenerEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto entity = mRegistry->CreateEntity(fg::NameComponent {.name = "Audio Listener"},
                                                fg::TransformComponent {},
                                                fg::AudioListenerComponent {});
    parentNewEntity(entity);
}

void HierarchyLayer::createLightEntity(fra::LightType type)
{
    if(mSimulation->IsPlaying())
    {
        return;
    }

    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = getLightDisplayName(type)}, makeDefaultLightTransform(type),
        makeDefaultLight(type));
    parentNewEntity(entity);
}

void HierarchyLayer::createBillboardEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }
    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Billboard"},
        fg::TransformComponent {.position = {0.0f, 1.0f, 0.0f}}, fg::BillboardComponent {});
    parentNewEntity(entity);
}

void HierarchyLayer::createParticleEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }
    const auto entity = mRegistry->CreateEntity(
        fg::NameComponent {.name = "Particles"},
        fg::TransformComponent {.position = {0.0f, 0.5f, 0.0f}},
        fg::ParticleEmitterComponent {});
    parentNewEntity(entity);
}

void HierarchyLayer::createFullscreenEffectEntity()
{
    if(mSimulation->IsPlaying())
    {
        return;
    }
    const auto entity =
        mRegistry->CreateEntity(fg::NameComponent {.name = "Cell Effect"},
                                fg::FullscreenEffectComponent {});
    parentNewEntity(entity);
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
    if(hasUserComponentType(kCharacterControllerTypeId))
    {
        const auto ops = mUserComponents->Find(kCharacterControllerTypeId);
        if(ops && ops->has && ops->has(*mRegistry, entity) && ops->remove)
        {
            ops->remove(*mRegistry, entity);
            mRegistry->ExecuteTasks();
        }
    }
    if(!mRegistry->HasComponent<fg::RigidBodyComponent>(entity))
    {
        mRegistry->AddComponents(entity, makeDefaultRigidBody(entity));
    }
}

void HierarchyLayer::addCharacterControllerToSelection()
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
    if(mRegistry->HasComponent<fg::RigidBodyComponent>(entity))
    {
        mRegistry->RemoveComponent<fg::RigidBodyComponent>(entity);
        mRegistry->ExecuteTasks();
    }
    if(hasUserComponentType(kCharacterControllerTypeId))
    {
        addUserComponentToEntity(entity, kCharacterControllerTypeId);
    }
}

void HierarchyLayer::addThirdPersonCameraToSelection()
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
    if(!mRegistry->HasComponent<fg::CameraComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::CameraComponent {});
    }
    if(hasUserComponentType(kThirdPersonCameraTypeId))
    {
        addUserComponentToEntity(entity, kThirdPersonCameraTypeId);
    }
}

void HierarchyLayer::addBillboardToSelection()
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
    if(!mRegistry->HasComponent<fg::BillboardComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::BillboardComponent {});
    }
}

void HierarchyLayer::addParticleEmitterToSelection()
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
    if(!mRegistry->HasComponent<fg::ParticleEmitterComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::ParticleEmitterComponent {});
    }
}

void HierarchyLayer::addHealthBarToSelection()
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
    if(!mRegistry->HasComponent<fg::HealthBarComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::HealthBarComponent {});
    }
}

void HierarchyLayer::addBillboardTextToSelection()
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
    if(!mRegistry->HasComponent<fg::BillboardTextComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::BillboardTextComponent {});
    }
}

void HierarchyLayer::addFullscreenEffectToSelection()
{
    if(!mSelection->HasSelection() || mSimulation->IsPlaying())
    {
        return;
    }
    const auto entity = mSelection->Get();
    if(!mRegistry->HasComponent<fg::FullscreenEffectComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::FullscreenEffectComponent {});
    }
}

void HierarchyLayer::addAudioSourceToSelection()
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
    if(!mRegistry->HasComponent<fg::AudioSourceComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::AudioSourceComponent {});
    }
}

void HierarchyLayer::addAudioListenerToSelection()
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
    if(!mRegistry->HasComponent<fg::AudioListenerComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::AudioListenerComponent {});
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

    const auto ops = mUserComponents->Find(typeId);
    if(!ops || !ops->addDefault || !ops->has)
    {
        return;
    }

    if(ops->has(*mRegistry, entity))
    {
        return;
    }

    if(typeId == kCharacterControllerTypeId)
    {
        if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
        {
            mRegistry->AddComponents(entity, fg::TransformComponent {});
        }
        if(mRegistry->HasComponent<fg::RigidBodyComponent>(entity))
        {
            mRegistry->RemoveComponent<fg::RigidBodyComponent>(entity);
            mRegistry->ExecuteTasks();
        }
    }
    else if(typeId == kThirdPersonCameraTypeId)
    {
        if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
        {
            mRegistry->AddComponents(entity, fg::TransformComponent {});
        }
        if(!mRegistry->HasComponent<fg::CameraComponent>(entity))
        {
            mRegistry->AddComponents(entity, fg::CameraComponent {});
        }
    }

    ops->addDefault(*mRegistry, entity);
    mRegistry->ExecuteTasks();
}

bool HierarchyLayer::hasUserComponentType(std::string_view typeId) const
{
    return mUserComponents && mUserComponents->Has(typeId);
}

void HierarchyLayer::drawModuleAddComponentMenus(fr::Entity entity)
{
    if(!mUserComponents)
    {
        return;
    }

    const auto types = mUserComponents->GetTypes();
    struct ModuleGroup
    {
        std::string id;
        std::string name;
        std::vector<fg::RuntimeComponentOps> components;
    };
    std::vector<ModuleGroup> groups;
    std::map<std::string, std::size_t> indexById;
    for(const auto &ops : types)
    {
        const auto id = ops.moduleId.empty() ? std::string("modules") : ops.moduleId;
        auto found    = indexById.find(id);
        if(found == indexById.end())
        {
            indexById.emplace(id, groups.size());
            groups.push_back(ModuleGroup {
                .id   = id,
                .name = ops.moduleName.empty() ? id : ops.moduleName,
            });
            found = indexById.find(id);
        }
        groups[found->second].components.push_back(ops);
    }
    if(groups.empty())
    {
        return;
    }

    ImGui::Separator();
    for(const auto &group : groups)
    {
        ImGui::PushID(group.id.c_str());
        if(ImGui::BeginMenu(group.name.c_str()))
        {
            for(const auto &ops : group.components)
            {
                ImGui::PushID(ops.typeId.c_str());
                const bool alreadyHas = ops.has && ops.has(*mRegistry, entity);
                ImGui::BeginDisabled(alreadyHas);
                const auto label =
                    ops.fields.empty() ? std::format("{} (tag)", ops.displayName) : ops.displayName;
                if(ImGui::MenuItem(label.c_str()))
                {
                    addUserComponentToEntity(entity, ops.typeId);
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }
        ImGui::PopID();
    }
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
    mRegistry->CreateMutation()->Each(
        [entity](fr::Entity candidate, fg::CameraComponent &camera) {
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

    SDL_ShowOpenFileDialog(onTextureDialog, this,
                           static_cast<SDL_Window *>(mWindow->NativeWindow()), kTextureFilters,
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
        case PendingTextureSlot::Billboard:
        case PendingTextureSlot::Particle:
        case PendingTextureSlot::None:
            break;
        }
        mPrimitives->UpdateMaterial(material.materialId, info);
    });

    if(slot == PendingTextureSlot::Billboard)
    {
        mRegistry->TryGetComponents<fg::BillboardComponent>(
            entity, [&](fg::BillboardComponent &billboard) {
                billboard.textureId = texture->textureId;
            });
    }
    if(slot == PendingTextureSlot::Particle)
    {
        mRegistry->TryGetComponents<fg::ParticleEmitterComponent>(
            entity, [&](fg::ParticleEmitterComponent &particles) {
                particles.textureId = texture->textureId;
            });
    }
}

void HierarchyLayer::onUpdate()
{
    processPendingTextureImport();
    processPendingPrefabSave();
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
        if(ImGui::MenuItem(ICON_BTSP_VOLUMEUP " Audio Source"))
        {
            createAudioSourceEntity();
        }
        if(ImGui::MenuItem(ICON_BTSP_MIC " Audio Listener"))
        {
            createAudioListenerEntity();
        }
        if(ImGui::MenuItem(ICON_BTSP_IMAGE " Billboard"))
        {
            createBillboardEntity();
        }
        if(ImGui::MenuItem(ICON_BTSP_STAR " Particles"))
        {
            createParticleEntity();
        }
        if(ImGui::MenuItem(ICON_BTSP_LAYERS " Cell Effect"))
        {
            createFullscreenEffectEntity();
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

    std::unordered_set<fr::Entity> nested;
    mRegistry->CreateMutation()->Each(
        [this, &nested](fr::Entity entity, fg::NameComponent &) {
            if(fg::TransformUtil::ParentOf(*mRegistry, entity) != fg::kInvalidEntity)
            {
                nested.insert(entity);
            }
        });
    mRegistry->CreateMutation()->Each(
        [this, &nested](fr::Entity entity, fg::NameComponent &name) {
            if(!nested.contains(entity))
            {
                drawEntityNode(entity, name);
            }
        });

    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x,
                        std::max(EditorUiScale::S(24.0f), ImGui::GetContentRegionAvail().y)));
    if(ImGui::BeginDragDropTarget())
    {
        if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kDragPayloadId))
        {
            const auto child = *static_cast<const fr::Entity *>(payload->Data);
            fg::TransformUtil::SetParent(*mRegistry, child, fg::kInvalidEntity, true);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();

    ImGui::Begin(componentsTitle.c_str());

    if(mSelection->HasSelection())
    {
        drawComponents();
        handleComponentClipboardInput();

        if(ImGui::BeginPopupContextWindow("##ComponentsPanelCtx", ImGuiPopupFlags_NoOpenOverItems))
        {
            ImGui::BeginDisabled(!canPasteComponent());
            if(ImGui::MenuItem("Paste Component", "Ctrl+V"))
            {
                pasteComponent();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}

void HierarchyLayer::copyComponent(std::string_view kind)
{
    if(!mSelection->HasSelection() || mSimulation->IsPlaying())
    {
        return;
    }
    if(ComponentClipboard::Copy(*mScene, mSelection->Get(), kind))
    {
        mActiveComponentKind = std::string {kind};
    }
}

void HierarchyLayer::pasteComponent()
{
    if(!mSelection->HasSelection() || mSimulation->IsPlaying())
    {
        return;
    }
    const fr::Entity entity = mSelection->Get();
    ensureTransformForPaste(entity);
    (void)ComponentClipboard::Paste(*mScene, entity);
    mRegistry->ExecuteTasks();
}

void HierarchyLayer::copyActiveComponent()
{
    if(mActiveComponentKind.empty())
    {
        return;
    }
    copyComponent(mActiveComponentKind);
}

bool HierarchyLayer::canPasteComponent() const
{
    return !mSimulation->IsPlaying() && mSelection->HasSelection() &&
           ComponentClipboard::HasData();
}

void HierarchyLayer::drawComponentContextMenu(std::string_view kind)
{
    ImGui::PushID(kind.data(), kind.data() + kind.size());
    if(ImGui::BeginPopupContextItem("##ComponentCtx"))
    {
        mActiveComponentKind = std::string {kind};

        if(ImGui::MenuItem("Copy Component", "Ctrl+C"))
        {
            copyComponent(kind);
        }

        ImGui::BeginDisabled(!canPasteComponent());
        if(ImGui::MenuItem("Paste Component", "Ctrl+V"))
        {
            pasteComponent();
        }
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }
    else if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        mActiveComponentKind = std::string {kind};
    }
    ImGui::PopID();
}

bool HierarchyLayer::drawComponentHeader(const char *label, std::string_view kind, bool *open)
{
    const bool expanded =
        open != nullptr
            ? ImGui::CollapsingHeader(label, open, ImGuiWindowFlags_ChildWindow)
            : ImGui::CollapsingHeader(label, nullptr, ImGuiWindowFlags_ChildWindow);
    drawComponentContextMenu(kind);
    return expanded;
}

bool HierarchyLayer::entityHasVisibleComponents(fr::Entity entity) const
{
    if(mRegistry->HasComponent<fg::TransformComponent>(entity) ||
       mRegistry->HasComponent<fg::CameraComponent>(entity) ||
       mRegistry->HasComponent<fg::LightComponent>(entity) ||
       mRegistry->HasComponent<fg::MeshComponent>(entity) ||
       mRegistry->HasComponent<fg::MaterialComponent>(entity) ||
       mRegistry->HasComponent<fg::RigidBodyComponent>(entity) ||
       mRegistry->HasComponent<fg::AnimatorComponent>(entity) ||
       mRegistry->HasComponent<fg::BillboardComponent>(entity) ||
       mRegistry->HasComponent<fg::ParticleEmitterComponent>(entity) ||
       mRegistry->HasComponent<fg::HealthBarComponent>(entity) ||
       mRegistry->HasComponent<fg::BillboardTextComponent>(entity) ||
       mRegistry->HasComponent<fg::FullscreenEffectComponent>(entity) ||
       mRegistry->HasComponent<fg::PrefabComponent>(entity) ||
       mRegistry->HasComponent<fg::AudioSourceComponent>(entity) ||
       mRegistry->HasComponent<fg::AudioListenerComponent>(entity))
    {
        return true;
    }

    if(!mUserComponents)
    {
        return false;
    }

    for(const auto &ops : mUserComponents->GetTypes())
    {
        if(ops.has && ops.has(*mRegistry, entity))
        {
            return true;
        }
    }

    return false;
}

void HierarchyLayer::ensureTransformForPaste(fr::Entity entity)
{
    if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::TransformComponent {});
    }
}

void HierarchyLayer::drawComponentsPanelActions()
{
    if(mSimulation->IsPlaying() || entityHasVisibleComponents(mSelection->Get()))
    {
        return;
    }

    ImGui::TextDisabled("No components on this entity.");
    ImGui::TextDisabled("Right-click to paste or use Component → Add Component.");
}

void HierarchyLayer::handleComponentClipboardInput()
{
    if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
       mSimulation->IsPlaying() || !mSelection->HasSelection())
    {
        return;
    }

    const ImGuiIO &io = ImGui::GetIO();
    if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !mActiveComponentKind.empty())
    {
        copyActiveComponent();
    }
    if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && canPasteComponent())
    {
        pasteComponent();
    }
}

void HierarchyLayer::drawEntityNode(fr::Entity entity, fg::NameComponent &name)
{
    std::vector<fr::Entity> children;
    mRegistry->TryGetComponents<fg::HierarchyComponent>(entity, [&](fg::HierarchyComponent &hierarchy) {
        children = hierarchy.children;
    });

    const bool renaming = nodeToRename == entity;

    ImGuiTreeNodeFlags flags =
        ((mSelection->Get() == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
    if(children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    else
    {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
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

    if(ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload(HierarchyLayer::kDragPayloadId, &entity, sizeof(entity));
        ImGui::TextUnformatted(name.name.c_str());
        ImGui::EndDragDropSource();
    }
    if(ImGui::BeginDragDropTarget())
    {
        if(const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kDragPayloadId))
        {
            const auto child = *static_cast<const fr::Entity *>(payload->Data);
            fg::TransformUtil::SetParent(*mRegistry, child, entity, true);
        }
        if(const ImGuiPayload *payload =
               ImGui::AcceptDragDropPayload(ResourcesLayer::kDragPayloadId))
        {
            const auto *drag =
                static_cast<const ResourcesLayer::ResourceDragPayload *>(payload->Data);
            if(mResources)
            {
                mResources->HandleDrop(*drag, entity);
            }
        }
        ImGui::EndDragDropTarget();
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
            const auto selected = mSelection->Get();
            const bool clearSelection =
                selected == entity ||
                fg::TransformUtil::WouldCreateCycle(*mRegistry, entity, selected);
            fg::TransformUtil::DestroySubtree(*mRegistry, entity);
            if(clearSelection)
            {
                mSelection->Clear();
            }
        }
        else if((locked || playLocked) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(playLocked ? "Stop Play mode to delete entities"
                                         : "Main Camera cannot be removed");
        }

        if(ImGui::MenuItem("Rename...", nullptr, false, !playLocked)) nodeToRename = entity;

        if(ImGui::MenuItem(ICON_BTSP_COLLECTION " Create Prefab...", nullptr, false, !playLocked))
        {
            requestSavePrefabDialog(entity);
        }

        const auto parent = fg::TransformUtil::ParentOf(*mRegistry, entity);
        if(ImGui::MenuItem("Unparent", nullptr, false,
                           !playLocked && parent != fg::kInvalidEntity))
        {
            fg::TransformUtil::SetParent(*mRegistry, entity, fg::kInvalidEntity, true);
        }

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

        if(ImGui::MenuItem("Add billboard"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::BillboardComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::BillboardComponent {});
            }
        }
        if(ImGui::MenuItem("Add particle emitter"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::ParticleEmitterComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::ParticleEmitterComponent {});
            }
        }
        if(ImGui::MenuItem("Add health bar"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::HealthBarComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::HealthBarComponent {});
            }
        }
        if(ImGui::MenuItem("Add billboard text"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::BillboardTextComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::BillboardTextComponent {});
            }
        }
        if(ImGui::MenuItem("Add fullscreen effect"))
        {
            if(!mRegistry->HasComponent<fg::FullscreenEffectComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::FullscreenEffectComponent {});
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
            if(hasUserComponentType(kCharacterControllerTypeId))
            {
                const auto ops = mUserComponents->Find(kCharacterControllerTypeId);
                if(ops && ops->has && ops->has(*mRegistry, entity) && ops->remove)
                {
                    ops->remove(*mRegistry, entity);
                    mRegistry->ExecuteTasks();
                }
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

        if(ImGui::MenuItem(ICON_BTSP_VOLUMEUP " Add audio source"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::AudioSourceComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::AudioSourceComponent {});
            }
        }
        if(ImGui::MenuItem(ICON_BTSP_MIC " Add audio listener"))
        {
            if(!mRegistry->HasComponent<fg::TransformComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::TransformComponent {});
            }
            if(!mRegistry->HasComponent<fg::AudioListenerComponent>(entity))
            {
                mRegistry->AddComponents(entity, fg::AudioListenerComponent {});
            }
        }

        drawModuleAddComponentMenus(entity);
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
        ImGui::PushItemWidth(std::max(EditorUiScale::S(24.0f), rowMax.x - labelX));
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

    if(opened)
    {
        for(const auto child : children)
        {
            mRegistry->TryGetComponents<fg::NameComponent>(
                child, [this, child](fg::NameComponent &childName) {
                    drawEntityNode(child, childName);
                });
        }
        ImGui::TreePop();
    }
}

void HierarchyLayer::drawComponents()
{
    const fr::Entity selection = mSelection->Get();

    mRegistry->TryGetComponents<fg::PrefabComponent>(
        selection, [](fg::PrefabComponent &prefab) {
            if(ImGui::CollapsingHeader("Prefab", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TextWrapped("Source: Resources/%s", prefab.source.c_str());
                ImGui::TextDisabled("Double-click the prefab in Resources to spawn another copy.");
            }
        });

    mRegistry->TryGetComponents<fg::TransformComponent>(
        selection, [this, selection](fg::TransformComponent &transform) {
            static glm::vec3 Rotation;

            const bool hasParent =
                fg::TransformUtil::ParentOf(*mRegistry, selection) != fg::kInvalidEntity;
            const char *header =
                hasParent ? "Transform (Local)" : "Transform Component";

            if(drawComponentHeader(header, "transform"))
            {
                ImGui::DragFloat3("Position", &transform.position[0], 0.1f);

                Rotation = glm::degrees(glm::eulerAngles(glm::normalize(transform.rotation)));

                if(ImGui::DragFloat3("Rotation", &Rotation[0], 0.1f))
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
                ImGui::DragFloat3("Scale", &transform.scale[0], 0.1f);
            }
        });

    mRegistry->TryGetComponents<fg::CameraComponent>(
        selection, [this, selection](fg::CameraComponent &camera) {
            if(drawComponentHeader("Camera Component", "camera"))
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

    mRegistry->TryGetComponents<fg::AudioListenerComponent>(
        selection, [this, selection](fg::AudioListenerComponent &listener) {
            bool open = true;
            if(drawComponentHeader("Audio Listener", "audioListener", &open))
            {
                EditorAudioUi::DrawListenerFields(listener, mSimulation->IsPlaying());
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::AudioListenerComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::AudioSourceComponent>(
        selection, [this, selection](fg::AudioSourceComponent &source) {
            bool open = true;
            if(drawComponentHeader("Audio Source", "audioSource", &open))
            {
                EditorAudioUi::DrawSourceFields(source, mAssets, mAudioController, selection,
                                                mSimulation->IsPlaying(), true);
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::AudioSourceComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::BillboardComponent>(
        selection, [this, selection](fg::BillboardComponent &billboard) {
            bool open = true;
            if(drawComponentHeader("Billboard", "billboard", &open))
            {
                ImGui::DragFloat2("Size", &billboard.size[0], 0.01f, 0.01f, 50.0f);
                ImGui::ColorEdit4("Color", &billboard.color[0]);
                ImGui::DragFloat4("UV Rect", &billboard.uvRect[0], 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat2("Local Offset", &billboard.localOffset[0], 0.01f);
                int align = static_cast<int>(billboard.align);
                if(ImGui::Combo("Align", &align, "Screen\0Cylindrical\0"))
                {
                    billboard.align = static_cast<fra::BillboardAlign>(align);
                }
                int blend = static_cast<int>(billboard.blend);
                if(ImGui::Combo("Blend", &blend, "Alpha\0Additive\0"))
                {
                    billboard.blend = static_cast<fra::BillboardBlend>(blend);
                }
                int layer = static_cast<int>(billboard.layer);
                if(ImGui::Combo("Layer", &layer, "Vfx\0Ui\0"))
                {
                    billboard.layer = static_cast<fra::BillboardLayer>(layer);
                }
                ImGui::Checkbox("Depth Test", &billboard.depthTest);
                ImGui::Checkbox("SDF", &billboard.sdf);
                ImGui::DragFloat("Clip Max", &billboard.clipMax, 0.01f, 0.0f, 1.0f);
                bool texChanged = false;
                drawTextureSlot("Texture", PendingTextureSlot::Billboard, billboard.textureId,
                                texChanged);
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::BillboardComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::ParticleEmitterComponent>(
        selection, [this, selection](fg::ParticleEmitterComponent &particles) {
            bool open = true;
            if(drawComponentHeader("Particle Emitter", "particles", &open))
            {
                ImGui::Checkbox("Playing", &particles.playing);
                ImGui::DragFloat3("Velocity", &particles.velocity[0], 0.01f);
                ImGui::DragFloat3("Jitter", &particles.velocityJitter[0], 0.01f);
                ImGui::DragFloat("Spawn Rate", &particles.spawnRate, 0.1f, 0.0f, 500.0f);
                ImGui::DragFloat("Lifetime", &particles.lifetime, 0.01f, 0.05f, 10.0f);
                ImGui::DragFloat("Size Start", &particles.size0, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Size End", &particles.size1, 0.01f, 0.0f, 5.0f);
                ImGui::ColorEdit4("Color Start", &particles.color0[0]);
                ImGui::ColorEdit4("Color End", &particles.color1[0]);
                int blend = static_cast<int>(particles.blend);
                if(ImGui::Combo("Blend", &blend, "Alpha\0Additive\0"))
                {
                    particles.blend = static_cast<fra::BillboardBlend>(blend);
                }
                int maxParticles = static_cast<int>(particles.maxParticles);
                if(ImGui::DragInt("Max Particles", &maxParticles, 1.0f, 1, 4096))
                {
                    particles.maxParticles = static_cast<std::uint32_t>(maxParticles);
                }
                bool texChanged = false;
                drawTextureSlot("Texture", PendingTextureSlot::Particle, particles.textureId,
                                texChanged);
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::ParticleEmitterComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::HealthBarComponent>(
        selection, [this, selection](fg::HealthBarComponent &bar) {
            bool open = true;
            if(drawComponentHeader("Health Bar", "healthBar", &open))
            {
                ImGui::SliderFloat("Fill", &bar.fill, 0.0f, 1.0f);
                ImGui::DragFloat("Width", &bar.width, 0.01f, 0.05f, 10.0f);
                ImGui::DragFloat("Height", &bar.height, 0.01f, 0.01f, 2.0f);
                ImGui::DragFloat3("Offset", &bar.offset[0], 0.01f);
                ImGui::ColorEdit4("Background", &bar.background[0]);
                ImGui::ColorEdit4("Foreground", &bar.foreground[0]);
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::HealthBarComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::BillboardTextComponent>(
        selection, [this, selection](fg::BillboardTextComponent &label) {
            bool open = true;
            if(drawComponentHeader("Billboard Text", "billboardText", &open))
            {
                char textBuf[256];
                char fontBuf[256];
                std::snprintf(textBuf, sizeof(textBuf), "%s", label.text.c_str());
                std::snprintf(fontBuf, sizeof(fontBuf), "%s", label.fontSource.c_str());
                if(ImGui::InputText("Text", textBuf, sizeof(textBuf)))
                {
                    label.text = textBuf;
                }
                if(ImGui::InputText("Font", fontBuf, sizeof(fontBuf)))
                {
                    label.fontSource = fontBuf;
                }
                ImGui::DragFloat("Height", &label.heightMeters, 0.01f, 0.02f, 5.0f);
                ImGui::DragFloat3("Offset", &label.offset[0], 0.01f);
                ImGui::ColorEdit4("Color", &label.color[0]);
                ImGui::DragFloat("Border Width", &label.borderWidth, 0.1f, 0.0f, 8.0f);
                ImGui::ColorEdit4("Border Color", &label.borderColor[0]);
                int align = static_cast<int>(label.align);
                if(ImGui::Combo("Align", &align, "Screen\0Cylindrical\0"))
                {
                    label.align = static_cast<fra::BillboardAlign>(align);
                }
                int layer = static_cast<int>(label.layer);
                if(ImGui::Combo("Layer", &layer, "Vfx\0Ui\0"))
                {
                    label.layer = static_cast<fra::BillboardLayer>(layer);
                }
                ImGui::TextDisabled("Place a TTF under Resources/Fonts/");
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::BillboardTextComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::FullscreenEffectComponent>(
        selection, [this, selection](fg::FullscreenEffectComponent &fx) {
            bool open = true;
            if(drawComponentHeader("Fullscreen Effect", "fullscreenEffect", &open))
            {
                char nameBuf[64];
                char fragBuf[256];
                std::snprintf(nameBuf, sizeof(nameBuf), "%s", fx.name.c_str());
                std::snprintf(fragBuf, sizeof(fragBuf), "%s", fx.fragment.c_str());
                if(ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                {
                    fx.name = nameBuf;
                }
                if(ImGui::InputText("Fragment", fragBuf, sizeof(fragBuf)))
                {
                    fx.fragment = fragBuf;
                }
                ImGui::Checkbox("Enabled", &fx.enabled);
                ImGui::Separator();
                ImGui::TextDisabled("Cell shader parameters");
                ImGui::DragFloat("Bands", &fx.bands, 0.1f, 1.0f, 16.0f);
                ImGui::DragFloat("Edge Depth", &fx.edgeDepthScale, 0.5f, 0.0f, 400.0f);
                ImGui::DragFloat("Edge Normal", &fx.edgeNormalScale, 0.05f, 0.0f, 20.0f);
                ImGui::DragFloat("Strength", &fx.strength, 0.01f, 0.0f, 1.0f);
                ImGui::ColorEdit4("Edge Color", &fx.edgeColor[0]);
                ImGui::DragFloat("Shadow Lift", &fx.shadowLift, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Edge Width", &fx.edgeWidth, 0.05f, 0.0f, 8.0f);
                ImGui::TextDisabled("Inserted before Freya BillboardVfx");
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::FullscreenEffectComponent>(selection);
            }
        });

    mRegistry->TryGetComponents<fg::LightComponent>(
        selection, [this, selection](fg::LightComponent &light) {
            bool open = true;
            if(drawComponentHeader("Light Component", "light", &open))
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

    mRegistry->TryGetComponents<fg::MeshComponent>(selection, [this, selection](fg::MeshComponent &mesh) {
        bool open = true;
        if(drawComponentHeader("Mesh Component", "mesh", &open))
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
        if(!open && !mSimulation->IsPlaying())
        {
            mRegistry->RemoveComponent<fg::MeshComponent>(selection);
            mRegistry->ExecuteTasks();
        }
    });

    mRegistry->TryGetComponents<fg::MaterialComponent>(
        selection, [this, selection](fg::MaterialComponent &material) {
            bool open = true;
            if(drawComponentHeader("Material Component", "material", &open))
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
            if(drawComponentHeader("Rigid Body Component", "rigidBody"))
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
            if(drawComponentHeader("Animator Component", "animator", &open))
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
                ImGui::Checkbox("Use Anim Graph", &animator.useAnimGraph);
                ImGui::TextDisabled("Bones: offset %u count %u", animator.boneOffset,
                                    animator.boneCount);
                ImGui::EndDisabled();
            }
            if(!open && !mSimulation->IsPlaying())
            {
                mRegistry->RemoveComponent<fg::AnimatorComponent>(selection);
            }
        });

    if(mUserComponents)
    {
        const auto types = mUserComponents->GetTypes();
        for(std::size_t typeIndex = 0; typeIndex < types.size(); ++typeIndex)
        {
            const auto &ops = types[typeIndex];
            if(!ops.has || !ops.has(*mRegistry, selection))
            {
                continue;
            }

            ImGui::PushID(static_cast<int>(typeIndex));

            bool open = true;
            const std::string userKind = std::format("user:{}", ops.typeId);
            if(drawComponentHeader(ops.displayName.c_str(), userKind, &open))
            {
                if(ops.drawInspector)
                {
                    fg::FriComponentInspector ui;
                    ui.entity       = selection;
                    ui.playing      = mSimulation->IsPlaying();
                    const auto handle = mSimulation->CharacterHandleOf(selection);
                    ui.hasCharacter = handle.IsValid();
                    ui.characterId  = handle.id;
                    ops.drawInspector(*mRegistry, selection, ui);
                }
                else
                {
                    fg::UserComponentInstance instance {};
                    if(ops.toInstance && ops.toInstance(*mRegistry, selection, instance))
                    {
                        ImGui::BeginDisabled(mSimulation->IsPlaying());
                        for(auto &property : instance.properties)
                        {
                            DrawNamedProperty(property);
                        }
                        ImGui::EndDisabled();
                        if(!mSimulation->IsPlaying() && ops.fromInstance)
                        {
                            ops.fromInstance(*mRegistry, selection, instance);
                        }
                    }
                }
            }

            if(!open && !mSimulation->IsPlaying() && ops.remove)
            {
                ops.remove(*mRegistry, selection);
                mRegistry->ExecuteTasks();
            }
            ImGui::PopID();
        }
    }

    drawComponentsPanelActions();
}
