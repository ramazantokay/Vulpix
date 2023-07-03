#include "Vulpix_Context.h"

namespace vulpix
{

void initializeContext(VkDevice device, VkCommandPool commandPool, VkQueue transferQueue, VkPhysicalDevice physicalDevice)
{
	m_context.m_device = device;
	m_context.m_CommandPool = commandPool;
	m_context.m_transferQueue = transferQueue;
	m_context.m_physicalDevice = physicalDevice;

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_context.m_physicalDeviceMemoryProperties);
}

void imageBarrier(VkCommandBuffer commandBuffer, VkImage image, VkImageSubresourceRange& subresourceRange, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageLayout oldL, VkImageLayout newL)
{
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcAccessMask = srcMask;
	imageMemoryBarrier.dstAccessMask = dstMask;
	imageMemoryBarrier.oldLayout = oldL;
	imageMemoryBarrier.newLayout = newL;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

uint32_t getMemTypeIndex(VkMemoryRequirements requirement, VkMemoryPropertyFlags properties)
{
	uint32_t result = 0;

	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if (requirement.memoryTypeBits & (1 << i))
		{
			if ((m_context.m_physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				result = i; 
				break;
			}
		}
	}
	return result;
}

VkDeviceOrHostAddressKHR getBufferDeviceAddress(const Buffer& buff)
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		nullptr,
		buff.getBuffer()
	};

	VkDeviceOrHostAddressKHR res = {};
	res.deviceAddress = vkGetBufferDeviceAddress(m_context.m_device, &bufferDeviceAddressInfo);
	return res;
}

VkDeviceOrHostAddressConstKHR getBufferDeviceAddressConst(const Buffer& buff)
{
	VkDeviceOrHostAddressKHR address = getBufferDeviceAddress(buff);

	VkDeviceOrHostAddressConstKHR res = {};
	res.deviceAddress = address.deviceAddress;
	return res;
}


} // namespace vulpix