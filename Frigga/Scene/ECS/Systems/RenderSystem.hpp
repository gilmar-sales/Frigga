#pragma once

#include "Core/Log.hpp"

#include "../Design.hpp"
#include <Freyr/Core/BaseSystem.hpp>

#include "Renderer/Renderer.hpp"
#include "Renderer/RendererAPI.hpp"

#include <glbinding/gl46core/gl.h>
#include <glbinding/glbinding.h>

using namespace gl;

FRIGGA_BEGIN

class RenderSystem: public freyr::BaseSystem
{
  public:
    using Signature = std::tuple<TransformComponent, MeshComponent>;

    uint32_t vertex;
    uint32_t fragment;
    uint32_t shader;

    const char *vertex_src = R"glsl(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(aPos, 1.0f);
        TexCoord = vec2(aTexCoord.x, aTexCoord.y);
    }
    )glsl";

    const char *fragment_src = R"glsl(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;

    // texture samplers
    uniform sampler2D texture1;
    uniform sampler2D texture2;

    void main()
    {
        FragColor = vec4(1,0,0,1);
    }
    )glsl";

    RenderSystem()
    {
        vertex   = glCreateShader(GL_VERTEX_SHADER);
        fragment = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(vertex, 1, &vertex_src, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        glShaderSource(fragment, 1, &fragment_src, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        shader = glCreateProgram();
        glAttachShader(shader, vertex);
        glAttachShader(shader, fragment);
        glLinkProgram(shader);
        checkCompileErrors(shader, "PROGRAM");

        glUseProgram(shader);
        FG_LOG_TRACE("RenderSystem initialized!");
    };

    ~RenderSystem() = default;

    void onUpdate() override;

    void checkCompileErrors(GLuint shader, std::string type)
    {
        GLint success;
        GLchar infoLog[1024];
        if(type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if(!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                FG_LOG_ERROR("ERROR::SHADER_COMPILATION_ERROR of type: {} \n {}\n -- "
                             "--------------------------------------------------- -- ",
                             type, infoLog);
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if(!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                FG_LOG_ERROR("ERROR::PROGRAM_LINKING_ERROR of type: {} \n {}\n -- "
                             "--------------------------------------------------- -- ",
                             type, infoLog);
            }
        }
    }
};

FRIGGA_END
