#include "Frigga/ECS/TransformPolicy.hpp"

#include "Frigga/ECS/TransformUtil.hpp"

namespace FRIGGA_NAMESPACE
{

    void TransformPolicy::OnRoot(fr::ComponentManager &components, fr::Entity entity) const
    {
        components.TryGetComponents<Local, World>(entity, [](Local &local, World &world) {
            world.matrix = TransformUtil::LocalMatrix(local);
        });
    }

    void TransformPolicy::Propagate(fr::ComponentManager &components, fr::Entity parent,
                                    fr::Entity child) const
    {
        glm::mat4 parentWorld(1.0f);
        components.TryGetComponents<World>(parent,
                                           [&](World &world) { parentWorld = world.matrix; });
        components.TryGetComponents<Local, World>(child, [&](Local &local, World &world) {
            world.matrix = parentWorld * TransformUtil::LocalMatrix(local);
        });
    }

} // namespace FRIGGA_NAMESPACE
