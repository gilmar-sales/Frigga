#pragma once

#include "Frigga/Macro.hpp"

#include <cstdint>

namespace FRIGGA_NAMESPACE
{

    struct PhysicsBodyHandle
    {
        static constexpr std::uint32_t InvalidId = 0xffffffffu;

        std::uint32_t id = InvalidId;

        [[nodiscard]] bool IsValid() const
        {
            return id != InvalidId;
        }

        void Reset()
        {
            id = InvalidId;
        }
    };

} // namespace FRIGGA_NAMESPACE
