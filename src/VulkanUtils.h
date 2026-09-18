#pragma once

#include <vulkan/vulkan.h>

#include <string_view>
#include <vector>

namespace myapp {

void checkVkResult(VkResult result);

[[nodiscard]] bool hasExtension(const std::vector<VkExtensionProperties>& properties,
                                std::string_view extension);

}  // namespace myapp
