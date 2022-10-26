#include "OpenGLFrameBuffer.hpp"

#include "Core/Log.hpp"

using namespace gl;

FRIGGA_BEGIN

static const uint MaxFrameBufferSize = 8192u;

OpenGLFrameBuffer::OpenGLFrameBuffer(const FrameBufferSpecification &spec): m_specification(spec)
{
    invalidate();
}

OpenGLFrameBuffer::~OpenGLFrameBuffer()
{
    glDeleteFramebuffers(1, &m_rendererID);
    glDeleteTextures(1, &m_colorAttachment);
    glDeleteTextures(1, &m_depthAttachment);
}

void OpenGLFrameBuffer::invalidate()
{
    if(m_rendererID)
    {
        glDeleteFramebuffers(1, &m_rendererID);
        glDeleteTextures(1, &m_colorAttachment);
        glDeleteTextures(1, &m_depthAttachment);
    }

    glCreateFramebuffers(1, &m_rendererID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_colorAttachment);
    glBindTexture(GL_TEXTURE_2D, m_colorAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_specification.width, m_specification.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorAttachment, 0);

    glCreateTextures(GL_TEXTURE_2D, 1, &m_depthAttachment);
    glBindTexture(GL_TEXTURE_2D, m_depthAttachment);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, m_specification.width, m_specification.height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_depthAttachment, 0);

    FG_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLFrameBuffer::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);
    glViewport(0, 0, m_specification.width, m_specification.height);
}

void OpenGLFrameBuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLFrameBuffer::resize(uint width, uint height)
{
    if(width == m_specification.width && height == m_specification.height)
    {
        // FG_LOG_WARNING("Attempted to rezize FrameBuffer with same dimensions");
        return;
    }

    if(width == 0 || height == 0 || width > MaxFrameBufferSize || height > MaxFrameBufferSize)
    {
        FG_LOG_WARNING("Attempted to rezize FrameBuffer to {}, {}", width, height);
        return;
    }
    m_specification.width  = width;
    m_specification.height = height;

    invalidate();
}

FRIGGA_END
