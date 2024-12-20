#pragma once

#include "Device.h"
#include "Pipeline.h"

namespace VulkanHelper
{
	class SBT
	{
	public:
		struct CreateInfo
		{
			uint32_t RGenCount = 0xFFFFFFFF;
			uint32_t MissCount = 0xFFFFFFFF;
			uint32_t HitCount = 0xFFFFFFFF;
			uint32_t CallableCount = 0xFFFFFFFF;

			Pipeline* RayTracingPipeline = nullptr;

			operator bool() const
			{
				return (RGenCount != 0xFFFFFFFF) && (MissCount != 0xFFFFFFFF) && (HitCount != 0xFFFFFFFF) && (CallableCount != 0xFFFFFFFF) && (RayTracingPipeline != nullptr);
			}
		};

		void Init(CreateInfo* createInfo);
		void Destroy();


		SBT() = default;
		SBT(CreateInfo* createInfo);
		~SBT();
		SBT(const SBT&) = delete;
		SBT& operator=(const SBT&) = delete;
		SBT(SBT&& other) noexcept;
		SBT& operator=(SBT&& other) noexcept;

	public:
		inline uint32_t GetRGenCount() const { return m_RGenCount; }
		inline uint32_t GetHitCount() const { return m_HitCount; }
		inline uint32_t GetMissCount() const { return m_MissCount; }
		inline uint32_t GetCallCount() const { return m_CallableCount; }

		inline VkStridedDeviceAddressRegionKHR GetRGenRegion() const { return m_RgenRegion; }
		inline VkStridedDeviceAddressRegionKHR GetMissRegion() const { return m_MissRegion; }
		inline VkStridedDeviceAddressRegionKHR GetHitRegion() const { return m_HitRegion; }
		inline VkStridedDeviceAddressRegionKHR GetCallRegion() const { return m_CallRegion; }

		inline const VkStridedDeviceAddressRegionKHR* GetRGenRegionPtr() const { return &m_RgenRegion; }
		inline const VkStridedDeviceAddressRegionKHR* GetMissRegionPtr() const { return &m_MissRegion; }
		inline const VkStridedDeviceAddressRegionKHR* GetHitRegionPtr() const { return &m_HitRegion; }
		inline const VkStridedDeviceAddressRegionKHR* GetCallRegionPtr() const { return &m_CallRegion; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:

		uint32_t m_RGenCount = 0;
		uint32_t m_MissCount = 0;
		uint32_t m_HitCount = 0;
		uint32_t m_CallableCount = 0;
		Pipeline* m_RayTracingPipeline = nullptr;

		VkStridedDeviceAddressRegionKHR m_RgenRegion{};
		VkStridedDeviceAddressRegionKHR m_MissRegion{};
		VkStridedDeviceAddressRegionKHR m_HitRegion{};
		VkStridedDeviceAddressRegionKHR m_CallRegion{};
		VulkanHelper::Buffer m_RtSBTBuffer;

		bool m_Initialized = false;

		void Reset();
	};
}