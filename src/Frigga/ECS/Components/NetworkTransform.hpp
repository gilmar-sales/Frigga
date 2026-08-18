#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    /// Marks a transform that the host snapshots each Simulation tick.
    struct NetworkTransform: fr::Component
    {
        bool interpolate = true;
    };

} // namespace FRIGGA_NAMESPACE
