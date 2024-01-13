#pragma once
#include "pch.h"

#include "Buffer.h"
#include "Device.h"
#include "Sampler.h"
#include "Vulture/Utility/Utility.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	enum class ImageType
	{
		Image2D,
		Image2DArray,
		Cubemap,
	};

	struct ImageInfo
	{
		uint32_t width = 0;
		uint32_t height = 0;
		VkFormat format;
		VkImageUsageFlags usage;
		VkMemoryPropertyFlags properties;
		VkImageAspectFlagBits aspect;
		VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		SamplerInfo samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST };
		int layerCount = 1;
		ImageType type = ImageType::Image2D;
	};

	class Image
	{
	public:
		Image(const ImageInfo& imageInfo);
		Image(const std::string& filepath, SamplerInfo samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST });
		~Image();
		static void TransitionImageLayout(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout, VkCommandBuffer cmdBuffer = 0, const VkImageSubresourceRange& subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height, VkOffset3D offset = {0, 0, 0});
		void CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkImageLayout layout, VkOffset3D srcOffset = { 0, 0, 0 }, VkOffset3D dstOffset = {0, 0, 0});
		
		inline VkImage GetImage() { return m_Image; }
		inline VkImageView GetImageView() { return m_ImageView; }
		inline VmaAllocationInfo GetAllocationInfo() { VmaAllocationInfo info{}; vmaGetAllocationInfo(Device::GetAllocator(), *m_Allocation, &info); return info; }
		inline VkSampler GetSampler() { return m_Sampler->GetSampler(); }
		inline glm::vec2 GetImageSize() { return m_Size; }
		inline VkImageView GetLayerView(int layer) { return m_LayersView[layer]; }

	private:
		void CreateImageView(VkFormat format, VkImageAspectFlagBits aspect, int layerCount = 1, VkImageViewType imageType = VK_IMAGE_VIEW_TYPE_2D);
		void CreateImage(const ImageInfo& imageInfo);
		void GenerateMipmaps();
		void CreateImageSampler(SamplerInfo samplerInfo);
		Ref<Sampler> m_Sampler;

		VkImage m_Image;
		VkImageView m_ImageView;
		std::vector<VkImageView> m_LayersView; // only for layered images
		VmaAllocation* m_Allocation;

		glm::vec2 m_Size;
		uint32_t m_MipLevels = 1;
	};

}