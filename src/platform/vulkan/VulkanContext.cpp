#include "platform/vulkan/VulkanContext.h"

#include "platform/vulkan/VulkanUtils.h"

#include "imgui_impl_vulkan.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace myapp {
namespace {

#ifndef NDEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL debugReport(
    [[maybe_unused]] VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT object_type,
    [[maybe_unused]] uint64_t object,
    [[maybe_unused]] size_t location,
    [[maybe_unused]] int32_t message_code,
    [[maybe_unused]] const char* layer_prefix,
    const char* message,
    [[maybe_unused]] void* user_data)  // NOLINT(misc-const-correctness)
{
    std::fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n",
                 object_type, message);
    return VK_FALSE;
}
#endif

}  // namespace

VulkanContext::VulkanContext(std::vector<const char*> instance_extensions)
{
    try
    {
        createInstance(instance_extensions);

        physical_device_ = ImGui_ImplVulkanH_SelectPhysicalDevice(instance_);
        if (physical_device_ == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to select a Vulkan physical device");
        }

        queue_family_ = ImGui_ImplVulkanH_SelectQueueFamilyIndex(physical_device_);
        if (queue_family_ == static_cast<uint32_t>(-1))
        {
            throw std::runtime_error("Failed to select a Vulkan queue family");
        }

        createDevice();
        createDescriptorPool();
    }
    catch (...)
    {
        destroy();
        throw;
    }
}

VulkanContext::~VulkanContext()
{
    destroy();
}

void VulkanContext::createInstance(std::vector<const char*>& instance_extensions)
{
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    uint32_t properties_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    std::vector<VkExtensionProperties> properties(properties_count);
    checkVkResult(vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.data()));

    if (hasExtension(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (hasExtension(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    std::vector<const char*> enabled_layers;
#ifndef NDEBUG
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> layer_properties(layer_count);
    checkVkResult(vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data()));

    if (hasLayer(layer_properties, "VK_LAYER_KHRONOS_validation"))
    {
        enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
    }
    else
    {
        std::fprintf(stderr,
                     "[vulkan] Warning: VK_LAYER_KHRONOS_validation is not present; "
                     "continuing without validation layers.\n");
    }

    if (hasExtension(properties, "VK_EXT_debug_report"))
    {
        instance_extensions.push_back("VK_EXT_debug_report");
    }
#endif
    create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
    create_info.ppEnabledLayerNames = enabled_layers.data();

    create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();
    checkVkResult(vkCreateInstance(&create_info, allocator_, &instance_));

#ifndef NDEBUG
    const auto vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugReportCallbackEXT"));
    if (vkCreateDebugReportCallbackEXT != nullptr)
    {
        VkDebugReportCallbackCreateInfoEXT debug_report_info{};
        debug_report_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                  VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        debug_report_info.pfnCallback = debugReport;
        checkVkResult(vkCreateDebugReportCallbackEXT(instance_, &debug_report_info, allocator_, &debug_report_));
    }
#endif
}

void VulkanContext::createDevice()
{
    std::vector<const char*> device_extensions{"VK_KHR_swapchain"};

    uint32_t properties_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &properties_count, nullptr);
    std::vector<VkExtensionProperties> properties(properties_count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &properties_count, properties.data());

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (hasExtension(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
    {
        device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }
#endif

    const float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    checkVkResult(vkCreateDevice(physical_device_, &create_info, allocator_, &device_));
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
}

void VulkanContext::createDescriptorPool()
{
    const std::array<VkDescriptorPoolSize, 2> pool_sizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE},
        {VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE},
    },};

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;
    for (const VkDescriptorPoolSize& pool_size : pool_sizes)
    {
        pool_info.maxSets += pool_size.descriptorCount;
    }
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    checkVkResult(vkCreateDescriptorPool(device_, &pool_info, allocator_, &descriptor_pool_));
}

void VulkanContext::destroy() noexcept
{
    if (device_ != VK_NULL_HANDLE && descriptor_pool_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device_, descriptor_pool_, allocator_);
        descriptor_pool_ = VK_NULL_HANDLE;
    }

#ifndef NDEBUG
    if (instance_ != VK_NULL_HANDLE && debug_report_ != VK_NULL_HANDLE)
    {
        const auto vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugReportCallbackEXT"));
        if (vkDestroyDebugReportCallbackEXT != nullptr)
        {
            vkDestroyDebugReportCallbackEXT(instance_, debug_report_, allocator_);
        }
        debug_report_ = VK_NULL_HANDLE;
    }
#endif

    if (device_ != VK_NULL_HANDLE)
    {
        vkDestroyDevice(device_, allocator_);
        device_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance_, allocator_);
        instance_ = VK_NULL_HANDLE;
    }
}

}  // namespace myapp
