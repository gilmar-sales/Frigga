#include "Styles.hpp"

namespace FRIGGA_NAMESPACE {

void StylePhantomLight(ImGuiStyle* dst)
{
    ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    // Accent color (modern indigo/violet).
    const ImVec4 accent    = ImVec4(0.42f, 0.44f, 0.94f, 1.00f);
    const ImVec4 accentHi  = ImVec4(0.50f, 0.52f, 0.96f, 1.00f);
    const ImVec4 accentLo  = ImVec4(0.36f, 0.37f, 0.80f, 1.00f);

    colors[ImGuiCol_Text] = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.56f, 0.62f, 1.00f);

    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.30f);

    colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.95f, 0.97f, 1.00f);
    colors[ImGuiCol_ChildBg]  = ImVec4(0.90f, 0.91f, 0.94f, 1.00f);
    colors[ImGuiCol_PopupBg]  = ImVec4(0.97f, 0.97f, 0.98f, 1.00f);

    colors[ImGuiCol_Border] = ImVec4(0.54f, 0.56f, 0.64f, 0.60f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.86f, 0.87f, 0.91f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.80f, 0.81f, 0.87f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.74f, 0.75f, 0.83f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.88f, 0.89f, 0.92f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.93f, 0.95f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.87f, 0.88f, 0.91f, 1.00f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.86f, 0.87f, 0.91f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.66f, 0.68f, 0.75f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.58f, 0.60f, 0.69f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(accentLo.x, accentLo.y, accentLo.z, 1.00f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = ImVec4(accentLo.x, accentLo.y, accentLo.z, 0.80f);
    colors[ImGuiCol_SliderGrabActive] = accent;

    colors[ImGuiCol_Button] = ImVec4(0.86f, 0.87f, 0.91f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.81f, 0.87f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.86f, 0.87f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.80f, 0.81f, 0.87f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 1.00f);

    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered] = ImVec4(accentHi.x, accentHi.y, accentHi.z, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(accentHi.x, accentHi.y, accentHi.z, 1.00f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.66f, 0.68f, 0.75f, 0.60f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.80f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(accentHi.x, accentHi.y, accentHi.z, 1.00f);

    colors[ImGuiCol_Tab] = ImLerp(colors[ImGuiCol_Header], colors[ImGuiCol_TitleBgActive], 0.90f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.80f, 0.81f, 0.87f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.90f);
    colors[ImGuiCol_TabUnfocusedActive] = ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.90f);

    colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.88f, 0.89f, 0.92f, 1.00f);

    colors[ImGuiCol_PlotLines] = ImVec4(0.58f, 0.60f, 0.69f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = accent;
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHi;

    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.86f, 0.87f, 0.91f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.60f, 0.62f, 0.70f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.74f, 0.75f, 0.82f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.04f);

    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_NavHighlight] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.25f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
}

}
