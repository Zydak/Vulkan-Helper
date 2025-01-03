#pragma once
#include "pch.h"
#include "../Renderer/Text.h"
#include "../Math/Transform.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Math/Quaternion.h"

#include "Entity.h"
#include "Asset/Asset.h"

#include "Effects/Tonemap.h"
#include "Effects/Bloom.h"

namespace VulkanHelper
{
	class ScriptInterface
	{
	public:
		ScriptInterface() = default;
		virtual ~ScriptInterface() {}
		virtual void OnCreate() = 0;
		virtual void OnDestroy() = 0;
		virtual void OnUpdate(double deltaTime) = 0;

		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

		Entity m_Entity; // Assigned when InitScripts is called on the scene object
	};

	class ScriptComponent
	{
	public:
		ScriptComponent() = default;

		~ScriptComponent();

		ScriptComponent(ScriptComponent&& other) noexcept;

		// TODO delete vector from here
		std::vector<ScriptInterface*> Scripts;
		std::vector<std::string> ScriptClassesNames;

		template<typename T>
		void AddScript()
		{
			Scripts.push_back(new T());

			std::string name = typeid(T).name();
			if (name.find("class ") != std::string::npos)
				name = name.substr(6, name.size() - 6);

			ScriptClassesNames.push_back(name);
		}

		void InitializeScripts();
		void UpdateScripts(double deltaTime);
		void DestroyScripts();

		inline std::vector<ScriptInterface*> GetScripts() const { return Scripts; }

		/**
		 * @brief Retrieves script at specified index.
		 *
		 * @return T* if specified T exists at scriptIndex, otherwise nullptr.
		 */
		template<typename T>
		T* GetScript(uint32_t scriptIndex)
		{
			T* returnVal;
			try
			{
				returnVal = dynamic_cast<T*>(Scripts.at(scriptIndex));
			}
			catch (const std::exception&)
			{
				returnVal = nullptr;
			}

			return returnVal;
		}

		inline uint32_t GetScriptCount() const { return (uint32_t)Scripts.size(); }

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);
	};

	class MeshComponent
	{
	public:
		MeshComponent() = default;
		~MeshComponent() = default;
		MeshComponent(MeshComponent&& other) noexcept { AssetHandle = std::move(other.AssetHandle); };
		MeshComponent(const MeshComponent& other) { AssetHandle = other.AssetHandle; };
		MeshComponent& operator=(const MeshComponent& other) { AssetHandle = other.AssetHandle; return *this; };
		MeshComponent& operator=(MeshComponent&& other) noexcept { AssetHandle = std::move(other.AssetHandle); return *this; };

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);

		VulkanHelper::AssetHandle AssetHandle;
	};

	class MaterialComponent
	{
	public:
		MaterialComponent() = default;
		~MaterialComponent() = default;
		MaterialComponent(MaterialComponent&& other) noexcept { AssetHandle = std::move(other.AssetHandle); };
		MaterialComponent(const MaterialComponent& other) { AssetHandle = other.AssetHandle; };
		MaterialComponent& operator=(const MaterialComponent& other) { AssetHandle = other.AssetHandle; return *this; };
		MaterialComponent& operator=(MaterialComponent&& other) noexcept { AssetHandle = std::move(other.AssetHandle); return *this; };

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);

		VulkanHelper::AssetHandle AssetHandle;
	};

	class TransformComponent
	{
	public:
		TransformComponent() = default;
		~TransformComponent() = default;
		TransformComponent(TransformComponent&& other) noexcept { Transform = std::move(other.Transform); };
		TransformComponent(const TransformComponent& other) { Transform = other.Transform; };
		TransformComponent& operator=(const TransformComponent& other) { Transform = other.Transform; return *this; };
		TransformComponent& operator=(TransformComponent&& other) noexcept { Transform = std::move(other.Transform); return *this; };

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);

		VulkanHelper::Transform Transform;
	};

	class NameComponent
	{
	public:
		NameComponent() = default;
		~NameComponent() = default;
		NameComponent(NameComponent&& other) noexcept { Name = std::move(other.Name); };
		NameComponent(const NameComponent& other) { Name = other.Name; };
		NameComponent& operator=(const NameComponent& other) { Name = other.Name; return *this; };
		NameComponent& operator=(NameComponent&& other) noexcept { Name = std::move(other.Name); return *this; };

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);

		std::string Name;
	};

	class TonemapperSettingsComponent
	{
	public:
		TonemapperSettingsComponent() = default;
		~TonemapperSettingsComponent() = default;
		TonemapperSettingsComponent(TonemapperSettingsComponent&& other) noexcept { Settings = std::move(other.Settings); };
		TonemapperSettingsComponent(const TonemapperSettingsComponent& other) { Settings = other.Settings; };
		TonemapperSettingsComponent& operator=(const TonemapperSettingsComponent& other) { Settings = other.Settings; return *this; };
		TonemapperSettingsComponent& operator=(TonemapperSettingsComponent&& other) noexcept { Settings = std::move(other.Settings); return *this; };

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);

		VulkanHelper::Tonemap::TonemapInfo Settings{};
	};

	class BloomSettingsComponent
	{
	public:
		BloomSettingsComponent() = default;
		~BloomSettingsComponent() = default;
		BloomSettingsComponent(BloomSettingsComponent&& other) noexcept { Settings = std::move(other.Settings); };
		BloomSettingsComponent(const BloomSettingsComponent& other) { Settings = other.Settings; };
		BloomSettingsComponent& operator=(const BloomSettingsComponent& other) { Settings = other.Settings; return *this; };
		BloomSettingsComponent& operator=(BloomSettingsComponent&& other) noexcept { Settings = std::move(other.Settings); return *this; };

		std::vector<char> Serialize();
		void Deserialize(const std::vector<char>& bytes);

		VulkanHelper::Bloom::BloomInfo Settings{};
	};
}