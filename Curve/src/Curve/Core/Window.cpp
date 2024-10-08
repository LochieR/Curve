#include "cvpch.h"
#include "Window.h"

#include <GLFW/glfw3.h>
#ifdef CV_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <windows.h>
#include <dwmapi.h>
#endif

namespace cv {

	static uint8_t s_GLFWWindowCount = 0;

	static void GLFWErrorCallback(int error, const char* description)
	{
		CV_TEMP_TAG("GLFW");
		CV_ERROR("GLFW Error (", error, "): ", description);
	}

	Window::Window(const std::string& title, uint32_t width, uint32_t height)
	{
		m_Data = {};
		m_Data.Title = title;
		m_Data.Width = width;
		m_Data.Height = height;

		if (s_GLFWWindowCount == 0)
		{
			CV_ASSERT(glfwInit() && "Failed to initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);
		}

		s_GLFWWindowCount++;

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		// TODO: adjust for application specification
		//glfwWindowHint(GLFW_TITLEBAR, GLFW_FALSE);

		m_Window = glfwCreateWindow((int)width, (int)height, title.c_str(), nullptr, nullptr);

		Input::SetActiveWindow(m_Window);

		glfwSetWindowUserPointer(m_Window, &m_Data);

		glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width = width;
			data.Height = height;

			WindowResizeEvent event(width, height);
			data.EventCallback(event);
		});

		glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			data.Width = width;
			data.Height = height;

			data.FramebufferResized = true;
		});

		glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
		});

		glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				KeyPressedEvent event((KeyCode)key, false);
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event((KeyCode)key);
				data.EventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event((KeyCode)key, true);
				data.EventCallback(event);
				break;
			}
			}
		});

		glfwSetCharCallback(m_Window, [](GLFWwindow* window, uint32_t keycode)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			KeyTypedEvent event(static_cast<KeyCode>(keycode));
			data.EventCallback(event);
		});

		glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				MouseButtonPressedEvent event(static_cast<MouseButton>(button));
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				MouseButtonReleasedEvent event(static_cast<MouseButton>(button));
				data.EventCallback(event);
				break;
			}
			}
		});

		glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			data.EventCallback(event);
		});

		glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
		});
	}

	Window::~Window()
	{
		glfwDestroyWindow(m_Window);
		s_GLFWWindowCount--;

		if (s_GLFWWindowCount == 0)
			glfwTerminate();
	}

	void Window::OnUpdate()
	{
		glfwPollEvents();
	}

	void Window::Show() const
	{
		glfwShowWindow(m_Window);
	}

	void Window::Hide() const
	{
		glfwHideWindow(m_Window);
	}

	void Window::Minimize() const
	{
		glfwIconifyWindow(m_Window);
	}

	void Window::Maximize() const
	{
		glfwMaximizeWindow(m_Window);
	}

	void Window::Restore() const
	{
		glfwRestoreWindow(m_Window);
	}

	bool Window::IsMaximized() const
	{
		return (bool)glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED);
	}

#ifdef CV_PLATFORM_WINDOWS
	void Window::SetWindowAttribute(WindowAttribute attribute, const glm::vec4& value)
	{
		HWND hwnd = glfwGetWin32Window(m_Window);
		COLORREF color = RGB((int)(value.x * 255.0f), (int)(value.y * 255.0f), (int)(value.z * 255.0f));

		switch (attribute)
		{
			case WindowAttribute::TitlebarColor:
			{
				DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(COLORREF));
				SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
				break;
			}
			case WindowAttribute::BorderColor:
			{
				DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(COLORREF));
				SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
				break;
			}
			default:
				break;
		}
	}

	glm::vec4 Window::GetWindowAttribute(WindowAttribute attribute) const
	{
		HWND hwnd = glfwGetWin32Window(m_Window);
		COLORREF color = 0;

		switch (attribute)
		{
			case WindowAttribute::TitlebarColor:
			{
				DwmGetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(COLORREF));
				break;
			}
			case WindowAttribute::BorderColor:
			{
				DwmGetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &color, sizeof(COLORREF));
				break;
			}
		}

		BYTE r = GetRValue(color);
		BYTE g = GetGValue(color);
		BYTE b = GetBValue(color);

		return { (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f };
	}
#endif

}
