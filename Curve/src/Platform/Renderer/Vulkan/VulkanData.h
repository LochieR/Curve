#pragma once

#include "VulkanRenderer.h"
#include "Curve/Core/Base.h"
#include "Curve/Core/Window.h"

#include "Curve/Renderer/Swapchain.h"

#include <vulkan/vulkan.h>

namespace cv {

	struct VulkanData
	{
		VkInstance Instance = nullptr;
		const VkAllocationCallbacks* Allocator = nullptr;
		VkDebugUtilsMessengerEXT DebugMessenger = nullptr;
		VkSurfaceKHR Surface = nullptr;
		VkPhysicalDevice PhysicalDevice = nullptr;
		VkDevice Device = nullptr;
		VkQueue GraphicsQueue = nullptr;
		VkQueue PresentQueue = nullptr;
		VkCommandPool CommandPool = nullptr;
		VkDescriptorPool DescriptorPool = nullptr;

		Swapchain* Swapchain = nullptr;

		VkSampleCountFlagBits MultisampleCount = VK_SAMPLE_COUNT_1_BIT;

		uint32_t CurrentFrameIndex = 0;

		std::array<std::array<VkSemaphore, CV_MAX_SUBMITS_PER_FRAME>, CV_FRAMES_IN_FLIGHT> FrameSemaphores = {};
		std::array<std::array<VkFence, CV_MAX_SUBMITS_PER_FRAME>, CV_FRAMES_IN_FLIGHT> InFlightFences = {};

		std::array<std::vector<VkSemaphore>, CV_FRAMES_IN_FLIGHT> InUseSemaphores = {};
		std::array<std::vector<VkFence>, CV_FRAMES_IN_FLIGHT> InUseFences = {};

		std::array<uint32_t, CV_FRAMES_IN_FLIGHT> UsedSemaphoreCount = {};
		std::array<uint32_t, CV_FRAMES_IN_FLIGHT> UsedFenceCount = {};
		std::array<bool, CV_FRAMES_IN_FLIGHT> FrameSuccess = {};

		std::array<std::vector<std::function<void(VulkanRenderer*)>>, CV_FRAMES_IN_FLIGHT> ResourceFreeQueue = {};
	};

	struct QueueFamilyIndices
	{
		uint32_t GraphicsFamily = static_cast<uint32_t>(-1);
		uint32_t PresentFamily = static_cast<uint32_t>(-1);

		bool IsComplete() const { return GraphicsFamily != static_cast<uint32_t>(-1) && PresentFamily != static_cast<uint32_t>(-1); }
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	struct SwapchainData
	{
		VkSwapchainKHR Swapchain = nullptr;
		VkFormat ImageFormat;
		VkExtent2D Extent;

		VkRenderPass RenderPass = nullptr;

		uint32_t ImageIndex = 0;

		size_t ImageCount;
		std::vector<VkImage> Images;
		std::vector<VkImageView> ImageViews;
		std::vector<VkFramebuffer> Framebuffers;

		VkImage DepthImage = nullptr;
		VkDeviceMemory DepthImageMemory = nullptr;
		VkImageView DepthImageView = nullptr;

		VkImage ColorImage = nullptr;
		VkDeviceMemory ColorImageMemory = nullptr;
		VkImageView ColorImageView = nullptr;
	};

	struct ShaderData
	{
		std::vector<uint32_t> VertexData;
		std::vector<uint32_t> FragmentData;
		VkShaderModule VertexModule = nullptr;
		VkShaderModule FragmentModule = nullptr;
	};

	struct BufferData
	{
		VkBuffer Buffer = nullptr;
		VkDeviceMemory Memory = nullptr;
		size_t Size = 0;
	};

	struct GraphicsPipelineData
	{
		VkPipelineLayout PipelineLayout = nullptr;
		VkPipeline Pipeline = nullptr;
		VkDescriptorSetLayout SetLayout = nullptr;
		VkDescriptorSet DescriptorSet = nullptr;
	};

	namespace Utils {

		// general
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
		SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

		uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

		void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		void CopyBuffer(CommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, size_t bufferSize);
		void CopyBuffer(VulkanRenderer* renderer, VkBuffer srcBuffer, VkBuffer dstBuffer, size_t bufferSize);
		void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
		VkImageView CreateImageView(VkDevice device, const VkAllocationCallbacks* allocator, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
		void TransitionImageLayout(CommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void TransitionImageLayout(VulkanRenderer* renderer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

		// swapchain
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(Window& window, const VkSurfaceCapabilitiesKHR& capabilities);
		VkFormat FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkFormat FindDepthFormat(VkPhysicalDevice device);
		bool HasStencilComponent(VkFormat format);

	}

}
