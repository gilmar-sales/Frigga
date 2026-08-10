#include "SceneSerializer.hpp"

#include "Frigga/ECS/Components/AnimatorComponent.hpp"
#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/MaterialComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/NameComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/Components/UserDataComponent.hpp"
#include "Frigga/ECS/UserComponentRegistry.hpp"

#define SIMDJSON_STATIC_REFLECTION 1
#include <simdjson.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr int64_t kSceneVersion = 4;

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
            std::optional<std::string> primitive;
            std::optional<std::string> source;
            std::optional<int64_t>     index;
            std::optional<bool>        castShadows;
        };

        struct SceneMaterialDto
        {
            [[= simdjson::rename<"default">]] bool isDefault = true;
            std::optional<std::string>             albedo;
            std::optional<std::string>             normal;
            std::optional<std::string>             roughness;
            std::optional<std::string>             emissive;
            std::optional<std::string>             metalness;
            std::optional<std::vector<float>>      albedoFactor;
            std::optional<float>                   roughnessFactor;
            std::optional<float>                   metalnessFactor;
            std::optional<std::vector<float>>      emissiveFactor;
            std::optional<float>                   aoFactor;
            std::optional<float>                   alphaCutoff;
            std::optional<std::string>             alphaMode;
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
            // Optional for forward-compat with fixtures that predate these fields.
            std::optional<float> halfWidth;
            std::optional<float> halfHeight;
            std::optional<bool>  castShadows;
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

        struct SceneAnimatorDto
        {
            std::string          modelSource;
            std::optional<std::string> clipName;
            std::optional<float> timeSec;
            std::optional<float> speed;
            std::optional<bool>  playing;
            std::optional<bool>  loop;
            std::optional<bool>  useGpu;
            std::optional<bool>  previewInEdit;
        };

        struct ScenePropertyDto
        {
            std::string                name;
            std::string                kind {"Float"};
            std::optional<bool>        boolValue;
            std::optional<int64_t>     intValue;
            std::optional<float>       floatValue;
            std::optional<std::string> stringValue;
            std::optional<std::vector<float>> vec;
        };

        struct SceneUserComponentDto
        {
            std::string                  typeId;
            std::vector<ScenePropertyDto> properties;
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
            std::optional<SceneAnimatorDto>    animator;
            std::optional<std::vector<SceneUserComponentDto>> userComponents;
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

        bool ReadVec4(const std::vector<float> &values, glm::vec4 &out)
        {
            if(values.size() == 4)
            {
                out = {values[0], values[1], values[2], values[3]};
                return true;
            }
            if(values.size() >= 8 && values.size() % 4 == 0)
            {
                const auto base = values.size() - 4;
                out = {values[base], values[base + 1], values[base + 2], values[base + 3]};
                return true;
            }
            return false;
        }

        bool ReadVec2(const std::vector<float> &values, glm::vec2 &out)
        {
            if(values.size() == 2)
            {
                out = {values[0], values[1]};
                return true;
            }
            if(values.size() >= 4 && values.size() % 2 == 0)
            {
                const auto base = values.size() - 2;
                out = {values[base], values[base + 1]};
                return true;
            }
            return false;
        }

        const char *PropertyKindToString(PropertyKind kind)
        {
            switch(kind)
            {
            case PropertyKind::Bool:
                return "Bool";
            case PropertyKind::Int64:
                return "Int64";
            case PropertyKind::Float:
                return "Float";
            case PropertyKind::String:
                return "String";
            case PropertyKind::Vec2:
                return "Vec2";
            case PropertyKind::Vec3:
                return "Vec3";
            case PropertyKind::Vec4:
                return "Vec4";
            }
            return "Float";
        }

        bool TryParsePropertyKind(std::string_view name, PropertyKind &out)
        {
            if(name == "Bool")
            {
                out = PropertyKind::Bool;
                return true;
            }
            if(name == "Int64")
            {
                out = PropertyKind::Int64;
                return true;
            }
            if(name == "Float")
            {
                out = PropertyKind::Float;
                return true;
            }
            if(name == "String")
            {
                out = PropertyKind::String;
                return true;
            }
            if(name == "Vec2")
            {
                out = PropertyKind::Vec2;
                return true;
            }
            if(name == "Vec3")
            {
                out = PropertyKind::Vec3;
                return true;
            }
            if(name == "Vec4")
            {
                out = PropertyKind::Vec4;
                return true;
            }
            return false;
        }

        ScenePropertyDto PropertyToDto(const NamedProperty &property)
        {
            ScenePropertyDto dto {.name = property.name,
                                  .kind = PropertyKindToString(property.value.kind)};
            switch(property.value.kind)
            {
            case PropertyKind::Bool:
                dto.boolValue = property.value.boolValue;
                break;
            case PropertyKind::Int64:
                dto.intValue = property.value.intValue;
                break;
            case PropertyKind::Float:
                dto.floatValue = property.value.floatValue;
                break;
            case PropertyKind::String:
                dto.stringValue = property.value.stringValue;
                break;
            case PropertyKind::Vec2:
                dto.vec = std::vector<float> {property.value.vec2Value.x,
                                              property.value.vec2Value.y};
                break;
            case PropertyKind::Vec3:
                dto.vec = std::vector<float> {property.value.vec3Value.x,
                                              property.value.vec3Value.y,
                                              property.value.vec3Value.z};
                break;
            case PropertyKind::Vec4:
                dto.vec = std::vector<float> {property.value.vec4Value.x,
                                              property.value.vec4Value.y,
                                              property.value.vec4Value.z,
                                              property.value.vec4Value.w};
                break;
            }
            return dto;
        }

        bool PropertyFromDto(const ScenePropertyDto &dto, NamedProperty &out)
        {
            PropertyKind kind = PropertyKind::Float;
            if(!TryParsePropertyKind(dto.kind, kind))
            {
                return false;
            }

            NamedProperty property {.name = dto.name};
            property.value.kind = kind;
            switch(kind)
            {
            case PropertyKind::Bool:
                property.value.boolValue = dto.boolValue.value_or(false);
                break;
            case PropertyKind::Int64:
                property.value.intValue = dto.intValue.value_or(0);
                break;
            case PropertyKind::Float:
                property.value.floatValue = dto.floatValue.value_or(0.0f);
                break;
            case PropertyKind::String:
                property.value.stringValue = dto.stringValue.value_or(std::string {});
                break;
            case PropertyKind::Vec2:
            {
                glm::vec2 vec {0.0f};
                if(dto.vec && !dto.vec->empty() && !ReadVec2(*dto.vec, vec))
                {
                    return false;
                }
                property.value.vec2Value = vec;
                break;
            }
            case PropertyKind::Vec3:
            {
                glm::vec3 vec {0.0f};
                if(dto.vec && !dto.vec->empty() && !ReadVec3(*dto.vec, vec))
                {
                    return false;
                }
                property.value.vec3Value = vec;
                break;
            }
            case PropertyKind::Vec4:
            {
                glm::vec4 vec {0.0f};
                if(dto.vec && !dto.vec->empty() && !ReadVec4(*dto.vec, vec))
                {
                    return false;
                }
                property.value.vec4Value = vec;
                break;
            }
            }
            out = std::move(property);
            return true;
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

        std::optional<std::string> TexturePathOrNull(const skr::Arc<AssetRegistry> &assets,
                                                     std::optional<std::uint32_t> textureId)
        {
            if(!textureId || !assets)
            {
                return std::nullopt;
            }
            std::string path;
            if(!assets->TryGetTexturePath(*textureId, path))
            {
                return std::nullopt;
            }
            return path;
        }

        SceneMaterialDto MaterialToDto(const skr::Arc<PrimitiveMeshFactory> &primitives,
                                       const skr::Arc<AssetRegistry> &assets,
                                       std::uint32_t materialId)
        {
            if(materialId == primitives->GetDefaultMaterial())
            {
                return SceneMaterialDto {.isDefault = true};
            }

            const auto info = primitives->GetMaterialCreateInfo(materialId);
            SceneMaterialDto dto {.isDefault = false};
            dto.albedo    = TexturePathOrNull(assets, info.albedo);
            dto.normal    = TexturePathOrNull(assets, info.normal);
            dto.roughness = TexturePathOrNull(assets, info.roughness);
            dto.emissive  = TexturePathOrNull(assets, info.emissive);
            dto.metalness = TexturePathOrNull(assets, info.metalness);
            dto.albedoFactor = std::vector<float> {info.albedoFactor.x, info.albedoFactor.y,
                                                   info.albedoFactor.z, info.albedoFactor.w};
            dto.roughnessFactor = info.roughnessFactor;
            dto.metalnessFactor = info.metalnessFactor;
            dto.emissiveFactor =
                std::vector<float> {info.emissiveFactor.x, info.emissiveFactor.y,
                                    info.emissiveFactor.z};
            dto.aoFactor     = info.aoFactor;
            dto.alphaCutoff  = info.alphaCutoff;
            switch(info.alphaMode)
            {
            case fra::AlphaMode::Mask:
                dto.alphaMode = "mask";
                break;
            case fra::AlphaMode::Blend:
                dto.alphaMode = "blend";
                break;
            case fra::AlphaMode::Opaque:
            default:
                dto.alphaMode = "opaque";
                break;
            }
            return dto;
        }

        bool MaterialFromDto(const skr::Arc<PrimitiveMeshFactory> &primitives,
                             const skr::Arc<AssetRegistry> &assets,
                             const skr::Arc<skr::Logger<Scene>> &logger,
                             const SceneMaterialDto &dto, MaterialComponent &outMaterial)
        {
            if(dto.isDefault)
            {
                outMaterial.materialId = primitives->GetDefaultMaterial();
                return true;
            }

            fra::MaterialCreateInfo info {};
            auto loadSlot = [&](const std::optional<std::string> &path,
                                std::optional<std::uint32_t> &slot) {
                if(!path || path->empty())
                {
                    return true;
                }
                std::uint32_t textureId = 0;
                if(!assets || !assets->TryGetTextureId(*path, textureId))
                {
                    logger->LogError("Failed to load material texture '{}'", *path);
                    return false;
                }
                slot = textureId;
                return true;
            };

            if(!loadSlot(dto.albedo, info.albedo) || !loadSlot(dto.normal, info.normal) ||
               !loadSlot(dto.roughness, info.roughness) || !loadSlot(dto.emissive, info.emissive) ||
               !loadSlot(dto.metalness, info.metalness))
            {
                return false;
            }

            if(dto.albedoFactor)
            {
                const auto &v = *dto.albedoFactor;
                if(v.size() == 3)
                {
                    info.albedoFactor = {v[0], v[1], v[2], 1.0f};
                }
                else if(v.size() >= 4)
                {
                    info.albedoFactor = {v[0], v[1], v[2], v[3]};
                }
            }
            if(dto.emissiveFactor)
            {
                const auto &v = *dto.emissiveFactor;
                if(v.size() >= 3)
                {
                    info.emissiveFactor = {v[0], v[1], v[2]};
                }
            }
            if(dto.roughnessFactor)
            {
                info.roughnessFactor = *dto.roughnessFactor;
            }
            if(dto.metalnessFactor)
            {
                info.metalnessFactor = *dto.metalnessFactor;
            }
            if(dto.aoFactor)
            {
                info.aoFactor = *dto.aoFactor;
            }
            if(dto.alphaCutoff)
            {
                info.alphaCutoff = *dto.alphaCutoff;
            }
            if(dto.alphaMode)
            {
                if(*dto.alphaMode == "mask")
                {
                    info.alphaMode = fra::AlphaMode::Mask;
                }
                else if(*dto.alphaMode == "blend")
                {
                    info.alphaMode = fra::AlphaMode::Blend;
                }
                else
                {
                    info.alphaMode = fra::AlphaMode::Opaque;
                }
            }

            if(assets)
            {
                outMaterial.materialId = assets->CreateMaterial(info, {}, false);
            }
            else
            {
                outMaterial.materialId = primitives->CreateMaterial(info);
            }
            return true;
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
                SceneMeshDto meshDto {.castShadows = mesh.castShadows};

                PrimitiveType primitive = PrimitiveType::Cube;
                if(primitives->TryFindPrimitive(mesh.meshId, primitive))
                {
                    meshDto.primitive = PrimitiveMeshFactory::GetDisplayName(primitive);
                }
                else
                {
                    ModelAsset model {};
                    std::uint32_t submesh = 0;
                    if(scene.mAssets &&
                       scene.mAssets->TryFindModelByMeshId(mesh.meshId, model, submesh))
                    {
                        meshDto.source = model.relativePath;
                        meshDto.index  = static_cast<int64_t>(submesh);
                    }
                    else
                    {
                        scene.mLogger->LogWarning(
                            "Scene save: meshId {} on '{}' is not a known primitive or imported "
                            "model; defaulting to Cube",
                            mesh.meshId, name.name);
                        meshDto.primitive = PrimitiveMeshFactory::GetDisplayName(PrimitiveType::Cube);
                    }
                }

                dto.mesh = std::move(meshDto);
            });

            registry->TryGetComponents<MaterialComponent>(entity, [&](MaterialComponent &material) {
                dto.material = MaterialToDto(primitives, scene.mAssets, material.materialId);
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
                    .halfWidth         = light.halfWidth,
                    .halfHeight        = light.halfHeight,
                    .castShadows       = light.castShadows,
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

            registry->TryGetComponents<AnimatorComponent>(entity, [&](AnimatorComponent &animator) {
                dto.animator = SceneAnimatorDto {
                    .modelSource   = animator.modelSource,
                    .clipName      = animator.clipName,
                    .timeSec       = animator.timeSec,
                    .speed         = animator.speed,
                    .playing       = animator.playing,
                    .loop          = animator.loop,
                    .useGpu        = animator.useGpu,
                    .previewInEdit = animator.previewInEdit,
                };
            });

            if(scene.mUserComponents)
            {
                std::vector<SceneUserComponentDto> components;
                for(const auto &ops : scene.mUserComponents->GetTypes())
                {
                    if(!ops.has || !ops.has(*registry, entity) || !ops.toInstance)
                    {
                        continue;
                    }
                    UserComponentInstance instance {};
                    if(!ops.toInstance(*registry, entity, instance))
                    {
                        continue;
                    }
                    SceneUserComponentDto userDto {.typeId = instance.typeId};
                    userDto.properties.reserve(instance.properties.size());
                    for(const auto &property : instance.properties)
                    {
                        userDto.properties.push_back(PropertyToDto(property));
                    }
                    components.push_back(std::move(userDto));
                }

                // Preserve gameplay data still waiting for the plugin to register types.
                for(const auto &deferred : scene.mUserComponents->GetDeferredForEntity(entity))
                {
                    const bool already =
                        std::ranges::any_of(components, [&](const SceneUserComponentDto &dto) {
                            return dto.typeId == deferred.instance.typeId;
                        });
                    if(already)
                    {
                        continue;
                    }
                    SceneUserComponentDto userDto {.typeId = deferred.instance.typeId};
                    userDto.properties.reserve(deferred.instance.properties.size());
                    for(const auto &property : deferred.instance.properties)
                    {
                        userDto.properties.push_back(PropertyToDto(property));
                    }
                    components.push_back(std::move(userDto));
                }

                if(!components.empty())
                {
                    dto.userComponents = std::move(components);
                }
            }

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
            NameComponent name {.name = entityDto.name};

            std::optional<TransformComponent> transform;
            if(entityDto.transform)
            {
                TransformComponent parsed {};
                if(!FromDto(*entityDto.transform, parsed))
                {
                    scene.mLogger->LogError("Invalid transform for entity '{}'", entityDto.name);
                    return false;
                }
                transform = parsed;
            }

            std::optional<MeshComponent> mesh;
            std::optional<MaterialComponent> material;
            if(entityDto.mesh)
            {
                const auto &meshDto = *entityDto.mesh;
                std::uint32_t meshId = 0;
                bool resolved        = false;

                if(meshDto.source && !meshDto.source->empty())
                {
                    const auto submesh = static_cast<std::uint32_t>(meshDto.index.value_or(0));
                    if(!scene.mAssets ||
                       !scene.mAssets->TryGetMeshId(*meshDto.source, submesh, meshId))
                    {
                        scene.mLogger->LogError(
                            "Failed to load mesh source '{}' (index {}) on entity '{}'",
                            *meshDto.source, submesh, entityDto.name);
                        return false;
                    }
                    resolved = true;
                }
                else
                {
                    const std::string primitiveName =
                        meshDto.primitive.value_or(std::string {"Cube"});
                    PrimitiveType primitive = PrimitiveType::Cube;
                    if(!PrimitiveMeshFactory::TryParsePrimitive(primitiveName, primitive))
                    {
                        scene.mLogger->LogError("Unknown primitive '{}' on entity '{}'",
                                                primitiveName, entityDto.name);
                        return false;
                    }
                    meshId   = primitives->GetMesh(primitive);
                    resolved = true;
                }

                if(!resolved)
                {
                    scene.mLogger->LogError("Mesh on entity '{}' has neither primitive nor source",
                                            entityDto.name);
                    return false;
                }

                mesh = MeshComponent {.meshId = meshId,
                                      .castShadows = meshDto.castShadows.value_or(true)};

                if(entityDto.material)
                {
                    MaterialComponent parsed {};
                    if(!MaterialFromDto(primitives, scene.mAssets, scene.mLogger,
                                        *entityDto.material, parsed))
                    {
                        return false;
                    }
                    material = parsed;
                }
                else
                {
                    material =
                        MaterialComponent {.materialId = primitives->GetDefaultMaterial()};
                }
            }
            else if(entityDto.material)
            {
                MaterialComponent parsed {};
                if(!MaterialFromDto(primitives, scene.mAssets, scene.mLogger, *entityDto.material,
                                    parsed))
                {
                    return false;
                }
                material = parsed;
            }

            std::optional<CameraComponent> camera;
            if(entityDto.camera)
            {
                const auto &cameraDto = *entityDto.camera;
                camera                = CameraComponent {
                                   .fovDegrees = cameraDto.fovDegrees,
                                   .nearPlane  = cameraDto.nearPlane,
                                   .farPlane   = cameraDto.farPlane,
                                   .primary    = cameraDto.primary,
                                   .locked     = cameraDto.locked,
                };
            }

            std::optional<LightComponent> light;
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

                light = LightComponent {
                    .type              = type,
                    .color             = color,
                    .radius            = lightDto.radius,
                    .intensity         = lightDto.intensity,
                    .innerAngleDegrees = lightDto.innerAngleDegrees,
                    .outerAngleDegrees = lightDto.outerAngleDegrees,
                    .halfWidth         = lightDto.halfWidth.value_or(1.0f),
                    .halfHeight        = lightDto.halfHeight.value_or(1.0f),
                    .castShadows       = lightDto.castShadows.value_or(false),
                };
            }

            std::optional<RigidBodyComponent> rigidBody;
            if(entityDto.rigidBody)
            {
                const auto &rbDto = *entityDto.rigidBody;
                RigidBodyComponent rb {};
                if(!TryParseMotion(rbDto.motion, rb.motion))
                {
                    scene.mLogger->LogError("Unknown rigid body motion '{}' on '{}'", rbDto.motion,
                                            entityDto.name);
                    return false;
                }
                if(!TryParseShape(rbDto.shape, rb.shape))
                {
                    scene.mLogger->LogError("Unknown collider shape '{}' on '{}'", rbDto.shape,
                                            entityDto.name);
                    return false;
                }
                if(rbDto.halfExtents.empty())
                {
                    rb.halfExtents = {0.5f, 0.5f, 0.5f};
                }
                else if(!ReadVec3(rbDto.halfExtents, rb.halfExtents))
                {
                    scene.mLogger->LogError("Invalid halfExtents on '{}'", entityDto.name);
                    return false;
                }
                rb.radius            = rbDto.radius;
                rb.height            = rbDto.height;
                rb.mass              = rbDto.mass;
                rb.friction          = rbDto.friction;
                rb.restitution       = rbDto.restitution;
                rb.collisionLayer    = static_cast<std::uint8_t>(
                    std::clamp<int64_t>(rbDto.collisionLayer, 0, 15));
                rb.collideWithLayers = static_cast<std::uint16_t>(
                    std::clamp<int64_t>(rbDto.collideWithLayers, 0, 0xffff));
                rigidBody = rb;
            }

            std::optional<AnimatorComponent> animator;
            if(entityDto.animator)
            {
                const auto &animDto = *entityDto.animator;
                if(animDto.modelSource.empty())
                {
                    scene.mLogger->LogError("Animator on '{}' missing modelSource", entityDto.name);
                    return false;
                }
                if(scene.mAssets)
                {
                    (void)scene.mAssets->LoadModel(animDto.modelSource);
                }
                animator = AnimatorComponent {
                    .modelSource   = animDto.modelSource,
                    .clipName      = animDto.clipName.value_or(std::string {}),
                    .timeSec       = animDto.timeSec.value_or(0.0f),
                    .speed         = animDto.speed.value_or(1.0f),
                    .playing       = animDto.playing.value_or(true),
                    .loop          = animDto.loop.value_or(true),
                    .useGpu        = animDto.useGpu.value_or(false),
                    .previewInEdit = animDto.previewInEdit.value_or(true),
                };
            }

            // Prefer a single CreateEntity(Name, ...) so Freyr writes one archetype row.
            // Piecewise AddComponents without per-step flush corrupt deferred migrations.
            fr::Entity entity {};
            const bool hasT  = transform.has_value();
            const bool hasM  = mesh.has_value();
            const bool hasMat = material.has_value();
            const bool hasC  = camera.has_value();
            const bool hasL  = light.has_value();
            const bool hasR  = rigidBody.has_value();
            const bool hasA  = animator.has_value();

            if(hasT && hasM && hasMat && !hasC && !hasL && hasR && hasA)
            {
                entity = registry->CreateEntity(name, *transform, *mesh, *material, *rigidBody,
                                                *animator);
            }
            else if(hasT && hasM && hasMat && !hasC && !hasL && !hasR && hasA)
            {
                entity = registry->CreateEntity(name, *transform, *mesh, *material, *animator);
            }
            else if(hasT && hasM && hasMat && !hasC && !hasL && hasR && !hasA)
            {
                entity = registry->CreateEntity(name, *transform, *mesh, *material, *rigidBody);
            }
            else if(hasT && hasM && hasMat && !hasC && !hasL && !hasR && !hasA)
            {
                entity = registry->CreateEntity(name, *transform, *mesh, *material);
            }
            else if(hasT && !hasM && !hasMat && hasC && !hasL && !hasR && !hasA)
            {
                entity = registry->CreateEntity(name, *transform, *camera);
            }
            else if(hasT && !hasM && !hasMat && !hasC && hasL && !hasR && !hasA)
            {
                entity = registry->CreateEntity(name, *transform, *light);
            }
            else if(hasT && !hasM && !hasMat && !hasC && !hasL && !hasR && !hasA)
            {
                entity = registry->CreateEntity(name, *transform);
            }
            else if(!hasT && !hasM && !hasMat && !hasC && !hasL && !hasR && !hasA)
            {
                entity = registry->CreateEntity(name);
            }
            else
            {
                // Uncommon combo: Name + flush between each attach.
                entity = registry->CreateEntity(name);
                scene.FlushEcs();
                const auto attach = [&](const auto &... comps) {
                    registry->AddComponents(entity, comps...);
                    scene.FlushEcs();
                };
                if(hasT)
                {
                    attach(*transform);
                }
                if(hasM && hasMat)
                {
                    attach(*mesh, *material);
                }
                else
                {
                    if(hasM)
                    {
                        attach(*mesh);
                    }
                    if(hasMat)
                    {
                        attach(*material);
                    }
                }
                if(hasC)
                {
                    attach(*camera);
                }
                if(hasL)
                {
                    attach(*light);
                }
                if(hasR)
                {
                    attach(*rigidBody);
                }
                if(hasA)
                {
                    attach(*animator);
                }
            }

            scene.FlushEcs();

            // Plugin gameplay components (Freyr SoA) via type-erased ops.
            if(entityDto.userComponents && !entityDto.userComponents->empty())
            {
                if(!scene.mUserComponents)
                {
                    scene.mLogger->LogWarning(
                        "Entity '{}' has userComponents but no UserComponentRegistry; skipped",
                        entityDto.name);
                }
                else
                {
                    for(const auto &userDto : *entityDto.userComponents)
                    {
                        if(userDto.typeId.empty())
                        {
                            scene.mLogger->LogError("userComponents entry on '{}' missing typeId",
                                                    entityDto.name);
                            return false;
                        }
                        const auto ops = scene.mUserComponents->Find(userDto.typeId);
                        UserComponentInstance instance {.typeId = userDto.typeId};
                        instance.properties.reserve(userDto.properties.size());
                        for(const auto &propertyDto : userDto.properties)
                        {
                            NamedProperty property {};
                            if(!PropertyFromDto(propertyDto, property))
                            {
                                scene.mLogger->LogError(
                                    "Invalid user component property '{}' on '{}' ({})",
                                    propertyDto.name, entityDto.name, userDto.typeId);
                                return false;
                            }
                            instance.properties.push_back(std::move(property));
                        }

                        if(!ops || !ops->fromInstance)
                        {
                            scene.mUserComponents->EnqueueDeferred(entity, std::move(instance));
                            scene.mLogger->LogWarning(
                                "Deferred gameplay component '{}' on '{}' until the plugin "
                                "registers it",
                                userDto.typeId, entityDto.name);
                            continue;
                        }

                        ops->fromInstance(*registry, entity, instance);
                        scene.FlushEcs();
                    }
                }
            }

            if(camera)
            {
                if(camera->locked)
                {
                    scene.mMainCameraEntity = entity;
                    foundLockedCamera       = true;
                }
                else if(camera->primary && !foundPrimaryCamera)
                {
                    firstPrimaryCamera = entity;
                    foundPrimaryCamera = true;
                }
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
