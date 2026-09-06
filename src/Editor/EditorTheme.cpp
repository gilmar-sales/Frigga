#include "EditorTheme.hpp"

#include <Frigga/Gui/Styles/Styles.hpp>
#include <imgui.h>

void ApplyTheme(EditorTheme theme)
{
    ImGuiStyle &style = ImGui::GetStyle();
    fg::StyleModernMetrics(&style);

    switch(theme)
    {
    case EditorTheme::PhantomDark:
        fg::StylePhantomDark(&style);
        break;
    case EditorTheme::PhantomLight:
        fg::StylePhantomLight(&style);
        break;
    case EditorTheme::Dark:
        ImGui::StyleColorsDark(&style);
        break;
    case EditorTheme::Light:
        ImGui::StyleColorsLight(&style);
        break;
    case EditorTheme::Classic:
        ImGui::StyleColorsClassic(&style);
        break;
    default:
       ApplyTheme(EditorTheme::Default);
        break;
    }
}
