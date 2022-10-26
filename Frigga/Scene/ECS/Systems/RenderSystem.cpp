#include "RenderSystem.hpp"

#include "../World.hpp"

FRIGGA_BEGIN

void RenderSystem::onUpdate()
{
    static glm::mat4 projection = glm::perspective(glm::radians(90.0f), (float)1280 / (float)720, 0.1f, 100.0f);
    static glm::mat4 view       = glm::lookAt(glm::vec3(0, 0, -5), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));

    for(auto entity: getRegisteredEntities())
    {
        TransformComponent &transform = world->getComponent<TransformComponent>(entity);
        MeshComponent &mesh           = world->getComponent<MeshComponent>(entity);

        glm::mat4 model = glm::mat4(1.0f);
        model           = glm::translate(model, glm::vec3(0, 0, 5));
        model           = glm::scale(model, transform.scale);

        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);

        RendererAPI::DrawIndexed(mesh.vertex_array_object, mesh.vertex_buffer_object, mesh.element_buffer_object, 12);
        // FG_LOG_INFO("Render entity {}", entity);
    }
}

FRIGGA_END
