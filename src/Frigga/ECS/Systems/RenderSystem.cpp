#include "RenderSystem.hpp"

#include "../Components/MeshComponent.hpp"
#include "../Components/TransformComponent.hpp"

namespace FRIGGA_NAMESPACE
{

    void RenderSystem::Update(float deltaTime)
    {
        static glm::mat4 projection = glm::perspective(
            glm::radians(90.0f), (float)mWindow->GetWidth() / (float)mWindow->GetHeight(), 0.1f, 100.0f);
        
        static glm::mat4 view = glm::lookAt(glm::vec3(0, 0, -5), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

        mScene->ForEach<TransformComponent, MeshComponent>([this](auto entity, auto &transform, auto &mesh) {
            glm::mat4 model = glm::mat4(1.0f);
            model           = glm::translate(model, transform.position);
            model           = glm::scale(model, transform.scale);
        });
    }

} // namespace FRIGGA_NAMESPACE
