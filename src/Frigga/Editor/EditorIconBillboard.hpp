#pragma once

#include "Frigga/Macro.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace FRIGGA_NAMESPACE
{
    /// Screen-space icon "billboards" using the Bootstrap Icons glyph font merged into ImGui.
    namespace EditorIconBillboard
    {
        // Same codepoints as Editor/BoostrapIconsFont.hpp (ImGui merge font).
        inline constexpr const char *kCameraVideo = "\uea3f";
        inline constexpr const char *kVolumeUp    = "\uef55";
        inline constexpr const char *kMic         = "\ued5c";

        /// Matches EditorUiScale::Sync → ImGuiIO::FontGlobalScale (multi-monitor DPI).
        /// Explicit ImDrawList font sizes do not inherit FontGlobalScale automatically.
        [[nodiscard]] inline float DisplayScale()
        {
            if(ImGui::GetCurrentContext() == nullptr)
            {
                return 1.0f;
            }
            return std::max(ImGui::GetIO().FontGlobalScale, 0.01f);
        }

        [[nodiscard]] inline float Dpi(float value)
        {
            return value * DisplayScale();
        }

        inline void Draw(ImDrawList *drawList, const ImVec2 &screen, const char *iconUtf8,
                         ImU32 iconColor, bool selected, float fontSize = 18.0f)
        {
            if(drawList == nullptr || iconUtf8 == nullptr || iconUtf8[0] == '\0')
            {
                return;
            }

            const float scaledSize = Dpi(fontSize);
            ImFont *font           = ImGui::GetFont();
            const ImVec2 textSize =
                font->CalcTextSizeA(scaledSize, FLT_MAX, 0.0f, iconUtf8);
            const float radius =
                std::max(textSize.x, textSize.y) * 0.72f + (selected ? Dpi(2.0f) : 0.0f);

            drawList->AddCircleFilled(screen, radius,
                                      selected ? IM_COL32(28, 32, 42, 230) : IM_COL32(18, 20, 26, 190),
                                      24);
            if(selected)
            {
                drawList->AddCircle(screen, radius + Dpi(1.5f), IM_COL32(80, 200, 255, 255), 24,
                                    Dpi(1.75f));
            }

            const ImVec2 pos {screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f};
            drawList->AddText(font, scaledSize, ImVec2 {pos.x + Dpi(1.0f), pos.y + Dpi(1.0f)},
                              IM_COL32(0, 0, 0, 160), iconUtf8);
            drawList->AddText(font, scaledSize, pos, iconColor, iconUtf8);
        }
    } // namespace EditorIconBillboard
} // namespace FRIGGA_NAMESPACE
