#pragma once

#include <Freya/Freya.hpp>
#include <Freyr/Freyr.hpp>

FRIGGA_BEGIN

class RenderSystem: public fr::System
{
  public:
    RenderSystem(Ref<fr::Scene> scene, Ref<fra::Renderer> renderer): System(scene), mRenderer(renderer) {};

    ~RenderSystem() = default;

    void Update(float deltaTime) override;

  private:
    Ref<fra::Renderer> mRenderer;
};

FRIGGA_END
