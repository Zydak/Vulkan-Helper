#pragma once

#include <array>

#include <cuda.h>
#include <cuda_runtime.h>

#include "imgui.h"

#include "optix_types.h"
#include <driver_types.h>

#include "Vulkan/Image.h"
#include "Vulkan/Sampler.h"

namespace VulkanHelper
{

static void contextLogCb(unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    VK_CORE_INFO("[{0}][{1}]:{2}", level, tag, message);
}

class Denoiser
{
public:
	void Init();
	void Destroy();

	Denoiser() = default;
	~Denoiser();

	Denoiser(const Denoiser& other) = delete;
	Denoiser(Denoiser&& other) noexcept = delete;

	Denoiser& operator=(const Denoiser& other) = delete;
	Denoiser& operator=(Denoiser&& other) noexcept = delete;

	void DenoiseImageBuffer(uint64_t& fenceValue, float blendFactor = 0.0f);
	void CreateSemaphore();

	inline VkSemaphore GetTLSemaphore() const { return m_Semaphore.Vk; }

	void AllocateBuffers(VkExtent2D imgSize);
	void BufferToImage(VkCommandBuffer cmdBuf, VulkanHelper::Image* imgOut);
	void ImageToBuffer(VkCommandBuffer cmdBuf, const std::vector<VulkanHelper::Image*>& imgIn);

	inline bool IsInitialized() const { return m_Initialized; }

private:
	void DestroyBuffer();

    Sampler m_Sampler;

    OptixDeviceContext     m_OptixDevice{};
    OptixDenoiser          m_Denoiser{};
    OptixDenoiserOptions   m_DenoiserOptions{};
    OptixDenoiserSizes     m_DenoiserSizes{};
    OptixDenoiserAlphaMode m_DenoiserAlpha{OPTIX_DENOISER_ALPHA_MODE_COPY};
    OptixPixelFormat       m_PixelFormat{};
    
    CUdeviceptr m_StateBuffer{};
    CUdeviceptr m_ScratchBuffer{};
    CUdeviceptr m_Intensity{};
    CUdeviceptr m_MinRGB{};
    CUstream    m_CudaStream{};
    
    VkExtent2D m_ImageSize{};
    uint32_t   m_SizeofPixel{};
    
	struct BufferCuda
	{
		VulkanHelper::Buffer BufferVk;
		HANDLE Handle = nullptr;  // The Win32 handle
		void* CudaPtr = nullptr;  // Pointer for cuda

		void Destroy();
	};

	std::array<BufferCuda, 3> m_PixelBufferIn;  // Buffers for the input images
	BufferCuda m_PixelBufferOut; // Result of the denoiser

	struct Semaphore
	{
		VkSemaphore Vk{};  // Vulkan
		cudaExternalSemaphore_t Cu{};  // Cuda version
		HANDLE Handle{ INVALID_HANDLE_VALUE };
	};

	Semaphore m_Semaphore;

	void CreateBufferHandles(BufferCuda& buf);

	bool m_Initialized = false;
};

}  // namespace VulkanHelper