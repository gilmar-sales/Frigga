#pragma once

#include <Freya/FreyaOptions.hpp>
#include <Freya/Core/Renderer.hpp>

#include "Editor/Preferences/EditorPreferences.hpp"

#include <algorithm>

namespace EditorViewport
{
    [[nodiscard]] inline int ClampQualityIndex(int value)
    {
        return std::clamp(value, 0, 4);
    }

    /// Apply Freya pass qualities when they differ (safe to call every frame).
    inline bool ApplyQualityPreferences(fra::Renderer &renderer,
                                        const ViewportQualityPreferences &quality)
    {
        bool changed = false;

        const auto shadow =
            static_cast<fra::ShadowQuality>(ClampQualityIndex(quality.shadowQuality));
        if(renderer.GetShadowQuality() != shadow)
        {
            renderer.SetShadowQuality(shadow);
            changed = true;
        }

        const auto ssao =
            static_cast<fra::SsaoQuality>(ClampQualityIndex(quality.ssaoQuality));
        if(renderer.GetSsaoQuality() != ssao)
        {
            renderer.SetSsaoQuality(ssao);
            changed = true;
        }

        const auto taa =
            static_cast<fra::TaaQuality>(ClampQualityIndex(quality.taaQuality));
        if(renderer.GetTaaQuality() != taa)
        {
            renderer.SetTaaQuality(taa);
            changed = true;
        }

        const auto bloom =
            static_cast<fra::BloomQuality>(ClampQualityIndex(quality.bloomQuality));
        if(renderer.GetBloomQuality() != bloom)
        {
            renderer.SetBloomQuality(bloom);
            changed = true;
        }

        return changed;
    }
} // namespace EditorViewport
