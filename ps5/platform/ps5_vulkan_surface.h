#pragma once
#include <vulkan/vulkan.h>
VkResult la_ps5_create_surface(VkInstance instance, VkPhysicalDevice gpu,
    VkSurfaceKHR *surface, uint32_t *width, uint32_t *height);
