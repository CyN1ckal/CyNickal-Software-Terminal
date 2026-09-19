#pragma once

#include "imgui_impl_vulkan.h"

#include <utility>

namespace myapp {

class VulkanContext;

class VulkanSwapchain {
public:
    static constexpr uint32_t kMinImageCount = 2;

    VulkanSwapchain(const VulkanContext& context, VkSurfaceKHR surface, std::pair<int, int> framebuffer_size);
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    void resize(int width, int height);
    void render(ImDrawData* draw_data);
    void present();
    void setClearColor(const ImVec4& color) noexcept;

    [[nodiscard]] uint32_t imageCount() const noexcept { return window_data_.ImageCount; }
    [[nodiscard]] VkRenderPass renderPass() const noexcept { return window_data_.RenderPass; }
    [[nodiscard]] int width() const noexcept { return window_data_.Width; }
    [[nodiscard]] int height() const noexcept { return window_data_.Height; }
    [[nodiscard]] bool needsRebuild() const noexcept { return rebuild_; }

private:
    void setup(int width, int height);
    void destroy() noexcept;

    const VulkanContext& context_;
    ImGui_ImplVulkanH_Window window_data_;
    bool rebuild_ = false;
};

}  // namespace myapp
