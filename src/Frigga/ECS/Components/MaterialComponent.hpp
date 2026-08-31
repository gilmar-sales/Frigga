#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    struct MaterialComponent: fr::Component
    {
        std::uint32_t materialId = 0;
    };

} // namespace FRIGGA_NAMESPACE
