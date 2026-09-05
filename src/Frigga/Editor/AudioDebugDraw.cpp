#include <Frigga/Editor/AudioDebugDraw.hpp>

#include "Frigga/ECS/Components/AudioSourceComponent.hpp"
#include "Frigga/ECS/Components/TransformComponent.hpp"
#include "Frigga/ECS/TransformUtil.hpp"
#include "Frigga/Editor/EditorIconBillboard.hpp"

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

        void ConsiderHit(fr::Entity entity, const glm::vec3 &world, const glm::mat4 &viewProj,
                         const ImVec2 &imageMin, const ImVec2 &imageSize, const ImVec2 &mouse,
                         float radiusSq, fr::Entity &bestEntity, float &bestDistSq, bool &found)
        {
            ImVec2 screen {};
            if(!Project(world, viewProj, imageMin, imageSize, screen))
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
        }
    } // namespace

    void AudioDebugDraw::Draw(ImDrawList *drawList, const skr::Arc<fr::Registry> &registry,
                              const glm::mat4 &view, const glm::mat4 &vulkanProjection,
                              const ImVec2 &imageMin, const ImVec2 &imageSize,
                              fr::Entity selectedEntity)
    {
        if(drawList == nullptr || imageSize.x < 1.0f || imageSize.y < 1.0f)
        {
            return;
        }

        const glm::mat4 viewProj = FlipYProjection(vulkanProjection) * view;

        registry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, AudioSourceComponent &) {
                ImVec2 screen {};
                if(!Project(TransformUtil::WorldPose(*registry, entity).position, viewProj,
                            imageMin, imageSize, screen))
                {
                    return;
                }

                const bool selected = entity == selectedEntity;
                EditorIconBillboard::Draw(drawList, screen, EditorIconBillboard::kVolumeUp,
                                          selected ? IM_COL32(120, 220, 160, 255)
                                                   : IM_COL32(90, 180, 130, 230),
                                          selected);
            });

        registry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, AudioListenerComponent &) {
                ImVec2 screen {};
                if(!Project(TransformUtil::WorldPose(*registry, entity).position, viewProj,
                            imageMin, imageSize, screen))
                {
                    return;
                }

                const bool selected = entity == selectedEntity;
                EditorIconBillboard::Draw(drawList, screen, EditorIconBillboard::kMic,
                                          selected ? IM_COL32(255, 190, 90, 255)
                                                   : IM_COL32(220, 160, 70, 230),
                                          selected);
            });
    }

    std::optional<fr::Entity> AudioDebugDraw::HitTest(const skr::Arc<fr::Registry> &registry,
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
            [&](fr::Entity entity, TransformComponent &, AudioSourceComponent &) {
                ConsiderHit(entity, TransformUtil::WorldPose(*registry, entity).position, viewProj,
                            imageMin, imageSize, mouse, radiusSq, bestEntity, bestDistSq, found);
            });

        registry->CreateMutation()->Each(
            [&](fr::Entity entity, TransformComponent &, AudioListenerComponent &) {
                ConsiderHit(entity, TransformUtil::WorldPose(*registry, entity).position, viewProj,
                            imageMin, imageSize, mouse, radiusSq, bestEntity, bestDistSq, found);
            });

        if(!found)
        {
            return std::nullopt;
        }
        return bestEntity;
    }

} // namespace FRIGGA_NAMESPACE
