#pragma once

#include <Frigga/Macro.hpp>

#include <cstdint>

// Project-file versions live apart from the rest of FormatVersions.hpp so that
// bumping a scene/asset format does not rebuild everything that includes
// ProjectDescriptor.hpp.
namespace FRIGGA_NAMESPACE::FormatVersion
{
    inline constexpr std::uint32_t LegacyProject = 1;
    inline constexpr std::uint32_t Project = 5;
} // namespace FRIGGA_NAMESPACE::FormatVersion
