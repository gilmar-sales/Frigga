#include "Scene/Scene.hpp"

FRIGGA_BEGIN

Scene::Scene(const Ref<fra::Renderer> &renderer, const Ref<skr::Logger<Scene>> &logger)
    : mRenderer(renderer), mLogger(logger)
{
    // auto camera = m_world.createEntity();

    // m_world.addComponents(camera, NameComponent{"Main Camera"}, TransformComponent{});
    // m_world.addTag<MainCameraTag>(camera);
}

void Scene::Update(float ts)
{
    mLogger->LogTrace("scene update");
    // m_world.update();
}

void Scene::OnEditorRender(float ts) {}

FRIGGA_END
