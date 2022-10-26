#pragma once

#include <glbinding/gl/gl.h>
#include <glbinding/gl45core/gl.h>
#include <glbinding/gl45ext/gl.h>
#include <glbinding/glbinding.h>

#include "Renderer/RendererAPI.hpp"

FRIGGA_BEGIN

class OpenGLRendererAPI : public RendererAPI
{
public:
	OpenGLRendererAPI() = default;

	virtual void init() override;
	virtual void setViewport(uint x, uint y, uint width, uint height) override;

	virtual void setClearColor(const glm::vec4& color) override;
	virtual void clear() override;

	virtual void drawIndexed(
		const uint vertex_array_object,
		const uint vertex_buffer_object,
		const uint element_buffer_object,
		uint index_count = 0
	) override;
};

FRIGGA_END
