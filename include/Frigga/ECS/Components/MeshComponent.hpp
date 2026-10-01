#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    struct MeshComponent
    {
        std::uint32_t meshId = 0;
        bool castShadows     = true;
    };

} // namespace FRIGGA_NAMESPACE
