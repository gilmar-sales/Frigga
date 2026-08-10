#pragma once

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace EditorViewport
{
    /// ImGui content sizes are in logical points; Freya render targets need pixels.
    /// For ImGui font / layout DPI (multi-monitor), use EditorUiScale in UiScale.hpp —
    /// do not reuse DisplayFramebufferScale as FontGlobalScale.
    [[nodiscard]] inline ImVec2 FramebufferScale()
    {
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        return {std::max(scale.x, 1.0f), std::max(scale.y, 1.0f)};
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
