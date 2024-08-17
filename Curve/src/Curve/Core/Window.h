#pragma once

#include "Event.h"

#include <string>
#include <functional>

struct GLFWwindow;

namespace cv {

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

		bool WasFramebufferResized() const { return m_Data.FramebufferResized; }
		void ResetFramebufferResized() { m_Data.FramebufferResized = false; }

		GLFWwindow* GetHandle() const { return m_Window; }
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
