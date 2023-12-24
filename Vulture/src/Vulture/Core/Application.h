#pragma once
#include "pch.h"

#include "Vulture/Utility/Utility.h"
#include "Vulkan/Device.h"

namespace Vulture
{
	struct ApplicationInfo
	{
		std::string WorkingDirectory = "";
		std::string Name = "";
		std::string Icon = "";
		bool RayTracingSupport = false;
	};

	class Application
	{
	public:
		Application(const ApplicationInfo& appInfo, float windowWidth = 600, float windowHeight = 600);
		virtual ~Application();

		virtual void OnUpdate(double delta) = 0;

		void Run();
	protected:

		ApplicationInfo m_ApplicationInfo;
		Ref<Window> m_Window;
	};

	// defined by client
	Application* CreateApplication();

}