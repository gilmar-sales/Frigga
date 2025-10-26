#pragma once

#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE {

struct TransformComponent: fr::Component
{
    glm::vec3 position{};
    glm::vec3 scale{1, 1, 1};
    glm::quat rotation{1, 0, 0, 0};
};

}
