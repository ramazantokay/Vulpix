#ifndef VULPIX_SHADER_BINDING_TABLE_H
#define VULPIX_SHADER_BINDING_TABLE_H

#include "../Common.h"
#include "Vulpix_Context.h"
#include "Buffer.h"

class VulpixShaderBindingTable
{
public:
	VulpixShaderBindingTable();
	~VulpixShaderBindingTable() = default;

	void initSBT(const uint32_t numHitGroups, const uint32_t numMissGroups, const uint32_t shaderHandleSize, const uint32_t shaderGroupAlignment);
	bool createSBT(VkDevice device, VkPipeline rtPipeline);
	void destroySBT();

	void setRaygenStage(const VkPipelineShaderStageCreateInfo& stage);
	void addStageToHitGroup(const std::vector<VkPipelineShaderStageCreateInfo>& stages, const uint32_t groupIndex);
	void addStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, const uint32_t groupIndex);

	uint32_t getGroupsStride() const { return m_shaderGroupAlignment; }
	uint32_t getNumGroups() const { return m_numHitGroups + m_numMissGroups + 1; }
	uint32_t getRaygenOffset() const { return 0; }
	uint32_t getRaygenSize() const { return m_shaderGroupAlignment; }
	uint32_t getHitGroupsOffset() const { return getRaygenOffset() + getRaygenSize(); }//tODO: will be changed
	uint32_t getHitGroupsSize() const { return m_numHitGroups * m_shaderGroupAlignment; }
	uint32_t getMissGroupsOffset() const { return getHitGroupsSize() + getHitGroupsOffset(); }
	uint32_t getMissGroupsSize() const { return m_numMissGroups * m_shaderGroupAlignment; }
	uint32_t getSBTSize() const { return getNumGroups() * m_shaderGroupAlignment; }
	uint32_t getNumStages() const { return static_cast<uint32_t>(m_shaderStages.size()); }
	
	VkDeviceAddress getSBTAddress() const { return vulpix::getBufferDeviceAddress(m_sbtBuffer).deviceAddress; }
	const VkPipelineShaderStageCreateInfo* getStages() const { return m_shaderStages.data(); }
	const VkRayTracingShaderGroupCreateInfoKHR* getGroups() const { return m_shaderGroups.data(); }



private:
	Buffer m_sbtBuffer;

	std::vector<uint32_t> m_numHitShaders;
	std::vector<uint32_t>m_numMissShaders;

	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shaderGroups;

	uint32_t m_shaderHandleSize;
	uint32_t m_shaderGroupAlignment;
	uint32_t m_numHitGroups;
	uint32_t m_numMissGroups;

};



#endif // VULPIX_SHADER_BINDING_TABLE_H
