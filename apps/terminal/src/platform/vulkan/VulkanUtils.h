#pragma once

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace terminal {

void checkVkResult(VkResult result);

[[nodiscard]] bool hasExtension(const std::vector<VkExtensionProperties>& properties,
                                std::string_view extension);

[[nodiscard]] bool hasLayer(const std::vector<VkLayerProperties>& properties, std::string_view layer);

}  // namespace terminal
