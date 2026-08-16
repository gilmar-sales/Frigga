#pragma once

#include "Frigga/Macro.hpp"

#include <Freyr/Freyr.hpp>

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Widget host for plugin component inspectors. Implemented by the Editor (ImGui);
    /// plugins call this instead of including imgui.
    class FriComponentInspector
    {
      public:
        fr::Entity    entity        = 0;
        bool          playing       = false;
        bool          hasCharacter  = false;
        std::uint32_t characterId   = 0;

        void BeginDisabled(bool disabled);
        void EndDisabled();

        void DragFloat(const char *label, float &value, float speed = 0.01f, float min = 0.0f,
                       float max = 0.0f);
        void DragFloat3(const char *label, glm::vec3 &value, float speed = 0.01f);
        bool InputText(const char *label, std::string &value);
        bool SliderInt(const char *label, int &value, int vmin, int vmax);
        void TextDisabled(const char *text);
        [[nodiscard]] bool IsItemHovered() const;
        void SetTooltip(const char *text);
    };

    /// Pulls inspector method .o into the Editor so plugin .so can resolve them at dlopen.
    void FriKeepComponentInspectorSymbols();

    template <typename T>
    using FriDrawComponent = void (*)(T &, FriComponentInspector &);

} // namespace FRIGGA_NAMESPACE
