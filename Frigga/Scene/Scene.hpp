#pragma once

#include "Renderer/SceneRenderer.hpp"

#include "ECS/World.hpp"

FRIGGA_BEGIN

class Scene
{
  public:
    Scene();
    ~Scene() = default;

    void update();

    void onEditorRender(shared<SceneRenderer> renderer, double ts);

    World m_world;
};

FRIGGA_END
