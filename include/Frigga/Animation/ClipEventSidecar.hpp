#pragma once

#ifndef FREYA_NAMESPACE
#define FREYA_NAMESPACE fra
#endif

#include <Frigga/Macro.hpp>

#include <Freya/Asset/AnimationClip.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{

    /// Clip name → authored event list. std::map keeps sidecar saves
    /// deterministic across runs.
    using ClipEventMap = std::map<std::string, std::vector<fra::AnimationEvent>>;

    /// Suffix appended to the model stem, e.g. `Models/Fox.glb` →
    /// `Models/Fox.anim-events.json` next to the model under Resources/.
    inline constexpr std::string_view kClipEventSidecarSuffix = ".anim-events.json";

    /// Sidecar path (relative to Resources/) for a normalized model key.
    [[nodiscard]] std::filesystem::path
    ClipEventSidecarRelativePath(const std::filesystem::path &modelRelativePath);

    /// Load a sidecar file. Missing `events` object loads as empty.
    /// Returns false with `*error` set on malformed JSON.
    [[nodiscard]] bool LoadClipEventSidecar(const std::filesystem::path &absolutePath,
                                            ClipEventMap &out, std::string *error = nullptr);

    /// Save the full event map (creates parent directories). Sorted by clip,
    /// events sorted by time.
    [[nodiscard]] bool SaveClipEventSidecar(const std::filesystem::path &absolutePath,
                                            const ClipEventMap &events,
                                            std::string *error = nullptr);

    /// Clamp times into [0, duration] (when duration > 0) and sort by time.
    void NormalizeClipEvents(std::vector<fra::AnimationEvent> &events, float duration);

    /// Replace `events` on every clip named in `overrides`. Unknown clip names
    /// are ignored; clips without an entry keep their imported events.
    void ApplyClipEventOverrides(std::vector<fra::AnimationClip> &clips,
                                 const ClipEventMap &overrides);

    /// Snapshot every clip (including empty lists) for sidecar persistence.
    [[nodiscard]] ClipEventMap SnapshotClipEvents(const std::vector<fra::AnimationClip> &clips);

} // namespace FRIGGA_NAMESPACE
