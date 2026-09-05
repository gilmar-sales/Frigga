#pragma once

#include <Frigga/Macro.hpp>

#include <cstdint>

namespace FRIGGA_NAMESPACE::FormatVersion
{
    inline constexpr std::uint32_t LegacyProject = 1;
    inline constexpr std::uint32_t Project = 5;
    inline constexpr std::uint32_t Scene = 5;
    inline constexpr std::uint32_t Prefab = Scene;
    inline constexpr std::uint32_t Material = 1;
    inline constexpr std::uint32_t AssetManifest = 2;
    inline constexpr std::uint32_t RuntimeProject = 1;
    inline constexpr std::uint32_t SdkAbi = 1;
} // namespace FRIGGA_NAMESPACE::FormatVersion
