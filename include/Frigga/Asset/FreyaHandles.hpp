#pragma once

#include <Freya/Freya.hpp>

#include <cstdint>
#include <optional>

namespace FRIGGA_NAMESPACE
{
    /// Freya 0.46 pool ids may be 0; validity lives on the handle flag.
    /// Frigga still stores raw ids from Freya pools — reconstruct engaged handles
    /// when calling Freya, and treat disengaged / missing optionals as null.

    [[nodiscard]] inline fra::MeshHandle AsMeshHandle(std::uint32_t id)
    {
        return fra::MeshHandle {id};
    }

    [[nodiscard]] inline fra::MaterialHandle AsMaterialHandle(std::uint32_t id)
    {
        return fra::MaterialHandle {id};
    }

    [[nodiscard]] inline fra::TextureHandle AsTextureHandle(std::uint32_t id)
    {
        return fra::TextureHandle {id};
    }

    [[nodiscard]] inline std::uint32_t FromHandle(fra::MeshHandle handle)
    {
        return handle.IsValid() ? handle.Id() : 0u;
    }

    [[nodiscard]] inline std::uint32_t FromHandle(fra::MaterialHandle handle)
    {
        return handle.IsValid() ? handle.Id() : 0u;
    }

    [[nodiscard]] inline std::uint32_t FromHandle(fra::TextureHandle handle)
    {
        return handle.IsValid() ? handle.Id() : 0u;
    }

    [[nodiscard]] inline std::optional<fra::TextureHandle> AsTextureSlot(
        std::optional<std::uint32_t> id)
    {
        if(!id)
        {
            return std::nullopt;
        }
        return fra::TextureHandle {*id};
    }

    [[nodiscard]] inline std::optional<std::uint32_t> FromTextureSlot(
        std::optional<fra::TextureHandle> handle)
    {
        if(!handle || !handle->IsValid())
        {
            return std::nullopt;
        }
        return handle->Id();
    }
} // namespace FRIGGA_NAMESPACE
