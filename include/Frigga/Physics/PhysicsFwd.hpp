#pragma once

#include <Frigga/Macro.hpp>

// Headers that only hold `skr::Arc<IPhysicsWorld>` should include this instead
// of IPhysicsWorld.hpp, so backend API changes do not rebuild scene/ECS code.
namespace FRIGGA_NAMESPACE
{

    class IPhysicsWorld;

} // namespace FRIGGA_NAMESPACE
