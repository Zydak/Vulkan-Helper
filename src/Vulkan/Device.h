#pragma once
#include "pch.h"

#include "Utility/Utility.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

#include "glm/glm.hpp"

#include "vulkan/vulkan_win32.h"

namespace VulkanHelper
{
	class Window;

	enum class Vendor
	{
		NVIDIA,
		AMD,
		INTEL,
		ImgTec,
		ARM,
		Qualcomm,
		Unknown
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;      // min/max number of images
		std::vector<VkSurfaceFormatKHR> Formats;    // pixel format, color space
		std::vector<VkPresentModeKHR> PresentModes;
	};

	struct QueueFamilyIndices
	{
		uint32_t GraphicsFamily = 0;
		uint32_t PresentFamily = 0;
		uint32_t ComputeFamily = 0;
		bool GraphicsFamilyHasValue = false;
		bool PresentFamilyHasValue = false;
		bool ComputeFamilyHasValue = false;

		bool IsComplete() const { return GraphicsFamilyHasValue && PresentFamilyHasValue && ComputeFamilyHasValue; }
	};

	struct Extension
	{
		const char* Name = "";
		bool supported = false;
	};

	struct CommandPool
	{
		VkCommandPool GraphicsCommandPool;
		VkCommandPool ComputeCommandPool;
	};

	class Device
	{
	public:

		struct PhysicalDeviceRequirements
		{
			SwapchainSupportDetails SwapchainSupport;
			QueueFamilyIndices QueueIndices;
			std::set<std::string> UnsupportedButRequiredExtensions; // This has to be for device to be suitable

			bool IsSuitable() const
			{
				return QueueIndices.IsComplete() && UnsupportedButRequiredExtensions.empty() && !SwapchainSupport.Formats.empty() && !SwapchainSupport.PresentModes.empty();
			}
		};

		struct PhysicalDevice
		{
			struct CreateInfo
			{
				Window* window = nullptr;
				std::vector<const char*> DeviceExtensions;
				std::vector<const char*> OptionalExtensions;
				VkPhysicalDeviceFeatures2 Features = {};

				bool UseMemoryAddress = true;
				bool UseRayTracing = false;
			};

			std::string Name = "INVALID DEVICE";
			Vendor Vendor = Vendor::Unknown;
			PhysicalDeviceRequirements Requirements{};
			bool Discrete = false;
			VkPhysicalDevice Handle = VK_NULL_HANDLE;

			/*
			 *	@brief Returns whether device is suitable for use given all the requirements in VulkanHelper::QueryDevices().
			 *	
			 *	@param outError - Message telling why the device isn't suitable for use. e.g. which extensions are missing.
			 */
			bool IsSuitable(std::string& outError) const
			{
				if (!Requirements.IsSuitable())
				{
					outError.append(std::format("{} device is incompatible!", Name));

					if (!Requirements.UnsupportedButRequiredExtensions.empty())
					{
						outError.append("\nIt doesn't support following extensions which are listed as required:");
						for (auto& extension : Requirements.UnsupportedButRequiredExtensions)
						{
							outError.append(std::format("\n\t{}", extension));
						}
					}

					if (!Requirements.QueueIndices.IsComplete())
					{
						outError.append(std::format("\nIt doesn't support following queue families:"));
						outError.append(std::format("\n\tGraphics Family Queue Supported - {}", Requirements.QueueIndices.GraphicsFamilyHasValue));
						outError.append(std::format("\n\tPresent Family Queue Supported - {}", Requirements.QueueIndices.PresentFamilyHasValue));
						outError.append(std::format("\n\tCompute Family Queue Supported - {}", Requirements.QueueIndices.ComputeFamilyHasValue));
					}

					if (Requirements.SwapchainSupport.PresentModes.empty() || Requirements.SwapchainSupport.Formats.empty())
						outError.append("\nThe swapchain can't be created on this device!");

					return false;
				}
				else
					return true;
			}
		};

		static [[nodiscard]] std::vector<PhysicalDevice> QueryPhysicalDevices(const PhysicalDevice::CreateInfo& createInfo);
		static void Init(const PhysicalDevice& device);
		static void Destroy();

		~Device();

		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;
		Device(Device&&) = delete;
		Device& operator=(Device&&) = delete;

		//static inline VkInstance GetInstance() { return s_Instance; }
		static inline VkDevice GetDevice() { return s_Device; }
		static inline VkPhysicalDevice GetPhysicalDevice() { return s_PhysicalDevice.Handle; }
		static inline SwapchainSupportDetails GetSwapchainSupport(VkSurfaceKHR surface) { return QuerySwapchainSupport(s_PhysicalDevice.Handle, surface); }
		static inline QueueFamilyIndices FindPhysicalQueueFamilies() { return s_PhysicalDevice.Requirements.QueueIndices; }
		static inline VkCommandPool& GetGraphicsCommandPool() { return s_CommandPools[std::this_thread::get_id()].GraphicsCommandPool; }
		static inline VkCommandPool& GetComputeCommandPool() { return s_CommandPools[std::this_thread::get_id()].ComputeCommandPool; }
		static inline VkQueue GetGraphicsQueue() { return s_GraphicsQueue; }
		static inline VkQueue GetPresentQueue() { return s_PresentQueue; }
		static inline VkQueue GetComputeQueue() { return s_ComputeQueue; }
		static inline VkPhysicalDeviceAccelerationStructurePropertiesKHR GetAccelerationProperties() { return s_AccelerationStructureProperties; }
		static inline void WaitIdle() { vkDeviceWaitIdle(s_Device); }
		static inline Vendor GetVendor() { return s_PhysicalDevice.Vendor; }

		static inline VmaAllocator GetAllocator() { return s_Allocator; }

		static inline bool IsInitialized() { return s_Initialized; }

		static inline VkPhysicalDeviceProperties2 GetDeviceProperties() { return s_Properties; }
		static inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetRayTracingProperties() { return s_RayTracingProperties; }
		static VkSampleCountFlagBits GetMaxSampleCount();

		static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		
		static void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool);
		static void BeginSingleTimeCommands(VkCommandBuffer& buffer, VkCommandPool pool);
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

		static void CreateBuffer(VkBufferCreateInfo& createInfo, VkBuffer& buffer, VmaAllocation& alloc, VkMemoryPropertyFlags customFlags = 0, VmaPool* poolOut = nullptr, bool noPool = false, VkDeviceSize minAlignment = 1);
		static void CreateImage(VkImageCreateInfo& createInfo, VkImage& image, VmaAllocation& alloc, VkMemoryPropertyFlags customFlags = 0);

		static void SetObjectName(VkObjectType type, uint64_t handle, const char* name);
		static void BeginLabel(VkCommandBuffer cmd, const char* name, glm::vec4 color);
		static void EndLabel(VkCommandBuffer cmd);
		static void InsertLabel(VkCommandBuffer cmd, const char* name, glm::vec4 color);

		static void CreateCommandPoolForThread();

		inline static std::mutex& GetGraphicsQueueMutex() { return s_GraphicsQueueMutex; };
		inline static std::mutex& GetComputeQueueMutex() { return s_ComputeQueueMutex; };

		//TODO description
		template <class integral>
		static VkDeviceSize GetAlignment(integral x, size_t a)
		{
			return integral((x + (integral(a) - 1)) & ~integral(a - 1));
		}

		static bool inline UseRayTracing() { return s_UseRayTracing; }
	private:
		Device() {} // make constructor private
		static bool s_Initialized;

		static PhysicalDeviceRequirements IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

		static void CreateMemoryAllocator();
		static void CreateMemoryPool(VkMemoryPropertyFlags propertyFlags, uint32_t memoryIndex, VmaPool& pool, VkDeviceSize MBSize = 10, VkDeviceSize ByteSize = 0, bool isImage = false);
		static void FindMemoryTypeIndex(VkMemoryPropertyFlags flags, uint32_t& memoryIndex);
		static void FindMemoryTypeIndexForBuffer(VkBufferCreateInfo& createInfo, uint32_t& memoryIndex, VkMemoryPropertyFlags flags = 0);
		static void FindMemoryTypeIndexForImage(VkImageCreateInfo& createInfo, uint32_t& memoryIndex, VkMemoryPropertyFlags flags = 0);

		static std::vector<PhysicalDevice> EnumeratePhysicalDevices(VkSurfaceKHR surface);
		static void CreateLogicalDevice();
		static void CreateCommandPools();

		static bool CheckValidationLayerSupport();
		static std::set<std::string> CheckDeviceExtensionSupport(VkPhysicalDevice device);
		static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

		static VkExportMemoryAllocateInfo s_ExportMemoryInfo;
		static VkExternalMemoryBufferCreateInfo s_ExternalMemoryBufferInfo;
		static VkExternalMemoryImageCreateInfo s_ExternalMemoryImageInfo;

		static VmaAllocator s_Allocator;
		static std::unordered_map<uint32_t, VmaPool> s_Pools;
		static inline std::unordered_map<uint32_t, VmaPool> s_ImagePools;
		static VkPhysicalDeviceProperties2 s_Properties;
		static VkSampleCountFlagBits s_MaxSampleCount;
		static VkPhysicalDeviceFeatures2 s_Features;
		static VkDebugUtilsMessengerEXT s_DebugMessenger;
		static inline PhysicalDevice s_PhysicalDevice;
		static VkDevice s_Device;
		static bool s_UseMemoryAddressFeature;

		static VkQueue s_GraphicsQueue;
		static std::mutex s_GraphicsQueueMutex;
		static VkQueue s_ComputeQueue;
		static std::mutex s_ComputeQueueMutex;

		static VkQueue s_PresentQueue;

		static std::unordered_map<std::thread::id, CommandPool> s_CommandPools;

		static bool s_UseRayTracing;
		static std::vector<const char*> s_DeviceExtensions;
		static std::vector<Extension> s_OptionalExtensions;
		static VkPhysicalDeviceRayTracingPipelinePropertiesKHR s_RayTracingProperties;
		static VkPhysicalDeviceAccelerationStructurePropertiesKHR s_AccelerationStructureProperties;
		static VkPhysicalDeviceSubgroupProperties s_SubgroupProperties;


#ifdef DISTRIBUTION
		static const bool s_EnableValidationLayers = false;
#else
		static const bool s_EnableValidationLayers = true;
#endif

	public:
		// Loaded functions
		static VkResult vkCreateAccelerationStructureKHR(VkDevice device, VkAccelerationStructureCreateInfoKHR* createInfo, VkAccelerationStructureKHR* structure);
		static void vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR structure);
		static void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos);
		static void vkCmdWriteAccelerationStructuresPropertiesKHR(VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery);
		static void vkCmdCopyAccelerationStructureKHR(VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo);
		static void vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo);
		static VkResult vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines);
		static VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkDevice device,const VkAccelerationStructureDeviceAddressInfoKHR* pInfo);
		static VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData);
		static void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth);
		static void vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites);
		static VkResult vkGetMemoryWin32HandleKHR(VkDevice device, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle);
		static VkResult vkGetSemaphoreWin32HandleKHR(VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle);
		static VkResult vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo);
		static void vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer,const VkDebugUtilsLabelEXT* pLabelInfo);
		static void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer);
		static void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo);
		static void vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo);
		static void vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer);
	};
}