#pragma once

#include <Freya/Core/Renderer.hpp>
#include <Skirnir/Skirnir.hpp>

FRIGGA_BEGIN

class Scene
{
  public:
    Scene(const Ref<fra::Renderer> &renderer, const Ref<skr::Logger<Scene>> &logger);
    ~Scene() = default;

    void Update(float ts);

    void OnEditorRender(float ts);

  private:
    Ref<fra::Renderer> mRenderer;
    Ref<skr::Logger<Scene>> mLogger;
};

FRIGGA_END
