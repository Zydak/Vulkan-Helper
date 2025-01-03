#pragma once

#include "pch.h"
#include "Device.h"

#include <shaderc/shaderc.hpp>

#include <slang.h>

#include "wrl/client.h"

namespace VulkanHelper
{
	class Shader
	{
	public:
		struct Define
		{
			std::string Name;
			std::string Value;
		};
		struct CreateInfo
		{
			std::string Filepath;
			VkShaderStageFlagBits Type;

			std::vector<Define> Defines;

			bool CacheToFile = false;
		};

		[[nodiscard]] bool Init(const CreateInfo& info);
		void Destroy();

		Shader() = default;
		Shader(const CreateInfo& info);
		~Shader();

		Shader(const Shader& other) = delete;
		Shader& operator=(const Shader& other) = delete;
		Shader(Shader&& other) noexcept;
		Shader& operator=(Shader&& other) noexcept;

		VkPipelineShaderStageCreateInfo GetStageCreateInfo();

		inline VkShaderModule GetModuleHandle() { return m_ModuleHandle; }
		inline VkShaderStageFlagBits GetType() { return m_Type; }

		inline bool IsInitialized() const { return m_Initialized; }
	private:

		std::string ReadShaderFile(const std::string& filepath);
		void CreateCacheDir();
		std::vector<uint32_t> CompileSource(const std::string& filepath, const std::vector<Define>& defines, bool cacheToFile);
		shaderc_shader_kind VkStageToScStage(VkShaderStageFlagBits stage);

		VkShaderModule m_ModuleHandle = VK_NULL_HANDLE;
		VkShaderStageFlagBits m_Type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		bool m_Initialized = false;

		void Reset();

		inline static Microsoft::WRL::ComPtr<slang::IGlobalSession> s_GlobalSession = nullptr;
	};
}