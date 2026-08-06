#pragma once

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <imgui.h>

#include <optional>

namespace FRIGGA_NAMESPACE
{

    class LightDebugDraw
    {
      public:
        static void Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                         const glm::mat4 &view, const glm::mat4 &vulkanProjection,
                         const ImVec2 &imageMin, const ImVec2 &imageSize,
                         fr::Entity selectedEntity = static_cast<fr::Entity>(-1));

        /// Closest light gizmo under the mouse within `pixelRadius`, or nullopt.
        static std::optional<fr::Entity> HitTest(const skr::Arc<fr::Registry> &registry,
                                                 const glm::mat4 &view,
                                                 const glm::mat4 &vulkanProjection,
                                                 const ImVec2 &imageMin, const ImVec2 &imageSize,
                                                 const ImVec2 &mouse, float pixelRadius = 14.0f);
    };

} // namespace FRIGGA_NAMESPACE
