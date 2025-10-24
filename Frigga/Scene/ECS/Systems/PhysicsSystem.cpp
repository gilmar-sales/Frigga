#include "PhysicsSystem.hpp"

#include "../Components/TransformComponent.hpp"

FRIGGA_BEGIN

void PhysicsSystem::Update(float deltaTime)
{
    mScene->ForEach<TransformComponent>([deltaTime](auto entity, auto &transform) {
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
    });
}

FRIGGA_END