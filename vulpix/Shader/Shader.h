#ifndef VULPIX_SHADER_H
#define VULPIX_SHADER_H

#include "../Core/Vulpix_Context.h"
#include "../Common.h"

class Shader
{
public:
	Shader();
	~Shader();

	bool load(std::string path);
	void destroyShader();

	VkPipelineShaderStageCreateInfo getShaderStageInfo(VkShaderStageFlagBits stage) const;

private:
	VkShaderModule m_shaderModule;
};


#endif // VULPIX_SHADER_H