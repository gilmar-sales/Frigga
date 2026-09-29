#include "Editor/Project/ProjectFile.hpp"
#include "Editor/Publish/AssetService.hpp"
#include "Editor/Publish/ProcessRunner.hpp"
#include "Editor/Publish/PublishPipeline.hpp"

#include <Skirnir/Skirnir.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    void WriteFile(const fs::path &path, std::string_view text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    /// Records commands and simulates cmake: `--install` materialises a tiny game tree.
    class FakeRunner final : public publish::IProcessRunner
    {
      public:
        int buildExit = 0;
        std::vector<std::string> commands;

        int Run(const std::string &command,
                const std::function<void(std::string_view)> &onLine) override
        {
            commands.push_back(command);
            if(command.starts_with("cmake --build"))
            {
                onLine("[1/2] Compiling\n");
                onLine("[2/2] Linking\n");
                return buildExit;
            }
            if(command.starts_with("cmake --install"))
            {
                const auto begin = command.find("--prefix \"") + 10;
                const auto end   = command.find('"', begin);
                const fs::path prefix = command.substr(begin, end - begin);
                WriteFile(prefix / "Game.exe", "binary");
                WriteFile(prefix / "Resources/Shaders/engine.spv", "spv");
            }
            return 0;
        }
    };

    /// Fake asset service: real validation would need a manifest on disk.
    class FakeAssets final : public publish::IAssetService
    {
      public:
        publish::AssetValidation ValidateManifest(const fs::path &) override
        {
            return {.ok = true};
        }

        publish::AssetCookOutcome Cook(const fs::path &projectResources,
                                       const fs::path &destination) override
        {
            // Mirror DefaultAssetService behavior for the fixture tree:
            // copy everything under Resources/Textures into the destination.
            std::error_code ec;
            std::size_t count = 0;
            const auto textures = projectResources / "Textures";
            if(fs::exists(textures, ec))
            {
                for(const auto &entry: fs::recursive_directory_iterator(textures, ec))
                {
                    if(!entry.is_regular_file(ec))
                    {
                        continue;
                    }
                    const auto rel = fs::relative(entry.path(), projectResources, ec);
                    const auto dst = destination / rel;
                    fs::create_directories(dst.parent_path(), ec);
                    fs::copy_file(entry.path(), dst,
                                  fs::copy_options::overwrite_existing, ec);
                    if(!ec)
                    {
                        ++count;
                    }
                }
            }
            // Engine resources installed by FakeRunner must survive cooking.
            return {.ok = true, .cookedCount = count};
        }
    };

    [[nodiscard]] skr::Arc<skr::Logger<publish::Pipeline>> NullPipelineLogger()
    {
        auto options = skr::MakeArc<skr::LoggerOptions>();
        options->AddSink(skr::MakeArc<skr::NullSink>());
        return skr::MakeArc<skr::Logger<publish::Pipeline>>(options);
    }

    class PublishPipelineTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            root = fs::temp_directory_path() /
                   ("frigga-publish-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            project = root / "project";
            sdk     = root / "sdk";
            out     = root / "out" / "game";
            WriteFile(sdk / "cmake/FriggaSdk.cmake", "# sdk");
            WriteFile(project / "CMakeLists.txt", "project(Game)");
            WriteFile(project / "Resources/Textures/hero.png", "png");

            descriptor.name                    = "Game";
            descriptor.branding.version        = "1.2.3";
            descriptor.friggaSdk               = sdk;
            descriptor.friggaBuild             = sdk;
            descriptor.EnsureBranding();
            ASSERT_TRUE(ProjectFile::Save(project / ProjectFile::FileName, descriptor));

            runner = skr::MakeArc<FakeRunner>();
            assets = skr::MakeArc<FakeAssets>();
            logger = NullPipelineLogger();
        }

        void TearDown() override
        {
            std::error_code ec;
            fs::remove_all(root, ec);
        }

        publish::Options Options() const
        {
            publish::Options options;
            options.projectRoot = project;
            options.destination = out;
            options.descriptor  = descriptor;
            return options;
        }

        publish::Pipeline MakePipeline(publish::Options options,
                                       publish::Observer observer = {}) const
        {
            return publish::Pipeline(std::move(options), runner, assets, logger,
                                     std::move(observer));
        }

        fs::path root, project, sdk, out;
        ProjectDescriptor descriptor;
        skr::Arc<FakeRunner> runner;
        skr::Arc<FakeAssets> assets;
        skr::Arc<skr::Logger<publish::Pipeline>> logger;
    };
} // namespace

TEST(PublishHelpers, Fnv1aIsStable)
{
    EXPECT_EQ(publish::Fnv1a64Hex(""), "cbf29ce484222325");
    EXPECT_EQ(publish::Fnv1a64Hex("a"), "af63dc4c8601ec8c");
}

TEST(PublishHelpers, VersionMustBeThreeNumericParts)
{
    EXPECT_TRUE(publish::IsValidVersion("1.0.0"));
    EXPECT_TRUE(publish::IsValidVersion("10.20.300"));
    EXPECT_FALSE(publish::IsValidVersion("1.0"));
    EXPECT_FALSE(publish::IsValidVersion("1.0.0.0"));
    EXPECT_FALSE(publish::IsValidVersion("1.0.x"));
    EXPECT_FALSE(publish::IsValidVersion("1..0"));
    EXPECT_FALSE(publish::IsValidVersion(""));
}

TEST(PublishHelpers, ParsesProfilesAndNinjaProgress)
{
    EXPECT_EQ(publish::ParseProfile("Shipping"), publish::Profile::Shipping);
    EXPECT_EQ(publish::ParseProfile("dev"), publish::Profile::Development);
    EXPECT_FALSE(publish::ParseProfile("debug").has_value());

    float fraction = 0.0f;
    EXPECT_TRUE(publish::TryParseNinjaProgress("[3/12] Building foo.o", fraction));
    EXPECT_FLOAT_EQ(fraction, 0.25f);
    EXPECT_FALSE(publish::TryParseNinjaProgress("no progress here", fraction));
    EXPECT_FALSE(publish::TryParseNinjaProgress("[1/0] bad", fraction));
}

TEST(PublishHelpers, ShellQuotingEscapesQuotes)
{
    const auto quoted = publish::QuotePath(std::filesystem::path("a\"b/c"));
    EXPECT_EQ(quoted.front(), '"');
    EXPECT_EQ(quoted.back(), '"');
    EXPECT_NE(quoted.find("\\\""), std::string::npos);
}

TEST_F(PublishPipelineTest, PublishesCompletePackageAndManifest)
{
    std::vector<publish::Stage> stages;
    publish::Observer observer;
    observer.onProgress = [&](publish::Stage stage, float, bool) {
        if(stages.empty() || stages.back() != stage)
        {
            stages.push_back(stage);
        }
    };

    publish::Pipeline pipeline = MakePipeline(Options(), observer);
    const auto report          = pipeline.Run();

    ASSERT_TRUE(report.ok) << report.error;
    EXPECT_EQ(report.exitCode, 0);
    EXPECT_TRUE(fs::exists(out / "Game.exe"));
    EXPECT_TRUE(fs::exists(out / "Resources/Shaders/engine.spv"));
    EXPECT_TRUE(fs::exists(out / "Resources/Textures/hero.png"));
    EXPECT_TRUE(fs::exists(out / ProjectFile::FileName));
    EXPECT_TRUE(fs::exists(out / publish::Pipeline::ReleaseManifestName));
    EXPECT_FALSE(fs::exists(out.parent_path() / ".game.frigga-staging"));

    ASSERT_EQ(runner->commands.size(), 3u);
    EXPECT_TRUE(runner->commands[0].starts_with("cmake -S"));
    EXPECT_NE(runner->commands[0].find("-DCMAKE_BUILD_TYPE=Release"), std::string::npos);
    EXPECT_TRUE(runner->commands[1].starts_with("cmake --build"));
    EXPECT_TRUE(runner->commands[2].starts_with("cmake --install"));

    const std::vector<publish::Stage> expected = {
        publish::Stage::Validate, publish::Stage::Configure, publish::Stage::Build,
        publish::Stage::Install,  publish::Stage::Cook,      publish::Stage::Stage,
        publish::Stage::Finalize,
    };
    EXPECT_EQ(stages, expected);
    EXPECT_EQ(report.timings.size(), expected.size());

    const auto manifest = publish::ReadTextFile(out / publish::Pipeline::ReleaseManifestName);
    EXPECT_NE(manifest.find("\"version\": \"1.2.3\""), std::string::npos);
    EXPECT_NE(manifest.find("\"path\": \"Game.exe\""), std::string::npos);
    EXPECT_NE(manifest.find("Resources/Textures/hero.png"), std::string::npos);
    // Manifest never lists itself.
    EXPECT_EQ(manifest.find("\"path\": \"frigga-release.json\""), std::string::npos);
}

TEST_F(PublishPipelineTest, PublishedProjectDropsHostEnginePaths)
{
    publish::Pipeline pipeline = MakePipeline(Options());
    ASSERT_TRUE(pipeline.Run().ok);

    const auto loaded = ProjectFile::Load(out / ProjectFile::FileName);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->friggaSdk.empty());
    EXPECT_TRUE(loaded->friggaBuild.empty());
}

TEST_F(PublishPipelineTest, RejectsInvalidVersionBeforeRunningAnything)
{
    descriptor.branding.version = "1.0";
    publish::Pipeline pipeline  = MakePipeline(Options());
    const auto report           = pipeline.Run();

    EXPECT_FALSE(report.ok);
    EXPECT_EQ(report.failedStage, publish::Stage::Validate);
    EXPECT_TRUE(runner->commands.empty());
    EXPECT_FALSE(fs::exists(out));
}

TEST_F(PublishPipelineTest, RejectsMissingIconAndMissingSdk)
{
    {
        descriptor.branding.iconWindows = "branding/missing.ico";
        publish::Pipeline pipeline      = MakePipeline(Options());
        const auto report               = pipeline.Run();
        EXPECT_FALSE(report.ok);
        EXPECT_NE(report.error.find("Icon file not found"), std::string::npos);
    }
    {
        descriptor.branding.iconWindows.clear();
        descriptor.friggaSdk = root / "nowhere";
        publish::Pipeline pipeline = MakePipeline(Options());
        const auto report          = pipeline.Run();
        EXPECT_FALSE(report.ok);
        EXPECT_NE(report.error.find("SDK not found"), std::string::npos);
    }
}

TEST_F(PublishPipelineTest, NonEmptyDestinationRequiresClean)
{
    WriteFile(out / "old.txt", "old");
    {
        publish::Pipeline pipeline = MakePipeline(Options());
        const auto report          = pipeline.Run();
        EXPECT_FALSE(report.ok);
        EXPECT_EQ(report.failedStage, publish::Stage::Validate);
        EXPECT_TRUE(fs::exists(out / "old.txt"));
    }
    {
        auto options             = Options();
        options.cleanDestination = true;
        publish::Pipeline pipeline = MakePipeline(std::move(options));
        ASSERT_TRUE(pipeline.Run().ok);
        EXPECT_FALSE(fs::exists(out / "old.txt"));
        EXPECT_TRUE(fs::exists(out / "Game.exe"));
        EXPECT_FALSE(fs::exists(out.parent_path() / ".game.frigga-old"));
    }
}

TEST_F(PublishPipelineTest, FailedBuildLeavesDestinationUntouched)
{
    WriteFile(out / "old.txt", "old");
    auto options             = Options();
    options.cleanDestination = true;
    runner->buildExit        = 7;
    publish::Pipeline pipeline = MakePipeline(std::move(options));
    const auto report          = pipeline.Run();

    EXPECT_FALSE(report.ok);
    EXPECT_EQ(report.exitCode, 7);
    EXPECT_EQ(report.failedStage, publish::Stage::Build);
    EXPECT_TRUE(fs::exists(out / "old.txt"));
    EXPECT_FALSE(fs::exists(out / "Game.exe"));
    EXPECT_FALSE(fs::exists(out.parent_path() / ".game.frigga-staging"));
}

TEST_F(PublishPipelineTest, RefusesDestinationContainingProject)
{
    auto options        = Options();
    options.destination = root;
    publish::Pipeline pipeline = MakePipeline(std::move(options));
    const auto report          = pipeline.Run();
    EXPECT_FALSE(report.ok);
    EXPECT_TRUE(runner->commands.empty());
}

TEST_F(PublishPipelineTest, RefusesDestinationInsideProject)
{
    auto options        = Options();
    options.destination = project / "out-game";
    publish::Pipeline pipeline = MakePipeline(std::move(options));
    const auto report          = pipeline.Run();
    EXPECT_FALSE(report.ok);
    EXPECT_EQ(report.failedStage, publish::Stage::Validate);
    EXPECT_TRUE(runner->commands.empty());
}

TEST(PublishHelpers, DiscoversCompilerFromLaterCacheWhenFirstLacksKey)
{
    const auto tmp = fs::temp_directory_path() / "frigga-cxx-probe";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    const auto first  = tmp / "first";
    const auto second = tmp / "second";
    WriteFile(first / "CMakeCache.txt", "# no compiler here\n");
    WriteFile(second / "CMakeCache.txt", "CMAKE_CXX_COMPILER:FILEPATH=/usr/bin/c++\n");

    ProjectDescriptor engine;
    engine.friggaBuild = first;
    engine.friggaSdk   = second;
    EXPECT_EQ(publish::DiscoverCxxCompiler(engine, {}), "/usr/bin/c++");

    fs::remove_all(tmp, ec);
}
