#pragma once

#include "pch.h"
#include "Vulkan/Device.h"
#include "Vulkan/Image.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PushConstant.h"

namespace Vulture
{
	class Tonemap
	{
	public:

		enum Tonemappers
		{
			Filmic,
			HillAces,
			NarkowiczAces,
			ExposureMapping,
			Uncharted2,
			ReinchardExtended
		};

		struct CreateInfo
		{
			std::vector<Ref<Image>> InputImages;
			std::vector<Ref<Image>> OutputImages;

			Tonemappers Tonemapper = Tonemappers::Filmic;
		};

		struct TonemapInfo
		{
			float Contrast = 1.0f;
			float Saturation = 1.0f;
			float Exposure = 0.5f;
			float Brightness = 0.0f;
			float Vignette = 0.0f;
			float Gamma = 1.0f;
			float Temperature = 0.0f;
			float Tint = 0.0f;
			glm::vec4 ColorFilter = { 1.0f, 1.0f, 1.0f, 1.0f };

			glm::ivec2 AberrationOffsets[3] = { {2.0f, -2.0f}, {-2.0f, 2.0f}, {2.0f, -2.0f} };
			float AberrationVignette = 1.0f;

			float whitePointReinhard = 3.0f;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		void RecompileShader(Tonemappers tonemapper, bool chromaticAberration);

		Tonemap() = default;
		Tonemap(const CreateInfo& info);
		~Tonemap();

		void Run(const TonemapInfo& info, VkCommandBuffer cmd, uint32_t imageIndex = 0);
	private:
		std::string GetTonemapperMacroDefinition(Tonemappers tonemapper);
		std::vector<DescriptorSet> m_Descriptor;
		Pipeline m_Pipeline;
		PushConstant<TonemapInfo> m_Push;

		VkExtent2D m_ImageSize;

		std::vector<Ref<Image>> m_InputImages;
		std::vector<Ref<Image>> m_OutputImages;

		bool m_Initialized = false;
	};
}