#include "platform/vulkan/VulkanUtils.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace myapp {

void checkVkResult(VkResult result)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    std::fprintf(stderr, "[vulkan] Error: VkResult = %d\n", result);
    if (result < 0)
    {
        std::abort();
    }
}

bool hasExtension(const std::vector<VkExtensionProperties>& properties, std::string_view extension)
{
    return std::any_of(properties.begin(), properties.end(),
                       [extension](const VkExtensionProperties& property) {
                           return extension == property.extensionName;
                       });
}

}  // namespace myapp
