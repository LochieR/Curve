#include "cvpch.h"
#include "Application.h"

#include "Curve/Renderer/Renderer.h"

namespace cv {

	struct RendererData
	{
		Shader* Shader = nullptr;
		GraphicsPipeline* Pipeline = nullptr;
		VertexBuffer* VertexBuffer = nullptr;
		IndexBuffer* IndexBuffer = nullptr;

		std::array<CommandBuffer, CV_FRAMES_IN_FLIGHT> CommandBuffers;
	};

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	static RendererData s_Data;

	Application::Application(const ApplicationSpecification& spec)
		: m_Specification(spec), m_Window(spec.WindowTitle, spec.WindowWidth, spec.WindowHeight)
	{
		m_Window.SetEventCallback([this](Event& event) { this->OnEvent(event); });

		m_Renderer = Renderer::Create(m_Window);

		InputLayout inputLayout{};
		inputLayout.VertexLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float4, "a_Color" }
		};
		inputLayout.PushConstants = {};
		inputLayout.ShaderResources = {};

		std::array indices = {
			0u, 1u, 2u, 2u, 3u, 0u
		};

		std::array vertices = {
			Vertex{ {  0.5f,  0.5f, 0.0f }, { 0.8f, 0.3f, 0.5f, 1.0f } },
			Vertex{ { -0.5f,  0.5f, 0.0f }, { 0.5f, 0.3f, 0.8f, 1.0f } },
			Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.3f, 0.8f, 0.5f, 1.0f } },
			Vertex{ {  0.5f, -0.5f, 0.0f }, { 0.4f, 0.5f, 0.8f, 1.0f } }
		};

		s_Data = {};
		s_Data.Shader = m_Renderer->CreateShader("Shaders/Basic.glsl");
		s_Data.Pipeline = m_Renderer->CreateGraphicsPipeline(s_Data.Shader, PrimitiveTopology::TriangleList, inputLayout);
		s_Data.VertexBuffer = m_Renderer->CreateVertexBuffer(sizeof(Vertex) * vertices.size());
		s_Data.IndexBuffer = m_Renderer->CreateIndexBuffer(indices.data(), (uint32_t)indices.size());
		s_Data.CommandBuffers = { m_Renderer->AllocateCommandBuffer(), m_Renderer->AllocateCommandBuffer() };

		s_Data.VertexBuffer->SetData(vertices.data(), (uint32_t)(sizeof(Vertex) * vertices.size()));
	}

	Application::~Application()
	{
		delete s_Data.IndexBuffer;
		delete s_Data.VertexBuffer;
		delete s_Data.Pipeline;
		delete s_Data.Shader;

		delete m_Renderer;
	}

	void Application::Run()
	{
		m_Window.Show();

		Swapchain* swapchain = m_Renderer->GetSwapchain();

		while (m_Running)
		{
			CommandBuffer commandBuffer = s_Data.CommandBuffers[m_Renderer->GetCurrentFrameIndex()];

			m_Renderer->BeginFrame();
			m_Renderer->BeginCommandBuffer(commandBuffer);
			swapchain->BeginRenderPass(commandBuffer);
			
			s_Data.Pipeline->Bind(commandBuffer);
			s_Data.VertexBuffer->Bind(commandBuffer);
			s_Data.IndexBuffer->Bind(commandBuffer);
			
			m_Renderer->DrawIndexed(commandBuffer, 6);

			swapchain->EndRenderPass(commandBuffer);
			m_Renderer->EndCommandBuffer(commandBuffer);
			m_Renderer->SubmitCommandBuffer(commandBuffer);
			m_Renderer->EndFrame();

			m_Window.OnUpdate();
		}

		m_Window.Hide();
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event) { return this->OnWindowCloseEvent(event); });
	}

	bool Application::OnWindowCloseEvent(WindowCloseEvent& event)
	{
		m_Running = false;
		return false;
	}

}
