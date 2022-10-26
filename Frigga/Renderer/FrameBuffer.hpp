#pragma once

#include <glm/glm.hpp>

FRIGGA_BEGIN

struct FrameBufferSpecification
{
	uint width = 0, height = 0;
	uint samples = 1;

	bool swap_chain_target = false;
};

class FrameBuffer
{
public:
    virtual ~FrameBuffer() = default;

	virtual void bind() = 0;
	virtual void unbind() = 0;

	virtual void resize(uint width, uint height) = 0;

	virtual uint getColorAttachmentRendererID() const = 0;

	virtual const FrameBufferSpecification &getSpecification() const = 0;

	static shared<FrameBuffer> create(const FrameBufferSpecification &spec);
};

FRIGGA_END
