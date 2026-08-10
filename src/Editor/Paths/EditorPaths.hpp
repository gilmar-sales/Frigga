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
    static constexpr const char *ProjectsFolderName  = "FriggaProjects";
    static constexpr const char *PreferencesFileName = "preferences.json";

    /// Writable app data dir (PhysFS/SDL "preferred" location). Created if needed.
    [[nodiscard]] static std::filesystem::path PreferredDir();

    /// User home directory (fallback-friendly).
    [[nodiscard]] static std::filesystem::path UserHomeDir();

    /// `UserHomeDir() / FriggaProjects` — default parent for new projects.
    [[nodiscard]] static std::filesystem::path DefaultProjectsDir();

    /// `PreferredDir() / preferences.json`
    [[nodiscard]] static std::filesystem::path PreferencesFile();

    /// Ensures PreferredDir + DefaultProjectsDir exist.
    static void EnsureDirectories();
};
