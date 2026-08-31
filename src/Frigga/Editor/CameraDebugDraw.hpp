#pragma once

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>
#include <imgui.h>

#include <optional>

namespace FRIGGA_NAMESPACE
{

    class CameraDebugDraw
    {
      public:
        /// Draws frustums for the selected camera (highlighted) and optionally the main camera
        /// (dimmed when not selected). Uses component FOV/near/far and local -Z aim.
        static void Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                         const glm::mat4 &view, const glm::mat4 &vulkanProjection,
                         const ImVec2 &imageMin, const ImVec2 &imageSize,
                         fr::Entity selectedEntity,
                         fr::Entity mainCameraEntity = static_cast<fr::Entity>(-1));

        /// Closest camera eye gizmo under the mouse within `pixelRadius`, or nullopt.
        static std::optional<fr::Entity> HitTest(const skr::Arc<fr::Registry> &registry,
                                                 const glm::mat4 &view,
                                                 const glm::mat4 &vulkanProjection,
                                                 const ImVec2 &imageMin, const ImVec2 &imageSize,
                                                 const ImVec2 &mouse, float pixelRadius = 14.0f);
    };

} // namespace FRIGGA_NAMESPACE
