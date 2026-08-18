#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    struct NetworkIdentity: fr::Component
    {
        std::uint32_t netId     = 0;
        std::uint32_t ownerPeer = 0;
    };

} // namespace FRIGGA_NAMESPACE
