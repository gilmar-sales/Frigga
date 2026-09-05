#include <Frigga/Editor/CameraDebugDraw.hpp>

#include "Frigga/ECS/Components/CameraComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Editor/EditorIconBillboard.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

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

        bool Project(const glm::vec3 &world, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                     const ImVec2 &imageSize, ImVec2 &out)
        {
            const glm::vec4 clip = viewProj * glm::vec4(world, 1.0f);
            if(clip.w <= 1e-4f)
            {
                return false;
            }

            const float invW = 1.0f / clip.w;
            const float ndcX = clip.x * invW;
            const float ndcY = clip.y * invW;
            out.x            = imageMin.x + (ndcX * 0.5f + 0.5f) * imageSize.x;
            out.y            = imageMin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * imageSize.y;
            return true;
        }

        void DrawSegment(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                         const ImVec2 &imageSize, const glm::vec3 &a, const glm::vec3 &b,
                         ImU32 color, float thickness)
        {
            ImVec2 sa {};
            ImVec2 sb {};
            if(!Project(a, viewProj, imageMin, imageSize, sa) ||
               !Project(b, viewProj, imageMin, imageSize, sb))
            {
                return;
            }
            drawList->AddLine(sa, sb, color, thickness);
        }

        std::array<glm::vec3, 4> FrustumCornersLocal(float zDist, float fovYRadians, float aspect)
        {
            const float hy = std::tan(fovYRadians * 0.5f) * zDist;
            const float hx = hy * aspect;
            return {
                glm::vec3 { hx,  hy, -zDist},
                glm::vec3 {-hx,  hy, -zDist},
                glm::vec3 {-hx, -hy, -zDist},
                glm::vec3 { hx, -hy, -zDist},
            };
        }

        void DrawFrustum(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                         const ImVec2 &imageSize, const TransformComponent &transform,
                         const CameraComponent &camera, float aspect, ImU32 color, float thickness)
        {
            const float nearZ = std::max(camera.nearPlane, 1e-3f);
            // Keep the far plane readable in the default scene (far=1000 is huge on screen).
            const float farZ =
                std::clamp(camera.farPlane, nearZ + 1e-2f, std::max(nearZ + 1.0f, 25.0f));
            const float fovY = glm::radians(std::clamp(camera.fovDegrees, 1.0f, 179.0f));
            const float safeAspect =
                aspect > 1e-3f ? aspect : (16.0f / 9.0f);

            const auto nearLocal = FrustumCornersLocal(nearZ, fovY, safeAspect);
            const auto farLocal  = FrustumCornersLocal(farZ, fovY, safeAspect);

            std::array<glm::vec3, 4> nearW {};
            std::array<glm::vec3, 4> farW {};
            for(int i = 0; i < 4; ++i)
            {
                nearW[i] = transform.position + transform.rotation * nearLocal[i];
                farW[i]  = transform.position + transform.rotation * farLocal[i];
            }

            for(int i = 0; i < 4; ++i)
            {
                const int j = (i + 1) % 4;
                DrawSegment(drawList, viewProj, imageMin, imageSize, nearW[i], nearW[j], color,
                            thickness);
                DrawSegment(drawList, viewProj, imageMin, imageSize, farW[i], farW[j], color,
                            thickness);
                DrawSegment(drawList, viewProj, imageMin, imageSize, nearW[i], farW[i], color,
                            thickness);
            }

            const glm::vec3 forward =
                glm::normalize(transform.rotation * glm::vec3 {0.0f, 0.0f, -1.0f});
            if(glm::dot(forward, forward) > 1e-6f)
            {
                const glm::vec3 tip = transform.position + forward * (nearZ * 0.5f);
                DrawSegment(drawList, viewProj, imageMin, imageSize, transform.position, tip, color,
                            thickness + 0.5f);

                // Eye cross at the camera position.
                const float s = nearZ * 0.35f;
                const glm::vec3 right =
                    glm::normalize(transform.rotation * glm::vec3 {1.0f, 0.0f, 0.0f});
                const glm::vec3 up =
                    glm::normalize(transform.rotation * glm::vec3 {0.0f, 1.0f, 0.0f});
                DrawSegment(drawList, viewProj, imageMin, imageSize,
                            transform.position - right * s, transform.position + right * s, color,
                            thickness);
                DrawSegment(drawList, viewProj, imageMin, imageSize, transform.position - up * s,
                            transform.position + up * s, color, thickness);
            }
        }
    } // namespace

    void CameraDebugDraw::Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                               const glm::mat4 &view, const glm::mat4 &vulkanProjection,
                               const ImVec2 &imageMin, const ImVec2 &imageSize,
                               fr::Entity selectedEntity, fr::Entity mainCameraEntity)
    {
        if(drawList == nullptr || imageSize.x < 1.0f || imageSize.y < 1.0f)
        {
            return;
        }

        const glm::mat4 viewProj = FlipYProjection(vulkanProjection) * view;
        const float aspect       = imageSize.x / imageSize.y;

        // Draw every scene camera so gizmos are visible and pickable; highlight selection.
        registry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &transform, CameraComponent &camera) {
                const auto pose = TransformUtil::WorldPose(*registry, entity);
                TransformComponent worldXf = transform;
                worldXf.position           = pose.position;
                worldXf.rotation           = pose.rotation;
                worldXf.scale              = pose.scale;
                const bool selected = entity == selectedEntity;
                const bool isMain   = entity == mainCameraEntity;
                ImU32 color         = IM_COL32(100, 140, 180, 110);
                float thickness     = 1.1f;
                if(selected)
                {
                    color     = IM_COL32(80, 200, 255, 255);
                    thickness = 2.5f;
                }
                else if(isMain)
                {
                    color     = IM_COL32(120, 160, 200, 160);
                    thickness = 1.35f;
                }
                DrawFrustum(drawList, viewProj, imageMin, imageSize, worldXf, camera, aspect,
                            color, thickness);

                ImVec2 screen {};
                if(Project(worldXf.position, viewProj, imageMin, imageSize, screen))
                {
                    EditorIconBillboard::Draw(drawList, screen, EditorIconBillboard::kCameraVideo,
                                              selected ? IM_COL32(120, 210, 255, 255)
                                                       : (isMain ? IM_COL32(140, 180, 220, 230)
                                                                 : IM_COL32(110, 150, 190, 210)),
                                              selected);
                }
            });
    }

    std::optional<fr::Entity> CameraDebugDraw::HitTest(const skr::Arc<fr::Registry> &registry,
                                                       const glm::mat4 &view,
                                                       const glm::mat4 &vulkanProjection,
                                                       const ImVec2 &imageMin,
                                                       const ImVec2 &imageSize, const ImVec2 &mouse,
                                                       float pixelRadius)
    {
        if(imageSize.x < 1.0f || imageSize.y < 1.0f)
        {
            return std::nullopt;
        }

        const glm::mat4 viewProj = FlipYProjection(vulkanProjection) * view;
        const float scaledRadius = EditorIconBillboard::Dpi(pixelRadius);
        const float radiusSq     = scaledRadius * scaledRadius;

        fr::Entity bestEntity {};
        float bestDistSq = std::numeric_limits<float>::max();
        bool found       = false;

        registry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &transform, CameraComponent &camera) {
                const auto pose = TransformUtil::WorldPose(*registry, entity);
                TransformComponent worldXf = transform;
                worldXf.position           = pose.position;
                worldXf.rotation           = pose.rotation;
                // Hit the eye and the near-plane center (both drawn as gizmos).
                const float nearZ = std::max(camera.nearPlane, 1e-3f);
                const glm::vec3 forward =
                    glm::normalize(worldXf.rotation * glm::vec3 {0.0f, 0.0f, -1.0f});
                const glm::vec3 nearCenter =
                    glm::dot(forward, forward) > 1e-6f
                        ? worldXf.position + forward * nearZ
                        : worldXf.position;

                const glm::vec3 candidates[] = {worldXf.position, nearCenter};
                for(const auto &world : candidates)
                {
                    ImVec2 screen {};
                    if(!Project(world, viewProj, imageMin, imageSize, screen))
                    {
                        continue;
                    }

                    const float dx = screen.x - mouse.x;
                    const float dy = screen.y - mouse.y;
                    const float d2 = dx * dx + dy * dy;
                    if(d2 <= radiusSq && d2 < bestDistSq)
                    {
                        bestDistSq = d2;
                        bestEntity = entity;
                        found      = true;
                    }
                }
            });

        if(!found)
        {
            return std::nullopt;
        }
        return bestEntity;
    }

} // namespace FRIGGA_NAMESPACE
