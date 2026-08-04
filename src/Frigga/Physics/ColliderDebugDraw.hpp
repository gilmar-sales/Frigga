#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <imgui.h>

namespace FRIGGA_NAMESPACE
{

    class ColliderDebugDraw
    {
      public:
        static void Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                         const skr::Arc<PrimitiveMeshFactory> &primitives, const glm::mat4 &view,
                         const glm::mat4 &vulkanProjection, const ImVec2 &imageMin,
                         const ImVec2 &imageSize);
    };

} // namespace FRIGGA_NAMESPACE
