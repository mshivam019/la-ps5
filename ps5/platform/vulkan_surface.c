/* Native direct-display surface, following the working Twilight/Dawn integration.
 * New port glue; MIT, as the application runtime. No desktop window system. */
#include "ps5_vulkan_surface.h"
#include <stdio.h>

VkResult la_ps5_create_surface(VkInstance instance, VkPhysicalDevice gpu,
    VkSurfaceKHR *surface, uint32_t *width, uint32_t *height) {
    VkDisplayPropertiesKHR displays[16];
    uint32_t count = 16;
    VkResult result = vkGetPhysicalDeviceDisplayPropertiesKHR(gpu, &count, displays);
    if (result != VK_SUCCESS || !count)
        return result == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : result;
    VkDisplayKHR display = VK_NULL_HANDLE;
    VkDisplayModeKHR mode = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
    uint64_t smallest = UINT64_MAX;
    uint32_t refresh = 0;
    for (uint32_t i = 0; i < count; ++i) {
        VkDisplayModePropertiesKHR modes[32];
        uint32_t mode_count = 32;
        result = vkGetDisplayModePropertiesKHR(gpu, displays[i].display, &mode_count, modes);
        if (result != VK_SUCCESS)
            return result;
        for (uint32_t j = 0; j < mode_count; ++j) {
            VkDisplayModeParametersKHR p = modes[j].parameters;
            if (p.refreshRate < 59000 || p.refreshRate > 61000 || !p.visibleRegion.width || !p.visibleRegion.height)
                continue;
            uint64_t pixels = (uint64_t)p.visibleRegion.width * p.visibleRegion.height;
            if (pixels < smallest || (pixels == smallest && p.refreshRate > refresh)) {
                display = displays[i].display;
                mode = modes[j].displayMode;
                extent = p.visibleRegion;
                refresh = p.refreshRate;
                smallest = pixels;
            }
        }
    }
    if (mode == VK_NULL_HANDLE)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    VkDisplayPlanePropertiesKHR planes[16];
    uint32_t plane_count = 16;
    result = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(gpu, &plane_count, planes);
    if (result != VK_SUCCESS)
        return result;
    for (uint32_t i = 0; i < plane_count; ++i) {
        if (planes[i].currentDisplay && planes[i].currentDisplay != display)
            continue;
        VkDisplayKHR supported[16];
        uint32_t supported_count = 16;
        result = vkGetDisplayPlaneSupportedDisplaysKHR(gpu, i, &supported_count, supported);
        if (result != VK_SUCCESS)
            return result;
        uint32_t j = 0;
        while (j < supported_count && supported[j] != display)
            ++j;
        if (j == supported_count)
            continue;
        VkDisplayPlaneCapabilitiesKHR caps;
        result = vkGetDisplayPlaneCapabilitiesKHR(gpu, mode, i, &caps);
        if (result != VK_SUCCESS)
            return result;
        if (!(caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR))
            continue;
        VkDisplaySurfaceCreateInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
            .displayMode = mode, .planeIndex = i, .planeStackIndex = planes[i].currentStackIndex,
            .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .globalAlpha = 1.0f, .alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR,
            .imageExtent = extent};
        result = vkCreateDisplayPlaneSurfaceKHR(instance, &info, NULL, surface);
        if (result == VK_SUCCESS) {
            *width = extent.width;
            *height = extent.height;
            fprintf(stderr, "[LA PS5] direct display %ux%u at %u mHz\n", *width, *height, refresh);
        }
        return result;
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}
