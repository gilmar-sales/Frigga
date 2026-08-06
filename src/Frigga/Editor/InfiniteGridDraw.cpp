#include "InfiniteGridDraw.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace FRIGGA_NAMESPACE
{
    namespace
    {
        constexpr float kNearW = 1e-4f;

        glm::mat4 FlipYProjection(const glm::mat4 &vulkanProjection)
        {
            glm::mat4 projection = vulkanProjection;
            projection[1][1] *= -1.0f;
            return projection;
        }

        ImVec2 ClipToScreen(const glm::vec4 &clip, const ImVec2 &imageMin, const ImVec2 &imageSize)
        {
            const float invW = 1.0f / clip.w;
            const float ndcX = clip.x * invW;
            const float ndcY = clip.y * invW;
            return {imageMin.x + (ndcX * 0.5f + 0.5f) * imageSize.x,
                    imageMin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * imageSize.y};
        }

        /// Clip a homogeneous segment to clip.w >= kNearW (in front of the camera).
        bool ClipNearHomogeneous(glm::vec4 &a, glm::vec4 &b)
        {
            const bool aFront = a.w >= kNearW;
            const bool bFront = b.w >= kNearW;
            if(!aFront && !bFront)
            {
                return false;
            }
            if(aFront && bFront)
            {
                return true;
            }

            const float t = (kNearW - a.w) / (b.w - a.w);
            const glm::vec4 clipped = a + t * (b - a);
            if(!aFront)
            {
                a = clipped;
            }
            else
            {
                b = clipped;
            }
            return true;
        }

        bool IntersectGround(const glm::vec3 &origin, const glm::vec3 &dir, glm::vec3 &hit)
        {
            if(std::abs(dir.y) < 1e-5f)
            {
                return false;
            }
            const float t = -origin.y / dir.y;
            if(t < 1e-4f)
            {
                return false;
            }
            hit = origin + dir * t;
            return std::isfinite(hit.x) && std::isfinite(hit.z);
        }

        bool SegmentGroundHit(const glm::vec3 &a, const glm::vec3 &b, glm::vec3 &hit)
        {
            const float ay = a.y;
            const float by = b.y;
            // No crossing (and not resting on the plane).
            if((ay > 0.0f && by > 0.0f) || (ay < 0.0f && by < 0.0f))
            {
                return false;
            }
            const float dy = by - ay;
            if(std::abs(dy) < 1e-8f)
            {
                if(std::abs(ay) > 1e-3f)
                {
                    return false;
                }
                hit = a;
                return true;
            }
            const float t = -ay / dy;
            if(t < -1e-4f || t > 1.0f + 1e-4f)
            {
                return false;
            }
            hit = a + std::clamp(t, 0.0f, 1.0f) * (b - a);
            hit.y = 0.0f;
            return std::isfinite(hit.x) && std::isfinite(hit.z);
        }

        float SnapDown(float value, float step)
        {
            return std::floor(value / step) * step;
        }

        float SnapUp(float value, float step)
        {
            return std::ceil(value / step) * step;
        }

        void DrawSegment(ImDrawList *drawList, const glm::mat4 &viewProj, const ImVec2 &imageMin,
                         const ImVec2 &imageSize, const glm::vec3 &a, const glm::vec3 &b,
                         ImU32 color, float thickness)
        {
            glm::vec4 ca = viewProj * glm::vec4(a, 1.0f);
            glm::vec4 cb = viewProj * glm::vec4(b, 1.0f);
            if(!ClipNearHomogeneous(ca, cb))
            {
                return;
            }
            drawList->AddLine(ClipToScreen(ca, imageMin, imageSize),
                              ClipToScreen(cb, imageMin, imageSize), color, thickness);
        }

        bool CollectVisibleGroundBounds(const glm::mat4 &view, const glm::mat4 &flipProj,
                                        float &outMinX, float &outMaxX, float &outMinZ,
                                        float &outMaxZ)
        {
            const glm::mat4 invView     = glm::inverse(view);
            const glm::vec3 camPos      = glm::vec3(invView[3]);
            const glm::mat4 invViewProj = glm::inverse(flipProj * view);

            auto unproject = [&](float ndcX, float ndcY, float ndcZ) -> glm::vec3 {
                glm::vec4 world = invViewProj * glm::vec4(ndcX, ndcY, ndcZ, 1.0f);
                if(std::abs(world.w) < 1e-8f)
                {
                    return camPos;
                }
                world /= world.w;
                return {world.x, world.y, world.z};
            };

            std::vector<glm::vec3> hits;
            hits.reserve(48);

            auto addHit = [&](const glm::vec3 &h) {
                if(std::isfinite(h.x) && std::isfinite(h.z))
                {
                    hits.push_back({h.x, 0.0f, h.z});
                }
            };

            // Frustum corners (covers reverse-Z and standard-Z via ndc Z={0,1}).
            std::array<glm::vec3, 8> corners {};
            int cornerIdx = 0;
            for(float z: {0.0f, 1.0f})
            {
                for(float y: {-1.0f, 1.0f})
                {
                    for(float x: {-1.0f, 1.0f})
                    {
                        corners[static_cast<size_t>(cornerIdx++)] = unproject(x, y, z);
                    }
                }
            }

            // Intersect all frustum edges with the ground plane (exact ground quad/trapezoid).
            static constexpr int kEdges[12][2] = {
                {0, 1}, {1, 3}, {3, 2}, {2, 0}, // z=0 face
                {4, 5}, {5, 7}, {7, 6}, {6, 4}, // z=1 face
                {0, 4}, {1, 5}, {2, 6}, {3, 7}, // sides
            };
            for(const auto &e: kEdges)
            {
                glm::vec3 hit {};
                if(SegmentGroundHit(corners[static_cast<size_t>(e[0])],
                                    corners[static_cast<size_t>(e[1])], hit))
                {
                    addHit(hit);
                }
            }

            // Ray-cast through dense NDC samples so horizon / infinite far cases still work
            // when the far plane never reaches the ground.
            constexpr float kSamples[] = {-1.f, -0.5f, 0.f, 0.5f, 1.f};
            for(float ny: kSamples)
            {
                for(float nx: kSamples)
                {
                    const glm::vec3 p0  = unproject(nx, ny, 0.0f);
                    const glm::vec3 p1  = unproject(nx, ny, 1.0f);
                    glm::vec3 dir       = p1 - p0;
                    if(glm::dot(dir, dir) < 1e-12f)
                    {
                        dir = p0 - camPos;
                    }
                    if(glm::dot(dir, dir) < 1e-12f)
                    {
                        continue;
                    }
                    dir = glm::normalize(dir);

                    glm::vec3 hit {};
                    if(IntersectGround(camPos, dir, hit))
                    {
                        addHit(hit);
                    }
                    if(IntersectGround(p0, dir, hit))
                    {
                        addHit(hit);
                    }
                }
            }

            if(hits.size() < 2)
            {
                const float height = std::max(std::abs(camPos.y), 1.0f);
                const float radius = std::clamp(height * 10.0f, 30.0f, 250.0f);
                outMinX            = camPos.x - radius;
                outMaxX            = camPos.x + radius;
                outMinZ            = camPos.z - radius;
                outMaxZ            = camPos.z + radius;
                return true;
            }

            outMinX = outMaxX = hits[0].x;
            outMinZ = outMaxZ = hits[0].z;
            for(const auto &h: hits)
            {
                outMinX = std::min(outMinX, h.x);
                outMaxX = std::max(outMaxX, h.x);
                outMinZ = std::min(outMinZ, h.z);
                outMaxZ = std::max(outMaxZ, h.z);
            }

            const float padX = (outMaxX - outMinX) * 0.08f + 2.0f;
            const float padZ = (outMaxZ - outMinZ) * 0.08f + 2.0f;
            outMinX -= padX;
            outMaxX += padX;
            outMinZ -= padZ;
            outMaxZ += padZ;

            // Soft cap so extreme glance angles don't spawn tens of thousands of lines.
            constexpr float kMaxHalfExtent = 500.0f;
            outMinX = std::max(outMinX, camPos.x - kMaxHalfExtent);
            outMaxX = std::min(outMaxX, camPos.x + kMaxHalfExtent);
            outMinZ = std::max(outMinZ, camPos.z - kMaxHalfExtent);
            outMaxZ = std::min(outMaxZ, camPos.z + kMaxHalfExtent);
            return true;
        }
    } // namespace

    void InfiniteGridDraw::Draw(ImDrawList *drawList, const glm::mat4 &view,
                                const glm::mat4 &vulkanProjection, const ImVec2 &imageMin,
                                const ImVec2 &imageSize, float minorStep, float majorStep)
    {
        if(drawList == nullptr || imageSize.x < 1.0f || imageSize.y < 1.0f)
        {
            return;
        }
        if(minorStep <= 1e-4f || majorStep < minorStep)
        {
            return;
        }

        const glm::mat4 flipProj = FlipYProjection(vulkanProjection);
        const glm::mat4 viewProj = flipProj * view;

        float minX = 0.0f;
        float maxX = 0.0f;
        float minZ = 0.0f;
        float maxZ = 0.0f;
        if(!CollectVisibleGroundBounds(view, flipProj, minX, maxX, minZ, maxZ))
        {
            return;
        }

        minX = SnapDown(minX, minorStep);
        maxX = SnapUp(maxX, minorStep);
        minZ = SnapDown(minZ, minorStep);
        maxZ = SnapUp(maxZ, minorStep);

        const ImU32 minorCol  = IM_COL32(200, 200, 200, 28);
        const ImU32 majorCol  = IM_COL32(210, 210, 210, 50);
        const ImU32 centerCol = IM_COL32(180, 180, 180, 72);

        const int majorMul = std::max(1, static_cast<int>(std::lround(majorStep / minorStep)));

        auto isMajor = [&](float coord) {
            const float cell    = coord / minorStep;
            const float rounded = std::round(cell);
            if(std::abs(cell - rounded) > 1e-3f)
            {
                return false;
            }
            return static_cast<int>(rounded) % majorMul == 0;
        };

        auto isCenter = [](float coord) { return std::abs(coord) < 1e-3f; };

        for(float x = minX; x <= maxX + minorStep * 0.5f; x += minorStep)
        {
            const bool center = isCenter(x);
            const bool major  = !center && isMajor(x);
            const ImU32 col   = center ? centerCol : (major ? majorCol : minorCol);
            const float thick = center ? 1.6f : (major ? 1.2f : 1.0f);
            DrawSegment(drawList, viewProj, imageMin, imageSize, {x, 0.0f, minZ},
                        {x, 0.0f, maxZ}, col, thick);
        }

        for(float z = minZ; z <= maxZ + minorStep * 0.5f; z += minorStep)
        {
            const bool center = isCenter(z);
            const bool major  = !center && isMajor(z);
            const ImU32 col   = center ? centerCol : (major ? majorCol : minorCol);
            const float thick = center ? 1.6f : (major ? 1.2f : 1.0f);
            DrawSegment(drawList, viewProj, imageMin, imageSize, {minX, 0.0f, z},
                        {maxX, 0.0f, z}, col, thick);
        }
    }

} // namespace FRIGGA_NAMESPACE
