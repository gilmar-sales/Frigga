#include "Scene.hpp"

#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/Scene/SceneSerializer.hpp"

#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        TransformComponent makeLookAtTransform(const glm::vec3 &position, const glm::vec3 &target,
                                               const glm::vec3 &worldUp = {0.0f, 1.0f, 0.0f})
        {
            const glm::vec3 forward = glm::normalize(target - position);
            // Match Freya/OpenGL: camera looks along local -Z.
            return TransformComponent {
                .position = position,
                .scale    = {1.0f, 1.0f, 1.0f},
                .rotation = glm::quatLookAt(forward, worldUp),
            };
        }
    } // namespace

    Scene::Scene(const skr::Arc<fra::Renderer> &renderer, const skr::Arc<skr::Logger<Scene>> &logger,
                 const skr::Arc<fr::Registry> &ecsRegistry,
                 const skr::Arc<PrimitiveMeshFactory> &primitives,
                 const skr::Arc<AssetRegistry> &assets,
                 const skr::Arc<UserComponentRegistry> &userComponents)
        : mEcsRegistry(ecsRegistry), mRenderer(renderer), mLogger(logger), mPrimitives(primitives),
          mAssets(assets), mUserComponents(userComponents)
    {
        CreateDefaultEntities();
    }

    void Scene::Update(float ts)
    {
        mLogger->LogTrace("scene update");
    }

    void Scene::OnEditorRender(float ts) {}

    void Scene::FlushEcs()
    {
        mEcsRegistry->ExecuteTasks();
    }

    void Scene::ClearEntities()
    {
        if(mUserComponents)
        {
            mUserComponents->ClearDeferred();
        }

        std::vector<fr::Entity> entities;
        mEcsRegistry->CreateMutation()->Each<NameComponent>(
            [&](auto entity, NameComponent &) { entities.push_back(entity); });

        for(const auto entity : entities)
        {
            mEcsRegistry->DestroyEntity(entity);
        }

        FlushEcs();
        mMainCameraEntity = {};
    }

    void Scene::CreateDefaultEntities()
    {
        CreateDefaultEntities3D();
    }

    void Scene::CreateDefaultEntities3D()
    {
        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Cube"}, TransformComponent {},
            MeshComponent {.meshId = mPrimitives->GetMesh(PrimitiveType::Cube)},
            MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});

        // Blender-like default framing: elevated 3/4 view looking at the unit cube.
        constexpr glm::vec3 cubeCenter {0.0f, 0.0f, 0.0f};
        constexpr glm::vec3 cameraPosition {4.0f, 3.0f, 4.0f};
        const auto          lookAt = makeLookAtTransform(cameraPosition, cubeCenter);

        mEditorCamera = EditorCamera {
            .transform   = lookAt,
            .fovDegrees  = 50.0f,
            .nearPlane   = 0.1f,
            .farPlane    = 1000.0f,
        };

        mMainCameraEntity = mEcsRegistry->CreateEntity(
            NameComponent {.name = "Main Camera"}, lookAt,
            CameraComponent {.fovDegrees = 50.0f,
                             .nearPlane  = 0.1f,
                             .farPlane   = 1000.0f,
                             .primary    = true,
                             .locked     = true});

        // Default key light: elevated, aiming down at the cube (-Y along local -Z).
        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Point Light"},
            TransformComponent {.position = {4.0f, 6.0f, 2.0f},
                                .scale    = {1.0f, 1.0f, 1.0f},
                                .rotation = glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f},
                                                            glm::vec3 {0.0f, 0.0f, 1.0f})},
            LightComponent {.type      = fra::LightType::Point,
                            .color     = {1.0f, 1.0f, 1.0f},
                            .radius    = 40.0f,
                            .intensity = 30.0f});

        FlushEcs();
    }

    void Scene::CreateDefaultEntities2D()
    {
        // Top-down gameplay plane (XZ). Perspective camera; treat XZ as the 2D plane.
        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Ground"},
            TransformComponent {.position = {0.0f, 0.0f, 0.0f},
                                .scale    = {8.0f, 1.0f, 8.0f},
                                .rotation = {1.0f, 0.0f, 0.0f, 0.0f}},
            MeshComponent {.meshId = mPrimitives->GetMesh(PrimitiveType::Plane)},
            MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});

        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Player"},
            TransformComponent {.position = {0.0f, 0.05f, 0.0f},
                                .scale    = {0.75f, 0.75f, 0.75f},
                                .rotation = {1.0f, 0.0f, 0.0f, 0.0f}},
            MeshComponent {.meshId = mPrimitives->GetMesh(PrimitiveType::Quad)},
            MaterialComponent {.materialId = mPrimitives->GetDefaultMaterial()});

        constexpr glm::vec3 lookTarget {0.0f, 0.0f, 0.0f};
        constexpr glm::vec3 cameraPosition {0.0f, 12.0f, 0.01f};
        const auto          lookAt = makeLookAtTransform(cameraPosition, lookTarget);

        mEditorCamera = EditorCamera {
            .transform   = lookAt,
            .fovDegrees  = 40.0f,
            .nearPlane   = 0.1f,
            .farPlane    = 1000.0f,
        };

        mMainCameraEntity = mEcsRegistry->CreateEntity(
            NameComponent {.name = "Main Camera"}, lookAt,
            CameraComponent {.fovDegrees = 40.0f,
                             .nearPlane  = 0.1f,
                             .farPlane   = 1000.0f,
                             .primary    = true,
                             .locked     = true});

        mEcsRegistry->CreateEntity(
            NameComponent {.name = "Sun"},
            TransformComponent {.position = {0.0f, 8.0f, 0.0f},
                                .scale    = {1.0f, 1.0f, 1.0f},
                                .rotation = glm::quatLookAt(glm::vec3 {0.0f, -1.0f, 0.0f},
                                                            glm::vec3 {0.0f, 0.0f, 1.0f})},
            LightComponent {.type      = fra::LightType::Directional,
                            .color     = {1.0f, 1.0f, 1.0f},
                            .radius    = 0.0f,
                            .intensity = 3.0f});

        FlushEcs();
    }

    void Scene::NewScene()
    {
        NewSceneFromTemplate(SceneTemplate::D3);
    }

    void Scene::NewSceneFromTemplate(SceneTemplate sceneTemplate)
    {
        ClearEntities();
        switch(sceneTemplate)
        {
        case SceneTemplate::D2:
            CreateDefaultEntities2D();
            break;
        case SceneTemplate::D3:
        default:
            CreateDefaultEntities3D();
            break;
        }
        mPath.clear();
        PreferEditorCamera();
        ClearRenderIsolation();
        mLogger->LogInformation("Created new scene (template {})",
                                sceneTemplate == SceneTemplate::D2 ? "2d" : "3d");
    }

    bool Scene::SaveScene(const std::filesystem::path &path)
    {
        if(!SceneSerializer::Save(*this, path))
        {
            return false;
        }
        mPath = path;
        return true;
    }

    bool Scene::SaveScene()
    {
        if(mPath.empty())
        {
            mLogger->LogWarning("SaveScene called without a path");
            return false;
        }
        return SaveScene(mPath);
    }

    bool Scene::LoadScene(const std::filesystem::path &path)
    {
        ClearEntities();
        if(!SceneSerializer::Load(*this, path))
        {
            CreateDefaultEntities();
            mPath.clear();
            return false;
        }

        FlushEcs();
        mPath = path;
        PreferEditorCamera();
        ClearRenderIsolation();
        return true;
    }

    bool Scene::CaptureSnapshot(std::string &outJson)
    {
        FlushEcs();
        return SceneSerializer::Serialize(*this, outJson);
    }

    bool Scene::RestoreSnapshot(std::string_view json)
    {
        // Clear only after a successful deserialize would lose the live edit scene on
        // parse failure — parse into a temporary Scene first isn't cheap, so: reject
        // empty payloads, then clear and restore; if restore fails recreate defaults.
        if(json.empty())
        {
            return false;
        }

        ClearEntities();
        if(!SceneSerializer::Deserialize(*this, json))
        {
            CreateDefaultEntities();
            return false;
        }

        FlushEcs();
        return true;
    }

} // namespace FRIGGA_NAMESPACE
