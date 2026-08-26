#pragma once

#include <Frigga/Frigga.hpp>
#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>

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

    /// Thick wrapper around Freya v0.42's renderer-owned offscreen viewport.
    ///
    /// v0.42 removed the app-owned `RenderTarget`/`SetOutputTarget` API; the
    /// renderer owns the viewport target and exposes it through
    /// `SetViewportTarget` / `GetViewportImage`. Each editor viewport panel
    /// claims the shared target while it is the active view and displays the
    /// composite through a Dear ImGui descriptor.
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
            const bool resizeNeeded = reactivating || !mImageValid || SizeChanged(width, height);

            if(mClaimed)
            {
                if(resizeNeeded)
                {
                    if(!mRenderer->SetViewportTarget(width, height))
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
                if(!mRenderer->SetViewportTarget(width, height))
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
                mRenderer->ClearOutputTarget();
            }
            mClaimed    = false;
            mImageValid = false;
            mWidth      = 0;
            mHeight     = 0;
        }

        /// Present the offscreen composite into an ImGui rectangle.
        void present(const ImVec2 &size) const
        {
            if(!IsActive() || mTextureId == VK_NULL_HANDLE || size.x <= 0.0f || size.y <= 0.0f)
            {
                return;
            }

            ImVec2 uv0 {0.0f, 0.0f};
            ImVec2 uv1 {1.0f, 1.0f};
            ImVec2 displaySize = size;

            if(mWidth > 0 && mHeight > 0)
            {
                const float texAspect   = static_cast<float>(mWidth) / static_cast<float>(mHeight);
                const float availAspect = size.x / size.y;
                if(std::abs(texAspect - availAspect) > 1.0e-3f)
                {
                    if(texAspect > availAspect)
                    {
                        const float visibleFraction = availAspect / texAspect;
                        const float crop            = (1.0f - visibleFraction) * 0.5f;
                        uv0.x                       = crop;
                        uv1.x                       = 1.0f - crop;
                    }
                    else
                    {
                        const float visibleFraction = texAspect / availAspect;
                        const float crop            = (1.0f - visibleFraction) * 0.5f;
                        uv0.y                       = crop;
                        uv1.y                       = 1.0f - crop;
                    }
                }
            }

            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(mTextureId)),
                         displaySize, uv0, uv1);
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

        void refreshTexture(bool forceRebind)
        {
            const fra::ImGuiViewportImage img = mRenderer->GetViewportImage();
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

            if(oldTextureId != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_RemoveTexture(oldTextureId);
            }
        }

        void releaseTexture()
        {
            if(mTextureId != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_RemoveTexture(mTextureId);
            }
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
