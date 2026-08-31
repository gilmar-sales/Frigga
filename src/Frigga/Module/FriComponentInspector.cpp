#include "Frigga/Module/FriComponentInspector.hpp"

#include <cstdio>
#include <imgui.h>

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
        volatile auto textDisabled  = &T::TextDisabled;
        volatile auto hovered       = &T::IsItemHovered;
        volatile auto tooltip       = &T::SetTooltip;
        (void)beginDisabled;
        (void)endDisabled;
        (void)dragFloat;
        (void)dragFloat3;
        (void)inputText;
        (void)sliderInt;
        (void)textDisabled;
        (void)hovered;
        (void)tooltip;
    }

} // namespace FRIGGA_NAMESPACE
