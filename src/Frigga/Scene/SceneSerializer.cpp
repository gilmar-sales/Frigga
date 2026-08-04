#include "SceneSerializer.hpp"

#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <simdjson.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr int64_t kSceneVersion = 1;

        struct SceneTransformDto
        {
            // Keep empty: simdjson static reflection appends into existing std::vector
            // values (seeded defaults + JSON => size 6/8 and breaks strict readers).
            std::vector<float> position;
            std::vector<float> scale;
            std::vector<float> rotation; // wxyz
        };

        struct SceneMeshDto
        {
            std::string primitive {"Cube"};
        };

        struct SceneMaterialDto
        {
            [[= simdjson::rename<"default">]] bool isDefault = true;
        };

        struct SceneCameraDto
        {
            float fovDegrees = 60.0f;
            float nearPlane  = 0.1f;
            float farPlane   = 1000.0f;
            bool  primary    = false;
            bool  locked     = false;
        };

        struct SceneLightDto
        {
            std::string        type {"Point"};
            std::vector<float> color;
            float              radius            = 40.0f;
            float              intensity         = 30.0f;
            float              innerAngleDegrees = 25.0f;
            float              outerAngleDegrees = 35.0f;
        };

        struct SceneRigidBodyDto
        {
            std::string        motion {"Dynamic"};
            std::string        shape {"Box"};
            std::vector<float> halfExtents;
            float              radius           = 0.5f;
            float              height            = 1.0f;
            float              mass              = 1.0f;
            float              friction          = 0.5f;
            float              restitution       = 0.0f;
            int64_t            collisionLayer    = 1;
            int64_t            collideWithLayers = 0xffff;
        };

        struct SceneEntityDto
        {
            std::string                        name;
            std::optional<SceneTransformDto>   transform;
            std::optional<SceneMeshDto>        mesh;
            std::optional<SceneMaterialDto>    material;
            std::optional<SceneCameraDto>      camera;
            std::optional<SceneLightDto>       light;
            std::optional<SceneRigidBodyDto>   rigidBody;
        };

        struct SceneEditorCameraDto
        {
            SceneTransformDto transform {};
            float             fovDegrees = 50.0f;
            float             nearPlane  = 0.1f;
            float             farPlane   = 1000.0f;
        };

        struct SceneDocument
        {
            int64_t                     version = kSceneVersion;
            SceneEditorCameraDto        editorCamera {};
            std::vector<SceneEntityDto> entities {};
        };

        bool ReadVec3(const std::vector<float> &values, glm::vec3 &out)
        {
            if(values.size() == 3)
            {
                out = {values[0], values[1], values[2]};
                return true;
            }
            // simdjson may append into pre-seeded vectors (legacy snapshots / defaults).
            if(values.size() >= 6 && values.size() % 3 == 0)
            {
                const auto base = values.size() - 3;
                out = {values[base], values[base + 1], values[base + 2]};
                return true;
            }
            return false;
        }

        bool ReadQuat(const std::vector<float> &values, glm::quat &out)
        {
            if(values.size() == 4)
            {
                out = glm::quat(values[0], values[1], values[2], values[3]); // wxyz
                return true;
            }
            if(values.size() >= 8 && values.size() % 4 == 0)
            {
                const auto base = values.size() - 4;
                out = glm::quat(values[base], values[base + 1], values[base + 2],
                                values[base + 3]);
                return true;
            }
            return false;
        }

        SceneTransformDto ToDto(const TransformComponent &transform)
        {
            return SceneTransformDto {
                .position = {transform.position.x, transform.position.y, transform.position.z},
                .scale    = {transform.scale.x, transform.scale.y, transform.scale.z},
                .rotation = {transform.rotation.w, transform.rotation.x, transform.rotation.y,
                             transform.rotation.z},
            };
        }

        bool FromDto(const SceneTransformDto &dto, TransformComponent &out)
        {
            TransformComponent transform {};
            if(dto.position.empty())
            {
                transform.position = {};
            }
            else if(!ReadVec3(dto.position, transform.position))
            {
                return false;
            }

            if(dto.scale.empty())
            {
                transform.scale = {1.0f, 1.0f, 1.0f};
            }
            else if(!ReadVec3(dto.scale, transform.scale))
            {
                return false;
            }

            if(dto.rotation.empty())
            {
                transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            }
            else if(!ReadQuat(dto.rotation, transform.rotation))
            {
                return false;
            }

            out = transform;
            return true;
        }

        const char *LightTypeToString(fra::LightType type)
        {
            switch(type)
            {
            case fra::LightType::Point:
                return "Point";
            case fra::LightType::Directional:
                return "Directional";
            case fra::LightType::Spot:
                return "Spot";
            case fra::LightType::Area:
                return "Area";
            }
            return "Point";
        }

        bool TryParseLightType(std::string_view name, fra::LightType &outType)
        {
            if(name == "Point")
            {
                outType = fra::LightType::Point;
                return true;
            }
            if(name == "Directional")
            {
                outType = fra::LightType::Directional;
                return true;
            }
            if(name == "Spot")
            {
                outType = fra::LightType::Spot;
                return true;
            }
            if(name == "Area")
            {
                outType = fra::LightType::Area;
                return true;
            }
            return false;
        }

        const char *MotionToString(BodyMotionType motion)
        {
            switch(motion)
            {
            case BodyMotionType::Static:
                return "Static";
            case BodyMotionType::Kinematic:
                return "Kinematic";
            case BodyMotionType::Dynamic:
                return "Dynamic";
            }
            return "Dynamic";
        }

        bool TryParseMotion(std::string_view name, BodyMotionType &out)
        {
            if(name == "Static")
            {
                out = BodyMotionType::Static;
                return true;
            }
            if(name == "Kinematic")
            {
                out = BodyMotionType::Kinematic;
                return true;
            }
            if(name == "Dynamic")
            {
                out = BodyMotionType::Dynamic;
                return true;
            }
            return false;
        }

        const char *ShapeToString(ColliderShape shape)
        {
            switch(shape)
            {
            case ColliderShape::Box:
                return "Box";
            case ColliderShape::Sphere:
                return "Sphere";
            case ColliderShape::Capsule:
                return "Capsule";
            case ColliderShape::Mesh:
                return "Mesh";
            }
            return "Box";
        }

        bool TryParseShape(std::string_view name, ColliderShape &out)
        {
            if(name == "Box")
            {
                out = ColliderShape::Box;
                return true;
            }
            if(name == "Sphere")
            {
                out = ColliderShape::Sphere;
                return true;
            }
            if(name == "Capsule")
            {
                out = ColliderShape::Capsule;
                return true;
            }
            if(name == "Mesh")
            {
                out = ColliderShape::Mesh;
                return true;
            }
            return false;
        }
    } // namespace

    bool SceneSerializer::Serialize(Scene &scene, std::string &outJson)
    {
        SceneDocument document {};
        document.version = kSceneVersion;

        const auto &editorCamera = scene.GetEditorCamera();
        document.editorCamera    = SceneEditorCameraDto {
               .transform  = ToDto(editorCamera.transform),
               .fovDegrees = editorCamera.fovDegrees,
               .nearPlane  = editorCamera.nearPlane,
               .farPlane   = editorCamera.farPlane,
        };

        auto registry   = scene.mEcsRegistry;
        auto primitives = scene.mPrimitives;

        registry->CreateMutation()->Each<NameComponent>([&](auto entity, NameComponent &name) {
            SceneEntityDto dto {.name = name.name};

            registry->TryGetComponents<TransformComponent>(
                entity, [&](TransformComponent &transform) { dto.transform = ToDto(transform); });

            registry->TryGetComponents<MeshComponent>(entity, [&](MeshComponent &mesh) {
                PrimitiveType primitive = PrimitiveType::Cube;
                if(!primitives->TryFindPrimitive(mesh.meshId, primitive))
                {
                    scene.mLogger->LogWarning(
                        "Scene save: meshId {} on '{}' is not a known primitive; defaulting to Cube",
                        mesh.meshId, name.name);
                    primitive = PrimitiveType::Cube;
                }
                dto.mesh = SceneMeshDto {.primitive =
                                             PrimitiveMeshFactory::GetDisplayName(primitive)};
            });

            registry->TryGetComponents<MaterialComponent>(entity, [&](MaterialComponent &) {
                dto.material = SceneMaterialDto {.isDefault = true};
            });

            registry->TryGetComponents<CameraComponent>(entity, [&](CameraComponent &camera) {
                dto.camera = SceneCameraDto {
                    .fovDegrees = camera.fovDegrees,
                    .nearPlane  = camera.nearPlane,
                    .farPlane   = camera.farPlane,
                    .primary    = camera.primary,
                    .locked     = camera.locked,
                };
            });

            registry->TryGetComponents<LightComponent>(entity, [&](LightComponent &light) {
                dto.light = SceneLightDto {
                    .type              = LightTypeToString(light.type),
                    .color             = {light.color.x, light.color.y, light.color.z},
                    .radius            = light.radius,
                    .intensity         = light.intensity,
                    .innerAngleDegrees = light.innerAngleDegrees,
                    .outerAngleDegrees = light.outerAngleDegrees,
                };
            });

            registry->TryGetComponents<RigidBodyComponent>(entity, [&](RigidBodyComponent &rb) {
                dto.rigidBody = SceneRigidBodyDto {
                    .motion            = MotionToString(rb.motion),
                    .shape             = ShapeToString(rb.shape),
                    .halfExtents       = {rb.halfExtents.x, rb.halfExtents.y, rb.halfExtents.z},
                    .radius            = rb.radius,
                    .height            = rb.height,
                    .mass              = rb.mass,
                    .friction          = rb.friction,
                    .restitution       = rb.restitution,
                    .collisionLayer    = rb.collisionLayer,
                    .collideWithLayers = rb.collideWithLayers,
                };
            });

            document.entities.push_back(std::move(dto));
        });

        outJson.clear();
        if(const auto error = simdjson::to_json(document, outJson); error)
        {
            scene.mLogger->LogError("Failed to serialize scene: {}",
                                    simdjson::error_message(error));
            return false;
        }

        return true;
    }

    bool SceneSerializer::Save(Scene &scene, const std::filesystem::path &path)
    {
        std::string json;
        if(!Serialize(scene, json))
        {
            return false;
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            scene.mLogger->LogError("Failed to open scene file for writing: {}", path.string());
            return false;
        }

        file << json;
        if(!json.empty() && json.back() != '\n')
        {
            file << '\n';
        }

        if(!file)
        {
            scene.mLogger->LogError("Failed to write scene file: {}", path.string());
            return false;
        }

        scene.mLogger->LogInformation("Saved scene to {}", path.string());
        return true;
    }

    bool SceneSerializer::Deserialize(Scene &scene, std::string_view json)
    {
        const simdjson::padded_string padded(json);
        SceneDocument document {};
        if(const auto error = simdjson::from(padded).get(document); error)
        {
            scene.mLogger->LogError("Failed to parse scene snapshot: {}",
                                    simdjson::error_message(error));
            return false;
        }

        if(document.version > kSceneVersion)
        {
            scene.mLogger->LogWarning("Scene version {} is newer than supported {}",
                                      document.version, kSceneVersion);
        }

        TransformComponent editorTransform {};
        if(!FromDto(document.editorCamera.transform, editorTransform))
        {
            scene.mLogger->LogWarning(
                "Invalid editorCamera.transform; using identity defaults");
            editorTransform = {};
        }

        scene.mEditorCamera = EditorCamera {
            .transform  = editorTransform,
            .fovDegrees = document.editorCamera.fovDegrees,
            .nearPlane  = document.editorCamera.nearPlane,
            .farPlane   = document.editorCamera.farPlane,
        };

        auto registry   = scene.mEcsRegistry;
        auto primitives = scene.mPrimitives;

        scene.mMainCameraEntity       = {};
        fr::Entity firstPrimaryCamera = {};
        bool foundLockedCamera        = false;
        bool foundPrimaryCamera       = false;

        for(const auto &entityDto : document.entities)
        {
            const auto entity = registry->CreateEntity(NameComponent {.name = entityDto.name});

            if(entityDto.transform)
            {
                TransformComponent transform {};
                if(!FromDto(*entityDto.transform, transform))
                {
                    scene.mLogger->LogError("Invalid transform for entity '{}'", entityDto.name);
                    return false;
                }
                registry->AddComponents(entity, transform);
            }

            if(entityDto.mesh)
            {
                PrimitiveType primitive = PrimitiveType::Cube;
                if(!PrimitiveMeshFactory::TryParsePrimitive(entityDto.mesh->primitive, primitive))
                {
                    scene.mLogger->LogError("Unknown primitive '{}' on entity '{}'",
                                            entityDto.mesh->primitive, entityDto.name);
                    return false;
                }

                registry->AddComponents(entity,
                                        MeshComponent {.meshId = primitives->GetMesh(primitive)});
                registry->AddComponents(
                    entity, MaterialComponent {.materialId = primitives->GetDefaultMaterial()});
            }
            else if(entityDto.material)
            {
                registry->AddComponents(
                    entity, MaterialComponent {.materialId = primitives->GetDefaultMaterial()});
            }

            if(entityDto.camera)
            {
                const auto &cameraDto = *entityDto.camera;
                const CameraComponent camera {
                    .fovDegrees = cameraDto.fovDegrees,
                    .nearPlane  = cameraDto.nearPlane,
                    .farPlane   = cameraDto.farPlane,
                    .primary    = cameraDto.primary,
                    .locked     = cameraDto.locked,
                };
                registry->AddComponents(entity, camera);

                if(camera.locked)
                {
                    scene.mMainCameraEntity = entity;
                    foundLockedCamera      = true;
                }
                else if(camera.primary && !foundPrimaryCamera)
                {
                    firstPrimaryCamera = entity;
                    foundPrimaryCamera = true;
                }
            }

            if(entityDto.light)
            {
                const auto &lightDto = *entityDto.light;
                fra::LightType type  = fra::LightType::Point;
                if(!TryParseLightType(lightDto.type, type))
                {
                    scene.mLogger->LogError("Unknown light type '{}' on entity '{}'", lightDto.type,
                                            entityDto.name);
                    return false;
                }

                glm::vec3 color {1.0f, 1.0f, 1.0f};
                if(!lightDto.color.empty() && !ReadVec3(lightDto.color, color))
                {
                    scene.mLogger->LogError("Invalid light color on entity '{}'", entityDto.name);
                    return false;
                }

                registry->AddComponents(entity,
                                        LightComponent {
                                            .type              = type,
                                            .color             = color,
                                            .radius            = lightDto.radius,
                                            .intensity         = lightDto.intensity,
                                            .innerAngleDegrees = lightDto.innerAngleDegrees,
                                            .outerAngleDegrees = lightDto.outerAngleDegrees,
                                        });
            }

            if(entityDto.rigidBody)
            {
                const auto &rbDto = *entityDto.rigidBody;
                RigidBodyComponent rigidBody {};
                if(!TryParseMotion(rbDto.motion, rigidBody.motion))
                {
                    scene.mLogger->LogError("Unknown rigid body motion '{}' on '{}'", rbDto.motion,
                                            entityDto.name);
                    return false;
                }
                if(!TryParseShape(rbDto.shape, rigidBody.shape))
                {
                    scene.mLogger->LogError("Unknown collider shape '{}' on '{}'", rbDto.shape,
                                            entityDto.name);
                    return false;
                }
                if(rbDto.halfExtents.empty())
                {
                    rigidBody.halfExtents = {0.5f, 0.5f, 0.5f};
                }
                else if(!ReadVec3(rbDto.halfExtents, rigidBody.halfExtents))
                {
                    scene.mLogger->LogError("Invalid halfExtents on '{}'", entityDto.name);
                    return false;
                }
                rigidBody.radius            = rbDto.radius;
                rigidBody.height            = rbDto.height;
                rigidBody.mass              = rbDto.mass;
                rigidBody.friction          = rbDto.friction;
                rigidBody.restitution       = rbDto.restitution;
                rigidBody.collisionLayer    = static_cast<std::uint8_t>(
                    std::clamp<int64_t>(rbDto.collisionLayer, 0, 15));
                rigidBody.collideWithLayers = static_cast<std::uint16_t>(
                    std::clamp<int64_t>(rbDto.collideWithLayers, 0, 0xffff));
                registry->AddComponents(entity, rigidBody);
            }
        }

        if(!foundLockedCamera && foundPrimaryCamera)
        {
            scene.mMainCameraEntity = firstPrimaryCamera;
        }

        return true;
    }

    bool SceneSerializer::Load(Scene &scene, const std::filesystem::path &path)
    {
        simdjson::padded_string json;
        if(const auto error = simdjson::padded_string::load(path.string()).get(json); error)
        {
            scene.mLogger->LogError("Failed to read scene '{}': {}", path.string(),
                                    simdjson::error_message(error));
            return false;
        }

        if(!Deserialize(scene, std::string_view(json.data(), json.size())))
        {
            return false;
        }

        scene.mLogger->LogInformation("Loaded scene from {}", path.string());
        return true;
    }

} // namespace FRIGGA_NAMESPACE
