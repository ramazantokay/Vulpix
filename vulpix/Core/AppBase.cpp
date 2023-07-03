#include "AppBase.h"
#include "volk.c"
#include <algorithm>

VulpixContext m_context;


AppBase::AppBase()
{
	m_settings = {};
	m_window = nullptr;
	
	m_instance = VK_NULL_HANDLE;
	m_physicalDevice = VK_NULL_HANDLE;
	m_device = VK_NULL_HANDLE;
	m_surfaceFormat = {};
	m_surface= VK_NULL_HANDLE;
	m_swapchain = VK_NULL_HANDLE;
	m_commandPool = VK_NULL_HANDLE;
	
	m_imageAcquiredSemaphore = VK_NULL_HANDLE;
	m_renderingCompleteSemaphore = VK_NULL_HANDLE;
	
	m_graphicsQueueFamilyIndex = 0u;
	m_computeQueueFamilyIndex = 0u;
	m_transferQueueFamilyIndex = 0u;
	m_graphicsQueue = VK_NULL_HANDLE;
	m_computeQueue = VK_NULL_HANDLE;
	m_transferQueue = VK_NULL_HANDLE;

}

AppBase::~AppBase()
{
	destroyApp();
}

void AppBase::run()
{
	if (init())
	{
		mainLoop();
		shutdown();
		freeResources();
	}
}

bool AppBase::init()
{
	if (!glfwInit())
	{
		return false;
	}

	if (!glfwVulkanSupported())
	{
		return false;
	}
	if (volkInitialize() != VK_SUCCESS)
	{
		return false;
	}

	initDefaultSettings();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow *window = glfwCreateWindow(static_cast<int>(m_settings.m_resolutionX),
		static_cast<int>(m_settings.m_resolutionY),
		m_settings.m_name.c_str(),
		nullptr, nullptr);

	if (!window)
	{
		return false;
	}

	glfwSetWindowUserPointer(window, this);

	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
		AppBase* app = reinterpret_cast<AppBase*>(glfwGetWindowUserPointer(window));
		app->onKeyboard(key, scancode, action, mods);
	});

	glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods)
		{
		AppBase* app = reinterpret_cast<AppBase*>(glfwGetWindowUserPointer(window));
		app->onMouseButton(button, action, mods);
	});

	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
		{
		AppBase* app = reinterpret_cast<AppBase*>(glfwGetWindowUserPointer(window));
		app->onMouseMove(static_cast<float>(x), static_cast<float>(y));
	});

	m_window = window;

	if (!initVulkan())
	{
		return false;
	}

	volkLoadInstance(m_instance);

	if (!initDevicesAndQueues())
	{
		return false;
	}

	if (!initSurface())
	{
		return false;
	}

	if (!initSwapchain())
	{
		return false;
	}

	if (!initFencesAndCommandPool())
	{
		return false;
	}

	vulpix::initializeContext(m_device, m_commandPool, m_graphicsQueue, m_physicalDevice);
	
	//VkDevice tempDevice = m_device;
	//VkDevice tempDevice2 = m_context.m_device;

	//std::cout <<"DEViCEee" << m_device << std::endl;

	if (!initOffscreenImage())
	{
		return false;
	}

	if (!initCommandBuffers())
	{
		return false;
	}
	if (!initSynchronization())
	{
		return false;
	}
	initApp();
	fillCommandBuffers();

	return true;
}

void AppBase::mainLoop()
{
	glfwSetTime(0.0);
	double currTime;
	double prevTime = 0.0;
	double dt = 0.0;


	while (!glfwWindowShouldClose(m_window))
	{
		currTime = glfwGetTime();
		dt = currTime - prevTime;
		prevTime = currTime;
		
		drawFrame(static_cast<float>(dt));
	
		glfwPollEvents();
	}
}

void AppBase::shutdown()
{
	vkDeviceWaitIdle(m_device);
	glfwTerminate();
}

void AppBase::initDefaultSettings()
{
	m_settings.m_name = "Vulpix App";
	m_settings.m_resolutionX = 1280u;
	m_settings.m_resolutionY = 720u;
	m_settings.m_surfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;
	m_settings.m_enableValidationLayers = false;
	m_settings.m_enableVSync = true;
	m_settings.m_supportRT = false;
	m_settings.m_supportDescriptorIndexing = false;

	// virtual setting
	initSettings();
}

bool AppBase::initVulkan()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = m_settings.m_name.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Vulpix";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_2;

	uint32_t requiredInstanceExtensionCount = 0u;
	const char** requiredInstanceExtensions = glfwGetRequiredInstanceExtensions(&requiredInstanceExtensionCount);

	std::vector<const char*> instanceExtensions;
	std::vector<const char*> instanceLayers;

	instanceExtensions.insert(instanceExtensions.begin(), requiredInstanceExtensions, requiredInstanceExtensions + requiredInstanceExtensionCount);

	if (m_settings.m_enableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
	}

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.flags = 0u;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	instanceCreateInfo.ppEnabledLayerNames = instanceLayers.data();
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());

	VkResult error = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
	if (error != VK_SUCCESS)
	{
		CHECK_VK_ERROR(error, "vkCreateInstance fail");
		return false;
	}

	return true;
}

bool AppBase::initDevicesAndQueues()
{
	
	uint32_t physicalDeviceCount = 0u;
	VkResult error = vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
	if (error != VK_SUCCESS || !physicalDeviceCount)
	{
		CHECK_VK_ERROR(error, "vkEnumeratePhysicalDevices fail");
		return false;
	}

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());
	m_physicalDevice = physicalDevices[0];

	const VkQueueFlagBits askingFlags[3] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
	uint32_t queuesIndex[3] = { UINT32_MAX, UINT32_MAX, UINT32_MAX };
	
	uint32_t queueFamilyCount = 0u;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	for (size_t i = 0; i < 3; ++i)
	{
		const VkQueueFlagBits flag = askingFlags[i];
		uint32_t& queueIdx = queuesIndex[i];

		if (flag == VK_QUEUE_COMPUTE_BIT) {
			for (uint32_t j = 0; j < queueFamilyCount; ++j) {
				if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
					queueIdx = j;
					break;
				}
			}
		}
		else if (flag == VK_QUEUE_TRANSFER_BIT) {
			for (uint32_t j = 0; j < queueFamilyCount; ++j) {
				if ((queueFamilyProperties[j].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
					!(queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
					queueIdx = j;
					break;
				}
			}
		}

		if (queueIdx == ~0u) {
			for (uint32_t j = 0; j < queueFamilyCount; ++j) {
				if (queueFamilyProperties[j].queueFlags & flag) {
					queueIdx = j;
					break;
				}
			}
		}
	}

	m_graphicsQueueFamilyIndex = queuesIndex[0];
	m_computeQueueFamilyIndex = queuesIndex[1];
	m_transferQueueFamilyIndex = queuesIndex[2];

	// device creation
	std::vector<VkDeviceQueueCreateInfo> deviceQeueuCreateInfos;
	float priority = 0.0f;

	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &priority;
	deviceQeueuCreateInfos.push_back(deviceQueueCreateInfo);

	if (m_graphicsQueueFamilyIndex != m_computeQueueFamilyIndex)
	{
		deviceQueueCreateInfo.queueFamilyIndex = m_computeQueueFamilyIndex;
		deviceQeueuCreateInfos.push_back(deviceQueueCreateInfo);
	}

	if (m_graphicsQueueFamilyIndex != m_transferQueueFamilyIndex && m_computeQueueFamilyIndex != m_transferQueueFamilyIndex)
	{
		deviceQueueCreateInfo.queueFamilyIndex = m_transferQueueFamilyIndex;
		deviceQeueuCreateInfos.push_back(deviceQueueCreateInfo);
	}

	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
	physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipeline = {};
	rayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR rayTracingStructure = {};
	rayTracingStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress = {};
	bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

	std::vector<const char*> deviceExtensions = { { VK_KHR_SWAPCHAIN_EXTENSION_NAME } };

	if (m_settings.m_supportRT)
	{
		deviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		deviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

		if (!m_settings.m_supportDescriptorIndexing)
		{
			m_settings.m_supportDescriptorIndexing = true;
		}

		bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;
		rayTracingPipeline.pNext = &bufferDeviceAddress;
		rayTracingPipeline.rayTracingPipeline = VK_TRUE;
		rayTracingStructure.pNext = &rayTracingPipeline;
		rayTracingStructure.accelerationStructure = VK_TRUE;
		physicalDeviceFeatures2.pNext = &rayTracingStructure;
	}

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing = { };
	descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

	if (m_settings.m_supportDescriptorIndexing)
	{
		deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		if (physicalDeviceFeatures2.pNext)
		{
			descriptorIndexing.pNext = physicalDeviceFeatures2.pNext;
		}

		physicalDeviceFeatures2.pNext = &descriptorIndexing;
	}


	vkGetPhysicalDeviceFeatures2(m_physicalDevice, &physicalDeviceFeatures2);

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &physicalDeviceFeatures2;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQeueuCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = deviceQeueuCreateInfos.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = nullptr;

	error = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
	if (error != VK_SUCCESS)
	{
		CHECK_VK_ERROR(error, "vkCreateDevice");
		return false;
	}

	// get queues
	vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, m_computeQueueFamilyIndex, 0, &m_computeQueue);
	vkGetDeviceQueue(m_device, m_transferQueueFamilyIndex, 0, &m_transferQueue);

	if (m_settings.m_supportRT)
	{
		m_rayTracingPipelineProperties = {  };
		m_rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

		VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {};
		physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		physicalDeviceProperties2.pNext = &m_rayTracingPipelineProperties;
		physicalDeviceProperties2.properties = { };

		vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties2);
	}

	return true;
}

bool AppBase::initSwapchain()
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);
	if (error != VK_SUCCESS)
	{
		CHECK_VK_ERROR(error, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		return false;
	}

	m_settings.m_resolutionX = std::clamp(m_settings.m_resolutionX, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.currentExtent.width);
	m_settings.m_resolutionY = std::clamp(m_settings.m_resolutionY, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.currentExtent.height);

	uint32_t numMode;
	error = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &numMode, nullptr);
	std::vector<VkPresentModeKHR> presentModes(numMode);
	error = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &numMode, presentModes.data());

	// finding best present mode
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

	if (!m_settings.m_enableVSync)
	{
		// if there is no vsync, we need to find the best present mode
		for (const VkPresentModeKHR mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = mode;
				break;
			}
			else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				presentMode = mode;
			}
		}
	}

	VkSwapchainKHR oldSwapchain = m_swapchain;

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = m_surface;
	swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
	swapchainCreateInfo.imageFormat = m_surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = m_surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = { m_settings.m_resolutionX, m_settings.m_resolutionY };
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = oldSwapchain;

	error = vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain);
	if (error != VK_SUCCESS)
	{
		CHECK_VK_ERROR(error, "vkCreateSwapchainKHR");
		return false;
	}

	if (oldSwapchain)
	{
		for (VkImageView& imageV : m_swapchainImageViews)
		{
			vkDestroyImageView(m_device, imageV, nullptr);
			imageV = VK_NULL_HANDLE;
		}
		vkDestroySwapchainKHR(m_device, oldSwapchain, nullptr);
	}

	uint32_t numSwapchainImage;
	error = vkGetSwapchainImagesKHR(m_device, m_swapchain, &numSwapchainImage, nullptr);
	m_swapchainImages.resize(numSwapchainImage);
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &numSwapchainImage, m_swapchainImages.data());

	m_swapchainImageViews.resize(numSwapchainImage);

	for (uint32_t i = 0; i < numSwapchainImage; ++i) {
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.format = m_surfaceFormat.format;
		imageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.image = m_swapchainImages[i];
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.components = { };

		error = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_swapchainImageViews[i]);
		if (VK_SUCCESS != error) {
			return false;
		}
	}

	return true;

}

bool AppBase::initCommandBuffers()
{
	m_commandBuffers.resize(m_swapchainImages.size());

	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = m_commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

	const VkResult error = vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, m_commandBuffers.data());
	return (VK_SUCCESS == error); 
}

bool AppBase::initSynchronization()
{
	VkSemaphoreCreateInfo semaphoreCreatInfo;
	semaphoreCreatInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreatInfo.pNext = nullptr;
	semaphoreCreatInfo.flags = 0;

	VkResult error = vkCreateSemaphore(m_device, &semaphoreCreatInfo, nullptr, &m_imageAcquiredSemaphore);
	if (VK_SUCCESS != error) {
		return false;
	}

	error = vkCreateSemaphore(m_device, &semaphoreCreatInfo, nullptr, &m_renderingCompleteSemaphore);
	return (VK_SUCCESS == error);
}

bool AppBase::initSurface()
{
	VkResult error = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
	if (error != VK_SUCCESS)
	{
		CHECK_VK_ERROR(error, "glfwCreateWindowSurface");
		return false;
	}

	VkBool32 supportedFlag = VK_FALSE;
	error = vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueFamilyIndex, m_surface, &supportedFlag);
	if (error != VK_SUCCESS || !supportedFlag)
	{
		CHECK_VK_ERROR(error, "vkGetPhysicalDeviceSurfaceSupportKHR");
		return false;
	}

	uint32_t numFormat;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &numFormat, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(numFormat);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &numFormat, surfaceFormats.data());

	if (numFormat == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		m_surfaceFormat.format = m_settings.m_surfaceFormat;
		m_surfaceFormat.colorSpace = surfaceFormats[0].colorSpace;
	}
	else
	{
		bool flagFound = false;
		for (const VkSurfaceFormatKHR& format : surfaceFormats)
		{
			if (format.format==m_settings.m_surfaceFormat)
			{
				m_surfaceFormat = format;
				flagFound = true;
				break;
			}
		}
		if (!flagFound)
		{
			m_surfaceFormat = surfaceFormats[0];
		}
	}

	return true;
}

bool AppBase::initFencesAndCommandPool()
{
	VkFenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	m_waitForFences.resize(m_swapchainImages.size());
	for (VkFence& fence: m_waitForFences)
	{
		//VkResult error = 
		vkCreateFence(m_device, &fenceCreateInfo, nullptr, &fence);
		/*	if (error != VK_SUCCESS)
			{
			CHECK_VK_ERROR(error, "vkCreateFence");
			return false;
		}*/
	}

	VkCommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

	VkResult error = vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool);
	return error == VK_SUCCESS;

}

bool AppBase::initOffscreenImage()
{
	const VkExtent3D extent = { m_settings.m_resolutionX, m_settings.m_resolutionY, 1 };

	VkResult error = m_offscreenImage.createImage(VK_IMAGE_TYPE_2D,
		m_surfaceFormat.format,
		extent,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (VK_SUCCESS != error) {
		return false;
	}

	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	error = m_offscreenImage.createImageView(VK_IMAGE_VIEW_TYPE_2D, m_surfaceFormat.format, range);
	return (VK_SUCCESS == error);
}

void AppBase::fillCommandBuffers()
{
	// TODO: will be checked later

	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	for (size_t i = 0; i < m_commandBuffers.size(); i++) {
		const VkCommandBuffer commandBuffer = m_commandBuffers[i];

		VkResult error = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		CHECK_VK_ERROR(error, "vkBeginCommandBuffer");

		vulpix::imageBarrier(commandBuffer,
			m_offscreenImage.getImage(),
			subresourceRange,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL);

		fillCommandBuffer(commandBuffer, static_cast<uint32_t>(i)); // user draw code

		vulpix::imageBarrier(commandBuffer,
			m_swapchainImages[i],
			subresourceRange,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vulpix::imageBarrier(commandBuffer,
			m_offscreenImage.getImage(),
			subresourceRange,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageCopy copyRegion;
		copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent = { m_settings.m_resolutionX, m_settings.m_resolutionY, 1 };
		vkCmdCopyImage(commandBuffer,
			m_offscreenImage.getImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_swapchainImages[i],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		vulpix::imageBarrier(commandBuffer,
			m_swapchainImages[i], subresourceRange,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			0,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		error = vkEndCommandBuffer(commandBuffer);
		CHECK_VK_ERROR(error, "vkEndCommandBuffer");
	}
}

void AppBase::drawFrame(const float dt)
{
	m_FPSCounter.update(dt);

	uint32_t imageIndex = 0;
	VkResult error = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAcquiredSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (VK_SUCCESS != error) {
		return;
	}

	const VkFence fence = m_waitForFences[imageIndex];
	error = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
	if (VK_SUCCESS != error) {
		return;
	}
	vkResetFences(m_device, 1, &fence);

	update(imageIndex, dt);


	const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;


	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAcquiredSemaphore;
	submitInfo.pWaitDstStageMask = &waitStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderingCompleteSemaphore;

	error = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence);
	if (VK_SUCCESS != error) {
		return;
	}

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderingCompleteSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	error = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
	if (VK_SUCCESS != error) {
		return;
	}

}

void AppBase::destroyApp()
{
	if (m_renderingCompleteSemaphore) {
		vkDestroySemaphore(m_device, m_renderingCompleteSemaphore, nullptr);
		m_renderingCompleteSemaphore = VK_NULL_HANDLE;
	}

	if (m_imageAcquiredSemaphore) {
		vkDestroySemaphore(m_device, m_imageAcquiredSemaphore, nullptr);
		m_imageAcquiredSemaphore = VK_NULL_HANDLE;
	}

	if (!m_commandBuffers.empty()) {
		vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
		m_commandBuffers.clear();
	}

	if (m_commandPool) {
		vkDestroyCommandPool(m_device, m_commandPool, nullptr);
		m_commandPool = VK_NULL_HANDLE;
	}

	for (VkFence& fence : m_waitForFences) {
		vkDestroyFence(m_device, fence, nullptr);
	}
	m_waitForFences.clear();

	m_offscreenImage.destroyImage();

	for (VkImageView& view : m_swapchainImageViews) {
		vkDestroyImageView(m_device, view, nullptr);
	}
	m_swapchainImageViews.clear();
	m_swapchainImageViews.clear();

	if (m_swapchain) {
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}

	if (m_surface) {
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		m_surface = VK_NULL_HANDLE;
	}

	if (m_device) {
		vkDestroyDevice(m_device, nullptr);
		m_device = VK_NULL_HANDLE;
	}

	if (m_instance) {
		vkDestroyInstance(m_instance, nullptr);
		m_instance = VK_NULL_HANDLE;
	}
}




// virtuals
void AppBase::initApp()
{

}

void AppBase::initSettings()
{
}

void AppBase::freeResources()
{
}

void AppBase::fillCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
}

void AppBase::onMouseMove(const float x, const float y)
{
}

void AppBase::onMouseButton(const int button, const int action, const int mods)
{
}

void AppBase::onKeyboard(const int key, const int scancode, const int action, const int mods)
{
}

void AppBase::update(size_t imageIndex, const float dt)
{
}

// virtual end