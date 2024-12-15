#include "pch.h"

#include "Shader.h"
#include "Utility/Utility.h"

#include <shaderc/libshaderc_util/file_finder.h>
#include <shaderc/glslc/file_includer.h>

#include "dxcapi.h"

namespace VulkanHelper
{
	bool Shader::Init(const CreateInfo& info)
	{
		if (m_Initialized)
			Destroy();

		if (s_GlobalSession == nullptr)
		{
			slang::createGlobalSession(&s_GlobalSession);
		}

		if (!std::filesystem::exists(info.Filepath))
		{
			VK_CORE_ERROR("File does not exist: {}", info.Filepath);
			return false;
		}

		m_Type = info.Type;

		std::vector<uint32_t> data = CompileSource(info.Filepath, info.Defines);
		if (data.empty())
			return false;

		VkShaderModuleCreateInfo createInfo{};

		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = data.size() * 4;
		createInfo.pCode = data.data();

		VK_CORE_RETURN_ASSERT(vkCreateShaderModule(Device::GetDevice(), &createInfo, nullptr, &m_ModuleHandle),
			VK_SUCCESS,
			"failed to create shader Module"
		);

		Device::SetObjectName(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)m_ModuleHandle, info.Filepath.c_str());

		m_Initialized = true;
		
		return true;
	}

	void Shader::Destroy()
	{
		if (!m_Initialized)
			return;

		vkDestroyShaderModule(Device::GetDevice(), m_ModuleHandle, nullptr);
		
		Reset();
	}

	Shader::Shader(const CreateInfo& info)
	{
		bool result = Init(info);
	}

	Shader::Shader(Shader&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_ModuleHandle	= std::move(other.m_ModuleHandle);
		m_Type			= std::move(other.m_Type);
		m_Initialized	= std::move(other.m_Initialized);

		other.Reset();
	}

	Shader& Shader::operator=(Shader&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_ModuleHandle	= std::move(other.m_ModuleHandle);
		m_Type			= std::move(other.m_Type);
		m_Initialized	= std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	Shader::~Shader()
	{
		Destroy();
	}

	static std::string GetLastPartAfterLastSlash(const std::string& str)
	{
		size_t lastSlashPos = str.find_last_of('/');
		if (lastSlashPos != std::string::npos && lastSlashPos != str.length() - 1) 
		{
			return str.substr(lastSlashPos + 1); // Extract the substring after the last slash
		}
		return str; // Return the original string if no slash is found or if the last character is a slash
	}

	std::vector<uint32_t> Shader::CompileSource(const std::string& filepath, const std::vector<Define>& defines)
	{
		if (filepath.find(".hlsl") != std::string::npos)
		{
			std::vector<uint32_t> data;
			std::wstring wfilepath(filepath.begin(), filepath.end());

			HRESULT hres;

			std::string source = File::ReadFromFile(filepath);

			Microsoft::WRL::ComPtr<IDxcUtils> utils;
			Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
			Microsoft::WRL::ComPtr<IDxcIncludeHandler> includer;

			hres = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
			hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()));
			hres = utils->CreateDefaultIncludeHandler(&includer);
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
			hres = utils->LoadFile(wfilepath.data(), nullptr, &sourceBlob);

			DxcBuffer sourceBuffer;
			sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
			sourceBuffer.Size = sourceBlob->GetBufferSize();
			sourceBuffer.Encoding = DXC_CP_ACP;

			std::vector<LPCWSTR> arguments = {
				// (Optional) name of the shader file to be displayed e.g. in an error message
				wfilepath.c_str(),
				// Shader main entry point
				L"-E", L"main",
				// Shader target profile
				L"-T", L"lib_6_3",
				// Compile to SPIRV
				L"-spirv",

				L"-fspv-target-env=vulkan1.1spirv1.4"
			};

			Microsoft::WRL::ComPtr<IDxcResult> compiledShaderBuffer{ nullptr };
			hres = compiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), includer.Get(), IID_PPV_ARGS(&compiledShaderBuffer));

			//Microsoft::WRL::ComPtr<IDxcBlobEncoding> errorBlob;
			//hres = compiledShaderBuffer->GetErrorBuffer(&errorBlob);
			//std::cerr << "Shader compilation failed :\n\n" << (const char*)errorBlob->GetBufferPointer();

			Microsoft::WRL::ComPtr<IDxcBlob> code;
			hres = compiledShaderBuffer->GetResult(&code);

			data.resize(code->GetBufferSize() / 4);
			memcpy(data.data(), code->GetBufferPointer(), code->GetBufferSize());

			return data;
		}

		if (filepath.find(".slang") != std::string::npos)
		{
			std::vector<uint32_t> data;

			slang::TargetDesc targetDesc{};
			targetDesc.format = SLANG_SPIRV;
			targetDesc.profile = s_GlobalSession->findProfile("spirv_1_4");

			const char* searchPaths[] = { "src/shaders/" };

			std::vector<slang::CompilerOptionEntry> compilerOptions(2);
			compilerOptions[0].name = slang::CompilerOptionName::Optimization;
			compilerOptions[0].value = slang::CompilerOptionValue{ slang::CompilerOptionValueKind::Int, SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_MAXIMAL };

			slang::SessionDesc sessionDesc{};

			std::vector<slang::PreprocessorMacroDesc> macros(defines.size());

			for (int i = 0; i < defines.size(); i++)
			{
				macros.emplace_back(defines[i].Name.c_str(), defines[i].Value.c_str());
			}

			sessionDesc.preprocessorMacros = macros.data();
			sessionDesc.preprocessorMacroCount = macros.size();

			sessionDesc.targets = &targetDesc;
			sessionDesc.targetCount = 1;

			sessionDesc.searchPaths = searchPaths;
			sessionDesc.searchPathCount = 1;

			sessionDesc.compilerOptionEntries = compilerOptions.data();
			sessionDesc.compilerOptionEntryCount = (uint32_t)compilerOptions.size();

			Microsoft::WRL::ComPtr<slang::ISession> session;
			s_GlobalSession->createSession(sessionDesc, &session);

			Microsoft::WRL::ComPtr<slang::IBlob> diagnostics;
			Microsoft::WRL::ComPtr<slang::IModule> module = session->loadModule(filepath.c_str(), &diagnostics);

			if (diagnostics)
			{
				std::string message((const char*)diagnostics->getBufferPointer());
				if (message.find(": error") != std::string::npos)
					VK_CORE_ERROR(message);
				else
					VK_CORE_WARN(message);
			}

			if (module == nullptr)
				return data;

			SlangResult res;

			Microsoft::WRL::ComPtr<slang::IEntryPoint> entryPoint;
			res = module->findEntryPointByName("main", &entryPoint);

			if (res != 0)
				return data;

			slang::IComponentType* components[] = { module.Get(), entryPoint.Get() };
			Microsoft::WRL::ComPtr<slang::IComponentType> program;
			res = session->createCompositeComponentType(components, 2, &program);

			if (res != 0)
				return data;

			Microsoft::WRL::ComPtr<slang::IComponentType> linkedProgram;
			Microsoft::WRL::ComPtr<ISlangBlob> diagnosticBlob;
			res = program->link(&linkedProgram, &diagnosticBlob);

			if (diagnosticBlob)
			{
				std::string message((const char*)diagnosticBlob->getBufferPointer());
				if (message.find(": error") != std::string::npos)
					VK_CORE_ERROR(message);
				else
					VK_CORE_WARN(message);
			}

			if (res != 0)
				return data;

			int entryPointIndex = 0; // only one entry point
			int targetIndex = 0; // only one target
			Microsoft::WRL::ComPtr<slang::IBlob> kernelBlob;
			Microsoft::WRL::ComPtr<slang::IBlob> diagnostics1;

			res = linkedProgram->getEntryPointCode(entryPointIndex, targetIndex, &kernelBlob, &diagnostics1);

			if (diagnostics1)
			{
				std::string message((const char*)diagnostics1->getBufferPointer());
				if (message.find(": error") != std::string::npos)
					VK_CORE_ERROR(message);
				else
					VK_CORE_WARN(message);
			}

			if (res != 0)
				return data;

			int codeSize = (int)kernelBlob->getBufferSize();

			data.resize(codeSize / 4);

			memcpy(data.data(), kernelBlob->getBufferPointer(), codeSize);

			return data;
		}

		CreateCacheDir();
		std::string sourceToCache = ReadShaderFile(filepath);

		for (int i = 0; i < defines.size(); i++)
		{
			sourceToCache += (defines[i].Name + defines[i].Value);
		}

		std::string shaderName = GetLastPartAfterLastSlash(filepath);
		if (std::filesystem::exists("CachedShaders/" + shaderName + ".cache"))
		{
			std::string cashedSrc = File::ReadFromFile("CachedShaders/" + shaderName + ".cache");
			if (sourceToCache == cashedSrc)
			{
				std::vector<uint32_t> data;
				File::ReadFromFileVec(data, "CachedShaders/" + shaderName + ".spv");
				return data;
			}
			else
			{
				VK_CORE_INFO("Compiling shader {}", filepath);

				shaderc::Compiler compiler;
				shaderc::CompileOptions options;
				options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
				options.SetOptimizationLevel(shaderc_optimization_level_performance);
				options.SetSourceLanguage(shaderc_source_language::shaderc_source_language_glsl);
				for (int i = 0; i < defines.size(); i++)
				{
					options.AddMacroDefinition(defines[i].Name, defines[i].Value);
				}
				shaderc_util::FileFinder fileFinder;
				options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));

				std::string source = File::ReadFromFile(filepath);
				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, VkStageToScStage(m_Type), filepath.c_str(), options);

				if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					VK_CORE_ERROR("Failed to compile shader! {}", module.GetErrorMessage());
					return std::vector<uint32_t>();
				}

				std::vector<uint32_t> data(module.cbegin(), module.cend());

				File::WriteToFile(sourceToCache.c_str(), (uint32_t)sourceToCache.size(), ("CachedShaders/" + shaderName + ".cache"));
				File::WriteToFile(data.data(), sizeof(uint32_t) * (uint32_t)data.size(), ("CachedShaders/" + shaderName + ".spv"));

				return data;
			}
		}
		else
		{
			VK_CORE_INFO("Compiling shader {}", filepath);

			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
			for (int i = 0; i < defines.size(); i++)
			{
				options.AddMacroDefinition(defines[i].Name, defines[i].Value);
			}
			shaderc_util::FileFinder fileFinder;
			options.SetIncluder(std::make_unique<glslc::FileIncluder>(&fileFinder));

			std::string source = File::ReadFromFile(filepath);
			shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, VkStageToScStage(m_Type), filepath.c_str(), options);

			if (module.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				VK_CORE_ERROR("Failed to compile shader! {}", module.GetErrorMessage());
				return std::vector<uint32_t>();
			}

			std::vector<uint32_t> data(module.cbegin(), module.cend());

			File::WriteToFile(sourceToCache.c_str(), (uint32_t)sourceToCache.size(), ("CachedShaders/" + shaderName + ".cache"));
			File::WriteToFile(data.data(), sizeof(uint32_t) * (uint32_t)data.size(), ("CachedShaders/" + shaderName + ".spv"));

			return data;
		}
	}

	void Shader::CreateCacheDir()
	{
		if (!std::filesystem::exists("CachedShaders"))
		{
			std::filesystem::create_directory("CachedShaders");
		}
	}

	VkPipelineShaderStageCreateInfo Shader::GetStageCreateInfo()
	{
		VkPipelineShaderStageCreateInfo stage;
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = m_Type;
		stage.module = m_ModuleHandle;
		stage.pName = "main";
		stage.flags = 0;
		stage.pNext = nullptr;
		stage.pSpecializationInfo = nullptr;

		return stage;
	}

	std::string Shader::ReadShaderFile(const std::string& filepath)
	{
		std::string source = File::ReadFromFile(filepath);

		while (true)
		{
			size_t includePos = source.find("#include");
			if (includePos != std::string::npos)
			{
				// Find the position of the first double quote after "#include"
				size_t startQuotePos = source.find('\"', includePos);
				if (startQuotePos != std::string::npos)
				{
					// Find the position of the second double quote after "#include"
					size_t endQuotePos = source.find('\"', startQuotePos + 1);
					if (endQuotePos != std::string::npos)
					{
						// Extract the substring between the double quotes
						std::string includedFile = source.substr(startQuotePos + 1, endQuotePos - startQuotePos - 1);
						
						std::string includedFilePath = filepath.substr(0, filepath.find_last_of('/')) + "/" + includedFile;

						size_t size = (endQuotePos + 1) - includePos;
						source.erase(includePos, size);

						std::string includedSource = File::ReadFromFile(includedFilePath);

						source.insert(includePos, includedSource);
					}
					else
						break;
				}
				else
					break;
			}
			else
				break;
		}

		return source;
	}

	shaderc_shader_kind Shader::VkStageToScStage(VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return shaderc_vertex_shader;
			break;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return shaderc_tess_control_shader;
			break;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return shaderc_tess_evaluation_shader;
			break;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return shaderc_geometry_shader;
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return shaderc_fragment_shader;
			break;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return shaderc_compute_shader;
			break;
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			return shaderc_raygen_shader;
			break;
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			return shaderc_anyhit_shader;
			break;
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			return shaderc_closesthit_shader;
			break;
		case VK_SHADER_STAGE_MISS_BIT_KHR:
			return shaderc_miss_shader;
			break;
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			return shaderc_intersection_shader;
			break;
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return shaderc_callable_shader;
			break;
		case VK_SHADER_STAGE_TASK_BIT_EXT:
			return shaderc_task_shader;
			break;
		case VK_SHADER_STAGE_MESH_BIT_EXT:
			return shaderc_mesh_shader;
			break;
		default:
			VK_CORE_ASSERT(false, "Incorrect shader type");
			break;
		}

		// just to get rid of the warning
		return shaderc_mesh_shader;
	}

	void Shader::Reset()
	{
		m_ModuleHandle = VK_NULL_HANDLE;
		m_Type = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		m_Initialized = false;
	}

}