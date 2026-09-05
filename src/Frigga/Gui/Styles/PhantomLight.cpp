#include <Frigga/Gui/Styles/Styles.hpp>

namespace FRIGGA_NAMESPACE
{

void StylePhantomLight(ImGuiStyle *dst)
{
    ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();
    ImVec4     *colors = style->Colors;

    const ImVec4 accent   = ImVec4(0.42f, 0.44f, 0.94f, 1.00f);
    const ImVec4 accentHi = ImVec4(0.50f, 0.52f, 0.96f, 1.00f);
    const ImVec4 accentLo = ImVec4(0.36f, 0.37f, 0.80f, 1.00f);
    const ImVec4 surface0 = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    const ImVec4 surface1 = ImVec4(0.91f, 0.92f, 0.95f, 1.00f);
    const ImVec4 surface2 = ImVec4(0.86f, 0.87f, 0.92f, 1.00f);
    const ImVec4 surface3 = ImVec4(0.80f, 0.81f, 0.88f, 1.00f);
    const ImVec4 border   = ImVec4(0.58f, 0.60f, 0.68f, 0.55f);

    colors[ImGuiCol_Text]           = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.52f, 0.54f, 0.60f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.28f);

    colors[ImGuiCol_WindowBg] = surface0;
    colors[ImGuiCol_ChildBg]  = surface1;
    colors[ImGuiCol_PopupBg]  = ImVec4(0.98f, 0.98f, 0.99f, 0.98f);

    colors[ImGuiCol_Border]       = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg]        = surface2;
    colors[ImGuiCol_FrameBgHovered] = surface3;
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.74f, 0.75f, 0.83f, 1.00f);

    colors[ImGuiCol_TitleBg]          = ImVec4(0.90f, 0.91f, 0.94f, 1.00f);
    colors[ImGuiCol_TitleBgActive]    = ImVec4(0.94f, 0.95f, 0.97f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.91f, 0.94f, 0.85f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(0.98f, 0.98f, 0.99f, 1.00f);

    colors[ImGuiCol_ScrollbarBg]          = surface1;
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.68f, 0.70f, 0.78f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.62f, 0.72f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = accentLo;

    colors[ImGuiCol_CheckMark]        = accent;
    colors[ImGuiCol_SliderGrab]       = ImVec4(accentLo.x, accentLo.y, accentLo.z, 0.85f);
    colors[ImGuiCol_SliderGrabActive] = accent;

    colors[ImGuiCol_Button]        = surface2;
    colors[ImGuiCol_ButtonHovered] = surface3;
    colors[ImGuiCol_ButtonActive]  = ImVec4(accent.x, accent.y, accent.z, 0.90f);

    colors[ImGuiCol_Header]        = surface2;
    colors[ImGuiCol_HeaderHovered] = surface3;
    colors[ImGuiCol_HeaderActive]  = ImVec4(accent.x, accent.y, accent.z, 0.45f);

    colors[ImGuiCol_Separator]        = border;
    colors[ImGuiCol_SeparatorHovered] = ImVec4(accentHi.x, accentHi.y, accentHi.z, 0.72f);
    colors[ImGuiCol_SeparatorActive]  = accentHi;

    colors[ImGuiCol_ResizeGrip]        = ImVec4(0.68f, 0.70f, 0.78f, 0.55f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.72f);
    colors[ImGuiCol_ResizeGripActive]  = accentHi;

    colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.82f);
    colors[ImGuiCol_TabHovered] = surface3;
    colors[ImGuiCol_TabActive]  = ImVec4(0.98f, 0.98f, 0.99f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabUnfocused] =
        ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.90f);
    colors[ImGuiCol_TabUnfocusedActive] =
        ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.85f);

    colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.60f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.90f, 0.91f, 0.94f, 1.00f);

    colors[ImGuiCol_PlotLines]            = ImVec4(0.58f, 0.60f, 0.69f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]     = accent;
    colors[ImGuiCol_PlotHistogram]        = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHi;

    colors[ImGuiCol_TableHeaderBg]     = surface2;
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.62f, 0.64f, 0.72f, 1.00f);
    colors[ImGuiCol_TableBorderLight]  = ImVec4(0.76f, 0.77f, 0.84f, 1.00f);
    colors[ImGuiCol_TableRowBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.00f, 0.00f, 0.00f, 0.03f);

    colors[ImGuiCol_DragDropTarget]        = accent;
    colors[ImGuiCol_NavHighlight]          = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.65f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.22f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.05f, 0.06f, 0.10f, 0.35f);
}

} // namespace FRIGGA_NAMESPACE
