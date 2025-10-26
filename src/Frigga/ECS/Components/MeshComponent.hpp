#pragma once

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE
{

    struct MeshComponent: fr::Component
    {
        uint32_t vertex_array_object;
        uint32_t vertex_buffer_object;
        uint32_t element_buffer_object;
        uint32_t index_count;
    };

} // namespace FRIGGA_NAMESPACE
