#include "RenderSystem.hpp"

#include "../Components/MeshComponent.hpp"
#include "../Components/TransformComponent.hpp"

FRIGGA_BEGIN

void RenderSystem::Update(float deltaTime)
{
    static glm::mat4 projection = glm::perspective(glm::radians(90.0f), (float)1280 / (float)720, 0.1f, 100.0f);
    static glm::mat4 view       = glm::lookAt(glm::vec3(0, 0, -5), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

    mScene->ForEach<TransformComponent, MeshComponent>([this](auto entity, auto &transform, auto &mesh) {
        glm::mat4 model = glm::mat4(1.0f);
        model           = glm::translate(model, transform.position);
        model           = glm::scale(model, transform.scale);
    });
}

FRIGGA_END
