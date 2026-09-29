#include "PublishCli.hpp"

#include "../Project/ProjectEnginePaths.hpp"
#include "../Project/ProjectFile.hpp"
#include "../Project/ProjectSession.hpp"
#include "PublishPipeline.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace publish
{
    namespace
    {
        void PrintUsage()
        {
            std::fputs("Usage: Editor --publish <frigga.project|project dir> --out <dir>\n"
                       "              [--profile development|shipping] [--clean]\n",
                       stderr);
        }

        /// Minimal Skirnir container for headless publish: logging sinks plus
        /// the same publish services the Editor UI resolves. No window, no
        /// Freya/SDL — but everything still flows through DI + skr::Logger.
        [[nodiscard]] skr::Arc<skr::ServiceProvider> BuildPublishProvider()
        {
            auto services = skr::MakeArc<skr::ServiceCollection>();
            services->AddSingleton<publish::IProcessRunner, publish::SystemProcessRunner>();
            services->AddSingleton<publish::IAssetService, publish::DefaultAssetService>();
            services->AddTransient<skr::Logger<publish::Pipeline>>();
            services->AddTransient<publish::PipelineFactory>();

            auto provider = services->CreateServiceProvider();

            // Mirror the Editor's LoggingExtension defaults (Main.cpp):
            // console unless FRIGGA_LOG_CONSOLE=0, file, optional JSON.
            const char *console  = std::getenv("FRIGGA_LOG_CONSOLE");
            const char *logFile  = std::getenv("FRIGGA_LOG_FILE");
            const char *jsonFile = std::getenv("FRIGGA_LOG_JSON");
            auto options         = provider->GetService<skr::LoggerOptions>();
            if(console == nullptr || std::string_view(console) != "0")
            {
                options->AddSink(skr::MakeArc<skr::ConsoleSink>());
            }
            options->AddSink(
                skr::MakeArc<skr::FileSink>(logFile != nullptr ? logFile : "frigga-publish.log"));
            if(jsonFile != nullptr && *jsonFile != '\0')
            {
                options->AddSink(skr::MakeArc<skr::JsonSink>(jsonFile));
            }
            return provider;
        }
    } // namespace

    bool IsPublishCommand(int argc, char *argv[])
    {
        for(int i = 1; i < argc; ++i)
        {
            if(std::strcmp(argv[i], "--publish") == 0)
            {
                return true;
            }
        }
        return false;
    }

    int RunPublishCli(int argc, char *argv[])
    {
        std::filesystem::path project;
        std::filesystem::path out;
        Profile profile = Profile::Shipping;
        bool clean      = false;

        for(int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            const auto next            = [&]() -> const char * {
                return i + 1 < argc ? argv[++i] : nullptr;
            };
            if(arg == "--publish" || arg == "--out")
            {
                const char *value = next();
                if(value == nullptr)
                {
                    PrintUsage();
                    return 2;
                }
                (arg == "--publish" ? project : out) = value;
            }
            else if(arg == "--profile")
            {
                const char *value = next();
                const auto parsed = value != nullptr ? ParseProfile(value) : std::nullopt;
                if(!parsed)
                {
                    std::fputs("Unknown profile (expected development|shipping)\n", stderr);
                    return 2;
                }
                profile = *parsed;
            }
            else if(arg == "--clean")
            {
                clean = true;
            }
        }
        if(project.empty() || out.empty())
        {
            PrintUsage();
            return 2;
        }

        if(std::filesystem::is_directory(project))
        {
            project /= ProjectFile::FileName;
        }
        auto loaded = ProjectFile::Load(project);
        if(!loaded)
        {
            std::fprintf(stderr, "Unable to load project file: %s\n", project.string().c_str());
            return 2;
        }

        ProjectDescriptor desc = std::move(*loaded);
        RefreshEnginePaths(desc, ProjectSession::DiscoverFriggaSdk(),
                           ProjectSession::DiscoverFriggaRoot(),
                           ProjectSession::DiscoverFriggaBuild());
        NormalizeModuleLibraryPaths(desc);

        Options options;
        options.projectRoot      = std::filesystem::absolute(project).parent_path();
        options.destination      = out;
        options.profile          = profile;
        options.cleanDestination = clean;
        options.cxxCompiler =
            DiscoverCxxCompiler(desc, ProjectSession::ExecutablePath().parent_path());
        options.descriptor = std::move(desc);

        auto provider = BuildPublishProvider();
        auto logger   = provider->GetService<skr::Logger<Pipeline>>();
        auto factory  = provider->GetService<PipelineFactory>();

        logger->LogInformation("Headless publish {} profile={} to {}", options.projectRoot.string(),
                               ToString(profile), options.destination.string());

        Observer observer;
        observer.onLog = [](std::string_view line) {
            // Stream to stdout for CI while the full record also goes
            // through skr::Logger sinks (console/file/json) via Pipeline::log.
            std::fwrite(line.data(), 1, line.size(), stdout);
        };
        Stage lastStage = Stage::Validate;
        bool first      = true;
        observer.onProgress = [logger, &lastStage, &first](Stage stage, float progress, bool) {
            if(first || stage != lastStage)
            {
                logger->LogInformation("[publish] {} ({:.0f}%)", StageLabel(stage),
                                       static_cast<double>(progress * 100.0f));
                lastStage = stage;
                first     = false;
            }
        };

        Pipeline pipeline  = factory->Create(std::move(options), std::move(observer));
        const auto report = pipeline.Run();
        if(!report.ok)
        {
            const std::string stage =
                report.failedStage ? std::string(ToString(*report.failedStage)) : "?";
            logger->LogError("[publish] FAILED at stage '{}': {}", stage, report.error);
            return report.exitCode != 0 ? report.exitCode : 1;
        }
        logger->LogInformation("[publish] OK -> {} ({} files)", report.destination.string(),
                               report.files.size());
        return 0;
    }
} // namespace publish
