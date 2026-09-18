#include "VulkanSwapchain.h"

#include "VulkanContext.h"
#include "VulkanUtils.h"

#include <array>
#include <stdexcept>

namespace myapp {

VulkanSwapchain::VulkanSwapchain(const VulkanContext& context, VkSurfaceKHR surface,
                                 std::pair<int, int> framebuffer_size)
    : context_(context)
{
    window_data_.Surface = surface;
    try
    {
        setup(framebuffer_size.first, framebuffer_size.second);
    }
    catch (...)
    {
        destroy();
        throw;
    }
}

VulkanSwapchain::~VulkanSwapchain()
{
    destroy();
}

void VulkanSwapchain::setup(int width, int height)
{
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(context_.physicalDevice(), context_.queueFamily(),
                                         window_data_.Surface, &supported);
    if (supported != VK_TRUE)
        throw std::runtime_error("Error: no WSI support on physical device 0");

    const std::array<VkFormat, 4> request_formats{
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
    };
    const VkColorSpaceKHR request_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    window_data_.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        context_.physicalDevice(), window_data_.Surface, request_formats.data(),
        static_cast<int>(request_formats.size()), request_color_space);

    const std::array<VkPresentModeKHR, 1> present_modes{VK_PRESENT_MODE_FIFO_KHR};
    window_data_.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        context_.physicalDevice(), window_data_.Surface, present_modes.data(),
        static_cast<int>(present_modes.size()));

    ImGui_ImplVulkanH_CreateOrResizeWindow(context_.instance(), context_.physicalDevice(),
                                           context_.device(), &window_data_, context_.queueFamily(),
                                           context_.allocator(), width, height, kMinImageCount, 0);
}

void VulkanSwapchain::resize(int width, int height)
{
    ImGui_ImplVulkan_SetMinImageCount(kMinImageCount);
    ImGui_ImplVulkanH_CreateOrResizeWindow(context_.instance(), context_.physicalDevice(),
                                           context_.device(), &window_data_, context_.queueFamily(),
                                           context_.allocator(), width, height, kMinImageCount, 0);
    window_data_.FrameIndex = 0;
    rebuild_ = false;
}

void VulkanSwapchain::setClearColor(const ImVec4& color) noexcept
{
    window_data_.ClearValue.color.float32[0] = color.x * color.w;
    window_data_.ClearValue.color.float32[1] = color.y * color.w;
    window_data_.ClearValue.color.float32[2] = color.z * color.w;
    window_data_.ClearValue.color.float32[3] = color.w;
}

void VulkanSwapchain::render(ImDrawData* draw_data)
{
    VkSemaphore image_acquired_semaphore = window_data_.FrameSemaphores[window_data_.SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = window_data_.FrameSemaphores[window_data_.SemaphoreIndex].RenderCompleteSemaphore;

    VkResult err = vkAcquireNextImageKHR(context_.device(), window_data_.Swapchain, UINT64_MAX,
                                         image_acquired_semaphore, VK_NULL_HANDLE, &window_data_.FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        rebuild_ = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        checkVkResult(err);

    ImGui_ImplVulkanH_Frame* frame = &window_data_.Frames[window_data_.FrameIndex];
    checkVkResult(vkWaitForFences(context_.device(), 1, &frame->Fence, VK_TRUE, UINT64_MAX));
    checkVkResult(vkResetFences(context_.device(), 1, &frame->Fence));

    checkVkResult(vkResetCommandPool(context_.device(), frame->CommandPool, 0));
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVkResult(vkBeginCommandBuffer(frame->CommandBuffer, &begin_info));

    VkRenderPassBeginInfo render_info{};
    render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_info.renderPass = window_data_.RenderPass;
    render_info.framebuffer = frame->Framebuffer;
    render_info.renderArea.extent.width = static_cast<uint32_t>(window_data_.Width);
    render_info.renderArea.extent.height = static_cast<uint32_t>(window_data_.Height);
    render_info.clearValueCount = 1;
    render_info.pClearValues = &window_data_.ClearValue;
    vkCmdBeginRenderPass(frame->CommandBuffer, &render_info, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(draw_data, frame->CommandBuffer);
    vkCmdEndRenderPass(frame->CommandBuffer);

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_acquired_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    checkVkResult(vkEndCommandBuffer(frame->CommandBuffer));
    checkVkResult(vkQueueSubmit(context_.queue(), 1, &submit_info, frame->Fence));
}

void VulkanSwapchain::present()
{
    if (rebuild_)
        return;

    VkSemaphore render_complete_semaphore = window_data_.FrameSemaphores[window_data_.SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_complete_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &window_data_.Swapchain;
    present_info.pImageIndices = &window_data_.FrameIndex;

    const VkResult err = vkQueuePresentKHR(context_.queue(), &present_info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        rebuild_ = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        checkVkResult(err);

    window_data_.SemaphoreIndex = (window_data_.SemaphoreIndex + 1) % window_data_.SemaphoreCount;
}

void VulkanSwapchain::destroy() noexcept
{
    if (window_data_.Swapchain != VK_NULL_HANDLE || window_data_.RenderPass != VK_NULL_HANDLE)
        ImGui_ImplVulkanH_DestroyWindow(context_.instance(), context_.device(), &window_data_,
                                        context_.allocator());

    if (window_data_.Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(context_.instance(), window_data_.Surface, context_.allocator());
        window_data_.Surface = VK_NULL_HANDLE;
    }
}

}  // namespace myapp
