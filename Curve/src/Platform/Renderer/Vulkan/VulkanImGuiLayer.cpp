#include "cvpch.h"
#include "VulkanImGuiLayer.h"

#include "VulkanData.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <glfw/glfw3.h>

namespace cv {

	static void CheckVkResult(VkResult result)
	{
		VK_CHECK(result, "ImGui Vulkan operation failed!");
	}

	VulkanImGuiLayer::VulkanImGuiLayer(VulkanRenderer* renderer)
		: m_Renderer(renderer)
	{
	}

	VulkanImGuiLayer::~VulkanImGuiLayer()
	{
	}

	void VulkanImGuiLayer::OnAttach()
	{
		m_Data = new ImGuiLayerData();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/OpenSans/static/OpenSans-Regular.ttf", 18.0f);
		
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ConfigureStyle();

		GLFWwindow* window = m_Renderer->GetWindow().GetHandle();

		auto& vkd = m_Renderer->GetVulkanData();

		CreateVulkanObjects();

		Swapchain* swapchain = m_Renderer->GetSwapchain();

		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = vkd.Instance;
		initInfo.PhysicalDevice = vkd.PhysicalDevice;
		initInfo.Device = vkd.Device;
		initInfo.QueueFamily = Utils::FindQueueFamilies(vkd.PhysicalDevice, vkd.Surface).GraphicsFamily;
		initInfo.Queue = vkd.GraphicsQueue;
		initInfo.PipelineCache = nullptr;
		initInfo.DescriptorPool = m_Data->DescriptorPool;
		initInfo.RenderPass = swapchain->GetNativeData<SwapchainData>().RenderPass;
		initInfo.Subpass = 0;
		initInfo.MinImageCount = swapchain->GetImageCount();
		initInfo.ImageCount = swapchain->GetImageCount();
		initInfo.MSAASamples = vkd.MultisampleCount;
		initInfo.Allocator = vkd.Allocator;
		initInfo.CheckVkResultFn = &CheckVkResult;

		ImGui_ImplGlfw_InitForVulkan(window, true);
		ImGui_ImplVulkan_Init(&initInfo);
	}

	void VulkanImGuiLayer::OnDetach()
	{
		m_Renderer->SubmitResourceFree([data = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::Shutdown();

			vkDestroyDescriptorPool(vkd.Device, data->DescriptorPool, vkd.Allocator);

			delete data;
		});
	}

	void VulkanImGuiLayer::Begin()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void VulkanImGuiLayer::End()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		Window& window = m_Renderer->GetWindow();

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = { (float)window.GetWidth(), (float)window.GetHeight() };

		ImGui::Render();

		Swapchain* swapchain = m_Renderer->GetSwapchain();
		CommandBuffer commandBuffer = m_Data->CommandBuffers[vkd.CurrentFrameIndex];

		m_Renderer->BeginCommandBuffer(commandBuffer);
		swapchain->BeginRenderPass(commandBuffer);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.As<VkCommandBuffer>());

		swapchain->EndRenderPass(commandBuffer);
		m_Renderer->EndCommandBuffer(commandBuffer);
		m_Renderer->SubmitCommandBuffer(commandBuffer);
	}

	void VulkanImGuiLayer::UpdateViewports()
	{
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void VulkanImGuiLayer::ConfigureStyle()
	{
	}

	void VulkanImGuiLayer::CreateVulkanObjects()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 }
		};

		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		createInfo.maxSets = 100;
		createInfo.poolSizeCount = 1;
		createInfo.pPoolSizes = poolSizes;

		VkResult result = vkCreateDescriptorPool(vkd.Device, &createInfo, vkd.Allocator, &m_Data->DescriptorPool);
		VK_CHECK(result, "Failed to create Vulkan descriptor pool!");

		for (size_t i = 0; i < CV_FRAMES_IN_FLIGHT; i++)
			m_Data->CommandBuffers[i] = m_Renderer->AllocateCommandBuffer();
	}

}
