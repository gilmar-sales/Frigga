#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "OpenGLRendererAPI.hpp"

using namespace gl;

FRIGGA_BEGIN

void OpenGLRendererAPI::init()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
}

void OpenGLRendererAPI::setViewport(uint x, uint y, uint width, uint height)
{
    glViewport(x, y, width, height);
}

void OpenGLRendererAPI::setClearColor(const glm::vec4 &color)
{
    glClearColor(color.r, color.g, color.b, color.a);
}

void OpenGLRendererAPI::clear()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRendererAPI::drawIndexed(const uint vertex_array_object, const uint vertex_buffer_object,
                                          const uint element_buffer_object, uint index_count)
{
    // glBindVertexArray(vertex_array_object);
    // glBindVertexBuffer(vertex_buffer_object);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_object);

    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr);
}

FRIGGA_END
