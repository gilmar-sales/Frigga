#include "PhysicsSystem.hpp"

#include "../World.hpp"

FRIGGA_BEGIN

void PhysicsSystem::onUpdate()
{
    for(auto entity: getRegisteredEntities())
    {
        TransformComponent &transform = world->getComponent<TransformComponent>(entity);

        if(transform.position.x > 100000)
        {
            transform.position.x -= 1;
        }
        else
        {
            transform.position.x += 1;
        }
        transform.position.y += 1;
        transform.position.z += 1;

        // PH_CORE_INFO("phys process entity {}", entity);
    }
}

FRIGGA_END