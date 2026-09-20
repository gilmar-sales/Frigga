#pragma once

#include <Frigga/Frigga.hpp>
#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>
#include <Frigga/Gui/ImGuiVulkanLifetime.hpp>

#include <Freya/Advanced.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>

namespace fg
{
    /// View (glm::lookAt) + Vulkan Y-flipped projection for a scene camera.
    struct ViewportCameraMatrices
    {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
    };

    /// Thick wrapper around Freya v0.46's renderer-owned offscreen viewport.
    ///
    /// The renderer owns the viewport target and exposes it through
    /// `fra::Advanced(*renderer).SetViewportTarget` / `GetViewportImage`. Each
    /// editor viewport panel claims the shared target while it is the active
    /// view and displays the composite through a Dear ImGui descriptor.
    ///
    /// Tab / play-mode switches call Suspend() so the shared Freya target stays
    /// allocated. Only the ImGui descriptor is dropped; Release() tears down GPU
    /// resources on panel detach.
    class ViewportTarget
    {
      public:
        static constexpr std::uint32_t kResizeThreshold = 2;

        explicit ViewportTarget(skr::Arc<fra::Renderer> renderer): mRenderer(std::move(renderer)) {}

        ~ViewportTarget()
        {
            Release();
        }

        ViewportTarget(const ViewportTarget &)            = delete;
        ViewportTarget &operator=(const ViewportTarget &) = delete;

        /// Route rendering into an offscreen viewport of the given pixel size
        /// and refresh the ImGui descriptor when the image changes.
        void Claim(std::uint32_t width, std::uint32_t height)
        {
            if(!mRenderer)
            {
                return;
            }

            if(width == 0 || height == 0)
            {
                Suspend();
                return;
            }

            const bool reactivating = !mClaimed;
            // Compare against Freya's live extent too: swapchain rebuilds (or
            // another panel) can change the shared target underneath while our
            // cached mWidth/mHeight still match the request — leaving a soft,
            // wrong-resolution ImGui blit after play/stop.
            const bool resizeNeeded =
                reactivating || !mImageValid || SizeChanged(width, height) ||
                FreyaExtentMismatched(width, height);

            if(mClaimed)
            {
                if(resizeNeeded)
                {
                    if(!fra::Advanced(*mRenderer).SetViewportTarget(width, height))
                    {
                        mImageValid = false;
                    }
                    else
                    {
                        mImageValid = true;
                    }
                }
            }
            else
            {
                if(!fra::Advanced(*mRenderer).SetViewportTarget(width, height))
                {
                    return;
                }
                mClaimed    = true;
                mImageValid = true;
            }

            refreshTexture(reactivating || resizeNeeded);

            mWidth  = width;
            mHeight = height;
        }

        /// Stop routing renders through this panel without freeing the shared
        /// Freya target. Drops the ImGui descriptor so stale handles cannot be
        /// sampled after another panel reuses the target.
        void Suspend()
        {
            mClaimed    = false;
            mImageValid = false;
            releaseTexture();
        }

        /// Restore direct presentation to the swapchain. Use only on panel detach.
        void Release()
        {
            releaseTexture();
            if(mRenderer && mClaimed)
            {
                fra::Advanced(*mRenderer).ClearOutputTarget();
            }
            mClaimed    = false;
            mImageValid = false;
            mWidth      = 0;
            mHeight     = 0;
        }

        /// UV crop applied when presenting an RT into an ImGui rect whose aspect
        /// does not match the texture (same math as `present`).
        static void ComputeLetterboxUv(std::uint32_t texW, std::uint32_t texH,
                                       const ImVec2 &displaySize, ImVec2 &uv0Out,
                                       ImVec2 &uv1Out)
        {
            uv0Out = ImVec2 {0.0f, 0.0f};
            uv1Out = ImVec2 {1.0f, 1.0f};
            if(texW == 0 || texH == 0 || displaySize.x <= 0.0f || displaySize.y <= 0.0f)
            {
                return;
            }

            const float texAspect   = static_cast<float>(texW) / static_cast<float>(texH);
            const float availAspect = displaySize.x / displaySize.y;
            if(std::abs(texAspect - availAspect) <= 1.0e-3f)
            {
                return;
            }

            if(texAspect > availAspect)
            {
                const float visibleFraction = availAspect / texAspect;
                const float crop            = (1.0f - visibleFraction) * 0.5f;
                uv0Out.x                    = crop;
                uv1Out.x                    = 1.0f - crop;
            }
            else
            {
                const float visibleFraction = texAspect / availAspect;
                const float crop            = (1.0f - visibleFraction) * 0.5f;
                uv0Out.y                    = crop;
                uv1Out.y                    = 1.0f - crop;
            }
        }

        /// Map an ImGui screen-space point over a presented image into framebuffer
        /// pixels, accounting for letterbox UV crop. Returns false when the point
        /// is outside the image rect.
        static bool MapScreenToFramebuffer(const ImVec2 &screen, const ImVec2 &imageMin,
                                           const ImVec2 &imageSize, std::uint32_t fbW,
                                           std::uint32_t fbH, float &fbXOut, float &fbYOut)
        {
            if(fbW == 0 || fbH == 0 || imageSize.x <= 0.0f || imageSize.y <= 0.0f)
            {
                return false;
            }

            const float u = (screen.x - imageMin.x) / imageSize.x;
            const float v = (screen.y - imageMin.y) / imageSize.y;
            if(u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f)
            {
                return false;
            }

            ImVec2 uv0;
            ImVec2 uv1;
            ComputeLetterboxUv(fbW, fbH, imageSize, uv0, uv1);
            const float texU = uv0.x + u * (uv1.x - uv0.x);
            const float texV = uv0.y + v * (uv1.y - uv0.y);
            fbXOut           = texU * static_cast<float>(fbW);
            fbYOut           = texV * static_cast<float>(fbH);
            return true;
        }

        /// Present the offscreen composite into an ImGui rectangle.
        void present(const ImVec2 &size) const
        {
            if(!IsActive() || mTextureId == VK_NULL_HANDLE || size.x <= 0.0f || size.y <= 0.0f)
            {
                return;
            }

            // Prefer Freya's live extent for letterboxing — cached mWidth/mHeight
            // can briefly disagree after a shared-target handoff.
            std::uint32_t texW = mWidth;
            std::uint32_t texH = mHeight;
            if(mRenderer)
            {
                const fra::ImGuiViewportImage img =
                    fra::Advanced(*mRenderer).GetViewportImage();
                if(img.valid && img.width > 0 && img.height > 0)
                {
                    texW = img.width;
                    texH = img.height;
                }
            }

            ImVec2 uv0;
            ImVec2 uv1;
            ComputeLetterboxUv(texW, texH, size, uv0, uv1);

            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mTextureId)),
                         size, uv0, uv1);
        }

        [[nodiscard]] bool IsActive() const
        {
            return mClaimed && mImageValid && mTextureId != VK_NULL_HANDLE;
        }

        [[nodiscard]] std::uint32_t Width() const
        {
            return mWidth;
        }

        [[nodiscard]] std::uint32_t Height() const
        {
            return mHeight;
        }

        /// Compute view/projection for a camera pose using the renderer's
        /// projection convention (Y-flipped, Freya -Z forward).
        static ViewportCameraMatrices Compute(const skr::Arc<fra::Renderer> &renderer,
                                              const glm::vec3 &position, const glm::quat &rotation,
                                              float fovDegrees, float nearPlane, float farPlane,
                                              float aspect)
        {
            const glm::vec3 forward = glm::dot(rotation * glm::vec3(0.0f, 0.0f, -1.0f),
                                               rotation * glm::vec3(0.0f, 0.0f, -1.0f)) > 1e-6f
                                          ? glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f))
                                          : glm::vec3(0.0f, 0.0f, -1.0f);
            const glm::vec3 up      = glm::dot(rotation * glm::vec3(0.0f, 1.0f, 0.0f),
                                               rotation * glm::vec3(0.0f, 1.0f, 0.0f)) > 1e-6f
                                          ? glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f))
                                          : glm::vec3(0.0f, 1.0f, 0.0f);

            ViewportCameraMatrices out;
            out.view       = glm::lookAt(position, position + forward, up);
            out.projection = renderer ? renderer->MakeProjection(glm::radians(fovDegrees), aspect,
                                                                 nearPlane, farPlane)
                                      : glm::identity<glm::mat4>();
            return out;
        }

      private:
        [[nodiscard]] bool SizeChanged(std::uint32_t width, std::uint32_t height) const
        {
            const auto delta = [](std::uint32_t a, std::uint32_t b) {
                return static_cast<std::uint32_t>(std::abs(static_cast<int>(a) - static_cast<int>(b)));
            };
            return delta(mWidth, width) >= kResizeThreshold ||
                   delta(mHeight, height) >= kResizeThreshold;
        }

        [[nodiscard]] bool FreyaExtentMismatched(std::uint32_t width,
                                                 std::uint32_t height) const
        {
            const fra::ImGuiViewportImage img =
                fra::Advanced(*mRenderer).GetViewportImage();
            if(!img.valid)
            {
                return true;
            }
            const auto delta = [](std::uint32_t a, std::uint32_t b) {
                return static_cast<std::uint32_t>(std::abs(static_cast<int>(a) - static_cast<int>(b)));
            };
            return delta(img.width, width) >= kResizeThreshold ||
                   delta(img.height, height) >= kResizeThreshold;
        }

        void refreshTexture(bool forceRebind)
        {
            const fra::ImGuiViewportImage img =
                fra::Advanced(*mRenderer).GetViewportImage();
            if(!img.valid || img.imageView == nullptr)
            {
                releaseTexture();
                mImageValid = false;
                return;
            }

            if(!forceRebind && mTextureId != VK_NULL_HANDLE && mBoundView == img.imageView &&
               mBoundSampler == img.sampler)
            {
                return;
            }

            const VkDescriptorSet newTextureId =
                ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(img.sampler),
                                            static_cast<VkImageView>(img.imageView),
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if(newTextureId == VK_NULL_HANDLE)
            {
                releaseTexture();
                mImageValid = false;
                return;
            }

            const VkDescriptorSet oldTextureId = mTextureId;
            mTextureId                           = newTextureId;
            mBoundView                           = static_cast<const void *>(img.imageView);
            mBoundSampler                        = static_cast<const void *>(img.sampler);

            // Defer free: in-flight ImGui draws may still sample the old set.
            ImGuiVulkanLifetime::DeferRemoveTexture(oldTextureId);
        }

        void releaseTexture()
        {
            ImGuiVulkanLifetime::DeferRemoveTexture(mTextureId);
            mTextureId    = VK_NULL_HANDLE;
            mBoundView    = nullptr;
            mBoundSampler = nullptr;
        }

        skr::Arc<fra::Renderer> mRenderer;
        VkDescriptorSet mTextureId = VK_NULL_HANDLE;
        const void *mBoundView     = nullptr;
        const void *mBoundSampler  = nullptr;
        std::uint32_t mWidth       = 0;
        std::uint32_t mHeight      = 0;
        bool mClaimed              = false;
        bool mImageValid           = false;
    };
} // namespace fg
