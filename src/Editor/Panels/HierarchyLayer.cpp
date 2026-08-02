#include "HierarchyLayer.hpp"

#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <format>
#include <imgui.h>
#include <sstream>

namespace
{
    constexpr fr::Entity InvalidEntity = static_cast<fr::Entity>(-1);
}

HierarchyLayer::HierarchyLayer(skr::Arc<fr::Registry> registry, skr::Arc<fg::Scene> scene,
                               skr::Arc<fg::PrimitiveMeshFactory> primitives)
    : mRegistry(std::move(registry)), mScene(std::move(scene)),
      mPrimitives(std::move(primitives)), selectionContext(InvalidEntity),
      nodeToRename(InvalidEntity)
{
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
    ImGui::Begin("Hierarchy");

    if(ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
    {
        selectionContext = InvalidEntity;
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

        ImGui::EndPopup();
    }

    mRegistry->CreateMutation()->Each<fg::NameComponent>(
        [this](auto entity, fg::NameComponent &name) { drawEntityNode(entity, name); });

    ImGui::End();

    ImGui::Begin("Components");

    if(selectionContext != InvalidEntity) drawComponents();

    ImGui::End();
}

void HierarchyLayer::drawEntityNode(fr::Entity entity, fg::NameComponent &name)
{
    ImGuiTreeNodeFlags flags = ((selectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
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
            if(selectionContext == entity) selectionContext = InvalidEntity;
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

        ImGui::EndPopup();
    }

    if(ImGui::IsItemClicked() || ImGui::IsItemClicked(1))
    {
        selectionContext = entity;
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
                selectionContext, [](fg::NameComponent &name) { name.name = buffer; });
            nodeToRename = InvalidEntity;
        }
    }

    if(opened) ImGui::TreePop();
}

void HierarchyLayer::drawComponents()
{
    mRegistry->TryGetComponents<fg::TransformComponent>(
        selectionContext, [](fg::TransformComponent &transform) {
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
        selectionContext, [this](fg::CameraComponent &camera) {
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
                        setPrimaryCamera(selectionContext);
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

    mRegistry->TryGetComponents<fg::MeshComponent>(selectionContext, [](fg::MeshComponent &mesh) {
        if(ImGui::CollapsingHeader("Mesh Component", nullptr, ImGuiWindowFlags_ChildWindow))
        {
            ImGui::Text("Mesh ID: %u", mesh.meshId);
        }
    });

    mRegistry->TryGetComponents<fg::MaterialComponent>(
        selectionContext, [](fg::MaterialComponent &material) {
            if(ImGui::CollapsingHeader("Material Component", nullptr, ImGuiWindowFlags_ChildWindow))
            {
                ImGui::Text("Material ID: %u", material.materialId);
            }
        });
}
