#pragma once

#include "Frigga/Asset/PrimitiveMeshFactory.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"
#include "Frigga/Physics/IPhysicsWorld.hpp"

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
                         const ImVec2 &imageSize,
                         fr::Entity selectedEntity = static_cast<fr::Entity>(-1),
                         const skr::Arc<IPhysicsWorld> &physicsWorld = {},
                         bool dimInactiveBodies = false,
                         const skr::Arc<UserComponentRegistry> &userComponents = {});
    };

} // namespace FRIGGA_NAMESPACE
