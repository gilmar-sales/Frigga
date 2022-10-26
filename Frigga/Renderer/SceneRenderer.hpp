#pragma once

#include "Renderer/Renderer.hpp"

#include "Renderer/FrameBuffer.hpp"

FRIGGA_BEGIN

class SceneRenderer
{
  public:
    SceneRenderer(shared<FrameBuffer> frame): frame(frame){};
    ~SceneRenderer(){};

  private:
    shared<FrameBuffer> frame;
};

FRIGGA_END
