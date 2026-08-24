#include "Styles.hpp"

namespace FRIGGA_NAMESPACE
{

void StylePhantomDark(ImGuiStyle *dst)
{
    ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();
    ImVec4     *colors = style->Colors;

    const ImVec4 accent   = ImVec4(0.45f, 0.47f, 0.98f, 1.00f);
    const ImVec4 accentHi = ImVec4(0.58f, 0.60f, 1.00f, 1.00f);
    const ImVec4 accentLo = ImVec4(0.34f, 0.36f, 0.82f, 1.00f);
    const ImVec4 surface0 = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    const ImVec4 surface1 = ImVec4(0.11f, 0.11f, 0.14f, 1.00f);
    const ImVec4 surface2 = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
    const ImVec4 surface3 = ImVec4(0.19f, 0.20f, 0.26f, 1.00f);
    const ImVec4 border   = ImVec4(0.28f, 0.29f, 0.38f, 0.72f);

    colors[ImGuiCol_Text]           = ImVec4(0.93f, 0.94f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.52f, 0.54f, 0.62f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.38f);

    colors[ImGuiCol_WindowBg] = surface0;
    colors[ImGuiCol_ChildBg]  = surface1;
    colors[ImGuiCol_PopupBg]  = ImVec4(0.10f, 0.10f, 0.13f, 0.98f);

    colors[ImGuiCol_Border]       = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg]        = surface2;
    colors[ImGuiCol_FrameBgHovered] = surface3;
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.24f, 0.25f, 0.32f, 1.00f);

    colors[ImGuiCol_TitleBg]          = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]    = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.07f, 0.09f, 0.85f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);

    colors[ImGuiCol_ScrollbarBg]          = surface0;
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.31f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.39f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = accentLo;

    colors[ImGuiCol_CheckMark]        = accentHi;
    colors[ImGuiCol_SliderGrab]       = ImVec4(accentLo.x, accentLo.y, accentLo.z, 0.85f);
    colors[ImGuiCol_SliderGrabActive] = accentHi;

    colors[ImGuiCol_Button]        = surface2;
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.27f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(accent.x, accent.y, accent.z, 0.92f);

    colors[ImGuiCol_Header]        = surface2;
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.27f, 0.34f, 1.00f);
    colors[ImGuiCol_HeaderActive]  = ImVec4(accent.x, accent.y, accent.z, 0.55f);

    colors[ImGuiCol_Separator]        = border;
    colors[ImGuiCol_SeparatorHovered] = ImVec4(accentHi.x, accentHi.y, accentHi.z, 0.72f);
    colors[ImGuiCol_SeparatorActive]  = accentHi;

    colors[ImGuiCol_ResizeGrip]        = ImVec4(0.30f, 0.31f, 0.40f, 0.55f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.78f);
    colors[ImGuiCol_ResizeGripActive]  = accentHi;

    colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.82f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.27f, 0.34f, 1.00f);
    colors[ImGuiCol_TabActive] =
        ImVec4(0.18f, 0.19f, 0.26f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.21f, 0.30f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabUnfocused] =
        ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.90f);
    colors[ImGuiCol_TabUnfocusedActive] =
        ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.85f);

    colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.65f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);

    colors[ImGuiCol_PlotLines]          = ImVec4(0.58f, 0.61f, 0.72f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]   = accentHi;
    colors[ImGuiCol_PlotHistogram]      = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHi;

    colors[ImGuiCol_TableHeaderBg]      = surface2;
    colors[ImGuiCol_TableBorderStrong]  = border;
    colors[ImGuiCol_TableBorderLight]   = ImVec4(0.20f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_TableRowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]      = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    colors[ImGuiCol_DragDropTarget]      = accentHi;
    colors[ImGuiCol_NavHighlight]        = accentHi;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.65f);
    colors[ImGuiCol_NavWindowingDimBg]   = ImVec4(0.00f, 0.00f, 0.00f, 0.58f);
    colors[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.02f, 0.02f, 0.04f, 0.62f);
}

} // namespace FRIGGA_NAMESPACE
