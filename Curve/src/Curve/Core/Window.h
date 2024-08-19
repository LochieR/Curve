#pragma once

#include "Base.h"
#include "Event.h"

#include <string>
#include <functional>

struct GLFWwindow;

namespace cv {

#ifdef CV_PLATFORM_WINDOWS
	enum class WindowAttribute
	{
		TitlebarColor,
		BorderColor
	};
#endif

	class Window
	{
	public:
		Window(const std::string& title, uint32_t width, uint32_t height);
		~Window();

		void SetEventCallback(std::function<void(Event&)>&& callback) { m_Data.EventCallback = callback; }

		void OnUpdate();

		void Show() const;
		void Hide() const;
		void Minimize() const;
		void Maximize() const;
		void Restore() const;
		
		bool IsMaximized() const;

		uint32_t GetWidth() const { return m_Data.Width; }
		uint32_t GetHeight() const { return m_Data.Height; }

		bool WasFramebufferResized() const { return m_Data.FramebufferResized; }
		void ResetFramebufferResized() { m_Data.FramebufferResized = false; }

		GLFWwindow* GetHandle() const { return m_Window; }

#ifdef CV_PLATFORM_WINDOWS
		void SetWindowAttribute(WindowAttribute attribute, const glm::vec4& value);
#endif
	private:
		GLFWwindow* m_Window = nullptr;

		struct WindowData
		{
			std::string Title;
			uint32_t Width, Height;

			bool FramebufferResized = false;

			std::function<void(Event&)> EventCallback = nullptr;
		};

		WindowData m_Data;
	};

}
