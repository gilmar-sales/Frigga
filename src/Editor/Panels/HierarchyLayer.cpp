#include "HierarchyLayer.hpp"

#include "Editor/DockLayout.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <format>
#include <imgui.h>
#include <sstream>

HierarchyLayer::HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::PrimitiveMeshFactory> primitives,
                               skr::Arc<SelectionContext> selection)
    : mRegistry(std::move(registry)), mScene(std::move(scene)),
      mPrimitives(std::move(primitives)), mSelection(std::move(selection)),
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
    }
    return "Light";
}

void HierarchyLayer::createEmptyEntity()
{
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
    const auto *displayName = fg::PrimitiveMeshFactory::GetDisplayName(type);
    const auto meshId       = mPrimitives->GetMesh(type);
    const auto materialId   = mPrimitives->GetDefaultMaterial();

    mRegistry->CreateEntity(fg::NameComponent {.name = displayName}, fg::TransformComponent {},
                            fg::MeshComponent {.meshId = meshId},
                            fg::MaterialComponent {.materialId = materialId});
}

void HierarchyLayer::createCameraEntity()
{
    mRegistry->CreateEntity(fg::NameComponent {.name = "Camera"},
                            fg::TransformComponent {.position = {0.0f, 1.5f, -5.0f}},
                            fg::CameraComponent {});
}

void HierarchyLayer::createLightEntity(fra::LightType type)
{
    mRegistry->CreateEntity(
        fg::NameComponent {.name = getLightDisplayName(type)},
        fg::TransformComponent {.position = {0.0f, 3.0f, 0.0f},
                                .scale    = {1.0f, 1.0f, 1.0f},
                                .rotation = glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f},
                                                            glm::vec3 {0.0f, 0.0f, 1.0f})},
        fg::LightComponent {.type = type});
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
            for(auto type:
                {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot})
            {
                if(ImGui::MenuItem(getLightDisplayName(type)))
                {
                    createLightEntity(type);
                }
            }
            ImGui::EndMenu();
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
        const bool locked = isEntityLocked(entity);

        if(ImGui::MenuItem("Delete", nullptr, false, !locked))
        {
            mRegistry->DestroyEntity(entity);
            if(mSelection->Get() == entity) mSelection->Clear();
        }
        else if(locked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip("Main Camera cannot be removed");
        }

        if(ImGui::MenuItem("Rename...")) nodeToRename = entity;

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
            for(auto type:
                {fra::LightType::Point, fra::LightType::Directional, fra::LightType::Spot})
            {
                if(ImGui::MenuItem(getLightDisplayName(type)))
                {
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
            }
            ImGui::EndMenu();
        }

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
                if(ImGui::Combo("Type", &typeIndex, "Point\0Directional\0Spot\0"))
                {
                    light.type = static_cast<fra::LightType>(typeIndex);
                }

                ImGui::ColorEdit3("Color", &light.color[0]);
                ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 1000.0f);

                if(light.type != fra::LightType::Directional)
                {
                    ImGui::DragFloat("Radius", &light.radius, 0.1f, 0.1f, 1000.0f);
                }

                if(light.type == fra::LightType::Spot)
                {
                    ImGui::DragFloat("Inner Angle", &light.innerAngleDegrees, 0.1f, 0.0f, 89.0f);
                    ImGui::DragFloat("Outer Angle", &light.outerAngleDegrees, 0.1f, 0.0f, 89.0f);
                }
            }
        });

    mRegistry->TryGetComponents<fg::MeshComponent>(selection, [](fg::MeshComponent &mesh) {
        if(ImGui::CollapsingHeader("Mesh Component", nullptr, ImGuiWindowFlags_ChildWindow))
        {
            ImGui::Text("Mesh ID: %u", mesh.meshId);
        }
    });

    mRegistry->TryGetComponents<fg::MaterialComponent>(
        selection, [](fg::MaterialComponent &material) {
            if(ImGui::CollapsingHeader("Material Component", nullptr, ImGuiWindowFlags_ChildWindow))
            {
                ImGui::Text("Material ID: %u", material.materialId);
            }
        });
}
