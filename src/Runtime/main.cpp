#include "RuntimeApplication.hpp"

#include <Frigga/Frigga.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace
{
    void PrintUsage(const char *executable)
    {
        std::cout << "Usage: " << executable
                  << " [--project <frigga.project>] [--scene <scene.json>]\n";
    }

    bool ParseArguments(int argc, char **argv, std::filesystem::path &projectFile,
                        std::optional<std::filesystem::path> &scene)
    {
        for(int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];
            if(argument == "--help" || argument == "-h")
            {
                PrintUsage(argv[0]);
                return false;
            }
            if(argument == "--project" || argument == "--scene")
            {
                if(i + 1 >= argc)
                {
                    std::cerr << argument << " requires a path\n";
                    return false;
                }
                const auto value = std::filesystem::path(argv[++i]);
                if(argument == "--project")
                {
                    projectFile = value;
                }
                else
                {
                    scene = value;
                }
                continue;
            }
            std::cerr << "Unknown option: " << argument << '\n';
            PrintUsage(argv[0]);
            return false;
        }
        return true;
    }
} // namespace

int main(int argc, char **argv)
{
    std::filesystem::path projectFile;
    std::optional<std::filesystem::path> sceneOverride;
    if(!ParseArguments(argc, argv, projectFile, sceneOverride))
    {
        return 0;
    }

    if(projectFile.empty())
    {
        projectFile = std::filesystem::absolute("frigga.project");
    }
    else
    {
        projectFile = std::filesystem::absolute(projectFile);
        if(std::filesystem::is_directory(projectFile))
        {
            projectFile /= "frigga.project";
        }
    }

    RuntimeProject project;
    std::string error;
    if(!RuntimeProject::Load(projectFile, project, error))
    {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }
    if(sceneOverride)
    {
        project.scene = *sceneOverride;
    }
    if(!std::filesystem::exists(project.ScenePath()))
    {
        std::cerr << "Startup scene does not exist: " << project.ScenePath() << '\n';
        return EXIT_FAILURE;
    }

    std::error_code ec;
    std::filesystem::current_path(project.root, ec);
    if(ec)
    {
        std::cerr << "Unable to use project directory as working directory: "
                  << ec.message() << '\n';
        return EXIT_FAILURE;
    }

    auto appBuilder =
        skr::ApplicationBuilder()
            .WithExtension<skr::LoggingExtension>([](skr::LoggingExtension &logging) {
                logging.AddConsoleSink().AddFileSink("frigga-runtime.log");
            })
            .WithExtension<fg::FriggaExtension>();

    appBuilder.GetServiceCollection()->AddSingleton<RuntimeProject>(
        [project](skr::ServiceProvider &) {
            return skr::MakeArc<RuntimeProject>(project);
        });

    auto app = appBuilder.Build<RuntimeApplication>();
    app->Run();
    return EXIT_SUCCESS;
}
