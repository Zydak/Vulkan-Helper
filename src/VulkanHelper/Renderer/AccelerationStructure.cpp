#include "pch.h"

#include "AccelerationStructure.h"
#include "Scene/Components.h"

namespace VulkanHelper
{
	/**
	 * @brief Converts a mesh to AccelerationStructure input data.
	 *
	 * This function takes a Mesh object and extracts device addresses of the vertex
	 * and index buffers, then constructs AccelerationStructure input data.
	 *
	 * @param mesh - Input Mesh object to convert.
	 * @return BlasInput - AccelerationStructure input data generated from the mesh.
	 */
	BlasInput AccelerationStructure::MeshToGeometry(Mesh* mesh)
	{
		// Get device addresses of the vertex and index buffers
		VkDeviceAddress vertexAddress	= mesh->GetVertexBuffer()->GetDeviceAddress();
		VkDeviceAddress indexAddress	= mesh->GetIndexBuffer()->GetDeviceAddress();

		uint32_t primitiveCount			= (uint32_t)mesh->GetIndexCount() / 3;

		// Describe buffer as array of Mesh::Vertex.
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = vertexAddress;
		triangles.vertexStride = sizeof(Mesh::Vertex);
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = indexAddress;
		triangles.transformData = {};
		triangles.maxVertex = (uint32_t)mesh->GetVertexCount() - 1;

		// Identify the above data as opaque triangles.
		VkAccelerationStructureGeometryKHR asGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		asGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeometry.geometry.triangles = triangles;

		// The entire array will be used to build the BLAS.
		VkAccelerationStructureBuildRangeInfoKHR range{};
		range.firstVertex = 0;
		range.primitiveCount = primitiveCount;
		range.primitiveOffset = 0;
		range.transformOffset = 0;

		// Our blas is made from only one geometry, but could be made of many geometries
		BlasInput input{};
		input.AsGeometry.emplace_back(asGeometry);
		input.AsRange.emplace_back(range);

		// Return the generated AccelerationStructure input data.
		return input;
	}

	/**
	 * @brief Creates bottom-level acceleration structures (BLAS) from given indices of blases.
	 *
	 * @param cmdBuf - Vulkan command buffer where acceleration structures will be built.
	 * @param indices - Vector of indices specifying the builds to be performed.
	 * @param buildAs - Vector of BuildAccelerationStructure objects containing build information.
	 * @param scratchAddress - Device address of the scratch memory used during acceleration structure builds.
	 * @param queryPool (Optional) - Vulkan query pool for measuring compaction size. Pass nullptr if not needed.
	 */
	void AccelerationStructure::CmdCreateBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress, VkQueryPool queryPool)
	{
		if (queryPool)  // For querying the compaction size
		{
			vkResetQueryPool(Device::GetDevice(), queryPool, 0, (uint32_t)indices.size());
		}
		uint32_t queryCount = 0;

		for (const auto& i : indices)
		{
			// Actual allocation of buffer and acceleration structure.
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size = buildAs[i].SizeInfo.accelerationStructureSize;
			CreateAcceleration(createInfo, buildAs[i].As);

			buildAs[i].BuildInfo.dstAccelerationStructure = buildAs[i].As.Accel;  // Setting where the build lands
			buildAs[i].BuildInfo.scratchData.deviceAddress = scratchAddress;  // All builds are using the same scratch buffer

			// Building the bottom-level-acceleration-structure
			Device::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[i].BuildInfo, &buildAs[i].RangeInfo);

			// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
			// is finished before starting the next one.
			VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

			if (queryPool)
			{
				// Add a query to find the 'real' amount of memory needed
				Device::vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildAs[i].BuildInfo.dstAccelerationStructure,
					VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCount);
				queryCount++;
			}
		}
	}

	/**
	 * @brief Compacts bottom-level acceleration structures (BLAS).
	 *
	 * This function compacts the BLAS by creating a new compact version and copying
	 * the original BLAS to the compact version.
	 *
	 * @param cmdBuf The Vulkan command buffer where acceleration structures will be compacted.
	 * @param indices A vector of indices specifying the builds to be compacted.
	 * @param buildAs A vector of BuildAccelerationStructure objects containing build information.
	 * @param queryPool Vulkan query pool used for retrieving compacted sizes.
	 */
	void AccelerationStructure::CmdCompactBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool)
	{
		uint32_t queryCount = 0;

		// Get the compacted size result back
		std::vector<VkDeviceSize> compactSizes((uint32_t)indices.size());
		vkGetQueryPoolResults(
			Device::GetDevice(),
			queryPool,
			0,
			(uint32_t)compactSizes.size(),
			compactSizes.size() * sizeof(VkDeviceSize),
			compactSizes.data(),
			sizeof(VkDeviceSize),
			VK_QUERY_RESULT_WAIT_BIT
		);

		for (auto i : indices)
		{
			buildAs[i].CleanupAs = buildAs[i].As; // previous AS to destroy
			buildAs[i].SizeInfo.accelerationStructureSize = compactSizes[queryCount]; // new reduced size
			queryCount++;

			// Creating a compact version of the AS
			VkAccelerationStructureCreateInfoKHR asCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			asCreateInfo.size = buildAs[i].SizeInfo.accelerationStructureSize;
			asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			CreateAcceleration(asCreateInfo, buildAs[i].As);

			// Copy the original BLAS to a compact version
			VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
			copyInfo.src = buildAs[i].BuildInfo.dstAccelerationStructure;
			copyInfo.dst = buildAs[i].As.Accel;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			Device::vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
		}
	}

	/**
	 * @brief Builds or updates a Top-Level Acceleration Structure (TLAS).
	 *
	 * @param cmdBuf - Vulkan command buffer in which the TLAS will be built or updated.
	 * @param instanceCount - Number of instances in the TLAS.
	 * @param instanceBufferAddr - Device address of the buffer containing the instance data.
	 * @param scratchBuffer - Buffer used for temporary storage during the TLAS build process.
	 * @param flags - Flags specifying acceleration structure build options.
	 * @param update - Flag indicating whether to update an existing TLAS (true) or create a new one (false).
	 */
	void AccelerationStructure::CmdCreateTlas(VkCommandBuffer cmdBuf, uint32_t instanceCount, VkDeviceAddress instanceBufferAddr, Buffer* scratchBuffer, VkBuildAccelerationStructureFlagsKHR flags, bool update)
	{
		// Wraps a device pointer to the above uploaded instances.
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		instancesVk.data.deviceAddress = instanceBufferAddr;

		// Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
		VkAccelerationStructureGeometryKHR topASGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topASGeometry.geometry.instances = instancesVk;

		// Find sizes
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topASGeometry;
		buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		Device::vkGetAccelerationStructureBuildSizesKHR(Device::GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &instanceCount, &sizeInfo);

		// Create TLAS
		if (update == false)
		{
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			createInfo.size = sizeInfo.accelerationStructureSize;

			CreateAcceleration(createInfo, m_Tlas);
		}

		// Allocate the scratch memory
		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = sizeInfo.buildScratchSize;
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		BufferInfo.MinMemoryAlignment = Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment;
		scratchBuffer->Init(BufferInfo);

		// Update build information
		buildInfo.srcAccelerationStructure = update ? m_Tlas.Accel : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = m_Tlas.Accel;
		buildInfo.scratchData.deviceAddress = scratchBuffer->GetDeviceAddress();

		// Build Offsets info: n instances
		VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ instanceCount, 0, 0, 0 };
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		// Build the TLAS
		Device::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);
	}

	/**
	 * @brief Creates a Vulkan acceleration structure based on the provided creation information.
	 *
	 * @param createInfo - Vulkan acceleration structure creation information.
	 * @param As - AccelKHR object representing the acceleration structure and associated buffer.
	 */
	void AccelerationStructure::CreateAcceleration(VkAccelerationStructureCreateInfoKHR& createInfo, AccelKHR& As)
	{
		// Create a Vulkan buffer for the acceleration structure
		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = createInfo.size;
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		BufferInfo.MinOffsetAlignment = Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment;
		As.Buffer = std::make_shared<Buffer>(BufferInfo);

		// Setting the buffer
		createInfo.buffer = As.Buffer->GetBuffer();

		// Create the acceleration structure
		Device::vkCreateAccelerationStructureKHR(Device::GetDevice(), &createInfo, &As.Accel);
	}

	/**
	 * @brief Destroys non-compacted Acceleration Structures (AS) with provided indices.
	 *
	 * @param indices - Vector of indices specifying the non-compacted Acceleration Structures to destroy.
	 * @param buildAs - Vector of BuildAccelerationStructure objects containing build information.
	 */
	void AccelerationStructure::DestroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs)
	{
		for (auto& i : indices)
		{
			Device::vkDestroyAccelerationStructureKHR(Device::GetDevice(), buildAs[i].CleanupAs.Accel);
		}
	}

	/**
	 * @brief Retrieves the device address of a Bottom-Level Acceleration Structure (BLAS) by index.
	 *
	 * @param blasID - Index of the BLAS for which to retrieve the device address.
	 * @return VkDeviceAddress - Device address of the specified BLAS.
	 */
	VkDeviceAddress AccelerationStructure::GetBlasDeviceAddress(uint32_t blasID)
	{
		// Ensure the provided BLAS index is within valid range
		VK_CORE_ASSERT(blasID < m_Blas.size(), "There is no such index");

		// Set up acceleration structure device address information
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addressInfo.accelerationStructure = m_Blas[blasID].As.Accel;

		// Retrieve and return the device address of the specified BLAS
		return Device::vkGetAccelerationStructureDeviceAddressKHR(Device::GetDevice(), &addressInfo);
	}

	void AccelerationStructure::Reset()
	{
		m_Blas.clear();
		m_Tlas = {};
		m_Initialized = false;
	}

	void AccelerationStructure::Destroy()
	{
		if (!m_Initialized)
			return;

		for (int i = 0; i < m_Blas.size(); i++)
		{
			Device::vkDestroyAccelerationStructureKHR(Device::GetDevice(), m_Blas[i].As.Accel);
		}
		Device::vkDestroyAccelerationStructureKHR(Device::GetDevice(), m_Tlas.Accel);

		Reset();
	}

	void AccelerationStructure::Init(const CreateInfo& info)
	{
		if (m_Initialized)
			Destroy();

		CreateBottomLevelAS(info);
		CreateTopLevelAS(info);

		m_Initialized = true;
	}

	AccelerationStructure::~AccelerationStructure()
	{
		Destroy();
	}

	AccelerationStructure::AccelerationStructure(const CreateInfo& info)
	{
		Init(info);
	}

	AccelerationStructure::AccelerationStructure(AccelerationStructure&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Blas = std::move(other.m_Blas);
		m_Tlas = std::move(other.m_Tlas);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	AccelerationStructure& AccelerationStructure::operator=(AccelerationStructure&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Blas = std::move(other.m_Blas);
		m_Tlas = std::move(other.m_Tlas);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
		
		return *this;
	}

	void AccelerationStructure::CreateTopLevelAS(const CreateInfo& info)
	{
		std::vector<VkAccelerationStructureInstanceKHR> tlas;
		int meshCount = 0;
		for (int i = 0; i < info.Instances.size(); i++)
		{
			VkAccelerationStructureInstanceKHR instance{};
			instance.transform = info.Instances[i].transform;
			instance.instanceCustomIndex = meshCount;
			instance.accelerationStructureReference = GetBlasDeviceAddress(meshCount);
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			tlas.emplace_back(instance);
			meshCount++;
		}

		uint32_t instanceCount = (uint32_t)tlas.size();

		Buffer stagingBuffer{};
		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = instanceCount * sizeof(VkAccelerationStructureInstanceKHR); // change instance count?
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		BufferInfo.MinOffsetAlignment = Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment;
		stagingBuffer.Init(BufferInfo);
		stagingBuffer.Map();
		stagingBuffer.WriteToBuffer(tlas.data());

		Buffer instancesBuffer{};
		BufferInfo.InstanceSize = instanceCount * sizeof(VkAccelerationStructureInstanceKHR); // change instance count?
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		BufferInfo.MinOffsetAlignment = Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment;
		instancesBuffer.Init(BufferInfo);
		instancesBuffer.CopyBuffer(stagingBuffer.GetBuffer(), instancesBuffer.GetBuffer(), instancesBuffer.GetBufferSize(), 0, 0, Device::GetGraphicsQueue(), 0, Device::GetGraphicsCommandPool());

		VkCommandBuffer cmdBuf;
		Device::BeginSingleTimeCommands(cmdBuf, Device::GetComputeCommandPool());

		// Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		// Creating the TLAS
		Buffer scratchBuffer;
		CmdCreateTlas(cmdBuf, instanceCount, instancesBuffer.GetDeviceAddress(), &scratchBuffer, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, false);

		// Finalizing and destroying temporary data
		Device::EndSingleTimeCommands(cmdBuf, Device::GetComputeQueue(), Device::GetComputeCommandPool());
		stagingBuffer.Unmap();
	}

	void AccelerationStructure::CreateBottomLevelAS(const CreateInfo& info)
	{
		std::vector<BlasInput> blases;

		for (int i = 0; i < info.Instances.size(); i++)
		{
			BlasInput blas = MeshToGeometry(info.Instances[i].mesh);

			blases.emplace_back(blas);
		}

		uint32_t     blasCount = (uint32_t)blases.size();
		VkDeviceSize totalSize = 0; // Memory size of all allocated BLASes
		uint32_t     compactionsCount = 0; // Number of BLAS requesting compaction
		VkDeviceSize scratchSize = 0;

		std::vector<BuildAccelerationStructure> buildAs(blasCount);
		for (uint32_t i = 0; i < blasCount; i++)
		{
			// Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
			auto& buildAsRef = buildAs[i];
			buildAsRef.BuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			buildAsRef.BuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildAsRef.BuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildAsRef.BuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
			buildAsRef.BuildInfo.geometryCount = (uint32_t)blases[i].AsGeometry.size();
			buildAsRef.BuildInfo.pGeometries = blases[i].AsGeometry.data();

			// Build range information
			buildAsRef.RangeInfo = blases[i].AsRange.data();

			// Build size information
			buildAsRef.SizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

			// Finding sizes to create acceleration structures
			std::vector<uint32_t> trianglesCount(blases[i].AsRange.size());
			for (uint32_t j = 0; j < blases[i].AsRange.size(); j++)
			{
				trianglesCount[j] = blases[i].AsRange[j].primitiveCount;  // Number of primitives/triangles
			}

			Device::vkGetAccelerationStructureBuildSizesKHR(
				Device::GetDevice(),
				VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildAsRef.BuildInfo,
				trianglesCount.data(),
				&buildAsRef.SizeInfo
			);

			// Extra info
			totalSize += buildAsRef.SizeInfo.accelerationStructureSize;
			scratchSize = std::max(scratchSize, buildAsRef.SizeInfo.buildScratchSize);
			if ((buildAsRef.BuildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) != 0)
			{
				compactionsCount++;
			}
		}

		// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
		Buffer scratchBuffer{};
		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = scratchSize; // change instance count?
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		BufferInfo.MinMemoryAlignment = Device::GetAccelerationProperties().minAccelerationStructureScratchOffsetAlignment;
		scratchBuffer.Init(BufferInfo);
		VkDeviceAddress scratchAddress = scratchBuffer.GetDeviceAddress();

		// Allocate a query pool for storing the needed size for every BLAS compaction.
		VkQueryPool queryPool = VK_NULL_HANDLE;
		if (compactionsCount > 0)  // Is compaction requested?
		{
			VK_CORE_ASSERT(compactionsCount == blasCount, "Don't allow mix of on/off compaction");
			VkQueryPoolCreateInfo qpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
			qpci.queryCount = blasCount;
			qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
			vkCreateQueryPool(Device::GetDevice(), &qpci, nullptr, &queryPool);
		}

		// Batching creation/compaction of BLAS to allow staying in restricted amount of memory
		std::vector<uint32_t> indices;  // Indices of the BLAS to create
		VkDeviceSize          batchSize = 0;
		VkDeviceSize          batchLimit = 256'000'000;  // 256 MB
		for (uint32_t i = 0; i < blasCount; i++)
		{
			indices.push_back(i);
			batchSize += buildAs[i].SizeInfo.accelerationStructureSize;
			// Over the limit or last BLAS element
			if (batchSize >= batchLimit || i == blasCount - 1)
			{
				VkCommandBuffer cmdBuf;
				Device::BeginSingleTimeCommands(cmdBuf, Device::GetComputeCommandPool());
				CmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
				Device::EndSingleTimeCommands(cmdBuf, Device::GetComputeQueue(), Device::GetComputeCommandPool());

				if (queryPool)
				{
					VkCommandBuffer cmdBuf;
					Device::BeginSingleTimeCommands(cmdBuf, Device::GetGraphicsCommandPool());
					CmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
					Device::EndSingleTimeCommands(cmdBuf, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());

					// Destroy the non-compacted version
					DestroyNonCompacted(indices, buildAs);
				}
				// Reset

				batchSize = 0;
				indices.clear();
			}
		}

		// Keeping all the created acceleration structures
		for (auto& b : buildAs)
		{
			m_Blas.emplace_back(b.As);
		}

		vkDestroyQueryPool(Device::GetDevice(), queryPool, nullptr);
	}
}

