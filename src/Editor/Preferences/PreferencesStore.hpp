#pragma once

#include "EditorPreferences.hpp"

#include <Skirnir/Configuration.hpp>

#include <filesystem>
#include <string>

class PreferencesStore
{
  public:
    static constexpr std::size_t MaxRecentProjects = 10;

    /// Default on-disk path (`EditorPaths::PreferencesFile()`).
    [[nodiscard]] static std::filesystem::path DefaultPath();

    /// Registers preferences.json on the application configuration builder when present.
    static void Configure(skr::ConfigurationBuilder &configurationBuilder,
                          const std::filesystem::path &path = {});

    /// Early Bind for Freya/Freyr WithOptions (before ApplicationBuilder::Build).
    static skr::Arc<EditorPreferences> Load(const std::filesystem::path &path = {});

    static void Save(const EditorPreferences &preferences,
                     const std::filesystem::path &path = {});

    /// Inserts or promotes a project to the front of recentProjects (deduped).
    static void TouchRecentProject(EditorPreferences &preferences,
                                   const std::filesystem::path &projectFile,
                                   const std::string &name);
};
