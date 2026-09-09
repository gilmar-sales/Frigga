#include "Frigga/Module/FriComponentInspector.hpp"

#include "Frigga/ECS/Components/HierarchyComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"

#include <cstdio>
#include <format>
#include <imgui.h>
#include <string>

namespace FRIGGA_NAMESPACE
{

    void FriComponentInspector::BeginDisabled(bool disabled)
    {
        ImGui::BeginDisabled(disabled);
    }

    void FriComponentInspector::EndDisabled()
    {
        ImGui::EndDisabled();
    }

    void FriComponentInspector::DragFloat(const char *label, float &value, float speed, float min,
                                          float max)
    {
        ImGui::DragFloat(label, &value, speed, min, max);
    }

    void FriComponentInspector::DragFloat3(const char *label, glm::vec3 &value, float speed)
    {
        ImGui::DragFloat3(label, &value[0], speed);
    }

    bool FriComponentInspector::InputText(const char *label, std::string &value)
    {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if(ImGui::InputText(label, buffer, sizeof(buffer)))
        {
            value = buffer;
            return true;
        }
        return false;
    }

    bool FriComponentInspector::SliderInt(const char *label, int &value, int vmin, int vmax)
    {
        return ImGui::SliderInt(label, &value, vmin, vmax);
    }

    bool FriComponentInspector::Checkbox(const char *label, bool &value)
    {
        return ImGui::Checkbox(label, &value);
    }

    bool FriComponentInspector::EntityField(const char *label, EntityRef &value)
    {
        bool changed = false;

        std::string display = "None";
        if(value.id != kInvalidEntity)
        {
            display = std::format("#{}", value.id);
            if(registry)
            {
                registry->TryGetComponents<NameComponent>(
                    value.id, [&](NameComponent &name) { display = name.name; });
            }
        }

        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        const ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x -
                                       ImGui::CalcTextSize("Clear").x - ImGui::GetStyle().ItemSpacing.x * 2.0f,
                                   0.0f);
        ImGui::Button(display.c_str(), size);
        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload *payload =
                   ImGui::AcceptDragDropPayload(kHierarchyEntityDragPayload))
            {
                if(payload->DataSize == static_cast<int>(sizeof(fr::Entity)))
                {
                    value.id = *static_cast<const fr::Entity *>(payload->Data);
                    changed  = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Drag an entity from Hierarchy");
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(value.id == kInvalidEntity);
        if(ImGui::SmallButton("Clear"))
        {
            value.id = kInvalidEntity;
            changed  = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        return changed;
    }

    void FriComponentInspector::TextDisabled(const char *text)
    {
        ImGui::TextDisabled("%s", text);
    }

    bool FriComponentInspector::IsItemHovered() const
    {
        return ImGui::IsItemHovered();
    }

    void FriComponentInspector::SetTooltip(const char *text)
    {
        ImGui::SetTooltip("%s", text);
    }

    void FriKeepComponentInspectorSymbols()
    {
        using T = FriComponentInspector;
        volatile auto beginDisabled = &T::BeginDisabled;
        volatile auto endDisabled   = &T::EndDisabled;
        volatile auto dragFloat     = &T::DragFloat;
        volatile auto dragFloat3    = &T::DragFloat3;
        volatile auto inputText     = &T::InputText;
        volatile auto sliderInt     = &T::SliderInt;
        volatile auto checkbox      = &T::Checkbox;
        volatile auto entityField   = &T::EntityField;
        volatile auto textDisabled  = &T::TextDisabled;
        volatile auto hovered       = &T::IsItemHovered;
        volatile auto tooltip       = &T::SetTooltip;
        (void)beginDisabled;
        (void)endDisabled;
        (void)dragFloat;
        (void)dragFloat3;
        (void)inputText;
        (void)sliderInt;
        (void)checkbox;
        (void)entityField;
        (void)textDisabled;
        (void)hovered;
        (void)tooltip;
    }

} // namespace FRIGGA_NAMESPACE
