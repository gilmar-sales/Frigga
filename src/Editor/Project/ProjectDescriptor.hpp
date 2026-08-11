#pragma once

#include <Frigga/Scene/Scene.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

struct ProjectDescriptor
{
    /// Persistent project format version written to frigga.project.
    /// Missing / 0 on disk is treated as LegacyFormatVersion (1).
    static constexpr int LegacyFormatVersion  = 1;
    static constexpr int CurrentFormatVersion = 8;

    int formatVersion = CurrentFormatVersion;

    std::string name;
    fg::SceneTemplate sceneTemplate = fg::SceneTemplate::D3;
    std::string sceneRelativePath   = "scenes/main.json";
    std::string pluginTarget        = "gameplay";
    /// Relative to project root (after cmake --build).
    std::string pluginLibraryRelative = "build/libgameplay.so";
    std::filesystem::path friggaRoot;
    std::filesystem::path friggaBuild;

    [[nodiscard]] std::string TemplateId() const
    {
        return sceneTemplate == fg::SceneTemplate::D2 ? "2d" : "3d";
    }

    static fg::SceneTemplate TemplateFromId(std::string_view id)
    {
        return id == "2d" ? fg::SceneTemplate::D2 : fg::SceneTemplate::D3;
    }
};
