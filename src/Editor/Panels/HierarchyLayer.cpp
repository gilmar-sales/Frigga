#include "HierarchyLayer.hpp"

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

    mRegistry->CreateEntity(
        fg::NameComponent {.name = getLightDisplayName(type)},
        fg::TransformComponent {.position = {0.0f, 3.0f, 0.0f},
                                .scale    = {1.0f, 1.0f, 1.0f},
                                .rotation = glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f},
                                                            glm::vec3 {0.0f, 0.0f, 1.0f})},
        fg::LightComponent {.type = type});
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
        mRegistry->AddComponents(entity, fg::RigidBodyComponent {});
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
        mRegistry->AddComponents(entity, fg::TransformComponent {});
    }
    if(!mRegistry->HasComponent<fg::LightComponent>(entity))
    {
        mRegistry->AddComponents(entity, fg::LightComponent {.type = type});
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

        if(ImGui::BeginMenu("Light"))
        {
            for(auto type: {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot,
                            fra::LightType::Area})
            {
                if(ImGui::MenuItem(getLightDisplayName(type)))
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

    bool opened =
        ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<uintptr_t>(entity)), flags, "%s",
                          name.name.c_str());

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

        if(ImGui::BeginMenu("Add light"))
        {
            for(auto type: {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot,
                            fra::LightType::Area})
            {
                if(ImGui::MenuItem(getLightDisplayName(type)))
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
                mRegistry->AddComponents(entity, fg::RigidBodyComponent {});
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
        selection, [](fg::LightComponent &light) {
            if(ImGui::CollapsingHeader("Light Component", nullptr, ImGuiWindowFlags_ChildWindow))
            {
                int typeIndex = static_cast<int>(light.type);
                if(ImGui::Combo("Type", &typeIndex, "Point\0Directional\0Spot\0Area\0"))
                {
                    light.type = static_cast<fra::LightType>(typeIndex);
                }

                ImGui::ColorEdit3("Color", &light.color[0]);
                ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 1000.0f);

                if(light.type == fra::LightType::Point || light.type == fra::LightType::Spot)
                {
                    ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.1f, 1000.0f);
                }

                if(light.type == fra::LightType::Spot)
                {
                    ImGui::DragFloat("Inner Angle", &light.innerAngleDegrees, 0.1f, 0.0f, 89.0f);
                    ImGui::DragFloat("Outer Angle", &light.outerAngleDegrees, 0.1f, 0.0f, 89.0f);
                }

                if(light.type == fra::LightType::Area)
                {
                    ImGui::DragFloat("Half Width", &light.halfWidth, 0.01f, 0.01f, 100.0f);
                    ImGui::DragFloat("Half Height", &light.halfHeight, 0.01f, 0.01f, 100.0f);
                }

                ImGui::Checkbox("Cast Shadows", &light.castShadows);
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

                int mask = rigidBody.collideWithLayers;
                if(ImGui::InputInt("Collide Mask", &mask))
                {
                    rigidBody.collideWithLayers =
                        static_cast<std::uint16_t>(std::clamp(mask, 0, 0xffff));
                }
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
