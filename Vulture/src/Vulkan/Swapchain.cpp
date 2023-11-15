#include "pch.h"
#include "Utility/Utility.h"

#include "Swapchain.h"
#include "Framebuffer.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	Swapchain::Swapchain(VkExtent2D windowExtent, const PresentModes& prefferedPresentMode)
		: m_WindowExtent(windowExtent)
	{
		m_SwapchainDepthFormat = FindDepthFormat();
		CreateSwapchain(prefferedPresentMode);
		CreateImageViews();
		CreateRenderPass();
		CreateFramebuffers();
		CreateSyncObjects();
	}

	Swapchain::Swapchain(VkExtent2D windowExtent, const PresentModes& prefferedPresentMode, Ref<Swapchain> previousSwapchain)
		: m_WindowExtent(windowExtent), m_OldSwapchain(previousSwapchain)
	{
		m_SwapchainDepthFormat = FindDepthFormat();
		CreateSwapchain(prefferedPresentMode);
		CreateImageViews();
		CreateRenderPass();
		CreateFramebuffers();
		CreateSyncObjects();

		m_OldSwapchain = nullptr;
	}

	Swapchain::~Swapchain()
	{
		for (auto imageView : m_PresentableImageViews) { vkDestroyImageView(Device::GetDevice(), imageView, nullptr); }
		m_PresentableImageViews.clear();

		if (m_Swapchain != 0)
		{
			vkDestroySwapchainKHR(Device::GetDevice(), m_Swapchain, nullptr);
			m_Swapchain = 0;
		}

		for (auto framebuffer : m_PresentableFramebuffers) { vkDestroyFramebuffer(Device::GetDevice(), framebuffer, nullptr); }

		// cleanup synchronization objects
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(Device::GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(Device::GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
			vkDestroyFence(Device::GetDevice(), m_InFlightFences[i], nullptr);
		}
	}

	VkSurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats)
		{
			// VK_COLOR_SPACE_SRGB_NONLINEAR_KHR specifies if SRGB color space is used
			// SRGB color space results in more accurate perceived colors* + it is standard for textures
			// * https://stackoverflow.com/questions/12524623/what-are-the-practical-differences-when-working-with-colors-in-a-linear-vs-a-no
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	/**
	 * @brief Chooses how to present images to Screen
	 * @brief Mailbox - Most efficient one, solves screen tearing issue in immediate mode. (not supported by Linux)
	 * @brief Immediate - Presents images on screen as fast as possible. Possible screen tearing.
	 * @brief V-Sync (FIFO) - Synchronizes presenting images with monitor refresh rate.
	*/
	VkPresentModeKHR Swapchain::ChooseSwapPresentMode(const PresentModes& presentMode)
	{
		switch (presentMode)
		{
		case PresentModes::VSync:
			if (m_AvailablePresentModes[0].Available)
				return VK_PRESENT_MODE_FIFO_KHR;
			else
				VL_CORE_ASSERT(false, "You chose unsupported format!");
			m_CurrentPresentMode = PresentModes::VSync;
			break;

		case PresentModes::Immediate:
			if (m_AvailablePresentModes[1].Available)
				return VK_PRESENT_MODE_IMMEDIATE_KHR;
			else
				VL_CORE_ASSERT(false, "You chose unsupported format!");
			m_CurrentPresentMode = PresentModes::Immediate;
			break;

		case PresentModes::MailBox:
			if (m_AvailablePresentModes[2].Available)
				return VK_PRESENT_MODE_MAILBOX_KHR;
			else
				VL_CORE_ASSERT(false, "You chose unsupported format!");
			m_CurrentPresentMode = PresentModes::MailBox;
			break;
		default:
			m_CurrentPresentMode = PresentModes::VSync;
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		return VK_PRESENT_MODE_FIFO_KHR; // just to get rid of compiler warning
	}

	VkExtent2D Swapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) { return capabilities.currentExtent; }
		else
		{
			VkExtent2D actualExtent = m_WindowExtent;
			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	void Swapchain::CreateSwapchain(const PresentModes& presentMode)
	{
		SwapchainSupportDetails swapChainSupport = Device::GetSwapchainSupport();

		FindPresentModes(swapChainSupport.PresentModes);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.Formats);
		VkPresentModeKHR swapchainPresentMode = ChooseSwapPresentMode(presentMode);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.Capabilities);

		uint32_t imageCount = swapChainSupport.Capabilities.minImageCount;
		if (imageCount < MAX_FRAMES_IN_FLIGHT)
			imageCount += MAX_FRAMES_IN_FLIGHT - imageCount;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = Device::GetSurface();

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = Device::FindPhysicalQueueFamilies();
		uint32_t queueFamilyIndices[] = { indices.GraphicsFamily, indices.PresentFamily };

		// if graphics and present queue are the same which happens on some hardware create images in sharing mode
		if (indices.GraphicsFamily != indices.PresentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;          // Optional
			createInfo.pQueueFamilyIndices = nullptr;    // Optional
		}

		createInfo.preTransform = swapChainSupport.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = swapchainPresentMode;
		createInfo.clipped = VK_TRUE;    // discards pixels that are obscured (for example behind other window)

		createInfo.oldSwapchain = m_OldSwapchain == nullptr ? VK_NULL_HANDLE : m_OldSwapchain->m_Swapchain;

		VL_CORE_RETURN_ASSERT(vkCreateSwapchainKHR(Device::GetDevice(), &createInfo, nullptr, &m_Swapchain),
			VK_SUCCESS, "failed to create swap chain");

		vkGetSwapchainImagesKHR(Device::GetDevice(), m_Swapchain, &imageCount, nullptr);
		m_PresentableImages.resize(imageCount);
		vkGetSwapchainImagesKHR(Device::GetDevice(), m_Swapchain, &imageCount, m_PresentableImages.data());

		m_SwapchainImageFormat = surfaceFormat.format;
		m_SwapchainExtent = extent;
	}

	void Swapchain::CreateImageViews()
	{
		m_PresentableImageViews.resize(m_PresentableImages.size());

		for (uint32_t i = 0; i < m_PresentableImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_PresentableImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_SwapchainImageFormat;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VL_CORE_RETURN_ASSERT(vkCreateImageView(Device::GetDevice(), &createInfo, nullptr, &m_PresentableImageViews[i]),
				VK_SUCCESS,
				"failed to create texture image view"
			);
		}
	}

	void Swapchain::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = GetSwapchainImageFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = nullptr;

		std::vector<VkAttachmentDescription> attachments = { colorAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 0;
		renderPassInfo.pDependencies = nullptr;

		m_RenderPass.CreateRenderPass(renderPassInfo);
	}

	VkFormat Swapchain::FindDepthFormat()
	{
		return Device::FindSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	void Swapchain::FindPresentModes(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		m_AvailablePresentModes.resize(3);
		m_AvailablePresentModes[0].Mode = PresentModes::VSync;
		m_AvailablePresentModes[0].Available = true;
		m_AvailablePresentModes[1].Mode = PresentModes::Immediate;
		m_AvailablePresentModes[1].Available = false;
		m_AvailablePresentModes[2].Mode = PresentModes::MailBox;
		m_AvailablePresentModes[2].Available = false;
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				m_AvailablePresentModes[1].Available = true;
			}
		}

		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				m_AvailablePresentModes[2].Available = true;
			}
		}
	}

	void Swapchain::CreateFramebuffers()
	{

		// HDR / Presentable
		{
			m_PresentableFramebuffers.resize(GetImageCount());
			for (uint32_t i = 0; i < GetImageCount(); i++)
			{
				std::vector<VkImageView> attachments = { m_PresentableImageViews[i] };

				VkFramebufferCreateInfo framebufferInfo = {};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = m_RenderPass.GetRenderPass();
				framebufferInfo.attachmentCount = (uint32_t)attachments.size();
				framebufferInfo.pAttachments = attachments.data();
				framebufferInfo.width = m_SwapchainExtent.width;
				framebufferInfo.height = m_SwapchainExtent.height;
				framebufferInfo.layers = 1;

				VL_CORE_RETURN_ASSERT(vkCreateFramebuffer(Device::GetDevice(), &framebufferInfo, nullptr, &m_PresentableFramebuffers[i]),
					VK_SUCCESS,
					"failed to create framebuffer!"
				);
			}
		}
	}

	/**
	 * @brief Synchronizes CPU-GPU work, submits command buffer into graphics queue and presents image
	*/
	VkResult Swapchain::SubmitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex)
	{
		// if (m_ImagesInFlight[*imageIndex] != VK_NULL_HANDLE) 
		// {
		// 	vkWaitForFences(Device::GetDevice(), 1, &m_ImagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
		// }
		// m_ImagesInFlight[*imageIndex] = m_InFlightFences[m_CurrentFrame];

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores = m_ImageAvailableSemaphores[m_CurrentFrame];
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = buffers;

		VkSemaphore signalSemaphores = m_RenderFinishedSemaphores[m_CurrentFrame];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphores;

		vkResetFences(Device::GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
		VL_CORE_RETURN_ASSERT(vkQueueSubmit(Device::GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]),
			VK_SUCCESS,
			"failed to submit draw command buffer!"
		);

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &signalSemaphores;

		VkSwapchainKHR swapChains[] = { m_Swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = imageIndex;

		auto result = vkQueuePresentKHR(Device::GetPresentQueue(), &presentInfo);

		m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		return result;
	}

	/**
	 * @brief Acquires next image from swapchain for rendering
	 *
	 * @param imageIndex Changes current image index to next Available image in swapchain
	 *
	 * @return Returns result of Acquiring image from swapchain
	*/
	VkResult Swapchain::AcquireNextImage(uint32_t* imageIndex)
	{
		vkWaitForFences(Device::GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		VkResult result = vkAcquireNextImageKHR(Device::GetDevice(), m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, imageIndex);

		return result;
	}

	/**
	 * @brief Creates objects for explicit synchronization
	*/
	void Swapchain::CreateSyncObjects()
	{
		m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		//m_ImagesInFlight.resize(GetImageCount(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(Device::GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(Device::GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(Device::GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
			{
				VL_CORE_ASSERT(false, "failed to create synchronization objects!");
			}
		}
	}

	VkFramebuffer Swapchain::GetPresentableFrameBuffer(int frameIndex)
	{
		return m_PresentableFramebuffers[frameIndex];
	}

}