#ifndef VULPIX_BASE_APP
#define VULPIX_BASE_APP

#include "../Common.h"
#include "Vulpix_Context.h"	
#include "GLFW/glfw3.h"
#include "Image.h"

struct AppSettings
{
	std::string m_name;
	uint32_t m_resolutionX;
	uint32_t m_resolutionY;
	VkFormat m_surfaceFormat;
	bool m_enableValidationLayers;
	bool m_enableVSync;
	bool m_supportRT;
	bool m_supportDescriptorIndexing;
};

struct FPSCounter
{
	static const size_t m_FPSHistorySize = 128;
	float m_FPSHistory[m_FPSHistorySize] = { 0.0f };
	size_t m_FPSHistoryIndex = 0;
	float m_fpsAccumulator = 0.0f;
	float m_fps = 0.0f;

	void update(const float dt)
	{
		m_fpsAccumulator += dt - m_FPSHistory[m_FPSHistoryIndex];
		m_FPSHistory[m_FPSHistoryIndex] = dt;
		m_FPSHistoryIndex = (m_FPSHistoryIndex + 1) % m_FPSHistorySize;
		m_fps = (m_fpsAccumulator > 0.0f) ? (1.0f / (m_fpsAccumulator / static_cast<float>(m_FPSHistorySize))) : FLT_MAX;
	}
	float getFPS() const { return m_fps; }
	float getFrameTime() const { return 1000.0f / m_fps; }
};

class AppBase
{
public:
	AppBase();
	virtual ~AppBase();

	void run();


protected:
	bool init();
	void mainLoop();
	void shutdown();

	void initDefaultSettings();
	bool initVulkan();
	bool initDevicesAndQueues();
	bool initSwapchain();
	bool initCommandBuffers();
	bool initSynchronization();
	bool initSurface();
	bool initFencesAndCommandPool();
	bool initOffscreenImage();
	void fillCommandBuffers();

	void drawFrame(const float dt);
	void destroyApp();

	// virtual functions
	virtual void initApp();
	virtual void initSettings();
	virtual void freeResources();
	virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	virtual void onMouseMove(const float x, const float y);
	virtual void onMouseButton(const int button, const int action, const int mods);
	virtual void onKeyboard(const int key, const int scancode, const int action, const int mods);
	virtual void update(size_t imageIndex, const float dt);


protected:
	AppSettings m_settings;
	GLFWwindow* m_window;

	VkInstance m_instance;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	VkSurfaceFormatKHR m_surfaceFormat;
	VkSurfaceKHR m_surface;
	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFence> m_waitForFences;
	VkCommandPool m_commandPool;
	Image m_offscreenImage;
	std::vector<VkCommandBuffer> m_commandBuffers;

	VkSemaphore m_imageAcquiredSemaphore;
	VkSemaphore m_renderingCompleteSemaphore;

	uint32_t m_graphicsQueueFamilyIndex;
	uint32_t m_computeQueueFamilyIndex;
	uint32_t m_transferQueueFamilyIndex;
	VkQueue m_graphicsQueue;
	VkQueue m_computeQueue;
	VkQueue m_transferQueue;


	VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties;

	FPSCounter m_FPSCounter;
};


#endif // VULPIX_BASE_APP