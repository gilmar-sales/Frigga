#pragma once

#include <Core/Layer.hpp>
#include <Renderer/FrameBuffer.hpp>
#include <Scene/Scene.hpp>

class GameLayer: public fg::Layer
{
  public:
    GameLayer(shared<fg::Scene> frame);
    ~GameLayer() = default;

    void onUpdate() override;
    void onGui() override;

  private:
    shared<fg::Scene> scene;
    shared<fg::FrameBuffer> frame;
    shared<fg::SceneRenderer> scene_renderer;
};
