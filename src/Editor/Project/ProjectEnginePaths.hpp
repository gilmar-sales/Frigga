#pragma once

#include "ProjectDescriptor.hpp"

#include <filesystem>
#include <string>
#include <string_view>

inline bool LooksLikeFriggaSdk(const std::filesystem::path &path)
{
    return !path.empty() &&
           std::filesystem::exists(path / "include/Frigga/Module/frigga_module.h") &&
           std::filesystem::exists(path / "cmake/FriggaSdk.cmake") &&
           std::filesystem::exists(path / "include/Freyr");
}

inline bool IsUsableFriggaSdk(const std::filesystem::path &path)
{
    return LooksLikeFriggaSdk(path);
}

inline bool LooksLikeFriggaEngineRoot(const std::filesystem::path &path)
{
    if(LooksLikeFriggaSdk(path))
    {
        return true;
    }
    return !path.empty() && std::filesystem::exists(path / "include/Frigga/Frigga.hpp") &&
           std::filesystem::exists(path / "CMakeLists.txt");
}

inline bool IsUsableFriggaRoot(const std::filesystem::path &path)
{
    return LooksLikeFriggaEngineRoot(path);
}

inline bool IsUsableEnginePath(const std::filesystem::path &path)
{
    return !path.empty() && std::filesystem::exists(path);
}

/// Packaged Sdk, else a usable source/SDK hint. Ignores stale cross-machine paths.
inline std::filesystem::path EffectiveFriggaSdk(const ProjectDescriptor &desc)
{
    if(IsUsableFriggaSdk(desc.friggaSdk))
    {
        return desc.friggaSdk;
    }
    if(IsUsableFriggaSdk(desc.friggaRoot))
    {
        return desc.friggaRoot;
    }
    if(IsUsableFriggaSdk(desc.friggaBuild))
    {
        return desc.friggaBuild;
    }
    return {};
}

inline std::filesystem::path EffectiveFriggaRoot(const ProjectDescriptor &desc)
{
    if(IsUsableFriggaRoot(desc.friggaRoot))
    {
        return desc.friggaRoot;
    }
    return EffectiveFriggaSdk(desc);
}

inline std::filesystem::path EffectiveFriggaBuild(const ProjectDescriptor &desc)
{
    if(IsUsableEnginePath(desc.friggaBuild))
    {
        return desc.friggaBuild;
    }
    const auto sdk = EffectiveFriggaSdk(desc);
    if(IsUsableFriggaSdk(sdk))
    {
        return sdk;
    }
    return EffectiveFriggaRoot(desc);
}

inline void FillMissingEnginePaths(ProjectDescriptor &desc)
{
    if(!IsUsableFriggaSdk(desc.friggaSdk))
    {
        desc.friggaSdk = EffectiveFriggaSdk(desc);
    }
    if(!IsUsableFriggaRoot(desc.friggaRoot))
    {
        desc.friggaRoot = desc.friggaSdk;
    }
    if(!IsUsableEnginePath(desc.friggaBuild))
    {
        desc.friggaBuild = IsUsableFriggaSdk(desc.friggaSdk) ? desc.friggaSdk : desc.friggaRoot;
    }
}

/// Replace empty/invalid host hints with paths discovered from the running Editor.
inline void RefreshEnginePaths(ProjectDescriptor &desc, const std::filesystem::path &discoveredSdk,
                               const std::filesystem::path &discoveredRoot,
                               const std::filesystem::path &discoveredBuild)
{
    if(!IsUsableFriggaSdk(desc.friggaSdk))
    {
        if(IsUsableFriggaSdk(discoveredSdk))
        {
            desc.friggaSdk = discoveredSdk;
        }
        else if(IsUsableFriggaSdk(discoveredRoot))
        {
            desc.friggaSdk = discoveredRoot;
        }
        else if(IsUsableFriggaSdk(discoveredBuild))
        {
            desc.friggaSdk = discoveredBuild;
        }
        else
        {
            desc.friggaSdk = discoveredSdk;
        }
    }
    if(!IsUsableFriggaRoot(desc.friggaRoot))
    {
        desc.friggaRoot = !discoveredRoot.empty() ? discoveredRoot : desc.friggaSdk;
    }
    if(!IsUsableEnginePath(desc.friggaBuild))
    {
        desc.friggaBuild = !discoveredBuild.empty() ? discoveredBuild : desc.friggaSdk;
    }
    FillMissingEnginePaths(desc);
}

inline std::string PlatformLibraryFileName(std::string_view target)
{
#ifdef _WIN32
    return std::string(target) + ".dll";
#elif defined(__APPLE__)
    return "lib" + std::string(target) + ".dylib";
#else
    return "lib" + std::string(target) + ".so";
#endif
}

/// Rewrite module library filenames for the current OS while keeping build/ vs Modules/.
inline void NormalizeModuleLibraryPaths(ProjectDescriptor &desc)
{
    for(auto &entry : desc.modules)
    {
        const auto target = entry.target.empty() ? entry.id : entry.target;
        if(target.empty())
        {
            continue;
        }
        std::filesystem::path lib = entry.libraryRelative.empty()
                                        ? std::filesystem::path(
                                              ProjectDescriptor::DefaultLibraryRelative(target))
                                        : std::filesystem::path(entry.libraryRelative);
        const auto parent = lib.parent_path().generic_string();
        const auto file   = PlatformLibraryFileName(target);
        entry.libraryRelative = parent.empty() ? file : (parent + "/" + file);
    }
    desc.SyncGameplayMirror();
    if(desc.moduleLibraryRelative.empty() && !desc.moduleTarget.empty())
    {
        desc.moduleLibraryRelative = ProjectDescriptor::DefaultLibraryRelative(desc.moduleTarget);
    }
}
