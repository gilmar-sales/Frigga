#pragma once

#include <Core/Layer.hpp>
#include <Renderer/FrameBuffer.hpp>
#include <Scene/Scene.hpp>

class SceneLayer: public fg::Layer
{
  public:
    SceneLayer(shared<fg::Scene> frame);
    ~SceneLayer() = default;

    void onUpdate() override;
    void onGui() override;

  private:
    shared<fg::Scene> scene;
    shared<fg::FrameBuffer> frame;
    shared<fg::SceneRenderer> scene_renderer;
};
