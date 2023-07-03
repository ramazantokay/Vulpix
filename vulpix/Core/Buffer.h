#ifndef VULPIX_BUFFER_H
#define VULPIX_BUFFER_H

#include "Vulpix_Context.h"
//#include <vulkan/vulkan.h>
#include "../Common.h"


class Buffer
{
public:
	Buffer();
	~Buffer();

	VkResult createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void destroyBuffer();
	void *mapMemory(VkDeviceSize size = UINT64_MAX, VkDeviceSize offset = 0);
	void unmapMemory();
	bool uploadData(void *data, VkDeviceSize size, VkDeviceSize offset = 0);

	// getters
	VkBuffer getBuffer() const { return m_buffer; }
	VkDeviceSize getSize() const { return m_size; }

private:
	VkBuffer m_buffer;
	VkDeviceMemory m_bufferMemory;
	VkDeviceSize m_size;
};

#endif // VULPIX_BUFFER_H