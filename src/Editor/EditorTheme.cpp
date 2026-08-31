#include "EditorTheme.hpp"

#include <Frigga/Gui/Styles/Styles.hpp>
#include <imgui.h>

namespace EditorTheme
{

void Apply(int themeIndex)
{
    ImGuiStyle &style = ImGui::GetStyle();
    fg::StyleModernMetrics(&style);

    switch(themeIndex)
    {
    case 0:
        fg::StylePhantomDark(&style);
        break;
    case 1:
        fg::StylePhantomLight(&style);
        break;
    case 2:
        ImGui::StyleColorsDark(&style);
        break;
    case 3:
        ImGui::StyleColorsLight(&style);
        break;
    case 4:
        ImGui::StyleColorsClassic(&style);
        break;
    default:
        fg::StylePhantomDark(&style);
        break;
    }
}

} // namespace EditorTheme
