#pragma once

FRIGGA_BEGIN

class RenderContext
{
public:
	virtual ~RenderContext() = default;

	virtual void init() = 0;
	virtual void swapBuffers() = 0;

	static unique<RenderContext> create(void* window);
};

FRIGGA_END