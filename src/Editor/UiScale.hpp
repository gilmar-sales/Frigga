#pragma once

#include <SDL3/SDL_events.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace EditorUiScale
{
    /// Design reference for proportional ImGui layout (logical work-area height).
    inline constexpr float kReferenceWorkHeight = 1080.0f;
    inline constexpr float kMinLayoutScale      = 0.75f;
    inline constexpr float kMaxLayoutScale      = 1.35f;

    [[nodiscard]] inline float ClampDisplayScale(float scale)
    {
        return scale > 0.0f ? scale : 1.0f;
    }

    /// Keep ImGui::GetIO().FontGlobalScale aligned with the window's current display.
    ///
    /// Call every frame (or at least whenever the window may have changed monitors).
    /// Relying only on SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED is unreliable: dragging
    /// between monitors often updates SDL_GetDisplayContentScale without that event
    /// until a resize/pixel-size change arrives.
    inline void Sync(float displayContentScale)
    {
        if(ImGui::GetCurrentContext() == nullptr)
        {
            return;
        }

        ImGuiIO   &io    = ImGui::GetIO();
        const float scale = ClampDisplayScale(displayContentScale);
        if(std::abs(io.FontGlobalScale - scale) > 0.001f)
        {
            io.FontGlobalScale = scale;
        }
    }

    /// True when this SDL event may imply a different display / DPI / framebuffer.
    [[nodiscard]] inline bool IsDisplayTopologyEvent(const SDL_Event &event)
    {
        // SDL3 flattens window events: event.type == event.window.type == SDL_EVENT_WINDOW_*.
        switch(event.type)
        {
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_MOVED:
        case SDL_EVENT_WINDOW_RESIZED:
            return true;
        default:
            return false;
        }
    }

    /// Proportional layout scale from the main viewport work size.
    /// Independent of FontGlobalScale (fonts already track display content scale).
    [[nodiscard]] inline float LayoutScale()
    {
        if(ImGui::GetCurrentContext() == nullptr)
        {
            return 1.0f;
        }

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        const float workHeight =
            viewport != nullptr ? viewport->WorkSize.y : kReferenceWorkHeight;
        const float raw = workHeight / kReferenceWorkHeight;
        return std::clamp(raw, kMinLayoutScale, kMaxLayoutScale);
    }

    [[nodiscard]] inline float S(float value)
    {
        return value * LayoutScale();
    }

    [[nodiscard]] inline ImVec2 V(float x, float y)
    {
        const float scale = LayoutScale();
        return {x * scale, y * scale};
    }

    /// Readable aliases for layout sizes that track the work-area scale.
    [[nodiscard]] inline float DpiAware(float value)
    {
        return S(value);
    }

    [[nodiscard]] inline ImVec2 DpiAware(float x, float y)
    {
        return V(x, y);
    }
} // namespace EditorUiScale
