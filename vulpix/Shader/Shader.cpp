#include "Shader.h"

Shader::Shader()
{
	m_shaderModule = VK_NULL_HANDLE;
}

Shader::~Shader()
{
	destroyShader();
}

bool Shader::load(std::string path)
{
	bool result = false;

	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);

	if (file)
	{
		file.seekg(0, std::ios::end);
		size_t fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

        std::vector<char> bytecode(fileSize);
        bytecode.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        VkShaderModuleCreateInfo shaderModuleCreateInfo;
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.codeSize = bytecode.size();
        shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(bytecode.data());
        shaderModuleCreateInfo.flags = 0;

        const VkResult error = vkCreateShaderModule(m_context.m_device, &shaderModuleCreateInfo, nullptr, &m_shaderModule);
        result = (VK_SUCCESS == error);
    }

    return result;
}

void Shader::destroyShader()
{
    if (m_shaderModule)
    {
        vkDestroyShaderModule(m_context.m_device, m_shaderModule, nullptr);
        m_shaderModule = VK_NULL_HANDLE;
    }
}

VkPipelineShaderStageCreateInfo Shader::getShaderStageInfo(VkShaderStageFlagBits stage) const
{
    return VkPipelineShaderStageCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		stage,
		m_shaderModule,
		"main",
		nullptr
    };
}
