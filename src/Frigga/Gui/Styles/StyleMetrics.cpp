#include <Frigga/Gui/Styles/Styles.hpp>

namespace FRIGGA_NAMESPACE
{

void StyleModernMetrics(ImGuiStyle *dst)
{
    ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();

    style->Colors[ImGuiCol_WindowBg].w = 1.0f;

    style->WindowRounding   = 10.0f;
    style->WindowBorderSize = 1.0f;
    style->WindowPadding    = {12.0f, 12.0f};

    style->GrabRounding = 6.0f;
    style->GrabMinSize  = 12.0f;

    style->AntiAliasedLines = true;
    style->AntiAliasedFill  = true;
    style->IndentSpacing    = 22.0f;

    style->ChildRounding   = 8.0f;
    style->ChildBorderSize = 1.0f;

    style->ScrollbarRounding = 8.0f;
    style->ScrollbarSize     = 14.0f;

    style->TabRounding   = 8.0f;
    style->TabBorderSize = 1.0f;

    style->FrameRounding   = 8.0f;
    style->FrameBorderSize = 1.0f;
    style->FramePadding    = {8.0f, 6.0f};

    style->PopupBorderSize = 1.0f;
    style->PopupRounding   = 10.0f;

    style->ItemInnerSpacing = {8.0f, 8.0f};
    style->ItemSpacing      = {8.0f, 8.0f};

    style->SelectableTextAlign     = {0.0f, 0.5f};
    style->ButtonTextAlign         = {0.5f, 0.5f};
    style->SeparatorTextBorderSize = 1.0f;
    style->SeparatorTextPadding    = {12.0f, 4.0f};

    style->Alpha                    = 1.0f;
    style->WindowMenuButtonPosition = ImGuiDir_None;
}

} // namespace FRIGGA_NAMESPACE
