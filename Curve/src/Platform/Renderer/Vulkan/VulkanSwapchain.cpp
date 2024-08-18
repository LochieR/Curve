#include "cvpch.h"
#include "VulkanSwapchain.h"

#include "VulkanData.h"

#include <GLFW/glfw3.h>

namespace cv {

	namespace Utils {

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
		{
			for (const auto& availableFormat : availableFormats)
			{
				if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					return availableFormat;
				}
			}

			return availableFormats[0];
		}

		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
		{
			for (const auto& availablePresentMode : availablePresentModes)
			{
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					return availablePresentMode;
				}
			}

			return VK_PRESENT_MODE_FIFO_KHR;
		}

		VkExtent2D ChooseSwapExtent(Window& window, const VkSurfaceCapabilitiesKHR& capabilities)
		{
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			{
				return capabilities.currentExtent;
			}
			else
			{
				int width, height;
				glfwGetFramebufferSize(window.GetHandle(), &width, &height);

				VkExtent2D actualExtent = {
					static_cast<uint32_t>(width),
					static_cast<uint32_t>(height)
				};

				actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
				actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

				return actualExtent;
			}
		}

		VkFormat FindSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
		{
			for (VkFormat format : candidates)
			{
				VkFormatProperties properties;
				vkGetPhysicalDeviceFormatProperties(device, format, &properties);

				if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
					return format;
				else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
					return format;
			}

			CV_ASSERT(false && "Failed to find supported Vulkan format!");
			return candidates[0];
		}

		VkFormat FindDepthFormat(VkPhysicalDevice device)
		{
			return FindSupportedFormat(
				device,
				{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
				VK_IMAGE_TILING_OPTIMAL,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
			);
		}

		bool HasStencilComponent(VkFormat format)
		{
			return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
		}

		static VkFormat CurveAttachmentFormatToVkFormat(VkFormat defaultFormat, AttachmentFormat format)
		{
			switch (format)
			{
				case AttachmentFormat::Default: return defaultFormat;
				case AttachmentFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
				case AttachmentFormat::R32SInt: return VK_FORMAT_R32_SINT;
				case AttachmentFormat::R32UInt: return VK_FORMAT_R32_UINT;
				case AttachmentFormat::Depth:   return (VkFormat)0;
			}
		}

	}

	VulkanSwapchain::VulkanSwapchain(VulkanRenderer* renderer, const SwapchainSpecification& spec)
		: m_Renderer(renderer), m_Specification(spec)
	{
		m_Data = new SwapchainData();

		CreateSwapchain();
		CreateRenderPass();
		CreateColorResources();
		CreateFramebuffers();
	}

	VulkanSwapchain::~VulkanSwapchain()
	{
		m_Renderer->SubmitResourceFree([scd = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			for (VkFramebuffer framebuffer : scd->Framebuffers)
				vkDestroyFramebuffer(vkd.Device, framebuffer, vkd.Allocator);

			vkDestroyImageView(vkd.Device, scd->DepthImageView, vkd.Allocator);
			vkDestroyImage(vkd.Device, scd->DepthImage, vkd.Allocator);
			vkFreeMemory(vkd.Device, scd->DepthImageMemory, vkd.Allocator);

			vkDestroyImageView(vkd.Device, scd->ColorImageView, vkd.Allocator);
			vkDestroyImage(vkd.Device, scd->ColorImage, vkd.Allocator);
			vkFreeMemory(vkd.Device, scd->ColorImageMemory, vkd.Allocator);

			for (VkImageView view : scd->ImageViews)
				vkDestroyImageView(vkd.Device, view, vkd.Allocator);

			vkDestroyRenderPass(vkd.Device, scd->RenderPass, vkd.Allocator);
			vkDestroySwapchainKHR(vkd.Device, scd->Swapchain, vkd.Allocator);

			delete scd;
		});
	}

	bool VulkanSwapchain::AcquireNextImage(uint32_t& imageIndex)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		if (!vkd.InUseFences[vkd.CurrentFrameIndex].empty())
			vkWaitForFences(vkd.Device, (uint32_t)vkd.InUseFences[vkd.CurrentFrameIndex].size(), vkd.InUseFences[vkd.CurrentFrameIndex].data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

		VkSemaphore semaphore = m_Renderer->GetNextFrameSemaphore();

		VkResult result = vkAcquireNextImageKHR(vkd.Device, m_Data->Swapchain, std::numeric_limits<uint64_t>::max(), semaphore, nullptr, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_Renderer->GetWindow().WasFramebufferResized())
		{
			m_Renderer->GetWindow().ResetFramebufferResized();
			RecreateSwapchain(semaphore);
			return false;
		}
		VK_CHECK(result, "Failed to acquire next Vulkan swapchain image!");

		if (!vkd.InUseFences[vkd.CurrentFrameIndex].empty())
			vkResetFences(vkd.Device, (uint32_t)vkd.InUseFences[vkd.CurrentFrameIndex].size(), vkd.InUseFences[vkd.CurrentFrameIndex].data());

		vkd.InUseSemaphores[vkd.CurrentFrameIndex].clear();
		vkd.InUseFences[vkd.CurrentFrameIndex].clear();
		vkd.InUseSemaphores[vkd.CurrentFrameIndex].push_back(semaphore);

		m_Data->ImageIndex = imageIndex;

		return true;
	}

	void VulkanSwapchain::BeginRenderPass(CommandBuffer commandBuffer) const
	{
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		beginInfo.renderArea.extent = m_Data->Extent;
		beginInfo.renderArea.offset = { 0, 0 };
		beginInfo.renderPass = m_Data->RenderPass;
		beginInfo.framebuffer = m_Data->Framebuffers[m_Data->ImageIndex];
		beginInfo.clearValueCount = (uint32_t)clearValues.size();
		beginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer.As<VkCommandBuffer>(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanSwapchain::EndRenderPass(CommandBuffer commandBuffer) const
	{
		vkCmdEndRenderPass(commandBuffer.As<VkCommandBuffer>());
	}

	uint32_t VulkanSwapchain::GetImageCount() const
	{
		return m_Data->ImageCount;
	}

	uint32_t VulkanSwapchain::GetImageIndex() const
	{
		return m_Data->ImageIndex;
	}

	void VulkanSwapchain::CreateSwapchain(void* oldSwapchain)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		SwapchainSupportDetails swapchainSupport = Utils::QuerySwapchainSupport(vkd.PhysicalDevice, vkd.Surface);
		VkSurfaceFormatKHR surfaceFormat = Utils::ChooseSwapSurfaceFormat(swapchainSupport.Formats);
		VkPresentModeKHR presentMode = Utils::ChooseSwapPresentMode(swapchainSupport.PresentModes);
		VkExtent2D extent = Utils::ChooseSwapExtent(m_Renderer->GetWindow(), swapchainSupport.Capabilities);

		uint32_t imageCount = swapchainSupport.Capabilities.minImageCount + 1;

		if (swapchainSupport.Capabilities.maxImageCount > 0 && imageCount > swapchainSupport.Capabilities.maxImageCount)
			imageCount = swapchainSupport.Capabilities.maxImageCount;

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = vkd.Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = Utils::FindQueueFamilies(vkd.PhysicalDevice, vkd.Surface);
		uint32_t queueFamilyIndices[] = { indices.GraphicsFamily, indices.PresentFamily };

		if (indices.GraphicsFamily != indices.PresentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapchainSupport.Capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = (VkSwapchainKHR)oldSwapchain;

		VkResult result = vkCreateSwapchainKHR(vkd.Device, &createInfo, vkd.Allocator, &m_Data->Swapchain);
		VK_CHECK(result, "Failed to create Vulkan swapchain!");

		vkGetSwapchainImagesKHR(vkd.Device, m_Data->Swapchain, &imageCount, nullptr);
		m_Data->Images.resize(imageCount);
		vkGetSwapchainImagesKHR(vkd.Device, m_Data->Swapchain, &imageCount, m_Data->Images.data());

		m_Data->ImageFormat = surfaceFormat.format;
		m_Data->Extent = extent;
		m_Data->ImageCount = imageCount;

		m_Data->ImageViews.resize(imageCount);

		for (size_t i = 0; i < imageCount; i++)
			m_Data->ImageViews[i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->Images[i], m_Data->ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		VkFormat depthFormat = Utils::FindDepthFormat(vkd.PhysicalDevice);

		Utils::CreateImage(
			vkd.Device,
			vkd.PhysicalDevice,
			vkd.Allocator,
			extent.width,
			extent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			vkd.MultisampleCount,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Data->DepthImage,
			m_Data->DepthImageMemory
		);
		m_Data->DepthImageView = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
		Utils::TransitionImageLayout(m_Renderer, m_Data->DepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}

	void VulkanSwapchain::CreateRenderPass()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		/*std::vector<VkAttachmentDescription> normalAttachments;

		for (AttachmentFormat format : m_Specification.Attachments)
		{
			if (format != AttachmentFormat::Depth)
			{
				VkAttachmentDescription attachment{};
				attachment.format = Utils::CurveAttachmentFormatToVkFormat(m_Data->ImageFormat, format);
				attachment.samples = m_Specification.Multisample ? vkd.MultisampleCount : VK_SAMPLE_COUNT_1_BIT;
				attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				normalAttachments.push_back(attachment);
			}
			else
			{
				VkAttachmentDescription attachment{};
				attachment.format = Utils::FindDepthFormat(vkd.PhysicalDevice);
				attachment.samples = VK_SAMPLE_COUNT_1_BIT;
				attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				normalAttachments.push_back(attachment);
			}
		}*/

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_Data->ImageFormat;
		colorAttachment.samples = vkd.MultisampleCount;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = Utils::FindDepthFormat(vkd.PhysicalDevice);
		depthAttachment.samples = vkd.MultisampleCount;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = m_Data->ImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		std::array attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VkResult result = vkCreateRenderPass(vkd.Device, &renderPassInfo, vkd.Allocator, &m_Data->RenderPass);
		VK_CHECK(result, "Failed to create Vulkan render pass!");
	}

	void VulkanSwapchain::CreateColorResources()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		VkFormat colorFormat = m_Data->ImageFormat;

		Utils::CreateImage(
			vkd.Device,
			vkd.PhysicalDevice,
			vkd.Allocator,
			m_Data->Extent.width,
			m_Data->Extent.height,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			vkd.MultisampleCount,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Data->ColorImage,
			m_Data->ColorImageMemory
		);
		m_Data->ColorImageView = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->ColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void VulkanSwapchain::CreateFramebuffers()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		m_Data->Framebuffers.resize(m_Data->ImageCount);

		for (size_t i = 0; i < m_Data->ImageCount; i++)
		{
			std::array attachments = {
				m_Data->ColorImageView,
				m_Data->DepthImageView,
				m_Data->ImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_Data->RenderPass;
			framebufferInfo.attachmentCount = (uint32_t)attachments.size();
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = m_Data->Extent.width;
			framebufferInfo.height = m_Data->Extent.height;
			framebufferInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(vkd.Device, &framebufferInfo, vkd.Allocator, &m_Data->Framebuffers[i]);
			VK_CHECK(result, "Failed to create Vulkan framebuffer!");
		}
	}

	void VulkanSwapchain::RecreateSwapchain(void* semaphore)
	{
		auto& vkd = m_Renderer->GetVulkanData();
		Window& window = m_Renderer->GetWindow();

		int width = 0, height = 0;
		glfwGetFramebufferSize(window.GetHandle(), &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window.GetHandle(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(vkd.Device);

		VkSwapchainKHR oldSwapchain = m_Data->Swapchain;
		std::vector<VkImageView> oldImageViews = m_Data->ImageViews;
		std::vector<VkFramebuffer> oldFramebuffers = m_Data->Framebuffers;
		VkImage oldDepthImage = m_Data->DepthImage;
		VkDeviceMemory oldDepthImageMemory = m_Data->DepthImageMemory;
		VkImageView oldDepthImageView = m_Data->DepthImageView;
		VkImage oldColorImage = m_Data->ColorImage;
		VkDeviceMemory oldColorImageMemory = m_Data->ColorImageMemory;
		VkImageView oldColorImageView = m_Data->ColorImageView;

		CreateSwapchain(oldSwapchain);
		CreateColorResources();
		CreateFramebuffers();

		m_Renderer->SubmitResourceFree([semaphore](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			for (size_t i = 0; i < vkd.FrameSemaphores[vkd.CurrentFrameIndex].size(); i++)
			{
				if (vkd.FrameSemaphores[vkd.CurrentFrameIndex][i] == (VkSemaphore)semaphore || semaphore == nullptr)
				{
					vkDestroySemaphore(vkd.Device, vkd.FrameSemaphores[vkd.CurrentFrameIndex][i], vkd.Allocator);

					VkSemaphoreCreateInfo semaphoreInfo{};
					semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

					VkResult result = vkCreateSemaphore(vkd.Device, &semaphoreInfo, vkd.Allocator, &vkd.FrameSemaphores[vkd.CurrentFrameIndex][i]);
					VK_CHECK(result, "Failed to create Vulkan semaphore!");
					if (semaphore)
						break;
				}
			}

			vkd.InUseSemaphores[vkd.CurrentFrameIndex].clear();
		});

		m_Renderer->SubmitResourceFree(
		[oldSwapchain, oldImageViews, oldFramebuffers, oldDepthImage,
		oldDepthImageMemory, oldDepthImageView, oldColorImage,
		oldColorImageMemory, oldColorImageView](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroyImageView(vkd.Device, oldColorImageView, vkd.Allocator);
			vkDestroyImage(vkd.Device, oldColorImage, vkd.Allocator);
			vkFreeMemory(vkd.Device, oldColorImageMemory, vkd.Allocator);

			vkDestroyImageView(vkd.Device, oldDepthImageView, vkd.Allocator);
			vkDestroyImage(vkd.Device, oldDepthImage, vkd.Allocator);
			vkFreeMemory(vkd.Device, oldDepthImageMemory, vkd.Allocator);

			for (VkFramebuffer framebuffer : oldFramebuffers)
				vkDestroyFramebuffer(vkd.Device, framebuffer, vkd.Allocator);
			for (VkImageView view : oldImageViews)
				vkDestroyImageView(vkd.Device, view, vkd.Allocator);

			vkDestroySwapchainKHR(vkd.Device, oldSwapchain, vkd.Allocator);
		});
	}

}
