#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

namespace FRIGGA_NAMESPACE
{

    class InfiniteGridDraw
    {
      public:
        /// Infinite ground grid (Y = 0). Only strokes cells that intersect the view frustum.
        /// `vulkanProjection` is Freya's matrix (Y-flipped for Vulkan); FlipY is applied internally.
        static void Draw(ImDrawList *drawList, const glm::mat4 &view,
                         const glm::mat4 &vulkanProjection, const ImVec2 &imageMin,
                         const ImVec2 &imageSize, float minorStep = 1.0f, float majorStep = 10.0f);
    };

} // namespace FRIGGA_NAMESPACE
