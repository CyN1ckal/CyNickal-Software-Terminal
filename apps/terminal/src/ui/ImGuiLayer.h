#pragma once

#include "imgui.h"

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace terminal {

class VulkanContext;
class VulkanSwapchain;

class ImGuiLayer {
public:
    ImGuiLayer(GLFWwindow* window, const VulkanContext& vulkan, const VulkanSwapchain& swapchain,
               float main_scale);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    static void newFrame();
    static void render(VulkanSwapchain& swapchain, const ImVec4& clear_color);

private:
    VkDevice device_ = VK_NULL_HANDLE;
};

}  // namespace terminal
