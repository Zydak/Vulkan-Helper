#pragma once
#include "entt/entt.h"
#include "pch.h"
#include "../Math/Transform.h"
#include "Vulkan/Window.h"
#include "Utility/Utility.h"

#include "System.h"

namespace Vulture
{
	// forward declaration
	class Entity;
	class AssetManager;

	class Scene
	{
	public:
		void Init(Ref<Window> window);
		void Destroy();

		Scene() = default;
		Scene(Ref<Window> window);
		~Scene();

		Entity CreateEntity();
		void DestroyEntity(Entity& entity);

		void InitScripts();
		void DestroyScripts();
		void UpdateScripts(double deltaTime);

		template<typename T>
		void AddSystem()
		{
			m_Systems.emplace_back(new T());
		}

		void InitSystems();
		void DestroySystems();
		void UpdateSystems(double deltaTime);

		inline entt::registry& GetRegistry() { return *m_Registry; }
		inline Ref<Window> GetWindow() const { return m_Window; }

	private:
		Ref<Window> m_Window;
		Ref<entt::registry> m_Registry;
		std::vector<SystemInterface*> m_Systems;

		bool m_Initialized = false;
	};

}