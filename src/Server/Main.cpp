#include "ServerApplication.hpp"

#include <Frigga/Frigga.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
    void PrintUsage()
    {
        std::cerr << "Usage: FriggaServer --project <frigga.project|dir> [--port 7777] "
                     "[--scene scenes/main.json]\n";
    }

    bool ParseArgs(int argc, char **argv, ServerOptions &options)
    {
        for(int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            auto take = [&](std::string &out) -> bool {
                if(i + 1 >= argc)
                {
                    return false;
                }
                out = argv[++i];
                return true;
            };
            if(arg == "--help" || arg == "-h")
            {
                PrintUsage();
                return false;
            }
            if(arg == "--project" || arg == "-p")
            {
                std::string value;
                if(!take(value))
                {
                    PrintUsage();
                    return false;
                }
                options.projectFile = value;
                continue;
            }
            if(arg == "--port")
            {
                std::string value;
                if(!take(value))
                {
                    PrintUsage();
                    return false;
                }
                options.port = static_cast<std::uint16_t>(std::strtoul(value.c_str(), nullptr, 10));
                if(options.port == 0)
                {
                    options.port = 7777;
                }
                continue;
            }
            if(arg == "--scene")
            {
                if(!take(options.sceneRelative))
                {
                    PrintUsage();
                    return false;
                }
                continue;
            }
            if(!arg.empty() && arg[0] != '-')
            {
                options.projectFile = arg;
                continue;
            }
            PrintUsage();
            return false;
        }
        if(options.projectFile.empty())
        {
            PrintUsage();
            return false;
        }
        return true;
    }
} // namespace

int main(int argc, char **argv)
{
    ServerOptions parsed;
    if(!ParseArgs(argc, argv, parsed))
    {
        return 2;
    }

    auto appBuilder =
        skr::ApplicationBuilder()
            .WithExtension<skr::LoggingExtension>([](skr::LoggingExtension &logging) {
                logging.AddConsoleSink().AddFileSink("frigga-server.log");
            })
            .WithExtension<fg::FriggaExtension>(
                [](fg::FriggaExtension &frigga) { frigga.SetHeadless(true); });

    appBuilder.GetServiceCollection()->AddSingleton<ServerOptions>(
        [parsed](skr::ServiceProvider &) { return skr::MakeArc<ServerOptions>(parsed); });

    auto app = appBuilder.Build<ServerApplication>();
    app->Run();
    return 0;
}
