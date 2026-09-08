#pragma once

#include <Frigga/Gui/Backends/imgui_impl_vulkan.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace FRIGGA_NAMESPACE::ImGuiVulkanLifetime
{
    /// Frames-in-flight used when deferring ImGui descriptor frees. Set once
    /// from GuiLayer init (max of Freya frameCount / swapchain images).
    inline std::uint32_t &FramesInFlight()
    {
        static std::uint32_t count = 2;
        return count;
    }

    inline void SetFramesInFlight(std::uint32_t count)
    {
        FramesInFlight() = std::max(2u, count);
    }

    struct PendingTexture
    {
        VkDescriptorSet set        = VK_NULL_HANDLE;
        std::uint32_t   framesLeft = 0;
    };

    inline std::vector<PendingTexture> &Queue()
    {
        static std::vector<PendingTexture> queue;
        return queue;
    }

    /// Queue a descriptor set for free after FramesInFlight presents so
    /// in-flight ImGui draws can finish sampling it.
    ///
    /// Uses FiF+1 because Tick() runs in GuiLayer::begin() before Freya's
    /// BeginFrame WaitNextFrame for the recycled frame slot.
    inline void DeferRemoveTexture(VkDescriptorSet set)
    {
        if(set == VK_NULL_HANDLE)
        {
            return;
        }
        Queue().push_back(PendingTexture {set, FramesInFlight() + 1u});
    }

    /// Drop one frame from each pending free; remove sets that have aged out.
    inline void Tick()
    {
        auto &queue = Queue();
        for(PendingTexture &entry : queue)
        {
            if(entry.framesLeft > 0)
            {
                --entry.framesLeft;
            }
        }

        std::size_t write = 0;
        for(std::size_t read = 0; read < queue.size(); ++read)
        {
            if(queue[read].framesLeft > 0)
            {
                queue[write++] = queue[read];
                continue;
            }
            if(queue[read].set != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_RemoveTexture(queue[read].set);
            }
        }
        queue.resize(write);
    }

    /// GPU idle then free every deferred set. Call before ImGui Vulkan shutdown.
    inline void FlushImmediate(VkDevice device)
    {
        if(device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(device);
        }

        for(PendingTexture &entry : Queue())
        {
            if(entry.set != VK_NULL_HANDLE)
            {
                ImGui_ImplVulkan_RemoveTexture(entry.set);
            }
        }
        Queue().clear();
    }
} // namespace FRIGGA_NAMESPACE::ImGuiVulkanLifetime
