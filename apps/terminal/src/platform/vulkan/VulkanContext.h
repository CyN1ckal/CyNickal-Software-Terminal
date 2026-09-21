// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace terminal {

class VulkanContext {
public:
    explicit VulkanContext(std::vector<const char*> instance_extensions);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    [[nodiscard]] VkInstance instance() const noexcept { return instance_; }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return physical_device_; }
    [[nodiscard]] VkDevice device() const noexcept { return device_; }
    [[nodiscard]] uint32_t queueFamily() const noexcept { return queue_family_; }
    [[nodiscard]] VkQueue queue() const noexcept { return queue_; }
    [[nodiscard]] VkPipelineCache pipelineCache() const noexcept { return pipeline_cache_; }
    [[nodiscard]] VkDescriptorPool descriptorPool() const noexcept { return descriptor_pool_; }
    [[nodiscard]] const VkAllocationCallbacks* allocator() const noexcept { return allocator_; }

private:
    void createInstance(std::vector<const char*>& instance_extensions);
    void createDevice();
    void createDescriptorPool();
    void destroy() noexcept;

    const VkAllocationCallbacks* allocator_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queue_family_ = UINT32_MAX;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
#ifndef NDEBUG
    VkDebugReportCallbackEXT debug_report_ = VK_NULL_HANDLE;
#endif
};

}  // namespace terminal
