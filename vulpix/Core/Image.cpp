#include "Image.h"
#include "Buffer.h"

#define STB_IMAGE_IMPLEMENTATION
// excluding old and unusefull formats
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

#include "stb_image.h"
#include <vulkan/vulkan.h>

//VulpixContext m_context;


Image::Image()
{
	m_format = VK_FORMAT_B8G8R8A8_UNORM;
	m_image = VK_NULL_HANDLE;
	m_imageMemory = VK_NULL_HANDLE;
	m_imageView = VK_NULL_HANDLE;
	m_sampler = VK_NULL_HANDLE;
}

Image::~Image()
{
	destroyImage();
}

void Image::destroyImage()
{
	if (m_image)
	{
		vkDestroyImage(m_context.m_device, m_image, nullptr);
		m_image = VK_NULL_HANDLE;
	}
	if (m_imageMemory)
	{
		vkFreeMemory(m_context.m_device, m_imageMemory, nullptr);
		m_imageMemory = VK_NULL_HANDLE;
	}
	if (m_imageView)
	{
		vkDestroyImageView(m_context.m_device, m_imageView, nullptr);
		m_imageView = VK_NULL_HANDLE;
	}
	if (m_sampler)
	{
		vkDestroySampler(m_context.m_device, m_sampler, nullptr);
		m_sampler = VK_NULL_HANDLE;
	}
}

bool Image::load(std::string path)
{
	int texWidth, texHeight, texChannels;
	bool textHDR = false;
	stbi_uc* imageData = nullptr;

	std::string fileName(path);
	std::string ext = fileName.substr(fileName.find_last_of(".") + 1);

	if (ext == "hdr")
	{
		textHDR = true;
		imageData = reinterpret_cast<stbi_uc*>(stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha));
	}
	else
	{
		imageData = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	}

	if (imageData)
	{
		int bpp = textHDR ? sizeof(float[4]) : sizeof(uint8_t[4]);
		VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * bpp);

		Buffer stagingBuffer;
		VkResult error = stagingBuffer.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		if (error == VK_SUCCESS && stagingBuffer.uploadData(imageData, imageSize))
		{
			stbi_image_free(imageData);

			VkExtent3D imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };

			const VkFormat format = textHDR ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_SRGB; // not UNOM ramazan, SRGB is better for color

			error = createImage(VK_IMAGE_TYPE_2D, format, imageExtent, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (error != VK_SUCCESS)
			{
				return false;
			}
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = m_context.m_CommandPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			error = vkAllocateCommandBuffers(m_context.m_device, &allocInfo, &commandBuffer);
			if (error != VK_SUCCESS)
			{
				return false;
			}

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			error = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (error != VK_SUCCESS)
			{
				return false;
			}

			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_image;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			VkBufferImageCopy region = {};
			region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.imageExtent = imageExtent;

			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.getBuffer(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
			
			error = vkEndCommandBuffer(commandBuffer);
			if (error != VK_SUCCESS)
			{
				vkFreeCommandBuffers(m_context.m_device, m_context.m_CommandPool, 1, &commandBuffer);
				return false;
			}

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			error = vkQueueSubmit(m_context.m_transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
			if (error !=VK_SUCCESS)
			{
				vkFreeCommandBuffers(m_context.m_device, m_context.m_CommandPool, 1, &commandBuffer);
				return false;
			}

			error = vkQueueWaitIdle(m_context.m_transferQueue);
			if (error != VK_SUCCESS)
			{
				vkFreeCommandBuffers(m_context.m_device, m_context.m_CommandPool, 1, &commandBuffer);
				return false;
			}
			vkFreeCommandBuffers(m_context.m_device, m_context.m_CommandPool, 1, &commandBuffer);
		}
		else
		{
			stbi_image_free(imageData);
		}

	}
	return true;

}

VkResult Image::createImage(VkImageType imageType, VkFormat format, VkExtent3D ext, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
	m_format = format;

	VkResult result = VK_SUCCESS;

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = imageType;
	imageInfo.format = format;
	imageInfo.extent = ext;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	try
	{
		VkDevice tempDevice = m_context.m_device;

		result = vkCreateImage(tempDevice, &imageInfo, nullptr, &m_image);

	}
	catch (const std::exception& e)
	{
		std::cout << "Error creating image" << e.what() << std::endl;
	}
	if (result == VK_SUCCESS)
	{
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_context.m_device, m_image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vulpix::getMemTypeIndex(memRequirements, properties);

		result = vkAllocateMemory(m_context.m_device, &allocInfo, nullptr, &m_imageMemory);

		if (result != VK_SUCCESS)
		{
			vkDestroyImage(m_context.m_device, m_image, nullptr);
			m_image = VK_NULL_HANDLE;
			m_imageMemory = VK_NULL_HANDLE;
		}
		else
		{
			result = vkBindImageMemory(m_context.m_device, m_image, m_imageMemory, 0);
			if (result != VK_SUCCESS)
			{
				vkDestroyImage(m_context.m_device, m_image, nullptr);
				vkFreeMemory(m_context.m_device, m_imageMemory, nullptr);
				m_image = VK_NULL_HANDLE;
				m_imageMemory = VK_NULL_HANDLE;
			}
		}
	
	}

	return result;
}


VkResult Image::createImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subResource)
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_image;
	viewInfo.viewType = viewType;
	viewInfo.format = format;
	viewInfo.subresourceRange = subResource;
	viewInfo.pNext = nullptr;
	viewInfo.flags = 0;
	viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	return vkCreateImageView(m_context.m_device, &viewInfo, nullptr, &m_imageView);
}

VkResult Image::createSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipMapMode, VkSamplerAddressMode addressMode)
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = magFilter;
	samplerInfo.minFilter = minFilter;
	samplerInfo.mipmapMode = mipMapMode;
	samplerInfo.addressModeU = addressMode;
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = 0;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.pNext = nullptr;
	samplerInfo.flags = 0;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	return vkCreateSampler(m_context.m_device, &samplerInfo, nullptr, &m_sampler);
}
