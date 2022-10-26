#pragma once

#include "Renderer/RendererAPI.hpp"

FRIGGA_BEGIN

class Renderer
{
public:
	static void init();
	static void shutdown();

	static void onWindowResize(uint width, uint height);

	static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
};

FRIGGA_END
