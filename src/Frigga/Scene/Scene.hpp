#pragma once

#include <Freya/Core/Renderer.hpp>
#include <Skirnir/Skirnir.hpp>
#include <Freyr/Freyr.hpp>

namespace FRIGGA_NAMESPACE {

class Scene
{
  public:
    Scene(const Ref<fra::Renderer> &renderer, const Ref<skr::Logger<Scene>> &logger, const Ref<fr::Scene>& ecsScene);
    ~Scene() = default;

    void Update(float ts);

    void OnEditorRender(float ts);

  private:
    Ref<fr::Scene> mEcsScene;
    Ref<fra::Renderer> mRenderer;
    Ref<skr::Logger<Scene>> mLogger;
};

}
