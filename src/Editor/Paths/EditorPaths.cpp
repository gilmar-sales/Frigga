#include "EditorPaths.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdlib>
#include <mutex>

namespace
{
    /**
     * Isolated SDL backend. Swap or mock this without touching call sites.
     */
    class SdlPathProvider
    {
      public:
        [[nodiscard]] static std::filesystem::path PreferredDir(const char *org, const char *app)
        {
            if(char *raw = SDL_GetPrefPath(org, app))
            {
                std::filesystem::path path(raw);
                SDL_free(raw);
                return path;
            }
            return {};
        }

        [[nodiscard]] static std::filesystem::path UserHomeDir()
        {
            if(const char *raw = SDL_GetUserFolder(SDL_FOLDER_HOME))
            {
                return std::filesystem::path(raw);
            }
            return {};
        }
    };

    [[nodiscard]] std::filesystem::path FallbackPreferredDir(const char *org, const char *app)
    {
#if defined(_WIN32)
        const char *base = std::getenv("APPDATA");
        if(base && *base)
        {
            return std::filesystem::path(base) / org / app;
        }
#elif defined(__APPLE__)
        const char *home = std::getenv("HOME");
        if(home && *home)
        {
            return std::filesystem::path(home) / "Library" / "Application Support" / org / app;
        }
#else
        if(const char *xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        {
            return std::filesystem::path(xdg) / org / app;
        }
        if(const char *home = std::getenv("HOME"); home && *home)
        {
            return std::filesystem::path(home) / ".local" / "share" / org / app;
        }
#endif
        return std::filesystem::current_path() / ".frigga-editor";
    }

    [[nodiscard]] std::filesystem::path FallbackHomeDir()
    {
        if(const char *home = std::getenv("HOME"); home && *home)
        {
            return std::filesystem::path(home);
        }
#if defined(_WIN32)
        if(const char *profile = std::getenv("USERPROFILE"); profile && *profile)
        {
            return std::filesystem::path(profile);
        }
#endif
        return std::filesystem::current_path();
    }

    std::once_flag g_dirsOnce;
} // namespace

std::filesystem::path EditorPaths::PreferredDir()
{
    auto path = SdlPathProvider::PreferredDir(OrgName, AppName);
    if(path.empty())
    {
        path = FallbackPreferredDir(OrgName, AppName);
    }
    return path.lexically_normal();
}

std::filesystem::path EditorPaths::UserHomeDir()
{
    auto path = SdlPathProvider::UserHomeDir();
    if(path.empty())
    {
        path = FallbackHomeDir();
    }
    return path.lexically_normal();
}

std::filesystem::path EditorPaths::FriggaHomeDir()
{
    return UserHomeDir() / FriggaHomeFolder;
}

std::filesystem::path EditorPaths::DefaultProjectsDir()
{
    return FriggaHomeDir() / ProjectsFolderName;
}

std::filesystem::path EditorPaths::LegacyProjectsDir()
{
    return UserHomeDir() / LegacyProjectsFolderName;
}

std::filesystem::path EditorPaths::DefaultModulesDir()
{
    return FriggaHomeDir() / ModulesFolderName;
}

std::filesystem::path EditorPaths::PreferencesFile()
{
    return PreferredDir() / PreferencesFileName;
}

void EditorPaths::EnsureDirectories()
{
    std::call_once(g_dirsOnce, [] {
        // Pref/user folder APIs are available without video; a zero-subsystem init
        // keeps them reliable when called before Freya brings up SDL.
        (void)SDL_Init(0);
        std::error_code ec;
        std::filesystem::create_directories(PreferredDir(), ec);
        std::filesystem::create_directories(DefaultProjectsDir(), ec);
        std::filesystem::create_directories(DefaultModulesDir(), ec);
    });
}
