#ifndef VULPIX_APP_H
#define VULPIX_APP_H

#include "Common.h"
#include "Core/AppBase.h"
#include "Renderer/Camera.h"
#include "Core/ShaderBindingTable.h"
#include "Core/Vulpix_Scene.h"
#include "Core/Image.h"
#include "Core/Buffer.h"
#include "Shader/Shader.h"
#include "Core/Vulpix_Context.h"

#include "Gui/imgui.h"
#include "Gui/imgui_impl_glfw.h"
#include "Gui/imgui_impl_vulkan.h"

#define NUM_DESCRIPTOR_SETS 6

class VulpixApp : public AppBase
{
public:
	VulpixApp();
	~VulpixApp();


protected:
	virtual void initApp() override;
	virtual void initSettings() override;
	virtual void freeResources() override;
	virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) override;

	virtual void onMouseMove(const float x, const float y) override;
	virtual void onMouseButton(const int button, const int action, const int mods) override;
	virtual void onKeyboard(const int key, const int scancode, const int action, const int mods) override;
	virtual void update(size_t imageIndex, const float dt) override;

private:
	void loadScene();
	void createScene();
	void createCamera();
	void updateCamera(struct UniformParams* params,const float dt);
	void createDescriptorSetLayouts();
	void createRTPipelineAndSBT();
	void updateDescriptorSets();
	void renderUI();
	void initImGui();



private:
	std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_pipeline;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;

	VulpixShaderBindingTable m_sbt;
	VulpixScene m_scene;
	Image m_envTexture;
	VkDescriptorImageInfo m_envTextureInfo;

	Camera m_camera;
	Buffer m_cameraBuffer;
	// keyboard and mouse
	bool m_WKeyDown;
	bool m_AKeyDown;
	bool m_SKeyDown;
	bool m_DKeyDown;
	bool m_ShiftDown;
	bool m_LMBDown;

	VkRenderPass m_renderPass;

	vulpix::math::vec2 m_cursorPos;

	// mouse and key settings
	float m_moveSpeed = 25.0f;
	float m_rotationSpeed = 0.25f;
	float m_mouseSensitivity = 15.0f;

};


#endif // VULPIX_APP_H