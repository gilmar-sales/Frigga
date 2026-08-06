#include "LightDebugDraw.hpp"

#include "Frigga/ECS/Components/LightComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

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
                         ImU32 color, float thickness = 1.5f)
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

        ImU32 ColorFromLight(const glm::vec3 &color, bool selected)
        {
            const int r = static_cast<int>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
            const int g = static_cast<int>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
            const int b = static_cast<int>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
            return IM_COL32(r, g, b, selected ? 255 : 220);
        }

        void DrawCross(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                       const ImVec2 &imageSize, const glm::vec3 &center, float size, ImU32 color,
                       float thickness)
        {
            DrawSegment(drawList, viewProj, imageMin, imageSize, center + glm::vec3 {-size, 0, 0},
                        center + glm::vec3 {size, 0, 0}, color, thickness);
            DrawSegment(drawList, viewProj, imageMin, imageSize, center + glm::vec3 {0, -size, 0},
                        center + glm::vec3 {0, size, 0}, color, thickness);
            DrawSegment(drawList, viewProj, imageMin, imageSize, center + glm::vec3 {0, 0, -size},
                        center + glm::vec3 {0, 0, size}, color, thickness);
        }

        void DrawCircle(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                        const ImVec2 &imageSize, const glm::vec3 &center, const glm::vec3 &axisX,
                        const glm::vec3 &axisY, float radius, ImU32 color, float thickness,
                        int segments = 24)
        {
            for(int i = 0; i < segments; ++i)
            {
                const float a0 =
                    (static_cast<float>(i) / segments) * 2.0f * std::numbers::pi_v<float>;
                const float a1 =
                    (static_cast<float>(i + 1) / segments) * 2.0f * std::numbers::pi_v<float>;
                const glm::vec3 p0 =
                    center + axisX * (std::cos(a0) * radius) + axisY * (std::sin(a0) * radius);
                const glm::vec3 p1 =
                    center + axisX * (std::cos(a1) * radius) + axisY * (std::sin(a1) * radius);
                DrawSegment(drawList, viewProj, imageMin, imageSize, p0, p1, color, thickness);
            }
        }

        void DrawCone(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                      const ImVec2 &imageSize, const glm::vec3 &origin, const glm::vec3 &direction,
                      float length, float angleRad, ImU32 color, float thickness)
        {
            const glm::vec3 dir = glm::normalize(direction);
            glm::vec3 up        = std::abs(dir.y) < 0.99f ? glm::vec3 {0, 1, 0} : glm::vec3 {1, 0, 0};
            const glm::vec3 right = glm::normalize(glm::cross(dir, up));
            up                    = glm::normalize(glm::cross(right, dir));

            const float radius = length * std::tan(angleRad);
            const glm::vec3 tip  = origin;
            const glm::vec3 base = origin + dir * length;

            constexpr int spokes = 8;
            for(int i = 0; i < spokes; ++i)
            {
                const float a =
                    (static_cast<float>(i) / spokes) * 2.0f * std::numbers::pi_v<float>;
                const glm::vec3 edge =
                    base + right * (std::cos(a) * radius) + up * (std::sin(a) * radius);
                DrawSegment(drawList, viewProj, imageMin, imageSize, tip, edge, color, thickness);
            }
            DrawCircle(drawList, viewProj, imageMin, imageSize, base, right, up, radius, color,
                       thickness);
        }

        void DrawRect(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                      const ImVec2 &imageSize, const glm::vec3 &center, const glm::vec3 &normal,
                      const glm::vec3 &tangent, float halfWidth, float halfHeight, ImU32 color,
                      float thickness)
        {
            const glm::vec3 n = glm::normalize(normal);
            glm::vec3 t       = tangent - n * glm::dot(tangent, n);
            if(glm::dot(t, t) < 1e-8f)
            {
                const glm::vec3 up =
                    std::abs(n.y) < 0.99f ? glm::vec3 {0, 1, 0} : glm::vec3 {1, 0, 0};
                t = glm::cross(up, n);
            }
            t                   = glm::normalize(t);
            const glm::vec3 bit = glm::normalize(glm::cross(n, t));

            const glm::vec3 c0 = center + t * halfWidth + bit * halfHeight;
            const glm::vec3 c1 = center - t * halfWidth + bit * halfHeight;
            const glm::vec3 c2 = center - t * halfWidth - bit * halfHeight;
            const glm::vec3 c3 = center + t * halfWidth - bit * halfHeight;

            DrawSegment(drawList, viewProj, imageMin, imageSize, c0, c1, color, thickness);
            DrawSegment(drawList, viewProj, imageMin, imageSize, c1, c2, color, thickness);
            DrawSegment(drawList, viewProj, imageMin, imageSize, c2, c3, color, thickness);
            DrawSegment(drawList, viewProj, imageMin, imageSize, c3, c0, color, thickness);
            DrawSegment(drawList, viewProj, imageMin, imageSize, center, center + n * 0.35f, color,
                        thickness);
        }

        void DrawLightGizmo(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                            const ImVec2 &imageSize, const TransformComponent &transform,
                            const LightComponent &light, bool selected)
        {
            const ImU32 color     = ColorFromLight(light.color, selected);
            const float thickness = selected ? 3.0f : 1.75f;
            const glm::vec3 pos   = transform.position;
            const glm::vec3 direction =
                glm::normalize(transform.rotation * glm::vec3 {0.0f, 0.0f, -1.0f});
            const glm::vec3 safeDir =
                glm::dot(direction, direction) > 1e-6f ? direction : glm::vec3 {0.0f, -1.0f, 0.0f};
            const glm::vec3 tangent = transform.rotation * glm::vec3 {1.0f, 0.0f, 0.0f};

            DrawCross(drawList, viewProj, imageMin, imageSize, pos, selected ? 0.28f : 0.2f, color,
                      thickness);

            if(selected)
            {
                ImVec2 screen {};
                if(Project(pos, viewProj, imageMin, imageSize, screen))
                {
                    drawList->AddCircle(screen, 10.0f, IM_COL32(255, 220, 80, 220), 16, 1.5f);
                }
            }

            switch(light.type)
            {
            case fra::LightType::Point:
            {
                const float ring = std::min(light.radius * 0.15f, 1.5f);
                DrawCircle(drawList, viewProj, imageMin, imageSize, pos, {1, 0, 0}, {0, 1, 0}, ring,
                           color, thickness);
                DrawCircle(drawList, viewProj, imageMin, imageSize, pos, {1, 0, 0}, {0, 0, 1}, ring,
                           color, thickness);
                DrawCircle(drawList, viewProj, imageMin, imageSize, pos, {0, 1, 0}, {0, 0, 1}, ring,
                           color, thickness);
                break;
            }
            case fra::LightType::Directional:
                DrawSegment(drawList, viewProj, imageMin, imageSize, pos, pos + safeDir * 2.0f,
                            color, thickness + 0.5f);
                break;
            case fra::LightType::Spot:
            {
                const float length = std::clamp(light.radius * 0.25f, 0.75f, 3.0f);
                DrawCone(drawList, viewProj, imageMin, imageSize, pos, safeDir, length,
                         glm::radians(std::max(light.outerAngleDegrees, 1.0f)), color, thickness);
                break;
            }
            case fra::LightType::Area:
                DrawRect(drawList, viewProj, imageMin, imageSize, pos, safeDir, tangent,
                         std::max(light.halfWidth, 0.01f), std::max(light.halfHeight, 0.01f), color,
                         thickness);
                break;
            }
        }
    } // namespace

    void LightDebugDraw::Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                              const glm::mat4 &view, const glm::mat4 &vulkanProjection,
                              const ImVec2 &imageMin, const ImVec2 &imageSize,
                              fr::Entity selectedEntity)
    {
        if(drawList == nullptr || imageSize.x < 1.0f || imageSize.y < 1.0f)
        {
            return;
        }

        const glm::mat4 viewProj = FlipYProjection(vulkanProjection) * view;

        registry->CreateMutation()->Each<TransformComponent, LightComponent>(
            [&](auto entity, TransformComponent &transform, LightComponent &light) {
                DrawLightGizmo(drawList, viewProj, imageMin, imageSize, transform, light,
                               entity == selectedEntity);
            });
    }

    std::optional<fr::Entity> LightDebugDraw::HitTest(const skr::Arc<fr::Registry> &registry,
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
        const float radiusSq     = pixelRadius * pixelRadius;

        fr::Entity bestEntity {};
        float bestDistSq = std::numeric_limits<float>::max();
        bool found       = false;

        registry->CreateMutation()->Each<TransformComponent, LightComponent>(
            [&](auto entity, TransformComponent &transform, LightComponent &) {
                ImVec2 screen {};
                if(!Project(transform.position, viewProj, imageMin, imageSize, screen))
                {
                    return;
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
            });

        if(!found)
        {
            return std::nullopt;
        }
        return bestEntity;
    }

} // namespace FRIGGA_NAMESPACE
