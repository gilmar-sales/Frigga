#include "HierarchyLayer.hpp"

#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <imgui.h>

HierarchyLayer::HierarchyLayer(Ref<fr::Scene> scene)
    : mFreyrScene(scene), selectionContext(-1), nodeToRename(-1)
{
}

void HierarchyLayer::createEmptyEntity()
{
    mFreyrScene->CreateEntity(
        [](auto entity, fg::NameComponent &name) {
            std::stringstream entity_name;
            entity_name << "Empty(" << entity << ")";
            name.name = entity_name.str();
        },
        fg::NameComponent{});
}

void HierarchyLayer::onGui()
{
    ImGui::Begin("Hierarchy");

    if(ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
    {
        selectionContext = -1;
    }
    else
    {
        if(ImGui::BeginPopupContextWindow("##HierarchyContext", 1))
        {
            if(ImGui::MenuItem("Empty entity"))
            {
                createEmptyEntity();

                ImGui::BulletText("Return to input text into a widget.");
            }

            if(ImGui::BeginMenu("3D Entity"))
            {
                if(ImGui::MenuItem("Cube"))
                { /* Do stuff */
                }
                if(ImGui::MenuItem("Sphere"))
                { /* Do stuff */
                }
                if(ImGui::MenuItem("Capsule"))
                { /* Do stuff */
                }
                ImGui::EndMenu();
            }

            if(ImGui::MenuItem("Camera"))
            { /* Do stuff */
            }
            if(ImGui::MenuItem("Terrain"))
            { /* Do stuff */
            }

            ImGui::EndPopup();
        }
    }

    mFreyrScene->ForEach<fg::NameComponent>(
        [this](auto entity, fg::NameComponent &name) { drawEntityNode(entity, name); });

    ImGui::End();

    ImGui::Begin("Components");

    if(selectionContext != -1) drawComponents();

    ImGui::End();
}

void HierarchyLayer::drawEntityNode(unsigned entity, fg::NameComponent &name)
{
    ImGuiTreeNodeFlags flags = ((selectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;

    bool opened =
        ImGui::TreeNodeEx(reinterpret_cast<void *>(entity), flags, "%s", name.name.data());

    const std::string popUpId  = std::format("##PopUp{}", entity);
    const std::string renameId = std::format("##Rename{}", entity);
    if(ImGui::BeginPopupContextItem(popUpId.data()))
    {
        if(ImGui::MenuItem("Delete"))
        {
            mFreyrScene->DestroyEntity(entity);
            if(selectionContext == entity) selectionContext = -1;
        }

        if(ImGui::MenuItem("Rename...")) nodeToRename = entity;

        if(ImGui::MenuItem("Add transform"))
        {
            mFreyrScene->AddComponents(entity, fg::TransformComponent{});
        }

        if(ImGui::MenuItem("Add mesh"))
        {
            mFreyrScene->AddComponents(entity, fg::MeshComponent{});
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
            mFreyrScene->TryGetComponents<fg::NameComponent>(
                selectionContext, [](fg::NameComponent &name) { name.name = buffer; });
            nodeToRename = -1;
        }
    }

    if(opened) ImGui::TreePop();
}

void HierarchyLayer::drawComponents()
{
    mFreyrScene->TryGetComponents<fg::TransformComponent>(
        selectionContext, [](fg::TransformComponent &transform) {
            static glm::vec3 Rotation;

            if(ImGui::CollapsingHeader("Transform Component", nullptr,
                                       ImGuiWindowFlags_ChildWindow))
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

    mFreyrScene->TryGetComponents<fg::MeshComponent>(selectionContext, [](fg::MeshComponent &mesh) {
        if(ImGui::CollapsingHeader("Mesh Component", nullptr, ImGuiWindowFlags_ChildWindow))
        {
        }
    });
}
