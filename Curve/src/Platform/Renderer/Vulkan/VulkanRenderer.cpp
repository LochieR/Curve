#include "cvpch.h"
#include "VulkanRenderer.h"

#include "Curve/Core/Base.h"

#include "VulkanData.h"
#include "VulkanBuffer.h"
#include "VulkanShader.h"
#include "VulkanSwapchain.h"
#include "VulkanFramebuffer.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanImGuiLayer.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <set>

namespace cv {

	constexpr bool s_EnableValidationLayers =
#ifdef CV_DEBUG
		true;
#else
		false;
#endif

	const static std::vector<const char*> s_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const static std::vector<const char*> s_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };

	namespace Utils {

		static std::vector<const char*> GetRequiredExtensions()
		{
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

			if (s_EnableValidationLayers)
			{
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}

			return extensions;
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)
		{
			if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
				return VK_FALSE;

			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				std::cerr << "\u001b[31m[Validation Layer] " << pCallbackData->pMessage << "\u001b[0m" << std::endl;
			//else
			//	std::cerr << "\033[38;5;208m[Validation Layer] " << pCallbackData->pMessage << "\u001b[0m" << std::endl;

			return VK_FALSE;
		}

		static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
		{
			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func != nullptr)
			{
				return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
			}
			else
			{
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}

		static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
		{
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != nullptr)
			{
				func(instance, debugMessenger, pAllocator);
			}
		}

		static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
		{
			createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo.pfnUserCallback = DebugCallback;
		}

		static bool CheckValidationLayerSupport()
		{
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			for (const char* layerName : s_ValidationLayers)
			{
				bool layerFound = false;

				for (const auto& layerProperties : availableLayers)
				{
					if (strcmp(layerName, layerProperties.layerName) == 0)
					{
						layerFound = true;
						break;
					}
				}

				if (!layerFound)
				{
					return false;
				}
			}

			return true;
		}

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			QueueFamilyIndices indices{};

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

			int i = 0;
			for (const auto& queueFamily : queueFamilies)
			{
				if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
					indices.GraphicsFamily = i;

				VkBool32 presentSupport = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

				if (presentSupport)
					indices.PresentFamily = i;

				if (indices.IsComplete())
					break;

				i++;
			}

			return indices;
		}

		static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
		{
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

			std::set<std::string> requiredExtensions;

			for (const char* ext : s_DeviceExtensions)
				requiredExtensions.insert(std::string(ext));

			for (const auto& extension : availableExtensions)
			{
				requiredExtensions.erase(std::string(extension.extensionName));
			}

			return requiredExtensions.empty();
		}

		SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			SwapchainSupportDetails details;

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

			if (formatCount != 0)
			{
				details.Formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());
			}

			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

			if (presentModeCount != 0)
			{
				details.PresentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data());
			}

			return details;
		}

		uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
			{
				if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
					return i;
			}

			CV_ASSERT(false && "Failed to find suitable memory type!");
			return 0;
		}

		void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
		{
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkResult result = vkCreateBuffer(device, &bufferInfo, allocator, &buffer);
			VK_CHECK(result, "Failed to create Vulkan buffer!");

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

			result = vkAllocateMemory(device, &allocInfo, allocator, &bufferMemory);
			VK_CHECK(result, "Failed to allocate Vulkan memory!");

			vkBindBufferMemory(device, buffer, bufferMemory, 0);
		}

		void CopyBuffer(CommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, size_t bufferSize)
		{
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = bufferSize;

			vkCmdCopyBuffer(
				commandBuffer.As<VkCommandBuffer>(),
				srcBuffer,
				dstBuffer,
				1, &copyRegion
			);
		}

		void CopyBuffer(VulkanRenderer* renderer, VkBuffer srcBuffer, VkBuffer dstBuffer, size_t bufferSize)
		{
			CommandBuffer commandBuffer = renderer->BeginSingleTimeCommands();
			CopyBuffer(commandBuffer, srcBuffer, dstBuffer, bufferSize);
			renderer->EndSingleTimeCommands(commandBuffer);
		}

		void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, const VkAllocationCallbacks* allocator, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
		{
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = usage;
			imageInfo.samples = samples;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkResult result = vkCreateImage(device, &imageInfo, allocator, &image);
			VK_CHECK(result, "Failed to create Vulkan image!");

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(device, image, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

			result = vkAllocateMemory(device, &allocInfo, allocator, &imageMemory);
			VK_CHECK(result, "Failed to allocate Vulkan memory!");

			vkBindImageMemory(device, image, imageMemory, 0);
		}

		VkImageView CreateImageView(VkDevice device, const VkAllocationCallbacks* allocator, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = format;
			viewInfo.subresourceRange.aspectMask = aspectFlags;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			VkImageView imageView;
			VkResult result = vkCreateImageView(device, &viewInfo, allocator, &imageView);
			VK_CHECK(result, "Failed to create Vulkan image view!");

			return imageView;
		}

		void TransitionImageLayout(CommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			VkPipelineStageFlags sourceStage = 0;
			VkPipelineStageFlags destinationStage = 0;

			if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				if (HasStencilComponent(format))
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}
			else
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}

			if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			}
			else
			{
				CV_ASSERT("Unsupported Vulkan image layout transition!");
			}

			vkCmdPipelineBarrier(
				commandBuffer.As<VkCommandBuffer>(),
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}

		void TransitionImageLayout(VulkanRenderer* renderer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
		{
			CommandBuffer commandBuffer = renderer->BeginSingleTimeCommands();
			TransitionImageLayout(commandBuffer, image, format, oldLayout, newLayout);
			renderer->EndSingleTimeCommands(commandBuffer);
		}

		static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			QueueFamilyIndices indices = FindQueueFamilies(device, surface);

			bool extensionsSupported = CheckDeviceExtensionSupport(device);

			bool swapchainAdequate = false;
			if (extensionsSupported)
			{
				SwapchainSupportDetails swapChainSupport = QuerySwapchainSupport(device, surface);
				swapchainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
			}

			VkPhysicalDeviceFeatures supportedFeatures;
			vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

			return
				indices.IsComplete() &&
				extensionsSupported &&
				swapchainAdequate &&
				supportedFeatures.samplerAnisotropy &&
				supportedFeatures.wideLines &&
				supportedFeatures.fragmentStoresAndAtomics &&
				supportedFeatures.independentBlend;
		}

		static VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice device)
		{
			VkPhysicalDeviceProperties physicalDeviceProperties;
			vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

			VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

#define PRINT_SAMPLE_COUNT_AND_RETURN(x) { CV_INFO("Using Vulkan sample count: " #x); return x; }

			if (counts & VK_SAMPLE_COUNT_64_BIT)
				PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_64_BIT);
			if (counts & VK_SAMPLE_COUNT_32_BIT)
				PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_32_BIT);
			if (counts & VK_SAMPLE_COUNT_16_BIT)
				PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_16_BIT);
			if (counts & VK_SAMPLE_COUNT_8_BIT)
				PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_8_BIT);
			if (counts & VK_SAMPLE_COUNT_4_BIT)
				PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_4_BIT);
			if (counts & VK_SAMPLE_COUNT_2_BIT)
				PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_2_BIT);

			PRINT_SAMPLE_COUNT_AND_RETURN(VK_SAMPLE_COUNT_1_BIT);

#undef PRINT_SAMPLE_COUNT_AND_RETURN
		}

	}

	VulkanRenderer::VulkanRenderer(Window& window)
		: m_Window(window)
	{
		CV_TAG("Renderer:Vulkan");

		m_VkD = new VulkanData();

		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateCommandPool();
		CreateDescriptorPool();
		CreateSyncObjects();

		SwapchainSpecification spec{};
		spec.Attachments = { AttachmentFormat::Default, AttachmentFormat::Depth };
		spec.Multisample = true;

		m_VkD->Swapchain = CreateSwapchain(spec);
	}

	VulkanRenderer::~VulkanRenderer()
	{
		vkDeviceWaitIdle(m_VkD->Device);

		delete m_VkD->Swapchain;

		for (auto& queue : m_VkD->ResourceFreeQueue)
		{
			for (auto& func : queue)
				func(this);
		}

		for (size_t i = 0; i < CV_FRAMES_IN_FLIGHT; i++)
		{
			for (size_t j = 0; j < CV_MAX_SUBMITS_PER_FRAME; j++)
			{
				vkDestroyFence(m_VkD->Device, m_VkD->InFlightFences[i][j], m_VkD->Allocator);
				vkDestroySemaphore(m_VkD->Device, m_VkD->FrameSemaphores[i][j], m_VkD->Allocator);
			}
		}

		vkDestroyDescriptorPool(m_VkD->Device, m_VkD->DescriptorPool, m_VkD->Allocator);
		vkDestroyCommandPool(m_VkD->Device, m_VkD->CommandPool, m_VkD->Allocator);

		vkDestroyDevice(m_VkD->Device, m_VkD->Allocator);
		vkDestroySurfaceKHR(m_VkD->Instance, m_VkD->Surface, m_VkD->Allocator);

		if (s_EnableValidationLayers)
			Utils::DestroyDebugUtilsMessengerEXT(m_VkD->Instance, m_VkD->DebugMessenger, m_VkD->Allocator);

		vkDestroyInstance(m_VkD->Instance, m_VkD->Allocator);

		delete m_VkD;
		m_VkD = nullptr;
	}

	void VulkanRenderer::BeginFrame()
	{
		m_VkD->UsedSemaphoreCount[m_VkD->CurrentFrameIndex] = 0;
		m_VkD->UsedFenceCount[m_VkD->CurrentFrameIndex] = 0;

		m_VkD->FrameSuccess[m_VkD->CurrentFrameIndex] = true;

		uint32_t imageIndex;
		if (!m_VkD->Swapchain->AcquireNextImage(imageIndex))
		{
			m_VkD->FrameSuccess[m_VkD->CurrentFrameIndex] = false;
			return;
		}
	}

	void VulkanRenderer::EndFrame()
	{
		if (m_VkD->FrameSuccess[m_VkD->CurrentFrameIndex])
		{
			auto& scd = m_VkD->Swapchain->GetNativeData<SwapchainData>();

			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = (uint32_t)m_VkD->InUseSemaphores[m_VkD->CurrentFrameIndex].size();
			presentInfo.pWaitSemaphores = m_VkD->InUseSemaphores[m_VkD->CurrentFrameIndex].data();
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &scd.Swapchain;
			presentInfo.pImageIndices = &scd.ImageIndex;

			VkResult result = vkQueuePresentKHR(m_VkD->PresentQueue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
				((VulkanSwapchain*)m_VkD->Swapchain)->RecreateSwapchain();
			else
			{
				VK_CHECK(result, "Failed to present Vulkan queue!");
			}
		}

		for (auto& func : m_VkD->ResourceFreeQueue[m_VkD->CurrentFrameIndex])
		{
			func(this);
		}
		m_VkD->ResourceFreeQueue[m_VkD->CurrentFrameIndex].clear();

		m_VkD->CurrentFrameIndex = (m_VkD->CurrentFrameIndex + 1) % CV_FRAMES_IN_FLIGHT;
	}

	void VulkanRenderer::Draw(CommandBuffer commandBuffer, size_t vertexCount, size_t vertexOffset) const
	{
		vkCmdDraw(commandBuffer.As<VkCommandBuffer>(), (uint32_t)vertexCount, 1, (uint32_t)vertexOffset, 0);
	}

	void VulkanRenderer::DrawIndexed(CommandBuffer commandBuffer, size_t indexCount, size_t indexOffset) const
	{
		vkCmdDrawIndexed(commandBuffer.As<VkCommandBuffer>(), (uint32_t)indexCount, 1, (uint32_t)indexOffset, 0, 0);
	}

	void VulkanRenderer::BeginCommandBuffer(CommandBuffer commandBuffer) const
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		
		VkResult result = vkBeginCommandBuffer(commandBuffer.As<VkCommandBuffer>(), &beginInfo);
		VK_CHECK(result, "Failed to begin Vulkan command buffer!");
	}

	void VulkanRenderer::EndCommandBuffer(CommandBuffer commandBuffer) const
	{
		VkResult result = vkEndCommandBuffer(commandBuffer.As<VkCommandBuffer>());
		VK_CHECK(result, "Failed to end Vulkan command buffer!");
	}

	void VulkanRenderer::SubmitCommandBuffer(CommandBuffer commandBuffer) const
	{
		if (m_VkD->FrameSuccess[m_VkD->CurrentFrameIndex])
		{
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

			VkCommandBuffer cmd = commandBuffer.As<VkCommandBuffer>();
			VkSemaphore semaphore = GetNextFrameSemaphore();

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = (uint32_t)m_VkD->InUseSemaphores[m_VkD->CurrentFrameIndex].size();
			submitInfo.pWaitSemaphores = m_VkD->InUseSemaphores[m_VkD->CurrentFrameIndex].data();
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &semaphore;

			VkFence fence = GetNextFrameFence();

			VkResult result = vkQueueSubmit(m_VkD->GraphicsQueue, 1, &submitInfo, fence);
			VK_CHECK(result, "Failed to submit to Vulkan queue!");

			m_VkD->InUseSemaphores[m_VkD->CurrentFrameIndex].clear();
			m_VkD->InUseSemaphores[m_VkD->CurrentFrameIndex].push_back(semaphore);

			m_VkD->InUseFences[m_VkD->CurrentFrameIndex].push_back(fence);
		}
	}

	Swapchain* VulkanRenderer::GetSwapchain() const
	{
		return m_VkD->Swapchain;
	}

	CommandBuffer VulkanRenderer::AllocateCommandBuffer() const
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_VkD->CommandPool;
		allocInfo.commandBufferCount = 1;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VkCommandBuffer commandBuffer = nullptr;
		VkResult result = vkAllocateCommandBuffers(m_VkD->Device, &allocInfo, &commandBuffer);
		VK_CHECK(result, "Failed to allocate Vulkan command buffer!");

		return CommandBuffer(commandBuffer);
	}

	CommandBuffer VulkanRenderer::BeginSingleTimeCommands() const
	{
		CommandBuffer commandBuffer = AllocateCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkResult result = vkBeginCommandBuffer(commandBuffer.As<VkCommandBuffer>(), &beginInfo);
		VK_CHECK(result, "Failed to begin Vulkan command buffer!");

		return commandBuffer;
	}

	void VulkanRenderer::EndSingleTimeCommands(CommandBuffer commandBuffer) const
	{
		VkCommandBuffer cmd = commandBuffer.As<VkCommandBuffer>();

		VkResult result = vkEndCommandBuffer(cmd);
		VK_CHECK(result, "Failed to end Vulkan command buffer!");

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		
		result = vkQueueSubmit(m_VkD->GraphicsQueue, 1, &submitInfo, nullptr);
		VK_CHECK(result, "Failed to submit to Vulkan queue!");

		result = vkQueueWaitIdle(m_VkD->GraphicsQueue);
		VK_CHECK(result, "An error occurred while waiting for Vulkan queue!");

		vkFreeCommandBuffers(m_VkD->Device, m_VkD->CommandPool, 1, &cmd);
	}

	Swapchain* VulkanRenderer::CreateSwapchain(const SwapchainSpecification& spec)
	{
		return new VulkanSwapchain(this, spec);
	}

	Shader* VulkanRenderer::CreateShader(const std::filesystem::path& path)
	{
		return new VulkanShader(this, path);
	}

	GraphicsPipeline* VulkanRenderer::CreateGraphicsPipeline(Shader* shader, PrimitiveTopology topology, const InputLayout& layout)
	{
		return new VulkanGraphicsPipeline(this, shader, topology, layout);
	}

	GraphicsPipeline* VulkanRenderer::CreateGraphicsPipeline(Shader* shader, PrimitiveTopology topology, const InputLayout& layout, Framebuffer* framebuffer)
	{
		return new VulkanGraphicsPipeline(this, shader, topology, layout, framebuffer);
	}

	VertexBuffer* VulkanRenderer::CreateVertexBuffer(size_t size)
	{
		return new VulkanVertexBuffer(this, size);
	}

	IndexBuffer* VulkanRenderer::CreateIndexBuffer(uint32_t* indices, uint32_t indexCount)
	{
		return new VulkanIndexBuffer(this, indices, indexCount);
	}

	Framebuffer* VulkanRenderer::CreateFramebuffer(const FramebufferSpecification& spec)
	{
		return new VulkanFramebuffer(this, spec);
	}

	ImGuiLayer* VulkanRenderer::CreateImGuiLayer()
	{
		return new VulkanImGuiLayer(this);
	}

	uint32_t VulkanRenderer::GetCurrentFrameIndex() const
	{
		return m_VkD->CurrentFrameIndex;
	}

	VkSemaphore VulkanRenderer::GetNextFrameSemaphore() const
	{
		uint32_t index = m_VkD->UsedSemaphoreCount[m_VkD->CurrentFrameIndex];
		m_VkD->UsedSemaphoreCount[m_VkD->CurrentFrameIndex]++;
		return m_VkD->FrameSemaphores[m_VkD->CurrentFrameIndex][index];
	}

	VkFence VulkanRenderer::GetNextFrameFence() const
	{
		uint32_t index = m_VkD->UsedFenceCount[m_VkD->CurrentFrameIndex];
		m_VkD->UsedFenceCount[m_VkD->CurrentFrameIndex]++;
		return m_VkD->InFlightFences[m_VkD->CurrentFrameIndex][index];
	}

	void VulkanRenderer::SubmitResourceFree(std::function<void(VulkanRenderer*)>&& func)
	{
		m_VkD->ResourceFreeQueue[m_VkD->CurrentFrameIndex].push_back(func);
	}

	void VulkanRenderer::CreateInstance()
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Curve";
		appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo instanceInfo{};
		instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceInfo.pApplicationInfo = &appInfo;

		std::vector<const char*> extensions = Utils::GetRequiredExtensions();
		instanceInfo.enabledExtensionCount = (uint32_t)extensions.size();
		instanceInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (s_EnableValidationLayers)
		{
			CV_ASSERT(Utils::CheckValidationLayerSupport() && "Validation layers have been requested, but are not supported!");

			instanceInfo.enabledLayerCount = (uint32_t)s_ValidationLayers.size();
			instanceInfo.ppEnabledLayerNames = s_ValidationLayers.data();

			Utils::PopulateDebugMessengerCreateInfo(debugCreateInfo);
			instanceInfo.pNext = &debugCreateInfo;
		}
		else
		{
			instanceInfo.enabledLayerCount = 0;
			instanceInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&instanceInfo, m_VkD->Allocator, &m_VkD->Instance);
		VK_CHECK(result, "Failed to create Vulkan instance!");

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> instanceExtensions(extensionCount);

		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, instanceExtensions.data());

		CV_INFO("Available extensions:");
		for (const auto& extension : instanceExtensions)
		{
			CV_INFO("\t", extension.extensionName);
		}
	}

	void VulkanRenderer::SetupDebugMessenger()
	{
		if (!s_EnableValidationLayers)
			return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		Utils::PopulateDebugMessengerCreateInfo(createInfo);

		VkResult result = Utils::CreateDebugUtilsMessengerEXT(m_VkD->Instance, &createInfo, m_VkD->Allocator, &m_VkD->DebugMessenger);
		VK_CHECK(result, "Failed to create Vulkan debug messenger!");
	}

	void VulkanRenderer::CreateSurface()
	{
		VkResult result = glfwCreateWindowSurface(m_VkD->Instance, m_Window.GetHandle(), m_VkD->Allocator, &m_VkD->Surface);
		VK_CHECK(result, "Failed to create Vulkan window surface!");
	}

	void VulkanRenderer::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_VkD->Instance, &deviceCount, nullptr);

		CV_ASSERT(deviceCount && "Failed to find GPUs with Vulkan support!");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_VkD->Instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (Utils::IsDeviceSuitable(device, m_VkD->Surface))
			{
				m_VkD->PhysicalDevice = device;
				m_VkD->MultisampleCount = Utils::GetMaxUsableSampleCount(device);
				break;
			}
		}

		CV_ASSERT(m_VkD->PhysicalDevice && "Failed to find a suitable GPU!");
	}

	void VulkanRenderer::CreateLogicalDevice()
	{
		QueueFamilyIndices indices = Utils::FindQueueFamilies(m_VkD->PhysicalDevice, m_VkD->Surface);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily, indices.PresentFamily };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos.emplace_back();
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.wideLines = VK_TRUE;
		deviceFeatures.sampleRateShading = VK_TRUE;
		deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
		deviceFeatures.independentBlend = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = (uint32_t)s_DeviceExtensions.size();
		createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

		if (s_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = (uint32_t)s_ValidationLayers.size();
			createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateDevice(m_VkD->PhysicalDevice, &createInfo, m_VkD->Allocator, &m_VkD->Device);
		VK_CHECK(result, "Failed to create Vulkan device!");

		vkGetDeviceQueue(m_VkD->Device, indices.GraphicsFamily, 0, &m_VkD->GraphicsQueue);
		vkGetDeviceQueue(m_VkD->Device, indices.PresentFamily, 0, &m_VkD->PresentQueue);
	}

	void VulkanRenderer::CreateCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VkResult result = vkCreateCommandPool(m_VkD->Device, &poolInfo, m_VkD->Allocator, &m_VkD->CommandPool);
		VK_CHECK(result, "Failed to create Vulkan command pool!");
	}

	void VulkanRenderer::CreateDescriptorPool()
	{
		VkDescriptorPoolSize samplers{};
		samplers.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplers.descriptorCount = 1000;

		VkDescriptorPoolSize uniforms{};
		uniforms.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniforms.descriptorCount = 1000;

		VkDescriptorPoolSize storage{};
		storage.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		storage.descriptorCount = 1000;

		std::array poolSizes = { samplers, uniforms, storage };

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1000 * (uint32_t)poolSizes.size();

		VkResult result = vkCreateDescriptorPool(m_VkD->Device, &poolInfo, m_VkD->Allocator, &m_VkD->DescriptorPool);
		VK_CHECK(result, "Failed to create Vulkan descriptor pool!");
	}

	void VulkanRenderer::CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		//fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < CV_FRAMES_IN_FLIGHT; i++)
		{
			for (size_t j = 0; j < CV_MAX_SUBMITS_PER_FRAME; j++)
			{
				VkResult result = vkCreateFence(m_VkD->Device, &fenceInfo, m_VkD->Allocator, &m_VkD->InFlightFences[i][j]);
				VK_CHECK(result, "Failed to create Vulkan fence!");
				
				result = vkCreateSemaphore(m_VkD->Device, &semaphoreInfo, m_VkD->Allocator, &m_VkD->FrameSemaphores[i][j]);
				VK_CHECK(result, "Failed to create Vulkan semaphore!");
			}
		}
	}

}
