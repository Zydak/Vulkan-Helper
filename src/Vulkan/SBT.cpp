#include "pch.h"

#include "sbt.h"

namespace VulkanHelper
{
	void SBT::Init(CreateInfo* createInfo)
	{
		if (m_Initialized)
			Destroy();

		VK_CORE_ASSERT(*createInfo, "Incorectly Initialized SBT Create Info!");
		VK_CORE_ASSERT(createInfo->RGenCount == 1, "RGenCount must be 1!");

		m_MissCount = createInfo->MissCount;
		m_HitCount = createInfo->HitCount;
		m_RGenCount = createInfo->RGenCount;
		m_CallableCount = createInfo->CallableCount;
		m_RayTracingPipeline = createInfo->RayTracingPipeline;

		uint32_t handleCount = m_MissCount + m_HitCount + m_RGenCount + m_CallableCount;
		uint32_t handleSize = VulkanHelper::Device::GetRayTracingProperties().shaderGroupHandleSize;
		uint32_t shaderGroupBaseAlignment = VulkanHelper::Device::GetRayTracingProperties().shaderGroupBaseAlignment;

		uint32_t handleSizeAligned = (uint32_t)VulkanHelper::Device::GetAlignment(handleSize, VulkanHelper::Device::GetRayTracingProperties().shaderGroupHandleAlignment);

		m_RgenRegion.stride = VulkanHelper::Device::GetAlignment(handleSizeAligned * m_RGenCount, shaderGroupBaseAlignment);
		m_RgenRegion.size = m_RgenRegion.stride;

		m_MissRegion.stride = VulkanHelper::Device::GetAlignment(m_MissCount * handleSizeAligned, shaderGroupBaseAlignment);
		m_MissRegion.size = m_MissRegion.stride;

		m_HitRegion.stride = VulkanHelper::Device::GetAlignment(m_HitCount * handleSizeAligned, shaderGroupBaseAlignment);
		m_HitRegion.size = m_HitRegion.stride;
		
		m_CallRegion.stride = VulkanHelper::Device::GetAlignment(m_CallableCount * handleSizeAligned, shaderGroupBaseAlignment);
		m_CallRegion.size = m_CallRegion.stride;

		// Get the shader group handles
		uint32_t dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		auto result = VulkanHelper::Device::vkGetRayTracingShaderGroupHandlesKHR(VulkanHelper::Device::GetDevice(), m_RayTracingPipeline->GetPipeline(), 0, handleCount, dataSize, handles.data());
		VK_CORE_ASSERT(result == VK_SUCCESS, "Failed to get shader group handles!");

		// Allocate a buffer for storing the SBT.
		VkDeviceSize sbtSize = m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size + m_CallRegion.size;

		VulkanHelper::Buffer::CreateInfo bufferInfo{};
		bufferInfo.InstanceSize = sbtSize;
		bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		VulkanHelper::Buffer stagingBuffer;
		stagingBuffer.Init(bufferInfo);

		bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		bufferInfo.MinMemoryAlignment = VulkanHelper::Device::GetRayTracingProperties().shaderGroupBaseAlignment;
		m_RtSBTBuffer.Init(bufferInfo);

		// Find the SBT addresses of each group
		m_RgenRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress();
		m_MissRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress() + m_RgenRegion.stride;
		m_HitRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress() + m_RgenRegion.stride + m_MissRegion.stride;
		m_CallRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress() + m_RgenRegion.stride + m_MissRegion.stride + m_HitRegion.stride;

		auto getHandle = [&](uint32_t i) { return handles.data() + (i * handleSize); };

		stagingBuffer.Map();
		uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(stagingBuffer.GetMappedMemory());
		uint8_t* pData = nullptr;
		uint32_t handleIdx = 0;

		// Ray gen
		pData = pSBTBuffer;

		for (uint32_t c = 0; c < m_RGenCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData = pSBTBuffer + m_RgenRegion.stride;
		}

		// Miss
		for (uint32_t c = 0; c < m_MissCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_MissRegion.stride;
		}

		// Hit
		pData = pSBTBuffer + m_RgenRegion.size + m_MissRegion.size;
		for (uint32_t c = 0; c < m_HitCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_HitRegion.stride;
		}

		// Callable
		pData = pSBTBuffer + m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size;
		for (uint32_t c = 0; c < m_CallableCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_CallRegion.stride;
		}

		stagingBuffer.Flush();

		// Copy the shader binding table to the device local buffer
		VulkanHelper::Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_RtSBTBuffer.GetBuffer(), sbtSize, 0, 0, VulkanHelper::Device::GetGraphicsQueue(), 0, VulkanHelper::Device::GetGraphicsCommandPool());

		stagingBuffer.Unmap();

		m_Initialized = true;
	}

	void SBT::Destroy()
	{
		if (!m_Initialized)
			return;

		m_RtSBTBuffer.Destroy();
		
		Reset();
	}

	SBT::~SBT()
	{
		Destroy();
	}

	SBT::SBT(CreateInfo* createInfo)
	{
		Init(createInfo);
	}

	SBT::SBT(SBT&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_RGenCount				= std::move(other.m_RGenCount);
		m_MissCount				= std::move(other.m_MissCount);
		m_HitCount				= std::move(other.m_HitCount);
		m_CallableCount			= std::move(other.m_CallableCount);
		m_RayTracingPipeline	= std::move(other.m_RayTracingPipeline);
		m_RgenRegion			= std::move(other.m_RgenRegion);
		m_MissRegion			= std::move(other.m_MissRegion);
		m_HitRegion				= std::move(other.m_HitRegion);
		m_CallRegion			= std::move(other.m_CallRegion);
		m_RtSBTBuffer			= std::move(other.m_RtSBTBuffer);
		m_Initialized			= std::move(other.m_Initialized);

		other.Reset();
	}

	SBT& SBT::operator=(SBT&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_RGenCount = std::move(other.m_RGenCount);
		m_MissCount = std::move(other.m_MissCount);
		m_HitCount = std::move(other.m_HitCount);
		m_CallableCount = std::move(other.m_CallableCount);
		m_RayTracingPipeline = std::move(other.m_RayTracingPipeline);
		m_RgenRegion = std::move(other.m_RgenRegion);
		m_MissRegion = std::move(other.m_MissRegion);
		m_HitRegion = std::move(other.m_HitRegion);
		m_CallRegion = std::move(other.m_CallRegion);
		m_Initialized = std::move(other.m_Initialized);
		m_RtSBTBuffer = std::move(other.m_RtSBTBuffer);

		other.Reset();

		return *this;
	}

	void SBT::Reset()
	{
		m_RGenCount = 0;
		m_MissCount = 0;
		m_HitCount = 0;
		m_CallableCount = 0;
		m_RayTracingPipeline = nullptr;
		m_RgenRegion = {};
		m_MissRegion = {};
		m_HitRegion = {};
		m_CallRegion = {};
		m_Initialized = false;
	}

}