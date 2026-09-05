#pragma once

#include <Frigga/Scene/Scene.hpp>

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class ModuleSource : std::uint8_t
{
    Project = 0,
    User,
};

struct ProjectModuleEntry
{
    std::string id;
    std::string target;
    std::string libraryRelative;
    bool enabled           = true;
    ModuleSource source    = ModuleSource::Project;

    [[nodiscard]] bool IsGameplay() const
    {
        return id == "gameplay" || target == "gameplay";
    }
};

struct ProjectBranding
{
    std::string displayName;
    std::string executableName;
    std::string publisher;
    std::string copyright;
    std::string version = "1.0.0";
    std::string identifier;
    std::filesystem::path iconWindows;
    std::filesystem::path iconLinux;
    std::filesystem::path iconMacOS;
};

struct ProjectDescriptor
{
    /// Persistent project format version written to frigga.project.
    /// Missing / 0 on disk is treated as LegacyFormatVersion (1).
    static constexpr int LegacyFormatVersion  = 1;
    static constexpr int CurrentFormatVersion = 5;
    static constexpr std::string_view ModulesDirName   = "Modules";
    static constexpr std::string_view ResourcesDirName = "Resources";

    int formatVersion = CurrentFormatVersion;

    std::string name;
    ProjectBranding branding;
    fg::SceneTemplate sceneTemplate = fg::SceneTemplate::D3;
    std::string sceneRelativePath   = "Scenes/main.json";
    std::string moduleTarget        = "gameplay";
    /// Relative to project root (after cmake --build). Gameplay convenience mirror.
    std::string moduleLibraryRelative = "build/libgameplay.so";
    std::vector<ProjectModuleEntry> modules;
    /// Packaged `Sdk/` next to the Editor, or the engine source tree. Last-used hint;
    /// CMake resolves via `-DFRIGGA_SDK`, `FRIGGA_SDK` env, or `CMakeUserPresets.json`.
    std::filesystem::path friggaSdk;
    std::filesystem::path friggaRoot;
    std::filesystem::path friggaBuild;

    void EnsureBranding()
    {
        if(branding.displayName.empty())
        {
            branding.displayName = name;
        }
        if(branding.executableName.empty())
        {
            branding.executableName = name;
        }
        if(branding.publisher.empty())
        {
            branding.publisher = "Frigga";
        }
        if(branding.copyright.empty())
        {
            branding.copyright = "Copyright © " + branding.publisher;
        }
        if(branding.identifier.empty())
        {
            std::string id = branding.executableName;
            for(char &ch : id)
            {
                if(!std::isalnum(static_cast<unsigned char>(ch)))
                {
                    ch = '-';
                }
                else
                {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
            }
            branding.identifier = "com.frigga." + id;
        }
    }

    [[nodiscard]] std::string TemplateId() const
    {
        return sceneTemplate == fg::SceneTemplate::D2 ? "2d" : "3d";
    }

    static fg::SceneTemplate TemplateFromId(std::string_view id)
    {
        return id == "2d" ? fg::SceneTemplate::D2 : fg::SceneTemplate::D3;
    }

    [[nodiscard]] static std::string DefaultLibraryRelative(std::string_view target)
    {
#ifdef _WIN32
        return "build/" + std::string(target) + ".dll";
#elif defined(__APPLE__)
        return "build/lib" + std::string(target) + ".dylib";
#else
        return "build/lib" + std::string(target) + ".so";
#endif
    }

    void SyncGameplayMirror()
    {
        for(const auto &entry : modules)
        {
            if(entry.IsGameplay())
            {
                moduleTarget           = entry.target.empty() ? "gameplay" : entry.target;
                moduleLibraryRelative  = entry.libraryRelative;
                return;
            }
        }
        if(moduleTarget.empty())
        {
            moduleTarget = "gameplay";
        }
        if(moduleLibraryRelative.empty())
        {
            moduleLibraryRelative = DefaultLibraryRelative(moduleTarget);
        }
    }

    void EnsureGameplayModule()
    {
        SyncGameplayMirror();
        for(const auto &entry : modules)
        {
            if(entry.IsGameplay())
            {
                return;
            }
        }
        modules.insert(modules.begin(),
                       ProjectModuleEntry {.id               = "gameplay",
                                           .target           = moduleTarget,
                                           .libraryRelative  = moduleLibraryRelative,
                                           .enabled          = true,
                                           .source           = ModuleSource::Project});
    }

    [[nodiscard]] std::vector<ProjectModuleEntry> LoadOrder() const
    {
        std::vector<ProjectModuleEntry> extras;
        std::vector<ProjectModuleEntry> gameplay;
        for(const auto &entry : modules)
        {
            if(!entry.enabled)
            {
                continue;
            }
            if(entry.IsGameplay())
            {
                gameplay.push_back(entry);
            }
            else
            {
                extras.push_back(entry);
            }
        }
        extras.insert(extras.end(), gameplay.begin(), gameplay.end());
        return extras;
    }
};
