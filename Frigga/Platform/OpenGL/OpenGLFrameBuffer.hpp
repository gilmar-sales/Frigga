#pragma once

#include <glbinding/gl/gl.h>
#include <glbinding/gl45core/gl.h>
#include <glbinding/gl45ext/gl.h>
#include <glbinding/glbinding.h>

#include "Renderer/FrameBuffer.hpp"

FRIGGA_BEGIN

class OpenGLFrameBuffer : public FrameBuffer
{
public:
	OpenGLFrameBuffer(const FrameBufferSpecification& spec);
	virtual ~OpenGLFrameBuffer();

	void invalidate();

	virtual void bind() override;
	virtual void unbind() override;

	virtual void resize(uint width, uint height) override;

	virtual uint getColorAttachmentRendererID() const override
    {
        return m_colorAttachment;
    }

	virtual const FrameBufferSpecification &getSpecification() const override
    {
        return m_specification;
    }

  private:
	uint m_rendererID = 0;
	uint m_colorAttachment = 0;
	uint m_depthAttachment = 0;
    FrameBufferSpecification m_specification;
};

FRIGGA_END
