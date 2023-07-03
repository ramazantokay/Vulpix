#ifndef VULPIX_SCENE_H
#define VULPIX_SCENE_H

#include "Vulpix_Mesh.h"
#include "Vulpix_Material.h"
#include "VulpixAS.h"

class VulpixScene
{
public:
	std::vector<VulpixMesh> m_meshes;
	std::vector<VulpixMaterial> m_materials;
	VulpixAccelerationStructure m_TLAS;

	std::vector< VkDescriptorBufferInfo> m_matBufferInfos;
	std::vector< VkDescriptorImageInfo> m_texBufferInfos;
	std::vector< VkDescriptorBufferInfo> m_attributesBufferInfos;
	std::vector< VkDescriptorBufferInfo> m_facesBufferInfos;


public:
	void buildTLAS(VkDevice device, VkCommandPool cPool, VkQueue queue);
	void buildBLAS(VkDevice device, VkCommandPool cPool, VkQueue queue);
};


#endif // VULPIX_SCENE_H