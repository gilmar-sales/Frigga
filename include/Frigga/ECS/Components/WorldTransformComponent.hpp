#pragma once

#include <Frigga/Macro.hpp>

#include <Freyr/Freyr.hpp>
#include <glm/glm.hpp>

namespace FRIGGA_NAMESPACE
{

    /// World matrix derived from TransformComponent through the Freyr hierarchy (PostUpdate).
    /// Read-only for gameplay code: write TransformComponent (or TransformUtil::SetWorld*).
    struct WorldTransformComponent: fr::Component
    {
        glm::mat4 matrix {1.0f};
    };

} // namespace FRIGGA_NAMESPACE
