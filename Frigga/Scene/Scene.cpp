#include "Scene/Scene.hpp"

#include "Core/Log.hpp"

FRIGGA_BEGIN

Scene::Scene(): m_world(1024u)
{
    auto camera = m_world.createEntity();

    m_world.addComponents(camera, NameComponent{"Main Camera"}, TransformComponent{});
    m_world.addTag<MainCameraTag>(camera);
}

void Scene::update()
{
    FG_LOG_TRACE("scene update");
    m_world.update();
}

void Scene::onEditorRender(shared<SceneRenderer> renderer, double ts) {}

FRIGGA_END
