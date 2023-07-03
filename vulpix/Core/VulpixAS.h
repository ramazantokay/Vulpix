#ifndef VULPIXAS_H
#define VULPIXAS_H

#include "Buffer.h"

class VulpixAccelerationStructure
{
public:
	Buffer m_Buffer;
	VkAccelerationStructureKHR m_AccelerationStructure;
	VkDeviceAddress m_DeviceAddress;
};

#endif	// VULPIXAS_H