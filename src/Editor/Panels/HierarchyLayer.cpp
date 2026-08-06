#include "HierarchyLayer.hpp"

#include "Editor/BoostrapIconsFont.hpp"
#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <algorithm>
#include <format>
#include <imgui.h>
#include <sstream>

namespace
{
    glm::quat LookDown()
    {
        return glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f}, glm::vec3 {0.0f, 0.0f, 1.0f});
    }
} // namespace

HierarchyLayer::HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::PrimitiveMeshFactory> primitives,
                               skr::Arc<SelectionContext> selection,
                               skr::Arc<fg::SceneSimulationState> simulation)
    : mRegistry(std::move(registry)), mScene(std::move(scene)),
      mPrimitives(std::move(primitives)), mSelection(std::move(selection)),
      mSimulation(std::move(simulation)), nodeToRename(SelectionContext::Invalid)
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
    ImGuiTreeNodeFlags flags =
        ((mSelection->Get() == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;

    const char *icon = resolveEntityIcon(entity);
    bool opened      = icon[0] != '\0'
                           ? ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)),
                                              flags, "%s %s", icon, name.name.c_str())
                           : ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)),
                                              flags, "%s", name.name.c_str());

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
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    if(ImGui::IsItemClicked() || ImGui::IsItemClicked(1))
    {
        mSelection->Select(entity);
    }

    if(nodeToRename == entity)
    {
        ImGui::SameLine();

        static std::string buffer = std::string("", 64);
        if(ImGui::IsMouseDown(1))

            ImGui::InputText(renameId.data(), buffer.data(), 65, ImGuiInputTextFlags_AutoSelectAll);

        if(ImGui::IsKeyReleased(ImGuiKey_Enter))
        {
            mRegistry->TryGetComponents<fg::NameComponent>(
                mSelection->Get(), [](fg::NameComponent &name) { name.name = buffer; });
            nodeToRename = SelectionContext::Invalid;
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
            fg::PrimitiveType current = fg::PrimitiveType::Cube;
            const bool known          = mPrimitives->TryFindPrimitive(mesh.meshId, current);
            const char *preview =
                known ? fg::PrimitiveMeshFactory::GetDisplayName(current) : "Unknown";

            if(ImGui::BeginCombo("Primitive", preview))
            {
                for(std::uint8_t i = 0; i < static_cast<std::uint8_t>(fg::PrimitiveType::Count);
                    ++i)
                {
                    const auto type     = static_cast<fg::PrimitiveType>(i);
                    const bool selected = known && type == current;
                    if(ImGui::Selectable(fg::PrimitiveMeshFactory::GetDisplayName(type), selected))
                    {
                        mesh.meshId = mPrimitives->GetMesh(type);
                    }
                    if(selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Cast Shadows", &mesh.castShadows);

            if(!known)
            {
                ImGui::TextDisabled("Mesh ID: %u", mesh.meshId);
            }
        }
    });

    mRegistry->TryGetComponents<fg::MaterialComponent>(
        selection, [this](fg::MaterialComponent &material) {
            if(ImGui::CollapsingHeader("Material Component", nullptr, ImGuiWindowFlags_ChildWindow))
            {
                const auto defaultMaterial = mPrimitives->GetDefaultMaterial();
                const bool isDefault       = material.materialId == defaultMaterial;
                const char *preview        = isDefault ? "Default" : "Unknown";

                if(ImGui::BeginCombo("Material", preview))
                {
                    if(ImGui::Selectable("Default", isDefault))
                    {
                        material.materialId = defaultMaterial;
                    }
                    if(isDefault)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if(!isDefault)
                {
                    ImGui::TextDisabled("Material ID: %u", material.materialId);
                }
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
}
