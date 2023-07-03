#include "ShaderBindingTable.h"

bool VulpixShaderBindingTable::createSBT(VkDevice device, VkPipeline rtPipeline)
{
    const size_t sbtSize = this->getSBTSize();

    VkResult error = m_sbtBuffer.createBuffer(sbtSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "mSBT.Create");

    if (VK_SUCCESS != error) {
        return false;
    }

    // get shader group handles
    std::vector<uint8_t> groupHandles(this->getNumGroups() * m_shaderHandleSize);
    error = vkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, this->getNumGroups(), groupHandles.size(), groupHandles.data());
    CHECK_VK_ERROR(error, L"vkGetRayTracingShaderGroupHandlesKHR");

    // now we fill our SBT
    uint8_t* mem = static_cast<uint8_t*>(m_sbtBuffer.mapMemory());
    for (size_t i = 0; i < this->getNumGroups(); ++i) {
        memcpy(mem, groupHandles.data() + i * m_shaderHandleSize, m_shaderHandleSize);
        mem += m_shaderGroupAlignment;
    }
    m_sbtBuffer.unmapMemory();

    return (VK_SUCCESS == error);
}

VulpixShaderBindingTable::VulpixShaderBindingTable()
{
    m_shaderHandleSize = 0u;
    m_shaderGroupAlignment = 0u;
    m_numHitGroups = 0u;
    m_numMissGroups = 0u;
}

void VulpixShaderBindingTable::initSBT(const uint32_t numHitGroups, const uint32_t numMissGroups, const uint32_t shaderHandleSize, const uint32_t shaderGroupAlignment)
{
    m_numHitGroups = numHitGroups;
	m_numMissGroups = numMissGroups;
	m_shaderHandleSize = shaderHandleSize;
	m_shaderGroupAlignment = shaderGroupAlignment;

    m_numHitShaders.resize(m_numHitGroups,0u);
    m_numMissShaders.resize(m_numMissGroups,0u);

    m_shaderStages.clear();
    m_shaderGroups.clear();

}

void VulpixShaderBindingTable::destroySBT()
{
    m_numHitShaders.clear();
    m_numMissShaders.clear();
    m_shaderStages.clear();
    m_shaderGroups.clear();

    m_sbtBuffer.destroyBuffer();
}

void VulpixShaderBindingTable::setRaygenStage(const VkPipelineShaderStageCreateInfo& stage)
{
    assert(m_shaderStages.empty());
    m_shaderStages.push_back(stage);

    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groupInfo.generalShader = 0;
    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
    m_shaderGroups.push_back(groupInfo);

}

void VulpixShaderBindingTable::addStageToHitGroup(const std::vector<VkPipelineShaderStageCreateInfo>& stages, const uint32_t groupIndex)
{
    assert(!m_shaderStages.empty());
    assert(groupIndex < m_numHitShaders.size());
	assert(!m_shaderStages.empty() && m_shaderStages.size() <= 3);
	assert(m_numHitShaders[groupIndex] == 0);

    uint32_t offset = 1; // there's always raygen shader

    for (uint32_t i = 0; i <= groupIndex; ++i) {
        offset += m_numHitShaders[i];
    }

    auto itStage = m_shaderStages.begin() + offset;
    m_shaderStages.insert(itStage, stages.begin(), stages.end());

    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    for (size_t i = 0; i < stages.size(); i++) {
        const VkPipelineShaderStageCreateInfo& stageInfo = stages[i];
        const uint32_t shaderIdx = static_cast<uint32_t>(offset + i);

        if (stageInfo.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) {
            groupInfo.closestHitShader = shaderIdx;
        }
        else if (stageInfo.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR) {
            groupInfo.anyHitShader = shaderIdx;
        }
    };

    m_shaderGroups.insert((m_shaderGroups.begin() + 1 + groupIndex), groupInfo);

    m_numHitShaders[groupIndex] += static_cast<uint32_t>(stages.size());
}

void VulpixShaderBindingTable::addStageToMissGroup(const VkPipelineShaderStageCreateInfo& stage, const uint32_t groupIndex)
{
    // raygen stage should go first!
    assert(!m_shaderStages.empty());

    assert(groupIndex < m_numMissShaders.size());
    assert(m_numMissShaders[groupIndex] == 0); // only 1 miss shader per group

    uint32_t offset = 1; // there's always raygen shader

    // now skip all hit shaders
    for (const uint32_t numHitShader : m_numHitShaders) {
        offset += numHitShader;
    }

    for (uint32_t i = 0; i <= groupIndex; ++i) {
        offset += m_numMissShaders[i];
    }

    m_shaderStages.insert(m_shaderStages.begin() + offset, stage);

    // group create info 
    VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
    groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groupInfo.generalShader = offset;
    groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
    groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

    // group 0 is always for raygen, then go hit shaders
    m_shaderGroups.insert((m_shaderGroups.begin() + (groupIndex + 1 + m_numHitGroups)), groupInfo);

    m_numMissShaders[groupIndex]++;
}

