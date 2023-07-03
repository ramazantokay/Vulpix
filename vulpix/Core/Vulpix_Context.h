#ifndef VULPIX_CONTEXT_H
#define VULPIX_CONTEXT_H


#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "volk.h"

#include "Buffer.h"
class Buffer;

#include <cassert>

#define CHECK_VK_ERROR(_error, _message) do {   \
    if (VK_SUCCESS != error) {                  \
        assert(false && _message);              \
    }                                           \
} while (false);

struct VulpixContext
{
	VkDevice m_device;
	VkPhysicalDevice m_physicalDevice;
	VkCommandPool m_CommandPool;
	VkQueue m_transferQueue;
	VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;

};

extern VulpixContext m_context;


namespace vulpix
{
    void initializeContext(VkDevice device, VkCommandPool commandPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice);
    void imageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageSubresourceRange& subresourceRange, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageLayout oldL, VkImageLayout newL);
	uint32_t getMemTypeIndex(VkMemoryRequirements requirement, VkMemoryPropertyFlags properties);

	VkDeviceOrHostAddressKHR getBufferDeviceAddress(const Buffer& buff);

	VkDeviceOrHostAddressConstKHR getBufferDeviceAddressConst(const Buffer& buff);
 
}// namespace vulpix

#endif // VULPIX_CONTEXT_H