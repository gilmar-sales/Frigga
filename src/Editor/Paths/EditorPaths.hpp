#pragma once

#include <filesystem>
#include <string>

/**
 * OS writable / user directories for the Editor.
 * Backed by SDL today (GetPrefPath / GetUserFolder); keep call sites on this
 * facade so the provider can be swapped without scattering SDL usage.
 */
class EditorPaths
{
  public:
    static constexpr const char *OrgName             = "Frigga";
    static constexpr const char *AppName             = "Editor";
    static constexpr const char *FriggaHomeFolder    = "Frigga";
    static constexpr const char *ProjectsFolderName  = "Projects";
    static constexpr const char *PluginsFolderName   = "Plugins";
    static constexpr const char *LegacyProjectsFolderName = "FriggaProjects";
    static constexpr const char *PreferencesFileName = "preferences.json";

    /// Writable app data dir (PhysFS/SDL "preferred" location). Created if needed.
    [[nodiscard]] static std::filesystem::path PreferredDir();

    /// User home directory (fallback-friendly).
    [[nodiscard]] static std::filesystem::path UserHomeDir();

    /// `UserHomeDir() / Frigga` — parent for Projects + Plugins.
    [[nodiscard]] static std::filesystem::path FriggaHomeDir();

    /// `UserHomeDir() / Frigga / Projects` — default parent for new projects.
    [[nodiscard]] static std::filesystem::path DefaultProjectsDir();

    /// `UserHomeDir() / FriggaProjects` — pre-v13 default (not migrated automatically).
    [[nodiscard]] static std::filesystem::path LegacyProjectsDir();

    /// `UserHomeDir() / Frigga / Plugins` — shared user plugin library.
    [[nodiscard]] static std::filesystem::path DefaultPluginsDir();

    /// `PreferredDir() / preferences.json`
    [[nodiscard]] static std::filesystem::path PreferencesFile();

    /// Ensures PreferredDir + Frigga home (Projects + Plugins) exist.
    static void EnsureDirectories();
};
