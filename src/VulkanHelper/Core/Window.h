#pragma once
#include "pch.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

#include "Input.h"

#include "Renderer/Renderer.h"

namespace VulkanHelper
{

	struct Monitor
	{
		std::string Name;
		GLFWmonitor* MonitorPtr;

		Monitor()
			: Name(""), MonitorPtr(nullptr)
		{

		};

		~Monitor() {};

		explicit Monitor(Monitor&& other) noexcept 
		{
			Name = std::move(other.Name);
			MonitorPtr = std::move(other.MonitorPtr);
		};

		explicit Monitor(const Monitor& other)
		{
			Name = other.Name;
			MonitorPtr = other.MonitorPtr;
		};

		Monitor& operator=(const Monitor& other)
		{
			Name = other.Name;
			MonitorPtr = other.MonitorPtr;
			return *this;
		};

		Monitor& operator=(Monitor&& other) noexcept
		{
			Name = std::move(other.Name);
			MonitorPtr = std::move(other.MonitorPtr);
			return *this;
		};
	};

	class Window;

	struct UserPointer
	{
		Window* Window;
		Input* Input;
	};

	class Window 
	{
	public:

		struct CreateInfo
		{
			int Width = 0;
			int Height = 0;
			std::string Name = "";
			std::string Icon = "";

			bool Resizable = true;

			uint32_t FramesInFlight = 2;
		};

		void Init(CreateInfo& createInfo);
		void InitRenderer();
		void Destroy();

		Window() = default;
		Window(CreateInfo& createInfo);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		Window(Window&& other) = delete;
		Window& operator=(Window&& other) = delete;

		inline Input* GetInput() { return &m_Input; }

		void PollEvents();

		inline VkSurfaceKHR GetSurface() const { return m_Surface; }

		inline bool WasWindowResized() const { return m_Resized; }
		inline bool ShouldClose() const { return glfwWindowShouldClose(m_Window); }

		inline void ResetWindowResizedFlag() { m_Resized = false; }
		inline void Close() { glfwSetWindowShouldClose(m_Window, GLFW_TRUE); }

		inline VkExtent2D GetExtent() const { return { (uint32_t)m_Width, (uint32_t)m_Height }; }
		inline float GetAspectRatio() const { return { (float)m_Width / (float)m_Height }; }
		inline const std::vector<Monitor>& GetMonitors() const { return m_Monitors; }
		inline int GetMonitorsCount() const { return m_MonitorsCount; }

		void Resize(const glm::vec2& extent);
		void SetFullscreen(bool val, GLFWmonitor* monitor);

		inline GLFWwindow* GetGLFWwindow() const { return m_Window; }

		inline bool IsInitialized() const { return m_Initialized; }

		inline Renderer* GetRenderer() { return &m_Renderer; }

	private:
		static void ResizeCallback(GLFWwindow* window, int width, int height);
		void CreateWindowSurface();

		int m_Width = 0;
		int m_Height = 0;
		std::string m_Name = "";
		bool m_Resized = false;

		uint32_t m_FramesInFlight = 0;
		Renderer m_Renderer;
		VkSurfaceKHR m_Surface;

		GLFWwindow* m_Window = nullptr;
		std::vector<Monitor> m_Monitors;
		int m_MonitorsCount = 0;
		Input m_Input;
		UserPointer m_UserPointer;

		bool m_Initialized = false;

		void Reset();
	};

}