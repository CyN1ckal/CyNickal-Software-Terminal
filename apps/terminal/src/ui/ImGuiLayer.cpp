#include "ui/ImGuiLayer.h"

#include "platform/vulkan/VulkanContext.h"
#include "platform/vulkan/VulkanSwapchain.h"
#include "platform/vulkan/VulkanUtils.h"
#include "ui/Theme.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"

namespace terminal {

ImGuiLayer::ImGuiLayer(GLFWwindow* window, const VulkanContext& vulkan, const VulkanSwapchain& swapchain,
                       float main_scale)
    : device_(vulkan.device())
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    Theme::ApplyBloombergStyle(style);
    Theme::LoadFonts(io);
    ImPlot::StyleColorsAuto();
    ImPlot::GetStyle().Colors[ImPlotCol_PlotBg] = Theme::kPanel;
    ImPlot::GetStyle().Colors[ImPlotCol_FrameBg] = Theme::kCanvas;
    ImPlot::GetStyle().Colors[ImPlotCol_PlotBorder] = Theme::kHairline;
    ImPlot::GetStyle().Colors[ImPlotCol_AxisText] = Theme::kMuted;
    ImPlot::GetStyle().Colors[ImPlotCol_AxisGrid] = Theme::kHairline;
    ImPlot::GetStyle().Colors[ImPlotCol_Crosshairs] = Theme::kHairline;
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = vulkan.instance();
    init_info.PhysicalDevice = vulkan.physicalDevice();
    init_info.Device = vulkan.device();
    init_info.QueueFamily = vulkan.queueFamily();
    init_info.Queue = vulkan.queue();
    init_info.PipelineCache = vulkan.pipelineCache();
    init_info.DescriptorPool = vulkan.descriptorPool();
    init_info.MinImageCount = VulkanSwapchain::kMinImageCount;
    init_info.ImageCount = swapchain.imageCount();
    init_info.Allocator = vulkan.allocator();
    init_info.PipelineInfoMain.RenderPass = swapchain.renderPass();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = checkVkResult;
    ImGui_ImplVulkan_Init(&init_info);
}

ImGuiLayer::~ImGuiLayer()
{
    if (device_ != VK_NULL_HANDLE)
    {
        checkVkResult(vkDeviceWaitIdle(device_));
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void ImGuiLayer::newFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(VulkanSwapchain& swapchain, const ImVec4& clear_color)
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool minimized = draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f;

    swapchain.setClearColor(clear_color);
    if (!minimized)
    {
        swapchain.render(draw_data);
    }

    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    if (!minimized)
    {
        swapchain.present();
    }
}

}  // namespace terminal
