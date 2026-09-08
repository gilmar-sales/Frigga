#pragma once

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace EditorViewport
{
    /// ImGui content sizes are in logical points; Freya render targets need pixels.
    /// Prefer `io.DisplayFramebufferScale` (refreshed every SDL3 NewFrame) and take
    /// the max with the main viewport scale. Do not use EditorUiScale / FontGlobalScale
    /// here — that tracks display *content* scale for fonts/layout (a different axis).
    [[nodiscard]] inline ImVec2 FramebufferScale()
    {
        if(ImGui::GetCurrentContext() == nullptr)
        {
            return {1.0f, 1.0f};
        }

        const ImGuiIO &io = ImGui::GetIO();
        ImVec2 scale {std::max(io.DisplayFramebufferScale.x, 1.0f),
                      std::max(io.DisplayFramebufferScale.y, 1.0f)};

        const ImGuiViewport *main = ImGui::GetMainViewport();
        if(main != nullptr)
        {
            scale.x = std::max(scale.x, std::max(main->FramebufferScale.x, 1.0f));
            scale.y = std::max(scale.y, std::max(main->FramebufferScale.y, 1.0f));
        }

        return scale;
    }

    inline void ContentSizeToRenderPixels(const ImVec2 &contentAvail, std::uint32_t &outWidth,
                                          std::uint32_t &outHeight)
    {
        const ImVec2 scale = FramebufferScale();
        outWidth =
            static_cast<std::uint32_t>(std::max(contentAvail.x * scale.x, 1.0f));
        outHeight =
            static_cast<std::uint32_t>(std::max(contentAvail.y * scale.y, 1.0f));
    }
} // namespace EditorViewport
