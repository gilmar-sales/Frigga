#pragma once

#include "../Project/ProjectDescriptor.hpp"
#include "AssetService.hpp"
#include "ProcessRunner.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Staged game publication: Validate → Configure → Build → Install → Cook → Stage → Finalize.
/// Composed through Skirnir DI (see PipelineFactory): process execution,
/// asset cooking/validation and logging are all injected, so the pipeline
/// runs identically in the Editor, headless CLI and unit tests.
namespace publish
{
    enum class Profile : std::uint8_t
    {
        Development,
        Shipping,
    };

    enum class Stage : std::uint8_t
    {
        Validate,
        Configure,
        Build,
        Install,
        Cook,
        Stage,
        Finalize,
    };

    [[nodiscard]] std::string_view ToString(Profile profile);
    [[nodiscard]] std::string_view ToString(Stage stage);
    /// Human readable, e.g. "Cooking assets…".
    [[nodiscard]] std::string_view StageLabel(Stage stage);
    [[nodiscard]] std::optional<Profile> ParseProfile(std::string_view text);

    struct Options
    {
        std::filesystem::path projectRoot;
        std::filesystem::path destination;
        Profile profile        = Profile::Shipping;
        /// Replace a non-empty destination (swapped in atomically at the end).
        bool cleanDestination  = false;
        /// Descriptor with engine paths (friggaSdk / friggaBuild) already resolved.
        ProjectDescriptor descriptor;
        std::string cxxCompiler;
        /// Defaults to `<projectRoot>/build-release`.
        std::filesystem::path buildDir;
    };

    struct StageTiming
    {
        Stage stage    = Stage::Validate;
        double seconds = 0.0;
    };

    struct FileEntry
    {
        std::string path; ///< relative to the published root, forward slashes
        std::uintmax_t size = 0;
        std::string hash; ///< FNV-1a 64, 16 hex digits
    };

    struct Report
    {
        bool ok = false;
        int exitCode = 0;
        std::optional<Stage> failedStage;
        std::string error;
        std::vector<StageTiming> timings;
        std::vector<FileEntry> files;
        std::filesystem::path destination;
        std::filesystem::path releaseManifest;
    };

    struct Observer
    {
        /// Overall progress in [0,1]; @p determinate is false while a stage cannot estimate.
        std::function<void(Stage, float progress, bool determinate)> onProgress;
        std::function<void(std::string_view)> onLog;
    };

    class Pipeline
    {
      public:
        static constexpr const char *ReleaseManifestName = "frigga-release.json";

        /// All collaborators are injected Skirnir services. @p runner and
        /// @p assets are shared singletons; @p logger is the per-category
        /// `skr::Logger<Pipeline>` resolved from the container.
        Pipeline(Options options, skr::Arc<IProcessRunner> runner,
                 skr::Arc<IAssetService> assets, skr::Arc<skr::Logger<Pipeline>> logger,
                 Observer observer = {});

        [[nodiscard]] Report Run();

      private:
        [[nodiscard]] bool validate();
        [[nodiscard]] bool configure();
        [[nodiscard]] bool build();
        [[nodiscard]] bool install();
        [[nodiscard]] bool cook();
        [[nodiscard]] bool stageProject();
        [[nodiscard]] bool finalize();

        void progress(Stage stage, float fraction, bool determinate = true);
        void log(std::string_view line);
        bool fail(std::string message, int exitCode = 1);

        Options mOptions;
        skr::Arc<IProcessRunner> mRunner;
        skr::Arc<IAssetService> mAssets;
        skr::Arc<skr::Logger<Pipeline>> mLogger;
        Observer mObserver;
        Report mReport;
        std::filesystem::path mStaging;
        std::filesystem::path mBuildDir;
        std::filesystem::path mSdk;
        std::string mSignature;
        Stage mCurrent = Stage::Validate;
    };

    /// Skirnir-friendly factory: Options/Observer are per-run data and
    /// cannot be container services, so this singleton holds the shared
    /// collaborators and stamps out Pipelines on demand.
    class PipelineFactory
    {
      public:
        PipelineFactory(skr::Arc<IProcessRunner> runner, skr::Arc<IAssetService> assets,
                        skr::Arc<skr::Logger<Pipeline>> logger);

        [[nodiscard]] Pipeline Create(Options options, Observer observer = {}) const;

      private:
        skr::Arc<IProcessRunner> mRunner;
        skr::Arc<IAssetService> mAssets;
        skr::Arc<skr::Logger<Pipeline>> mLogger;
    };

    // Helpers exposed for tests and the Editor.
    [[nodiscard]] std::string Fnv1a64Hex(std::string_view data);
    [[nodiscard]] std::optional<std::string> HashFileHex(const std::filesystem::path &path);
    [[nodiscard]] std::string ReadTextFile(const std::filesystem::path &path);
    [[nodiscard]] std::string EscapeJson(std::string_view value);
    [[nodiscard]] std::string FormatUtcTimestamp();
    /// Quote a path for the shell, escaping embedded quotes.
    [[nodiscard]] std::string QuotePath(const std::filesystem::path &path);
    /// `N.N.N` with numeric components only.
    [[nodiscard]] bool IsValidVersion(std::string_view version);
    /// C++ compiler recorded in the Editor/SDK CMakeCache, so modules match the host ABI.
    [[nodiscard]] std::string DiscoverCxxCompiler(const ProjectDescriptor &engine,
                                                  const std::filesystem::path &executableDir);
    [[nodiscard]] std::string PublishedModuleLibrary(std::string_view target);
    [[nodiscard]] std::string ReleaseBuildSignature(const std::filesystem::path &projectRoot,
                                                    const std::filesystem::path &sdk,
                                                    std::string_view compiler, Profile profile);
    /// Resets an incompatible/untracked Release build dir, then makes sure it exists.
    [[nodiscard]] bool PrepareReleaseBuild(const std::filesystem::path &buildDir,
                                           const std::string &signature, std::string &error);
    [[nodiscard]] bool WriteReleaseBuildMarker(const std::filesystem::path &buildDir,
                                               const std::string &signature);
} // namespace publish
