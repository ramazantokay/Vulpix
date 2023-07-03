#include "Vulpix_Scene.h"
#include "../Math/Vulpix_Math.h"

void VulpixScene::buildTLAS(VkDevice device, VkCommandPool cPool, VkQueue queue)
{
    const VkTransformMatrixKHR transform = {
       1.0f, 0.0f, 0.0f, 0.0f,
       0.0f, 1.0f, 0.0f, 0.0f,
       0.0f, 0.0f, 1.0f, 0.0f,
    };

    const size_t numMeshes = m_meshes.size();

    // create instances for our meshes
    std::vector<VkAccelerationStructureInstanceKHR> instances(numMeshes, VkAccelerationStructureInstanceKHR{});
    for (size_t i = 0; i < numMeshes; ++i) {
        VulpixMesh& mesh = m_meshes[i];

        VkAccelerationStructureInstanceKHR& instance = instances[i];
        instance.transform = transform;
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        instance.mask = 0xff;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = mesh.m_BLAS.m_DeviceAddress;
    }

    Buffer instancesBuffer;
    VkResult error = instancesBuffer.createBuffer(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK_VK_ERROR(error, "instancesBuffer.Create");

    if (!instancesBuffer.uploadData(instances.data(), instancesBuffer.getSize())) {
        assert(false && "Failed to upload instances buffer");
    }


    // and here we create out top-level acceleration structure that'll represent our scene
    VkAccelerationStructureGeometryInstancesDataKHR tlasInstancesInfo = {};
    tlasInstancesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasInstancesInfo.data = vulpix::getBufferDeviceAddressConst(instancesBuffer);

    VkAccelerationStructureGeometryKHR  tlasGeoInfo = {};
    tlasGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeoInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeoInfo.geometry.instances = tlasInstancesInfo;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &tlasGeoInfo;

    const uint32_t numInstances = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &numInstances, &sizeInfo);

    m_TLAS.m_Buffer.createBuffer(sizeInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.buffer = m_TLAS.m_Buffer.getBuffer();

    error = vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &m_TLAS.m_AccelerationStructure);
    CHECK_VK_ERROR(error, "vkCreateAccelerationStructureKHR");


    Buffer scratchBuffer;
    error = scratchBuffer.createBuffer(sizeInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_VK_ERROR(error, "scratchBuffer.Create");

    buildInfo.scratchData = vulpix::getBufferDeviceAddress(scratchBuffer);
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure = m_TLAS.m_AccelerationStructure;

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = cPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    error = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    CHECK_VK_ERROR(error, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkAccelerationStructureBuildRangeInfoKHR range = {};
    range.primitiveCount = numInstances;

    const VkAccelerationStructureBuildRangeInfoKHR* ranges[1] = { &range };

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, ranges);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    error = vkQueueWaitIdle(queue);
    CHECK_VK_ERROR(error, "vkQueueWaitIdle");
    vkFreeCommandBuffers(device, cPool, 1, &commandBuffer);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = m_TLAS.m_AccelerationStructure;
    m_TLAS.m_DeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

void VulpixScene::buildBLAS(VkDevice device, VkCommandPool cPool, VkQueue queue)
{
    const size_t numMeshes = m_meshes.size();

    std::vector<VkAccelerationStructureGeometryKHR> geometries(numMeshes, VkAccelerationStructureGeometryKHR{});
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges(numMeshes, VkAccelerationStructureBuildRangeInfoKHR{});
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(numMeshes, VkAccelerationStructureBuildGeometryInfoKHR{});
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> sizeInfos(numMeshes, VkAccelerationStructureBuildSizesInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR });

    for (size_t i = 0; i < numMeshes; ++i) {
        VulpixMesh& mesh = m_meshes[i];

        VkAccelerationStructureGeometryKHR& geometry = geometries[i];
        VkAccelerationStructureBuildRangeInfoKHR& range = ranges[i];
        VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];

        range.primitiveCount = mesh.m_faceCount;

        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData = vulpix::getBufferDeviceAddressConst(mesh.m_position);
        geometry.geometry.triangles.vertexStride = sizeof(vulpix::math::vec3);
        geometry.geometry.triangles.maxVertex = mesh.m_vertexCount;
        geometry.geometry.triangles.indexData = vulpix::getBufferDeviceAddressConst(mesh.m_index);
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        vkGetAccelerationStructureBuildSizesKHR(device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &range.primitiveCount,
            &sizeInfos[i]);
    }

    VkDeviceSize maximumBlasSize = 0;
    for (const auto& sizeInfo : sizeInfos) {
        maximumBlasSize = std::max(sizeInfo.buildScratchSize, maximumBlasSize);
    }

    Buffer scratchBuffer;
    VkResult error = scratchBuffer.createBuffer(maximumBlasSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CHECK_VK_ERROR(error, "scratchBuffer.Create");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = cPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    error = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    CHECK_VK_ERROR(error, "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    // build bottom-level ASs
    for (size_t i = 0; i < numMeshes; ++i) {
        VulpixMesh& mesh = m_meshes[i];

        mesh.m_BLAS.m_Buffer.createBuffer(sizeInfos[i].accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkAccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        createInfo.size = sizeInfos[i].accelerationStructureSize;
        createInfo.buffer = mesh.m_BLAS.m_Buffer.getBuffer();

        error = vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &mesh.m_BLAS.m_AccelerationStructure);
        CHECK_VK_ERROR(error, "vkCreateAccelerationStructureKHR");


        VkAccelerationStructureBuildGeometryInfoKHR& buildInfo = buildInfos[i];
        buildInfo.scratchData = vulpix::getBufferDeviceAddress(scratchBuffer);
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = mesh.m_BLAS.m_AccelerationStructure;

        const VkAccelerationStructureBuildRangeInfoKHR* range[1] = { &ranges[i] };

        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, range);

        // guard our scratch buffer
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
    }

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    error = vkQueueWaitIdle(queue);
    CHECK_VK_ERROR(error, "vkQueueWaitIdle");
    vkFreeCommandBuffers(device, cPool, 1, &commandBuffer);

    // get handles
    for (size_t i = 0; i < numMeshes; ++i) {
        VulpixMesh& mesh = m_meshes[i];

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = mesh.m_BLAS.m_AccelerationStructure;
        mesh.m_BLAS.m_DeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
    }
}
