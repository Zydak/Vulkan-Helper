#pragma once
#include "pch.h"
#include "glfw/glfw3.h"

#include "glm/glm.hpp"

#define VK_KEY_SPACE              32
#define VK_KEY_APOSTROPHE         39  /* ' */
#define VK_KEY_COMMA              44  /* , */
#define VK_KEY_MINUS              45  /* - */
#define VK_KEY_PERIOD             46  /* . */
#define VK_KEY_SLASH              47  /* / */
#define VK_KEY_0                  48
#define VK_KEY_1                  49
#define VK_KEY_2                  50
#define VK_KEY_3                  51
#define VK_KEY_4                  52
#define VK_KEY_5                  53
#define VK_KEY_6                  54
#define VK_KEY_7                  55
#define VK_KEY_8                  56
#define VK_KEY_9                  57
#define VK_KEY_SEMICOLON          59  /* ; */
#define VK_KEY_EQUAL              61  /* = */
#define VK_KEY_A                  65
#define VK_KEY_B                  66
#define VK_KEY_C                  67
#define VK_KEY_D                  68
#define VK_KEY_E                  69
#define VK_KEY_F                  70
#define VK_KEY_G                  71
#define VK_KEY_H                  72
#define VK_KEY_I                  73
#define VK_KEY_J                  74
#define VK_KEY_K                  75
#define VK_KEY_L                  76
#define VK_KEY_M                  77
#define VK_KEY_N                  78
#define VK_KEY_O                  79
#define VK_KEY_P                  80
#define VK_KEY_Q                  81
#define VK_KEY_R                  82
#define VK_KEY_S                  83
#define VK_KEY_T                  84
#define VK_KEY_U                  85
#define VK_KEY_V                  86
#define VK_KEY_W                  87
#define VK_KEY_X                  88
#define VK_KEY_Y                  89
#define VK_KEY_Z                  90
#define VK_KEY_LEFT_BRACKET       91  /* [ */
#define VK_KEY_BACKSLASH          92  /* \ */
#define VK_KEY_RIGHT_BRACKET      93  /* ] */
#define VK_KEY_GRAVE_ACCENT       96  /* ` */
#define VK_KEY_WORLD_1            161 /* non-US #1 */
#define VK_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define VK_KEY_ESCAPE             256
#define VK_KEY_ENTER              257
#define VK_KEY_TAB                258
#define VK_KEY_BACKSPACE          259
#define VK_KEY_INSERT             260
#define VK_KEY_DELETE             261
#define VK_KEY_RIGHT              262
#define VK_KEY_LEFT               263
#define VK_KEY_DOWN               264
#define VK_KEY_UP                 265
#define VK_KEY_PAGE_UP            266
#define VK_KEY_PAGE_DOWN          267
#define VK_KEY_HOME               268
#define VK_KEY_END                269
#define VK_KEY_CAPS_LOCK          280
#define VK_KEY_SCROLL_LOCK        281
#define VK_KEY_NUM_LOCK           282
#define VK_KEY_PRINT_SCREEN       283
#define VK_KEY_PAUSE              284
#define VK_KEY_F1                 290
#define VK_KEY_F2                 291
#define VK_KEY_F3                 292
#define VK_KEY_F4                 293
#define VK_KEY_F5                 294
#define VK_KEY_F6                 295
#define VK_KEY_F7                 296
#define VK_KEY_F8                 297
#define VK_KEY_F9                 298
#define VK_KEY_F10                299
#define VK_KEY_F11                300
#define VK_KEY_F12                301
#define VK_KEY_F13                302
#define VK_KEY_F14                303
#define VK_KEY_F15                304
#define VK_KEY_F16                305
#define VK_KEY_F17                306
#define VK_KEY_F18                307
#define VK_KEY_F19                308
#define VK_KEY_F20                309
#define VK_KEY_F21                310
#define VK_KEY_F22                311
#define VK_KEY_F23                312
#define VK_KEY_F24                313
#define VK_KEY_F25                314
#define VK_KEY_KP_0               320
#define VK_KEY_KP_1               321
#define VK_KEY_KP_2               322
#define VK_KEY_KP_3               323
#define VK_KEY_KP_4               324
#define VK_KEY_KP_5               325
#define VK_KEY_KP_6               326
#define VK_KEY_KP_7               327
#define VK_KEY_KP_8               328
#define VK_KEY_KP_9               329
#define VK_KEY_KP_DECIMAL         330
#define VK_KEY_KP_DIVIDE          331
#define VK_KEY_KP_MULTIPLY        332
#define VK_KEY_KP_SUBTRACT        333
#define VK_KEY_KP_ADD             334
#define VK_KEY_KP_ENTER           335
#define VK_KEY_KP_EQUAL           336
#define VK_KEY_LEFT_SHIFT         340
#define VK_KEY_LEFT_CONTROL       341
#define VK_KEY_LEFT_ALT           342
#define VK_KEY_LEFT_SUPER         343
#define VK_KEY_RIGHT_SHIFT        344
#define VK_KEY_RIGHT_CONTROL      345
#define VK_KEY_RIGHT_ALT          346
#define VK_KEY_RIGHT_SUPER        347
#define VK_KEY_MENU               348

#define VK_BIND_KEY_FUNCTION(function, object) std::bind(&function, &object, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)

namespace VulkanHelper
{
	class Input
	{
	public:
		void Init(GLFWwindow* window);
		bool IsKeyPressed(int keyCode);
		bool IsMousePressed(int mouseButton);
		glm::vec2 GetMousePosition();
		float GetScrollValue();
		void SetScrollValue(float x);

		void SetKeyCallback(int key, std::function<bool(int, int, int, int)> function);

		static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	private:

		std::unordered_map<uint32_t, std::vector<std::function<bool(int, int, int, int)>>> m_Keys;

		GLFWwindow* m_Window;
	};
}