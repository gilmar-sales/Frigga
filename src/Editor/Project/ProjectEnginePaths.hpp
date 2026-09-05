#pragma once

#include "ProjectDescriptor.hpp"

#include <filesystem>

inline bool LooksLikeFriggaSdk(const std::filesystem::path &path)
{
    return std::filesystem::exists(path / "include/Frigga/Module/frigga_module.h") &&
           std::filesystem::exists(path / "cmake/FriggaSdk.cmake") &&
           std::filesystem::exists(path / "include/Freyr");
}

inline bool LooksLikeFriggaEngineRoot(const std::filesystem::path &path)
{
    if(LooksLikeFriggaSdk(path))
    {
        return true;
    }
    return std::filesystem::exists(path / "include/Frigga/Frigga.hpp") &&
           std::filesystem::exists(path / "CMakeLists.txt");
}

/// Packaged Sdk, else last-known source tree (`friggaRoot`).
inline std::filesystem::path EffectiveFriggaSdk(const ProjectDescriptor &desc)
{
    if(!desc.friggaSdk.empty())
    {
        return desc.friggaSdk;
    }
    if(LooksLikeFriggaSdk(desc.friggaRoot))
    {
        return desc.friggaRoot;
    }
    if(LooksLikeFriggaSdk(desc.friggaBuild))
    {
        return desc.friggaBuild;
    }
    return desc.friggaRoot;
}

inline std::filesystem::path EffectiveFriggaRoot(const ProjectDescriptor &desc)
{
    return desc.friggaRoot.empty() ? EffectiveFriggaSdk(desc) : desc.friggaRoot;
}

inline std::filesystem::path EffectiveFriggaBuild(const ProjectDescriptor &desc)
{
    if(!desc.friggaBuild.empty())
    {
        return desc.friggaBuild;
    }
    const auto sdk = EffectiveFriggaSdk(desc);
    if(LooksLikeFriggaSdk(sdk))
    {
        return sdk;
    }
    return sdk;
}

inline void FillMissingEnginePaths(ProjectDescriptor &desc)
{
    if(desc.friggaSdk.empty())
    {
        desc.friggaSdk = EffectiveFriggaSdk(desc);
    }
    if(desc.friggaRoot.empty())
    {
        desc.friggaRoot = desc.friggaSdk;
    }
    if(desc.friggaBuild.empty())
    {
        desc.friggaBuild = LooksLikeFriggaSdk(desc.friggaSdk) ? desc.friggaSdk : desc.friggaRoot;
    }
}
