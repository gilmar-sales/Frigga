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

    /// Per-layer cache of the last applied prefs. Lets callers skip the
    /// renderer getters entirely when prefs are unchanged (cheap early-out
    /// before touching Freya state every frame).
    struct AppliedQualityCache
    {
        int shadow = -1;
        int ssao   = -1;
        int taa    = -1;
        int bloom  = -1;

        [[nodiscard]] bool Matches(const ViewportQualityPreferences &quality) const
        {
            return shadow == ClampQualityIndex(quality.shadowQuality) &&
                   ssao == ClampQualityIndex(quality.ssaoQuality) &&
                   taa == ClampQualityIndex(quality.taaQuality) &&
                   bloom == ClampQualityIndex(quality.bloomQuality);
        }

        void Store(const ViewportQualityPreferences &quality)
        {
            shadow = ClampQualityIndex(quality.shadowQuality);
            ssao   = ClampQualityIndex(quality.ssaoQuality);
            taa    = ClampQualityIndex(quality.taaQuality);
            bloom  = ClampQualityIndex(quality.bloomQuality);
        }
    };

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

    /// Cached variant: returns false immediately when prefs match the last
    /// applied set, without querying the renderer. Callers keep one cache per
    /// viewport prefs block (editor / gameplay / preview).
    inline bool ApplyQualityPreferences(fra::Renderer &renderer,
                                        const ViewportQualityPreferences &quality,
                                        AppliedQualityCache &cache)
    {
        if(cache.Matches(quality))
        {
            return false;
        }
        const bool changed =
            ApplyQualityPreferences(renderer, quality);
        cache.Store(quality);
        return changed;
    }
} // namespace EditorViewport
