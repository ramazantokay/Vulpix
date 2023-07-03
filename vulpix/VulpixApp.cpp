#include "VulpixApp.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "Shader/Shader_Config.h"

#define SHADER_FOLDER "shaders/"
#define MODEL_FOLDER "assets/scene"
#define ENVIRONMENT_FOLDER "assets/env_map"

// scene settings
static const vulpix::math::vec3 sunPos = vulpix::math::vec3(1474.4f, 1940.45f, 397.55f);
static const float ambientLight = 0.1f;

VulpixApp::VulpixApp() : AppBase()
{
	m_pipelineLayout = VK_NULL_HANDLE;
	m_pipeline = VK_NULL_HANDLE;
	m_descriptorPool = VK_NULL_HANDLE;

	m_WKeyDown = false;
	m_AKeyDown = false;
	m_SKeyDown = false;
	m_DKeyDown = false;
	m_ShiftDown = false;
	m_LMBDown = false;

}

VulpixApp::~VulpixApp()
{
	freeResources();
}

void VulpixApp::initApp()
{
	loadScene();
	createScene();
	createCamera();
	createDescriptorSetLayouts();
	createRTPipelineAndSBT();
	updateDescriptorSets();
}

void VulpixApp::initSettings()
{
	m_settings.m_resolutionX = 2560;
	m_settings.m_resolutionY= 1440;
	m_settings.m_name = "Vulpix";
	m_settings.m_enableValidationLayers= true;
	m_settings.m_enableVSync = false;
	m_settings.m_supportRT = true;
	m_settings.m_supportDescriptorIndexing = true;
}

void VulpixApp::freeResources()
{
	for (VulpixMesh& mesh: m_scene.m_meshes)
	{
		vkDestroyAccelerationStructureKHR(m_device, mesh.m_BLAS.m_AccelerationStructure, nullptr);
	}
	m_scene.m_meshes.clear();
	m_scene.m_materials.clear();

	if (m_scene.m_TLAS.m_AccelerationStructure)
	{
		vkDestroyAccelerationStructureKHR(m_device, m_scene.m_TLAS.m_AccelerationStructure, nullptr);
		m_scene.m_TLAS.m_AccelerationStructure = VK_NULL_HANDLE;
	}

	if (m_descriptorPool)
	{
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
		m_descriptorPool = VK_NULL_HANDLE;
	}

	m_sbt.destroySBT();

	if (m_pipeline)
	{
		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		m_pipeline = VK_NULL_HANDLE;
	}

	if (m_pipelineLayout)
	{
		vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
		m_pipelineLayout = VK_NULL_HANDLE;
	}

	for (VkDescriptorSetLayout& layout : m_descriptorSetLayouts)
	{
		vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
	}
	m_descriptorSetLayouts.clear();

}

void VulpixApp::fillCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	vkCmdBindPipeline(commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_pipeline);

	vkCmdBindDescriptorSets(commandBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		m_pipelineLayout, 0,
		static_cast<uint32_t>(m_descriptorSets.size()), m_descriptorSets.data(),
		0, 0);

	VkStridedDeviceAddressRegionKHR raygenRegion = {
		m_sbt.getSBTAddress() + m_sbt.getRaygenOffset(),
		m_sbt.getGroupsStride(),
		m_sbt.getRaygenSize()
	};

	VkStridedDeviceAddressRegionKHR missRegion = {
		m_sbt.getSBTAddress() + m_sbt.getMissGroupsOffset(),
		m_sbt.getGroupsStride(),
		m_sbt.getMissGroupsSize()
	};

	VkStridedDeviceAddressRegionKHR hitRegion = {
		m_sbt.getSBTAddress() + m_sbt.getHitGroupsOffset(),
		m_sbt.getGroupsStride(),
		m_sbt.getHitGroupsSize()
	};

	VkStridedDeviceAddressRegionKHR callableRegion = {};

	vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callableRegion, m_settings.m_resolutionX, m_settings.m_resolutionY, 1u);

}

void VulpixApp::onMouseMove(const float x, const float y)
{
	vulpix::math::vec2 newCursorPos(x, y);
	vulpix::math::vec2 delta = m_cursorPos - newCursorPos;

	if (m_LMBDown)
	{
		m_camera.rotateCamera(delta.x * m_rotationSpeed, delta.y * m_rotationSpeed);
	}
	m_cursorPos = newCursorPos;
}

void VulpixApp::onMouseButton(const int button, const int action, const int mods)
{

	if (button == GLFW_MOUSE_BUTTON_LEFT)
	{
		if (action == GLFW_PRESS)
		{
			m_LMBDown = true;
		}
		else if (action == GLFW_RELEASE)
		{
			m_LMBDown = false;
		}
	}
}

void VulpixApp::onKeyboard(const int key, const int scancode, const int action, const int mods)
{
	if (GLFW_PRESS == action) {
		switch (key) {
		case GLFW_KEY_W: m_WKeyDown = true; break;
		case GLFW_KEY_A: m_AKeyDown = true; break;
		case GLFW_KEY_S: m_SKeyDown = true; break;
		case GLFW_KEY_D: m_DKeyDown = true; break;

		case GLFW_KEY_LEFT_SHIFT:
		case GLFW_KEY_RIGHT_SHIFT:
			m_ShiftDown = true;
			break;
		}
	}
	else if (GLFW_RELEASE == action) {
		switch (key) {
		case GLFW_KEY_W: m_WKeyDown = false; break;
		case GLFW_KEY_A: m_AKeyDown = false; break;
		case GLFW_KEY_S: m_SKeyDown = false; break;
		case GLFW_KEY_D: m_DKeyDown = false; break;

		case GLFW_KEY_LEFT_SHIFT:
		case GLFW_KEY_RIGHT_SHIFT:
			m_ShiftDown = false;
			break;
		}
	}
}

void VulpixApp::update(size_t imageIndex, const float dt)
{
	std::string camPos = "x: " + std::to_string(m_camera.getPosition().x) + " y: " + std::to_string(m_camera.getPosition().y) + " z:" + std::to_string(m_camera.getPosition().z);
	std::string frameStat = "Frame: " + std::to_string(m_FPSCounter.getFPS()) + "   "+ std::to_string(m_FPSCounter.getFrameTime()) + " ms " + " Camera Position: " + camPos;
	std::string title = m_settings.m_name + " " + frameStat;
	glfwSetWindowTitle(m_window, title.c_str());

	UniformParams *params =  reinterpret_cast<UniformParams*>(m_cameraBuffer.mapMemory());
	params->m_sunPosAndAmbient = vulpix::math::vec4(sunPos, ambientLight);
	updateCamera(params, dt);
	m_cameraBuffer.unmapMemory();

	//renderUI();
}

void VulpixApp::loadScene()
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, error;
	
	//std::string fileName = "assets/scene/vulpix_scene/vulpix_scene.obj";
	std::string fileName = "assets/scene/sponza/vulpix_sponza.obj";
	std::string baseDir = fileName;
	const size_t slash = baseDir.find_last_of('/');
	if (slash != std::string::npos) {
		baseDir.erase(slash);
	}

	const bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, fileName.c_str(), baseDir.c_str(), true);
	if (result) {
		m_scene.m_meshes.resize(shapes.size());
		m_scene.m_materials.resize(materials.size());

		for (size_t meshIdx = 0; meshIdx < shapes.size(); ++meshIdx) {
			VulpixMesh& mesh = m_scene.m_meshes[meshIdx];
			const tinyobj::shape_t& shape = shapes[meshIdx];

			const size_t numFaces = shape.mesh.num_face_vertices.size();
			const size_t numVertices = numFaces * 3;

			mesh.m_vertexCount = static_cast<uint32_t>(numVertices);
			mesh.m_faceCount = static_cast<uint32_t>(numFaces);

			const size_t positionsBufferSize = numVertices * sizeof(vec3);
			const size_t indicesBufferSize = numFaces * 3 * sizeof(uint32_t);
			const size_t facesBufferSize = numFaces * 4 * sizeof(uint32_t);
			const size_t attribsBufferSize = numVertices * sizeof(VertexAttributes);
			const size_t matIDsBufferSize = numFaces * sizeof(uint32_t);

			VkResult error = mesh.m_position.createBuffer(positionsBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CHECK_VK_ERROR(error, "mesh.positions.Create");

			error = mesh.m_index.createBuffer(indicesBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CHECK_VK_ERROR(error, "mesh.indices.Create");

			error = mesh.m_faces.createBuffer(facesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CHECK_VK_ERROR(error, "mesh.faces.Create");

			error = mesh.m_attribute.createBuffer(attribsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CHECK_VK_ERROR(error, "mesh.attribs.Create");

			error = mesh.m_material.createBuffer(matIDsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CHECK_VK_ERROR(error, "mesh.matIDs.Create");

			vec3* positions = reinterpret_cast<vec3*>(mesh.m_position.mapMemory());
			VertexAttributes* attribs = reinterpret_cast<VertexAttributes*>(mesh.m_attribute.mapMemory());
			uint32_t* indices = reinterpret_cast<uint32_t*>(mesh.m_index.mapMemory());
			uint32_t* faces = reinterpret_cast<uint32_t*>(mesh.m_faces.mapMemory());
			uint32_t* matIDs = reinterpret_cast<uint32_t*>(mesh.m_material.mapMemory());

			size_t vIdx = 0;
			for (size_t f = 0; f < numFaces; ++f) {
				assert(shape.mesh.num_face_vertices[f] == 3);
				for (size_t j = 0; j < 3; ++j, ++vIdx) {
					const tinyobj::index_t& i = shape.mesh.indices[vIdx];

					vec3& pos = positions[vIdx];
					vec4& normal = attribs[vIdx].m_normal;
					vec4& uv = attribs[vIdx].m_uv;

					pos.x = attrib.vertices[3 * i.vertex_index + 0];
					pos.y = attrib.vertices[3 * i.vertex_index + 1];
					pos.z = attrib.vertices[3 * i.vertex_index + 2];
					normal.x = attrib.normals[3 * i.normal_index + 0];
					normal.y = attrib.normals[3 * i.normal_index + 1];
					normal.z = attrib.normals[3 * i.normal_index + 2];
					uv.x = attrib.texcoords[2 * i.texcoord_index + 0];
					uv.y = attrib.texcoords[2 * i.texcoord_index + 1];
				}

				const uint32_t a = static_cast<uint32_t>(3 * f + 0);
				const uint32_t b = static_cast<uint32_t>(3 * f + 1);
				const uint32_t c = static_cast<uint32_t>(3 * f + 2);
				indices[a] = a;
				indices[b] = b;
				indices[c] = c;
				faces[4 * f + 0] = a;
				faces[4 * f + 1] = b;
				faces[4 * f + 2] = c;

				matIDs[f] = static_cast<uint32_t>(shape.mesh.material_ids[f]);
			}

			mesh.m_material.unmapMemory();
			mesh.m_index.unmapMemory();
			mesh.m_faces.unmapMemory();
			mesh.m_attribute.unmapMemory();
			mesh.m_position.unmapMemory();
		}

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		for (size_t i = 0; i < materials.size(); ++i) {
			const tinyobj::material_t& srcMat = materials[i];
			VulpixMaterial& dstMat = m_scene.m_materials[i];

			std::string fullTexturePath = baseDir + "/" + srcMat.diffuse_texname;
			if (dstMat.m_texture.load(fullTexturePath.c_str())) {
				dstMat.m_texture.createImageView(VK_IMAGE_VIEW_TYPE_2D, dstMat.m_texture.getFormat(), subresourceRange);
				dstMat.m_texture.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
			}
		}
	}

	// prepare shader resources infos
	const size_t numMeshes = m_scene.m_meshes.size();
	const size_t numMaterials = m_scene.m_materials.size();

	m_scene.m_matBufferInfos.resize(numMeshes);
	m_scene.m_attributesBufferInfos.resize(numMeshes);
	m_scene.m_facesBufferInfos.resize(numMeshes);

	for (size_t i = 0; i < numMeshes; ++i) {
		const VulpixMesh& mesh = m_scene.m_meshes[i];
		VkDescriptorBufferInfo& matIDsInfo = m_scene.m_matBufferInfos[i];
		VkDescriptorBufferInfo& attribsInfo = m_scene.m_attributesBufferInfos[i];
		VkDescriptorBufferInfo& facesInfo = m_scene.m_facesBufferInfos[i];

		matIDsInfo.buffer = mesh.m_material.getBuffer();
		matIDsInfo.offset = 0;
		matIDsInfo.range = mesh.m_material.getSize();

		attribsInfo.buffer = mesh.m_attribute.getBuffer();
		attribsInfo.offset = 0;
		attribsInfo.range = mesh.m_attribute.getSize();

		facesInfo.buffer = mesh.m_faces.getBuffer();
		facesInfo.offset = 0;
		facesInfo.range = mesh.m_faces.getSize();
	}

	m_scene.m_texBufferInfos.resize(numMaterials);
	for (size_t i = 0; i < numMaterials; ++i) {
		const VulpixMaterial& mat = m_scene.m_materials[i];
		VkDescriptorImageInfo& textureInfo = m_scene.m_texBufferInfos[i];

		textureInfo.sampler = mat.m_texture.getSampler();
		textureInfo.imageView = mat.m_texture.getImageView();
		textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
}

void VulpixApp::createScene()
{
	m_scene.buildBLAS(m_device, m_commandPool, m_graphicsQueue);
	m_scene.buildTLAS(m_device, m_commandPool, m_graphicsQueue);

	m_envTexture.load("assets/env_map/blue_photo_studio_4k.hdr");

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	m_envTexture.createImageView(VK_IMAGE_VIEW_TYPE_2D, m_envTexture.getFormat(), subresourceRange);
	m_envTexture.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

	m_envTextureInfo.sampler = m_envTexture.getSampler();
	m_envTextureInfo.imageView = m_envTexture.getImageView();
	m_envTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

}

void VulpixApp::createCamera()
{
	VkResult error = m_cameraBuffer.createBuffer(sizeof(UniformParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK_VK_ERROR(error, "Could not create camera buffer");

	m_camera.setViewport( { 0, 0, static_cast<int>(m_settings.m_resolutionX), static_cast<int>(m_settings.m_resolutionY) } ) ;
	m_camera.setPlanes(0.1f, 10000.f);
	m_camera.setFOV(45.0f);
	m_camera.lookCameraAt(vulpix::math::vec3(0.f, 2.f, 15.f), vulpix::math::vec3(0.f, 0.f, 0.f));

}

void VulpixApp::updateCamera(UniformParams* params, const float dt)
{
	vec2 moveDelta(0.0f, 0.0f);
	if (m_WKeyDown) {
		moveDelta.y += 1.0f;
	}
	if (m_SKeyDown) {
		moveDelta.y -= 1.0f;
	}
	if (m_AKeyDown) {
		moveDelta.x -= 1.0f;
	}
	if (m_DKeyDown) {
		moveDelta.x += 1.0f;
	}

	moveDelta *= m_moveSpeed * dt * (m_ShiftDown ? m_mouseSensitivity : 1.0f);
	m_camera.moveCamera(moveDelta.x, moveDelta.y);

	params->m_cameraPosition = vec4(m_camera.getPosition(), 0.0f);
	params->m_cameraDirection = vec4(m_camera.getForward(), 0.0f);
	params->m_cameraUp = vec4(m_camera.getUp(), 0.0f);
	params->m_cameraRight = vec4(m_camera.getSide(), 0.0f);
	params->m_cameraNearFarFOV = vec4(m_camera.getNearPlane(), m_camera.getFarPlane(), glm::radians(m_camera.getFOV()), 0.0f);


	
}

void VulpixApp::createDescriptorSetLayouts()
{
	const uint32_t numMeshes = static_cast<uint32_t>(m_scene.m_meshes.size());
	const uint32_t numMaterials = static_cast<uint32_t>(m_scene.m_materials.size());

	m_descriptorSetLayouts.resize(NUM_DESCRIPTOR_SETS);

	// First set:
	//  binding 0  ->  AS
	//  binding 1  ->  output image
	//  binding 2  ->  Camera data

	VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
	accelerationStructureLayoutBinding.binding = VULPIX_SCENE_AS_BINDING;
	accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureLayoutBinding.descriptorCount = 1;
	accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding resultImageLayoutBinding;
	resultImageLayoutBinding.binding = VULPIX_RESULT_IMAGE_BINDING;
	resultImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageLayoutBinding.descriptorCount = 1;
	resultImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	resultImageLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding camdataBufferBinding;
	camdataBufferBinding.binding = VULPIX_CAMDATA_BINDING;
	camdataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	camdataBufferBinding.descriptorCount = 1;
	camdataBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	camdataBufferBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings({
		accelerationStructureLayoutBinding,
		resultImageLayoutBinding,
		camdataBufferBinding
		});

	VkDescriptorSetLayoutCreateInfo set0LayoutInfo;
	set0LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set0LayoutInfo.pNext = nullptr;
	set0LayoutInfo.flags = 0;
	set0LayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	set0LayoutInfo.pBindings = bindings.data();

	VkResult error = vkCreateDescriptorSetLayout(m_device, &set0LayoutInfo, nullptr, &m_descriptorSetLayouts[VULPIX_SCENE_AS_SET]);
	CHECK_VK_ERROR(error, "vkCreateDescriptorSetLayout");

	// Second set:
	//  binding 0 .. N  ->  per-face material IDs for our meshes  (N = num meshes)

	const VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags;
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pNext = nullptr;
	bindingFlags.pBindingFlags = &flag;
	bindingFlags.bindingCount = 1;

	VkDescriptorSetLayoutBinding ssboBinding;
	ssboBinding.binding = 0;
	ssboBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ssboBinding.descriptorCount = numMeshes;
	ssboBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	ssboBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo set1LayoutInfo;
	set1LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set1LayoutInfo.pNext = &bindingFlags;
	set1LayoutInfo.flags = 0;
	set1LayoutInfo.bindingCount = 1;
	set1LayoutInfo.pBindings = &ssboBinding;

	error = vkCreateDescriptorSetLayout(m_device, &set1LayoutInfo, nullptr, &m_descriptorSetLayouts[VULPIX_MATERIAL_IDS_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// Third set:
	//  binding 0 .. N  ->  vertex attributes for our meshes  (N = num meshes)
	//   (re-using second's set info)

	error = vkCreateDescriptorSetLayout(m_device, &set1LayoutInfo, nullptr, &m_descriptorSetLayouts[VULPIX_ATTRIBUTES_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// Fourth set:
	//  binding 0 .. N  ->  faces info (indices) for our meshes  (N = num meshes)
	//   (re-using second's set info)

	error = vkCreateDescriptorSetLayout(m_device, &set1LayoutInfo, nullptr, &m_descriptorSetLayouts[VULPIX_FACES_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// Fifth set:
	//  binding 0 .. N  ->  textures (N = num materials)

	VkDescriptorSetLayoutBinding textureBinding;
	textureBinding.binding = 0;
	textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBinding.descriptorCount = numMaterials;
	textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	textureBinding.pImmutableSamplers = nullptr;

	set1LayoutInfo.pBindings = &textureBinding;

	error = vkCreateDescriptorSetLayout(m_device, &set1LayoutInfo, nullptr, &m_descriptorSetLayouts[VULPIX_TEXTURES_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

	// Sixth set:
	//  binding 0 ->  env texture

	VkDescriptorSetLayoutBinding envBinding;
	envBinding.binding = 0;
	envBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	envBinding.descriptorCount = 1;
	envBinding.stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;
	envBinding.pImmutableSamplers = nullptr;

	set1LayoutInfo.pBindings = &envBinding;

	error = vkCreateDescriptorSetLayout(m_device, &set1LayoutInfo, nullptr, &m_descriptorSetLayouts[VULPIX_ENVIRONMENT_MAP_SET]);
	CHECK_VK_ERROR(error, L"vkCreateDescriptorSetLayout");

}

void VulpixApp::createRTPipelineAndSBT()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = NUM_DESCRIPTOR_SETS;
	pipelineLayoutCreateInfo.pSetLayouts = m_descriptorSetLayouts.data();

	VkResult error = vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
	CHECK_VK_ERROR(error, "vkCreatePipelineLayout");


	Shader rayGenShader, rayChitShader, rayMissShader, shadowChit, shadowMiss;
	rayGenShader.load(("assets/out_shaders/ray_gen.bin"));
	rayChitShader.load(("assets/out_shaders/ray_chit.bin"));
	rayMissShader.load(("assets/out_shaders/ray_miss.bin"));
	shadowChit.load(("assets/out_shaders/shadow_ray_chit.bin"));
	shadowMiss.load(("assets/out_shaders/shadow_ray_miss.bin"));

	m_sbt.initSBT(2, 2, m_rayTracingPipelineProperties.shaderGroupHandleSize, m_rayTracingPipelineProperties.shaderGroupBaseAlignment);

	m_sbt.setRaygenStage(rayGenShader.getShaderStageInfo(VK_SHADER_STAGE_RAYGEN_BIT_KHR));

	m_sbt.addStageToHitGroup({ rayChitShader.getShaderStageInfo(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, VULPIX_PRIMARY_HIT_SHADERS_INDEX);
	m_sbt.addStageToHitGroup({ shadowChit.getShaderStageInfo(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) }, VULPIX_PRIMARY_SHADOW_HIT_SHADERS_INDEX);

	m_sbt.addStageToMissGroup(rayMissShader.getShaderStageInfo(VK_SHADER_STAGE_MISS_BIT_KHR), VULPIX_PRIMARY_MISS_SHADERS_INDEX);
	m_sbt.addStageToMissGroup(shadowMiss.getShaderStageInfo(VK_SHADER_STAGE_MISS_BIT_KHR), VULPIX_PRIMARY_SHADOW_MISS_SHADERS_INDEX);


	VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {};
	rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayPipelineInfo.stageCount = m_sbt.getNumStages();
	rayPipelineInfo.pStages = m_sbt.getStages();
	rayPipelineInfo.groupCount = m_sbt.getNumGroups();
	rayPipelineInfo.pGroups = m_sbt.getGroups();
	rayPipelineInfo.maxPipelineRayRecursionDepth = 1;
	rayPipelineInfo.layout = m_pipelineLayout;

	error = vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayPipelineInfo, VK_NULL_HANDLE, &m_pipeline);
	CHECK_VK_ERROR(error, "vkCreateRayTracingPipelinesKHR");

	m_sbt.createSBT(m_device, m_pipeline);

}

void VulpixApp::updateDescriptorSets()
{
	const uint32_t numMeshes = static_cast<uint32_t>(m_scene.m_meshes.size());
	const uint32_t numMaterials = static_cast<uint32_t>(m_scene.m_materials.size());

	std::vector<VkDescriptorPoolSize> poolSizes({
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },       // top-level AS
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },                    // output image
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },                   // Camera data
		//
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numMeshes * 3 },       // per-face material IDs for each mesh
		// vertex attribs for each mesh
		// faces buffer for each mesh
//
{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numMaterials },// textures for each material

//
{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }            // environment texture
		});

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = NUM_DESCRIPTOR_SETS;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

	VkResult error = vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool);
	CHECK_VK_ERROR(error, "vkCreateDescriptorPool");

	m_descriptorSets.resize(NUM_DESCRIPTOR_SETS);

	std::vector<uint32_t> variableDescriptorCounts({
		1,
		numMeshes,      // per-face material IDs for each mesh
		numMeshes,      // vertex attribs for each mesh
		numMeshes,      // faces buffer for each mesh
		numMaterials,   // textures for each material
		1,              // environment texture
		});

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo;
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.pNext = nullptr;
	variableDescriptorCountInfo.descriptorSetCount = NUM_DESCRIPTOR_SETS;
	variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = &variableDescriptorCountInfo;
	descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = NUM_DESCRIPTOR_SETS;
	descriptorSetAllocateInfo.pSetLayouts = m_descriptorSetLayouts.data();

	error = vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, m_descriptorSets.data());
	CHECK_VK_ERROR(error, "vkAllocateDescriptorSets");

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
	descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	descriptorAccelerationStructureInfo.pNext = nullptr;
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
	descriptorAccelerationStructureInfo.pAccelerationStructures = &m_scene.m_TLAS.m_AccelerationStructure;

	VkWriteDescriptorSet accelerationStructureWrite;
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
	accelerationStructureWrite.dstSet = m_descriptorSets[VULPIX_SCENE_AS_SET];
	accelerationStructureWrite.dstBinding = VULPIX_SCENE_AS_BINDING;
	accelerationStructureWrite.dstArrayElement = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	accelerationStructureWrite.pImageInfo = nullptr;
	accelerationStructureWrite.pBufferInfo = nullptr;
	accelerationStructureWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkDescriptorImageInfo descriptorOutputImageInfo;
	descriptorOutputImageInfo.sampler = VK_NULL_HANDLE;
	descriptorOutputImageInfo.imageView = m_offscreenImage.getImageView();
	descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet resultImageWrite;
	resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	resultImageWrite.pNext = nullptr;
	resultImageWrite.dstSet = m_descriptorSets[VULPIX_RESULT_IMAGE_SET];
	resultImageWrite.dstBinding = VULPIX_RESULT_IMAGE_BINDING;
	resultImageWrite.dstArrayElement = 0;
	resultImageWrite.descriptorCount = 1;
	resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	resultImageWrite.pImageInfo = &descriptorOutputImageInfo;
	resultImageWrite.pBufferInfo = nullptr;
	resultImageWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkDescriptorBufferInfo camdataBufferInfo;
	camdataBufferInfo.buffer = m_cameraBuffer.getBuffer();
	camdataBufferInfo.offset = 0;
	camdataBufferInfo.range = m_cameraBuffer.getSize();

	VkWriteDescriptorSet camdataBufferWrite;
	camdataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	camdataBufferWrite.pNext = nullptr;
	camdataBufferWrite.dstSet = m_descriptorSets[VULPIX_CAMDATA_SET];
	camdataBufferWrite.dstBinding = VULPIX_CAMDATA_BINDING;
	camdataBufferWrite.dstArrayElement = 0;
	camdataBufferWrite.descriptorCount = 1;
	camdataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	camdataBufferWrite.pImageInfo = nullptr;
	camdataBufferWrite.pBufferInfo = &camdataBufferInfo;
	camdataBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet matIDsBufferWrite;
	matIDsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	matIDsBufferWrite.pNext = nullptr;
	matIDsBufferWrite.dstSet = m_descriptorSets[VULPIX_MATERIAL_IDS_SET];
	matIDsBufferWrite.dstBinding = 0;
	matIDsBufferWrite.dstArrayElement = 0;
	matIDsBufferWrite.descriptorCount = numMeshes;
	matIDsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	matIDsBufferWrite.pImageInfo = nullptr;
	matIDsBufferWrite.pBufferInfo = m_scene.m_matBufferInfos.data();
	matIDsBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet attribsBufferWrite;
	attribsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	attribsBufferWrite.pNext = nullptr;
	attribsBufferWrite.dstSet = m_descriptorSets[VULPIX_ATTRIBUTES_SET];
	attribsBufferWrite.dstBinding = 0;
	attribsBufferWrite.dstArrayElement = 0;
	attribsBufferWrite.descriptorCount = numMeshes;
	attribsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	attribsBufferWrite.pImageInfo = nullptr;
	attribsBufferWrite.pBufferInfo = m_scene.m_attributesBufferInfos.data();
	attribsBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet facesBufferWrite;
	facesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	facesBufferWrite.pNext = nullptr;
	facesBufferWrite.dstSet = m_descriptorSets[VULPIX_FACES_SET];
	facesBufferWrite.dstBinding = 0;
	facesBufferWrite.dstArrayElement = 0;
	facesBufferWrite.descriptorCount = numMeshes;
	facesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	facesBufferWrite.pImageInfo = nullptr;
	facesBufferWrite.pBufferInfo = m_scene.m_facesBufferInfos.data();
	facesBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet texturesBufferWrite;
	texturesBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesBufferWrite.pNext = nullptr;
	texturesBufferWrite.dstSet = m_descriptorSets[VULPIX_TEXTURES_SET];
	texturesBufferWrite.dstBinding = 0;
	texturesBufferWrite.dstArrayElement = 0;
	texturesBufferWrite.descriptorCount = numMaterials;
	texturesBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesBufferWrite.pImageInfo = m_scene.m_texBufferInfos.data();
	texturesBufferWrite.pBufferInfo = nullptr;
	texturesBufferWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	VkWriteDescriptorSet envTexturesWrite;
	envTexturesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	envTexturesWrite.pNext = nullptr;
	envTexturesWrite.dstSet = m_descriptorSets[VULPIX_ENVIRONMENT_MAP_SET];
	envTexturesWrite.dstBinding = 0;
	envTexturesWrite.dstArrayElement = 0;
	envTexturesWrite.descriptorCount = 1;
	envTexturesWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	envTexturesWrite.pImageInfo = &m_envTextureInfo;
	envTexturesWrite.pBufferInfo = nullptr;
	envTexturesWrite.pTexelBufferView = nullptr;

	///////////////////////////////////////////////////////////

	std::vector<VkWriteDescriptorSet> descriptorWrites({
		accelerationStructureWrite,
		resultImageWrite,
		camdataBufferWrite,
		//
		matIDsBufferWrite,
		//
		attribsBufferWrite,
		//
		facesBufferWrite,
		//
		texturesBufferWrite,
		//
		envTexturesWrite
		});

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, VK_NULL_HANDLE);

}

void VulpixApp::renderUI()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		//ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		//ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		//ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

		if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		//ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::End();
	}

        ImGui::Render();

		// Rendering
		ImGui::Render();
		



}

void VulpixApp::initImGui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}


	ImGui_ImplGlfw_InitForVulkan(m_window, true);

	VkAttachmentReference color_attachment = {};
	color_attachment.attachment = 0;
	color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkAttachmentDescription attachment = {};
	attachment.format = m_surfaceFormat.format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;  // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;
	if (vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS) {
		throw std::runtime_error("Could not create Dear ImGui's render pass");
	}

	// Setup Platform/Renderer backends
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_instance;
	init_info.PhysicalDevice = m_context.m_physicalDevice;
	init_info.Device = m_context.m_device;
	init_info.QueueFamily = m_graphicsQueueFamilyIndex;
	init_info.Queue = m_context.m_transferQueue;
	//init_info.PipelineCache = g_PipelineCache;
	init_info.DescriptorPool = m_descriptorPool;
	init_info.Subpass = 0;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	//init_info.Allocator = g_Allocator;
	//init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info, m_renderPass);







}
