#ifndef VULPIX_IMAGE_H
#define VULPIX_IMAGE_H

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring> // for memcpy

#include "Vulpix_Context.h"

class Image
{
public:
	Image();
	~Image();

	void destroyImage();
	bool load(std::string path);
	
	VkResult createImage(VkImageType imageType, VkFormat format, VkExtent3D ext, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	VkResult createImageView(VkImageViewType viewType, VkFormat format, VkImageSubresourceRange subResource);
	VkResult createSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipMapMode ,VkSamplerAddressMode addressMode);

	// getters
	VkImage getImage() const { return m_image; }
	VkImageView getImageView() const { return m_imageView; }
	VkSampler getSampler() const { return m_sampler; }
	VkFormat getFormat() const { return m_format; }

private:
	VkFormat m_format;
	VkImage m_image;
	VkDeviceMemory m_imageMemory;
	VkImageView m_imageView;
	VkSampler m_sampler;
};


#endif // VULPIX_IMAGE_H