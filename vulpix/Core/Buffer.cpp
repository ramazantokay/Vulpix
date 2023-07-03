#include "Buffer.h"

//VulpixContext m_context;

Buffer::Buffer()
{
	m_buffer = VK_NULL_HANDLE;
	m_bufferMemory = VK_NULL_HANDLE;
	m_size = 0;
}

Buffer::~Buffer()
{
	destroyBuffer();
}

VkResult Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	VkResult result = VK_SUCCESS;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.queueFamilyIndexCount = 0;
	bufferCreateInfo.pQueueFamilyIndices = nullptr;

	m_size = size;

	result = vkCreateBuffer(m_context.m_device, &bufferCreateInfo, nullptr, &m_buffer);

	if (result == VK_SUCCESS)
	{
		VkMemoryRequirements memoryRequirements = {};
		vkGetBufferMemoryRequirements(m_context.m_device, m_buffer, &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.pNext = nullptr;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = vulpix::getMemTypeIndex(memoryRequirements, properties);

		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {};
		memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
		memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
		}

		result = vkAllocateMemory(m_context.m_device, &memoryAllocateInfo, nullptr, &m_bufferMemory);
		
		if (result != VK_SUCCESS)
		{
			vkDestroyBuffer(m_context.m_device, m_buffer, nullptr);
			m_buffer = VK_NULL_HANDLE;
			m_bufferMemory = VK_NULL_HANDLE;
		}
		else
		{
			result = vkBindBufferMemory(m_context.m_device, m_buffer, m_bufferMemory, 0);
			if (result != VK_SUCCESS)
			{
				// TODO: changed with destroyBuffer()
				vkDestroyBuffer(m_context.m_device, m_buffer, nullptr);
				vkFreeMemory(m_context.m_device, m_bufferMemory, nullptr);
				m_buffer = VK_NULL_HANDLE;
				m_bufferMemory = VK_NULL_HANDLE;
			}
		}
	}

	return result;

}


void Buffer::destroyBuffer()
{
	if (m_buffer)
	{
		vkDestroyBuffer(m_context.m_device, m_buffer, nullptr);
		m_buffer = VK_NULL_HANDLE;
	}
	if (m_bufferMemory)
	{
		vkFreeMemory(m_context.m_device, m_bufferMemory, nullptr);
		m_bufferMemory = VK_NULL_HANDLE;
	}
}

void* Buffer::mapMemory(VkDeviceSize size, VkDeviceSize offset)
{
	void* memData = nullptr;
	if (size > m_size)
	{
		size = m_size;
	}
	VkResult result = vkMapMemory(m_context.m_device, m_bufferMemory, offset, size, 0, &memData);
	if (VK_SUCCESS != result)
	{
		memData = nullptr;
	}

	return memData;
}

void Buffer::unmapMemory()
{
	vkUnmapMemory(m_context.m_device, m_bufferMemory);
}

bool Buffer::uploadData(void* data, VkDeviceSize size, VkDeviceSize offset)
{
	bool result = false;
	void* memData = mapMemory(size, offset);
	
	if (memData)
	{
		std::memcpy(memData, data, size);
		unmapMemory();
		result = true;
	}
	
	return result;
}
