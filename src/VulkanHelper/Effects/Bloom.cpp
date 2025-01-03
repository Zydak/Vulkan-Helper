#include "pch.h"
#include "Bloom.h"

#include "Renderer/Renderer.h"
#include "Vulkan/DeleteQueue.h"

#include "Core/Window.h"

namespace VulkanHelper
{

	void Bloom::Init(const CreateInfo& info)
	{
		if (m_Initialized)
			Destroy();

		m_Context = info.Context;

		m_InputImage = info.InputImage;
		m_OutputImage = info.OutputImage;

		//-----------------------------------------------
		// Pipelines
		//-----------------------------------------------

		m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });
		// Bloom Separate Bright Values
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../VulkanHelper/src/VulkanHelper/Shaders/SeparateBrightValues.glsl" , VK_SHADER_STAGE_COMPUTE_BIT, {}, true });
			info.Shader = &shader;
			info.PushConstants = m_Push.GetRangePtr();

			info.DescriptorSetLayouts = {
				imageLayout.GetDescriptorSetLayoutHandle()
			};

			info.debugName = "Bloom Separate Bright Values Pipeline";

			m_SeparateBrightValuesPipeline.Init(info);
		}

		// Bloom Accumulate
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../VulkanHelper/src/VulkanHelper/Shaders/BloomUpSample.glsl" , VK_SHADER_STAGE_COMPUTE_BIT, {}, true });
			info.Shader = &shader;
			info.PushConstants = m_Push.GetRangePtr();

			info.DescriptorSetLayouts = {
					imageLayout.GetDescriptorSetLayoutHandle()
			};

			info.debugName = "Bloom Accumulate Pipeline";

			m_AccumulatePipeline.Init(info);
		}

		// Bloom Down Sample
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../VulkanHelper/src/VulkanHelper/Shaders/BloomDownSample.glsl" , VK_SHADER_STAGE_COMPUTE_BIT, {}, true });
			info.Shader = &shader;
			info.PushConstants = m_Push.GetRangePtr();

			info.DescriptorSetLayouts = {
				imageLayout.GetDescriptorSetLayoutHandle()
			};

			info.debugName = "Bloom Down Sample Pipeline";

			m_DownSamplePipeline.Init(info);
		}

		CreateBloomMips();

		m_Initialized = true;
	}

	void Bloom::Destroy()
	{
		if (!m_Initialized)
			return;

		m_SeparateBrightValuesSet.Destroy();
		m_AccumulatePipeline.Destroy();
		m_SeparateBrightValuesPipeline.Destroy();
		m_DownSamplePipeline.Destroy();

		Reset();
	}

	Bloom::Bloom(const CreateInfo& info)
	{
		Init(info);
	}

	Bloom::Bloom(Bloom&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Push = std::move(other.m_Push);
		m_ImageSize = std::move(other.m_ImageSize);
		m_SeparateBrightValuesSet = std::move(other.m_SeparateBrightValuesSet);
		m_DownSampleSet = std::move(other.m_DownSampleSet);
		m_AccumulateSet = std::move(other.m_AccumulateSet);
		m_BloomImages = std::move(other.m_BloomImages);
		m_SeparateBrightValuesPipeline = std::move(other.m_SeparateBrightValuesPipeline);
		m_DownSamplePipeline = std::move(other.m_DownSamplePipeline);
		m_AccumulatePipeline = std::move(other.m_AccumulatePipeline);
		m_InputImage = std::move(other.m_InputImage);
		m_OutputImage = std::move(other.m_OutputImage);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	Bloom& Bloom::operator=(Bloom&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Push = std::move(other.m_Push);
		m_ImageSize = std::move(other.m_ImageSize);
		m_SeparateBrightValuesSet = std::move(other.m_SeparateBrightValuesSet);
		m_DownSampleSet = std::move(other.m_DownSampleSet);
		m_AccumulateSet = std::move(other.m_AccumulateSet);
		m_BloomImages = std::move(other.m_BloomImages);
		m_SeparateBrightValuesPipeline = std::move(other.m_SeparateBrightValuesPipeline);
		m_DownSamplePipeline = std::move(other.m_DownSamplePipeline);
		m_AccumulatePipeline = std::move(other.m_AccumulatePipeline);
		m_InputImage = std::move(other.m_InputImage);
		m_OutputImage = std::move(other.m_OutputImage);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	Bloom::~Bloom()
	{
		Destroy();
	}

	void Bloom::Run(const BloomInfo& _bloomInfo, VkCommandBuffer cmd)
	{
		BloomInfo bloomInfo = _bloomInfo;

		if (bloomInfo.MipCount != m_CurrentMipCount)
		{
			RecreateDescriptors(bloomInfo.MipCount);
		}

		if (m_InputImage->GetImage() != m_OutputImage->GetImage())
		{
			VkImageLayout prevInputLayout = m_InputImage->GetLayout();
			m_InputImage->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd);
			m_OutputImage->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd);
			VkImageBlit region{};
			region.dstOffsets[0] = { 0, 0, 0 };
			region.dstOffsets[1] = { (int)m_OutputImage->GetImageSize().width, (int)m_OutputImage->GetImageSize().height, 1 };
			region.srcOffsets[0] = { 0, 0, 0 };
			region.srcOffsets[1] = { (int)m_InputImage->GetImageSize().width, (int)m_InputImage->GetImageSize().height, 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			vkCmdBlitImage(cmd, m_InputImage->GetImage(), m_InputImage->GetLayout(), m_OutputImage->GetImage(), m_OutputImage->GetLayout(), 1, &region, VK_FILTER_LINEAR);
			m_OutputImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
			m_InputImage->TransitionImageLayout(prevInputLayout, cmd);
		}

		if (bloomInfo.MipCount <= 0 || bloomInfo.MipCount > 10)
		{
			VK_CORE_WARN("Incorrect mips count! {}. Min = 1 & Max = 10", bloomInfo.MipCount);
		}
		bloomInfo.MipCount = glm::clamp(bloomInfo.MipCount, (uint32_t)0, (uint32_t)10);

		auto* data = m_Push.GetDataPtr();
		data->MipCount = bloomInfo.MipCount;
		data->Strength = bloomInfo.Strength;
		data->Threshold = bloomInfo.Threshold;

		m_BloomImages[0].TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);

		m_SeparateBrightValuesPipeline.Bind(cmd);
		m_SeparateBrightValuesSet.Bind(0, m_SeparateBrightValuesPipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

		m_Push.Push(m_SeparateBrightValuesPipeline.GetPipelineLayout(), cmd);

		vkCmdDispatch(cmd, m_BloomImages[0].GetImageSize().width / 8 + 1, m_BloomImages[0].GetImageSize().height / 8 + 1, 1);

		m_BloomImages[0].TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);

		for (int i = 1; i < (int)bloomInfo.MipCount + 1; i++)
		{
			m_BloomImages[i].TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);
		}

		m_DownSamplePipeline.Bind(cmd);
		m_Push.Push(m_AccumulatePipeline.GetPipelineLayout(), cmd);
		for (int i = 1; i < (int)bloomInfo.MipCount + 1; i++)
		{
			m_DownSampleSet[i - 1].Bind(0, m_DownSamplePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

			vkCmdDispatch(cmd, m_BloomImages[i].GetImageSize().width / 8 + 1, m_BloomImages[i].GetImageSize().height / 8 + 1, 1);

			m_BloomImages[i].TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
		}

		m_AccumulatePipeline.Bind(cmd);
		m_Push.Push(m_AccumulatePipeline.GetPipelineLayout(), cmd);

		int i;
		int idx;
		for (i = 0; i < (int)bloomInfo.MipCount; i++)
		{
			idx = bloomInfo.MipCount - i;

			if (idx != bloomInfo.MipCount)
			{
				m_BloomImages[idx].TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
			}

			m_BloomImages[idx - 1].TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);

			m_AccumulateSet[i].Bind(0, m_AccumulatePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

			vkCmdDispatch(cmd, m_BloomImages[idx - 1].GetImageSize().width / 8 + 1, m_BloomImages[idx - 1].GetImageSize().height / 8 + 1, 1);
		}

		m_OutputImage->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);

		m_AccumulateSet[i].Bind(0, m_AccumulatePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

		vkCmdDispatch(cmd, m_InputImage->GetImageSize().width / 8 + 1, m_InputImage->GetImageSize().height / 8 + 1, 1);
	}

	void Bloom::UpdateDescriptors(const CreateInfo& info)
	{
		m_InputImage = info.InputImage;
		m_OutputImage = info.OutputImage;

		VkDescriptorImageInfo imageInfo = { 
			m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), // input image is copied to output at the start of bloom pass
			m_OutputImage->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		m_SeparateBrightValuesSet.UpdateImageSampler(
			0,
			imageInfo
		);

		imageInfo = { 
			m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(),
			m_OutputImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL 
		};

		// TODO: detect error when accidentally calling AddImageSampelr instead of Update, could be useful
		m_AccumulateSet[m_CurrentMipCount].UpdateImageSampler(
			1,
			imageInfo
		);
	}

	void Bloom::RecreateDescriptors(uint32_t mipsCount)
	{
		m_CurrentMipCount = mipsCount;

		if (mipsCount <= 0 || mipsCount > 10)
		{
			VK_CORE_WARN("Incorrect mips count! {}. Min = 1 & Max = 10", mipsCount);
		}
		mipsCount = glm::clamp(mipsCount, (uint32_t)0, (uint32_t)10);

		//-----------------------------------------------
		// Descriptor sets
		//-----------------------------------------------

		// Bloom Separate Bright Values
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_SeparateBrightValuesSet.Init(&m_Context.Window->GetRenderer()->GetDescriptorPool(), { bin, bin1 });
			m_SeparateBrightValuesSet.AddImageSampler(
				0,
				{ m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), // input image is copied to output at the start of bloom pass
				m_OutputImage->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			);
			m_SeparateBrightValuesSet.AddImageSampler(
				1,
				{ m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(),
				m_BloomImages[0].GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL }
			);
			m_SeparateBrightValuesSet.Build();
		}

		// Bloom Accumulate
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_AccumulateSet.clear();
			m_AccumulateSet.resize(mipsCount + 1);
			int j;
			int descIdx;
			for (j = 0; j < m_AccumulateSet.size() - 1; j++)
			{
				descIdx = (mipsCount)-j;
				m_AccumulateSet[j].Init(&m_Context.Window->GetRenderer()->GetDescriptorPool(), { bin, bin1 });

				m_AccumulateSet[j].AddImageSampler(0, { m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), m_BloomImages[descIdx].GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				m_AccumulateSet[j].AddImageSampler(1, { m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), m_BloomImages[descIdx - 1].GetImageView(), VK_IMAGE_LAYOUT_GENERAL });
				m_AccumulateSet[j].Build();
			}
			m_AccumulateSet[j].Init(&m_Context.Window->GetRenderer()->GetDescriptorPool(), { bin, bin1 });

			m_AccumulateSet[j].AddImageSampler(0, { m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), m_BloomImages[descIdx].GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
			m_AccumulateSet[j].AddImageSampler(1, { m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), m_OutputImage->GetImageView(), VK_IMAGE_LAYOUT_GENERAL });
			m_AccumulateSet[j].Build();
		}

		// Bloom Down Sample
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_DownSampleSet.clear();
			m_DownSampleSet.resize(mipsCount);
			for (int j = 0; j < m_DownSampleSet.size(); j++)
			{
				m_DownSampleSet[j].Init(&m_Context.Window->GetRenderer()->GetDescriptorPool(), { bin, bin1 });
				m_DownSampleSet[j].AddImageSampler(0, { m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), m_BloomImages[j].GetImageView() ,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
				);
				m_DownSampleSet[j].AddImageSampler(1, { m_Context.Window->GetRenderer()->GetLinearSampler().GetSamplerHandle(), m_BloomImages[j + 1].GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL }
				);
				m_DownSampleSet[j].Build();
			}
		}
	}

	void Bloom::CreateBloomMips()
	{
		Image::CreateInfo info{};
		info.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		info.Width = m_InputImage->GetImageSize().width;
		info.Height = m_InputImage->GetImageSize().height;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Properties = m_InputImage->GetMemoryProperties();
		info.SamplerInfo = SamplerInfo{};

		m_BloomImages.resize(10 + 1); // TODO: Don't create 11 images, just create mipCount images
		// First Image is just a copy with separated bright values
		info.DebugName = "Bright Values Image";
		m_BloomImages[0].Init(info);

		for (int j = 0; j < 10; j++)
		{
			std::string s = "Bloom Mip Image " + std::to_string(j);
			const char* g = s.c_str();
			info.DebugName = g;
			info.Width = glm::max(1, (int)info.Width / 2);
			info.Height = glm::max(1, (int)info.Height / 2);
			m_BloomImages[j + 1].Init(info);
		}
	}

	void Bloom::Reset()
	{
		m_ImageSize = { 0, 0 };

		m_DownSampleSet.clear();
		m_AccumulateSet.clear();
		m_BloomImages.clear();
		m_InputImage = nullptr;
		m_OutputImage = nullptr;
		m_Initialized = false;
		m_CurrentMipCount = 0;
	}

}