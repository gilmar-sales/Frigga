#pragma once

#include <Freyr/Freyr.hpp>

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    struct MeshComponent: fr::Component
    {
        std::uint32_t meshId      = 0;
        bool          castShadows = true;
    };

} // namespace FRIGGA_NAMESPACE
