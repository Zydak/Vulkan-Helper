#pragma once
#include "pch.h"
#include "FontAtlas.h"
#include "Mesh.h"
#include "../Vulkan/Swapchain.h"

namespace VulkanHelper
{
	class Text
	{
	public:
		struct CreateInfo
		{
			std::string Text = "";
			VulkanHelper::FontAtlas* FontAtlas = nullptr;
			glm::vec4 Color = { -1.0f, -1.0f, -1.0f, -1.0f };
			float KerningOffset = 0.0f;
			int MaxLettersCount = 0;
			bool Resizable = false;

			operator bool() const
			{
				return (Color != glm::vec4(-1.0f)) && (FontAtlas != nullptr);
			}
		};

		void Init(const CreateInfo& createInfo);
		void Destroy();

		Text() = default;
		Text(const CreateInfo& createInfo);
		~Text();

		Text(const Text& other) = delete;
		Text& operator=(const Text& other) = delete;
		Text(Text&& other) noexcept;
		Text& operator=(Text&& other) noexcept;

		void ChangeText(const std::string& text, float kerningOffset = 0.0f, VkCommandBuffer cmdBuffer = 0);

		std::string GetTextString() const { return m_Text; }
		inline bool IsResizable() const { return m_Resizable; }
		inline float GetMaxHeight() const { return m_Height; }
		inline float GetMaxWidth() const { return m_Width; }
		inline const Mesh* GetTextMesh() const { return &m_TextMesh; }
		inline Mesh* GetTextMesh() { return &m_TextMesh; }

		inline bool IsInitialized() const { return m_Initialized; }

		glm::vec4 GetTextColor() const { return m_Color; }
		VulkanHelper::FontAtlas* GetFontAtlas() const { return m_FontAtlas; }
	private:
		void GetTextVertices(std::vector<Mesh::Vertex>& vertices, std::vector<uint32_t>& indices);

		float m_Width = 0;
		float m_Height = 0;
		VulkanHelper::FontAtlas* m_FontAtlas;
		float m_KerningOffset = 0.0f;
		std::string m_Text = "";
		Mesh m_TextMesh;
		glm::vec4 m_Color = { 0.0f, 0.0f, 0.0f, 0.0f };
		bool m_Resizable = false;

		bool m_Initialized = false;

		void Reset();
	};
}