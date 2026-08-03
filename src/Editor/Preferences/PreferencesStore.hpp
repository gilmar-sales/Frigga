#pragma once

#include "EditorPreferences.hpp"

#include <Skirnir/Configuration.hpp>

#include <filesystem>

class PreferencesStore
{
  public:
    static constexpr const char *DefaultPath = "preferences.json";

    /// Registers preferences.json on the application configuration builder when present.
    static void Configure(skr::ConfigurationBuilder &configurationBuilder,
                          const std::filesystem::path &path = DefaultPath);

    /// Early Bind for Freya/Freyr WithOptions (before ApplicationBuilder::Build).
    static skr::Arc<EditorPreferences> Load(
        const std::filesystem::path &path = DefaultPath);

    static void Save(const EditorPreferences &preferences,
                     const std::filesystem::path &path = DefaultPath);
};
