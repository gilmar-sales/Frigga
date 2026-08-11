#include "ColliderDebugDraw.hpp"

#include "Frigga/ECS/Components/CharacterControllerComponent.hpp"
#include "Frigga/ECS/Components/MeshComponent.hpp"
#include "Frigga/ECS/Components/RigidBodyComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <numbers>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        glm::mat4 FlipYProjection(const glm::mat4 &vulkanProjection)
        {
            glm::mat4 projection = vulkanProjection;
            projection[1][1] *= -1.0f;
            return projection;
        }

        glm::mat4 BuildModel(const TransformComponent &transform)
        {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position);
            model           = model * glm::mat4_cast(transform.rotation);
            model           = glm::scale(model, transform.scale);
            return model;
        }

        /// Project a segment, clipping against the near plane when one endpoint is behind.
        void DrawSegment(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                         const ImVec2 &imageSize, const glm::vec3 &a, const glm::vec3 &b,
                         ImU32 color, float thickness)
        {
            glm::vec4 ca = viewProj * glm::vec4(a, 1.0f);
            glm::vec4 cb = viewProj * glm::vec4(b, 1.0f);
            constexpr float kNearW = 1e-4f;

            if(ca.w <= kNearW && cb.w <= kNearW)
            {
                return;
            }

            if(ca.w <= kNearW || cb.w <= kNearW)
            {
                const float t = (kNearW - ca.w) / (cb.w - ca.w);
                const glm::vec4 clipped = ca + t * (cb - ca);
                if(ca.w <= kNearW)
                {
                    ca = clipped;
                }
                else
                {
                    cb = clipped;
                }
            }

            const float invWa = 1.0f / ca.w;
            const float invWb = 1.0f / cb.w;
            const ImVec2 sa {imageMin.x + (ca.x * invWa * 0.5f + 0.5f) * imageSize.x,
                             imageMin.y + (1.0f - (ca.y * invWa * 0.5f + 0.5f)) * imageSize.y};
            const ImVec2 sb {imageMin.x + (cb.x * invWb * 0.5f + 0.5f) * imageSize.x,
                             imageMin.y + (1.0f - (cb.y * invWb * 0.5f + 0.5f)) * imageSize.y};
            drawList->AddLine(sa, sb, color, thickness);
        }

        ImU32 ColorForMotion(BodyMotionType motion)
        {
            switch(motion)
            {
            case BodyMotionType::Static:
                return IM_COL32(80, 220, 120, 220);
            case BodyMotionType::Kinematic:
                return IM_COL32(80, 180, 255, 220);
            case BodyMotionType::Dynamic:
                return IM_COL32(255, 200, 70, 220);
            }
            return IM_COL32(255, 255, 255, 220);
        }

        ImU32 AdjustColor(ImU32 color, bool selected, bool inactive)
        {
            if(selected)
            {
                return IM_COL32(255, 230, 90, 255);
            }
            if(inactive)
            {
                const auto rgb = color & 0x00FFFFFFu;
                return rgb | (90u << 24);
            }
            return color;
        }

        void DrawBox(ImDrawList *drawList, const glm::mat4 &model, const glm::mat4 &viewProj,
                     const ImVec2 &imageMin, const ImVec2 &imageSize, const glm::vec3 &halfExtents,
                     ImU32 color, float thickness)
        {
            const glm::vec3 corners[8] = {
                {-halfExtents.x, -halfExtents.y, -halfExtents.z},
                { halfExtents.x, -halfExtents.y, -halfExtents.z},
                { halfExtents.x,  halfExtents.y, -halfExtents.z},
                {-halfExtents.x,  halfExtents.y, -halfExtents.z},
                {-halfExtents.x, -halfExtents.y,  halfExtents.z},
                { halfExtents.x, -halfExtents.y,  halfExtents.z},
                { halfExtents.x,  halfExtents.y,  halfExtents.z},
                {-halfExtents.x,  halfExtents.y,  halfExtents.z},
            };

            glm::vec3 world[8];
            for(int i = 0; i < 8; ++i)
            {
                world[i] = glm::vec3(model * glm::vec4(corners[i], 1.0f));
            }

            static constexpr int edges[12][2] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
            };
            for(const auto &edge : edges)
            {
                DrawSegment(drawList, viewProj, imageMin, imageSize, world[edge[0]],
                            world[edge[1]], color, thickness);
            }
        }

        void DrawSphere(ImDrawList *drawList, const glm::mat4 &model, const glm::mat4 &viewProj,
                        const ImVec2 &imageMin, const ImVec2 &imageSize, float radius, ImU32 color,
                        float thickness)
        {
            constexpr int segments = 24;
            for(int axis = 0; axis < 3; ++axis)
            {
                for(int i = 0; i < segments; ++i)
                {
                    const float a0 =
                        (static_cast<float>(i) / segments) * 2.0f * std::numbers::pi_v<float>;
                    const float a1 =
                        (static_cast<float>(i + 1) / segments) * 2.0f * std::numbers::pi_v<float>;
                    glm::vec3 p0 {};
                    glm::vec3 p1 {};
                    if(axis == 0)
                    {
                        p0 = {0.0f, radius * std::cos(a0), radius * std::sin(a0)};
                        p1 = {0.0f, radius * std::cos(a1), radius * std::sin(a1)};
                    }
                    else if(axis == 1)
                    {
                        p0 = {radius * std::cos(a0), 0.0f, radius * std::sin(a0)};
                        p1 = {radius * std::cos(a1), 0.0f, radius * std::sin(a1)};
                    }
                    else
                    {
                        p0 = {radius * std::cos(a0), radius * std::sin(a0), 0.0f};
                        p1 = {radius * std::cos(a1), radius * std::sin(a1), 0.0f};
                    }
                    DrawSegment(drawList, viewProj, imageMin, imageSize,
                                glm::vec3(model * glm::vec4(p0, 1.0f)),
                                glm::vec3(model * glm::vec4(p1, 1.0f)), color, thickness);
                }
            }
        }

        void DrawCapsule(ImDrawList *drawList, const glm::mat4 &model, const glm::mat4 &viewProj,
                         const ImVec2 &imageMin, const ImVec2 &imageSize, float radius, float height,
                         ImU32 color, float thickness)
        {
            const float halfCylinder = std::max(0.5f * height, 0.001f);
            constexpr int segments   = 20;

            for(int i = 0; i < segments; ++i)
            {
                const float a0 =
                    (static_cast<float>(i) / segments) * 2.0f * std::numbers::pi_v<float>;
                const float a1 =
                    (static_cast<float>(i + 1) / segments) * 2.0f * std::numbers::pi_v<float>;
                const glm::vec3 top0 {radius * std::cos(a0), halfCylinder, radius * std::sin(a0)};
                const glm::vec3 top1 {radius * std::cos(a1), halfCylinder, radius * std::sin(a1)};
                const glm::vec3 bot0 {radius * std::cos(a0), -halfCylinder, radius * std::sin(a0)};
                const glm::vec3 bot1 {radius * std::cos(a1), -halfCylinder, radius * std::sin(a1)};

                DrawSegment(drawList, viewProj, imageMin, imageSize,
                            glm::vec3(model * glm::vec4(top0, 1.0f)),
                            glm::vec3(model * glm::vec4(top1, 1.0f)), color, thickness);
                DrawSegment(drawList, viewProj, imageMin, imageSize,
                            glm::vec3(model * glm::vec4(bot0, 1.0f)),
                            glm::vec3(model * glm::vec4(bot1, 1.0f)), color, thickness);
            }

            for(int i = 0; i < 4; ++i)
            {
                const float a = (static_cast<float>(i) / 4.0f) * 2.0f * std::numbers::pi_v<float>;
                const glm::vec3 top {radius * std::cos(a), halfCylinder, radius * std::sin(a)};
                const glm::vec3 bot {radius * std::cos(a), -halfCylinder, radius * std::sin(a)};
                DrawSegment(drawList, viewProj, imageMin, imageSize,
                            glm::vec3(model * glm::vec4(top, 1.0f)),
                            glm::vec3(model * glm::vec4(bot, 1.0f)), color, thickness);
            }

            for(int hemi = 0; hemi < 2; ++hemi)
            {
                const float yBase = hemi == 0 ? halfCylinder : -halfCylinder;
                const float ySign = hemi == 0 ? 1.0f : -1.0f;
                for(int plane = 0; plane < 2; ++plane)
                {
                    for(int i = 0; i < segments / 2; ++i)
                    {
                        const float a0 =
                            (static_cast<float>(i) / (segments / 2)) * std::numbers::pi_v<float>;
                        const float a1 = (static_cast<float>(i + 1) / (segments / 2)) *
                                         std::numbers::pi_v<float>;
                        glm::vec3 p0 {};
                        glm::vec3 p1 {};
                        if(plane == 0)
                        {
                            p0 = {radius * std::sin(a0), yBase + ySign * radius * std::cos(a0),
                                  0.0f};
                            p1 = {radius * std::sin(a1), yBase + ySign * radius * std::cos(a1),
                                  0.0f};
                        }
                        else
                        {
                            p0 = {0.0f, yBase + ySign * radius * std::cos(a0),
                                  radius * std::sin(a0)};
                            p1 = {0.0f, yBase + ySign * radius * std::cos(a1),
                                  radius * std::sin(a1)};
                        }
                        DrawSegment(drawList, viewProj, imageMin, imageSize,
                                    glm::vec3(model * glm::vec4(p0, 1.0f)),
                                    glm::vec3(model * glm::vec4(p1, 1.0f)), color, thickness);
                    }
                }
            }
        }

        void DrawMeshHull(ImDrawList *drawList, const glm::mat4 &model, const glm::mat4 &viewProj,
                          const ImVec2 &imageMin, const ImVec2 &imageSize,
                          const std::vector<glm::vec3> &points, ImU32 color, float thickness)
        {
            if(points.size() < 2)
            {
                return;
            }

            glm::vec3 minP = points[0];
            glm::vec3 maxP = points[0];
            for(const auto &p : points)
            {
                minP = glm::min(minP, p);
                maxP = glm::max(maxP, p);
            }
            const glm::vec3 half   = 0.5f * (maxP - minP);
            const glm::vec3 center = 0.5f * (maxP + minP);
            glm::mat4 centered     = model * glm::translate(glm::mat4(1.0f), center);
            DrawBox(drawList, centered, viewProj, imageMin, imageSize, half, color, thickness);
        }
    } // namespace

    void ColliderDebugDraw::Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                                 const skr::Arc<PrimitiveMeshFactory> &primitives,
                                 const glm::mat4 &view, const glm::mat4 &vulkanProjection,
                                 const ImVec2 &imageMin, const ImVec2 &imageSize,
                                 fr::Entity selectedEntity,
                                 const skr::Arc<IPhysicsWorld> &physicsWorld,
                                 bool dimInactiveBodies)
    {
        if(drawList == nullptr || imageSize.x < 1.0f || imageSize.y < 1.0f)
        {
            return;
        }

        const glm::mat4 proj     = FlipYProjection(vulkanProjection);
        const glm::mat4 viewProj = proj * view;

        registry->CreateMutation()->Each<TransformComponent, RigidBodyComponent>(
            [&](auto entity, TransformComponent &transform, RigidBodyComponent &rigidBody) {
                const bool selected = entity == selectedEntity;
                // Static/Kinematic are never "active" in Jolt — only dim sleeping Dynamics.
                bool inactive = false;
                if(dimInactiveBodies && physicsWorld && rigidBody.body.IsValid() &&
                   rigidBody.motion == BodyMotionType::Dynamic)
                {
                    inactive = !physicsWorld->IsBodyActive(rigidBody.body);
                }

                const ImU32 color =
                    AdjustColor(ColorForMotion(rigidBody.motion), selected, inactive);
                const float thickness = selected ? 3.0f : 1.5f;
                const glm::mat4 model = BuildModel(transform);

                switch(rigidBody.shape)
                {
                case ColliderShape::Box:
                    DrawBox(drawList, model, viewProj, imageMin, imageSize, rigidBody.halfExtents,
                            color, thickness);
                    break;
                case ColliderShape::Sphere:
                    DrawSphere(drawList, model, viewProj, imageMin, imageSize, rigidBody.radius,
                               color, thickness);
                    break;
                case ColliderShape::Capsule:
                    DrawCapsule(drawList, model, viewProj, imageMin, imageSize, rigidBody.radius,
                                rigidBody.height, color, thickness);
                    break;
                case ColliderShape::Mesh:
                {
                    PrimitiveType primitive = PrimitiveType::Cube;
                    bool found              = false;
                    registry->TryGetComponents<MeshComponent>(entity, [&](MeshComponent &mesh) {
                        found = primitives->TryFindPrimitive(mesh.meshId, primitive);
                    });
                    if(!found)
                    {
                        primitive = PrimitiveType::Cube;
                    }
                    DrawMeshHull(drawList, model, viewProj, imageMin, imageSize,
                                 PrimitiveMeshFactory::GetColliderHullPoints(primitive), color,
                                 thickness);
                    break;
                }
                }
            });

        // Character capsules: Transform is feet; CapsuleCenterLocal includes centerOffset.
        registry->CreateMutation()->Each<TransformComponent, CharacterControllerComponent>(
            [&](auto entity, TransformComponent &transform,
                CharacterControllerComponent &controller) {
                const bool selected   = entity == selectedEntity;
                const ImU32 color     = AdjustColor(IM_COL32(220, 120, 255, 220), selected, false);
                const float thickness = selected ? 3.0f : 1.5f;

                const float radius = std::max(controller.radius, 0.001f);
                const glm::vec3 center =
                    transform.position + transform.rotation * controller.CapsuleCenterLocal();

                glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
                model           = model * glm::mat4_cast(transform.rotation);
                DrawCapsule(drawList, model, viewProj, imageMin, imageSize, radius,
                            controller.height, color, thickness);
            });
    }

} // namespace FRIGGA_NAMESPACE
