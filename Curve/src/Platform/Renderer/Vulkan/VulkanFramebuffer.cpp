#include "cvpch.h"
#include "VulkanFramebuffer.h"

#include "VulkanData.h"

#include <backends/imgui_impl_vulkan.h>

namespace cv {

	namespace Utils {

		static VkFormat AttachmentFormatToVkFormat(AttachmentFormat format)
		{
			switch (format)
			{
				case AttachmentFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
				case AttachmentFormat::R32SInt: return VK_FORMAT_R32_SINT;
				case AttachmentFormat::R32UInt: return VK_FORMAT_R32_UINT;
				default:
					return (VkFormat)0;
			}
		}

		static VkSampleCountFlagBits GetMaxUsableSampleCount(VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			VkPhysicalDeviceProperties physicalDeviceProperties;
			vkGetPhysicalDeviceProperties(vkd.PhysicalDevice, &physicalDeviceProperties);

			VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
			if (counts & VK_SAMPLE_COUNT_64_BIT)
				return VK_SAMPLE_COUNT_64_BIT;
			if (counts & VK_SAMPLE_COUNT_32_BIT)
				return VK_SAMPLE_COUNT_32_BIT;
			if (counts & VK_SAMPLE_COUNT_16_BIT)
				return VK_SAMPLE_COUNT_16_BIT;
			if (counts & VK_SAMPLE_COUNT_8_BIT)
				return VK_SAMPLE_COUNT_8_BIT;
			if (counts & VK_SAMPLE_COUNT_4_BIT)
				return VK_SAMPLE_COUNT_4_BIT;
			if (counts & VK_SAMPLE_COUNT_2_BIT)
				return VK_SAMPLE_COUNT_2_BIT;

			return VK_SAMPLE_COUNT_1_BIT;
		}

	}

	VulkanFramebuffer::VulkanFramebuffer(VulkanRenderer* renderer, const FramebufferSpecification& spec)
		: m_Renderer(renderer), m_Specification(spec)
	{
		m_Data = new FramebufferData();

		auto& vkd = renderer->GetVulkanData();

		Swapchain* swapchain = renderer->GetSwapchain();
		m_Data->ImageCount = swapchain->GetImageCount();

		if (spec.Multisample)
			m_Data->MSAASampleCount = Utils::GetMaxUsableSampleCount(renderer);
		else
			m_Data->MSAASampleCount = VK_SAMPLE_COUNT_1_BIT;

		m_Data->Images.resize(m_Data->ImageCount);
		m_Data->ImageMemorys.resize(m_Data->ImageCount);
		m_Data->ImageViews.resize(m_Data->ImageCount);
		m_Data->Framebuffers.resize(m_Data->ImageCount);
		m_Data->Descriptors.resize(m_Data->ImageCount);

		uint32_t attachmentCount = 0;
		for (uint32_t i = 0; i < spec.Attachments.size(); i++)
		{
			if (i == 0)
				continue;
			if (spec.Attachments[i] == AttachmentFormat::Depth)
				continue;
			attachmentCount++;
		}

		m_Data->AttachmentImages.resize((size_t)attachmentCount);
		m_Data->AttachmentImageMemorys.resize((size_t)attachmentCount);
		m_Data->AttachmentImageViews.resize((size_t)attachmentCount);
		for (uint32_t i = 0; i < m_Data->AttachmentImages.size(); i++)
		{
			m_Data->AttachmentImages[i].resize(m_Data->ImageCount);
			m_Data->AttachmentImageMemorys[i].resize(m_Data->ImageCount);
			m_Data->AttachmentImageViews[i].resize(m_Data->ImageCount);
		}

		for (uint32_t i = 0; i < m_Data->ImageCount; i++)
		{
			uint32_t j = 0, k = 0;
			for (AttachmentFormat attachmentFormat : spec.Attachments)
			{
				VkFormat format = Utils::AttachmentFormatToVkFormat(attachmentFormat);
				if (format == (VkFormat)0 && attachmentFormat != AttachmentFormat::Depth)
				{
					format = swapchain->GetNativeData<SwapchainData>().ImageFormat;
				}
				else if (attachmentFormat == AttachmentFormat::Depth)
				{
					format = Utils::FindDepthFormat(vkd.PhysicalDevice);

					if (m_Data->DepthImage)
					{
						j++;
						continue;
					}

					Utils::CreateImage(
						vkd.Device,
						vkd.PhysicalDevice,
						vkd.Allocator,
						spec.Width,
						spec.Height,
						format,
						VK_IMAGE_TILING_OPTIMAL,
						m_Data->MSAASampleCount,
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						m_Data->DepthImage,
						m_Data->DepthImageMemory
					);

					m_Data->DepthImageView = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->DepthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT);
					Utils::TransitionImageLayout(renderer, m_Data->DepthImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

					j++;
					continue;
				}

				if (j == 0)
				{
					Utils::CreateImage(
						vkd.Device,
						vkd.PhysicalDevice,
						vkd.Allocator,
						spec.Width, spec.Height,
						format,
						VK_IMAGE_TILING_OPTIMAL,
						VK_SAMPLE_COUNT_1_BIT,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						m_Data->Images[i],
						m_Data->ImageMemorys[i]
					);

					m_Data->ImageViews[i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->Images[i], swapchain->GetNativeData<SwapchainData>().ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
				}
				else
				{
					Utils::CreateImage(
						vkd.Device,
						vkd.PhysicalDevice,
						vkd.Allocator,
						spec.Width,
						spec.Height,
						format,
						VK_IMAGE_TILING_OPTIMAL,
						VK_SAMPLE_COUNT_1_BIT,
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
						m_Data->AttachmentImages[k][i],
						m_Data->AttachmentImageMemorys[k][i]
					);
					m_Data->AttachmentImageViews[k][i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->AttachmentImages[k][i], format, VK_IMAGE_ASPECT_COLOR_BIT);
					k++;
				}
				j++;
			}
		}

		{
			std::vector<VkAttachmentDescription> attachments;
			std::vector<VkAttachmentReference> colorRefs;
			VkAttachmentReference depthRef{};

			for (uint32_t i = 0; i < spec.Attachments.size(); i++)
			{
				AttachmentFormat attachmentFormat = spec.Attachments[i];

				bool depth = false;

				VkFormat format = Utils::AttachmentFormatToVkFormat(attachmentFormat);
				if (format == (VkFormat)0 && attachmentFormat != AttachmentFormat::Depth)
				{
					format = swapchain->GetNativeData<SwapchainData>().ImageFormat;
				}
				else if (attachmentFormat == AttachmentFormat::Depth)
				{
					format = Utils::FindDepthFormat(vkd.PhysicalDevice);
					depth = true;
				}

				VkAttachmentDescription attachment{};
				attachment.format = format;
				attachment.samples = m_Data->MSAASampleCount;
				attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				if (i == 0)
					attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				else if (depth)
					attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				else
					attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				attachments.push_back(attachment);

				VkAttachmentReference attachmentRef{};
				attachmentRef.attachment = i;
				attachmentRef.layout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				if (!depth)
					colorRefs.push_back(attachmentRef);
				else
					depthRef = attachmentRef;
			}

			std::vector<VkAttachmentReference> resolveRefs;
			if (spec.Multisample)
			{
				for (uint32_t i = 0; i < colorRefs.size(); i++)
				{
					uint32_t attachment = colorRefs[i].attachment;
					VkAttachmentDescription resolve{};
					resolve.format = attachments[attachment].format;
					resolve.samples = VK_SAMPLE_COUNT_1_BIT;
					resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					resolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					attachments.push_back(resolve);

					VkAttachmentReference resolveRef{};
					resolveRef.attachment = (uint32_t)attachments.size() - 1;
					resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					resolveRefs.push_back(resolveRef);
				}
			}

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = (uint32_t)colorRefs.size();
			subpass.pColorAttachments = colorRefs.data();
			subpass.pDepthStencilAttachment = &depthRef;

			if (spec.Multisample)
			{
				subpass.pResolveAttachments = resolveRefs.data();
			}

			std::array<VkSubpassDependency, 2> dependencies{};

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = (uint32_t)attachments.size();
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = (uint32_t)dependencies.size();
			renderPassInfo.pDependencies = dependencies.data();

			VkResult result = vkCreateRenderPass(vkd.Device, &renderPassInfo, vkd.Allocator, &m_Data->RenderPass);
			VK_CHECK(result, "Failed to create Vulkan render pass!");

			m_Data->ColorImages.resize(resolveRefs.size());
			m_Data->ColorImageMemorys.resize(resolveRefs.size());
			m_Data->ColorImageViews.resize(resolveRefs.size());
			for (uint32_t i = 0; i < resolveRefs.size(); i++)
			{
				uint32_t attachment = resolveRefs[i].attachment;
				VkFormat format = attachments[attachment].format;

				Utils::CreateImage(
					vkd.Device,
					vkd.PhysicalDevice,
					vkd.Allocator,
					spec.Width,
					spec.Height,
					format,
					VK_IMAGE_TILING_OPTIMAL,
					m_Data->MSAASampleCount,
					VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					m_Data->ColorImages[i],
					m_Data->ColorImageMemorys[i]
				);

				m_Data->ColorImageViews[i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->ColorImages[i], format, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}

		for (uint32_t i = 0; i < m_Data->ImageCount; i++)
		{
			std::vector<VkImageView> toAdd;
			std::vector<VkImageView> attachments;

			if (spec.Multisample)
			{
				attachments.push_back(m_Data->ColorImageViews[0]);
				toAdd.push_back(m_Data->ImageViews[i]);
			}
			else
				attachments.push_back(m_Data->ImageViews[i]);

			uint32_t nonDepthAttachmentIndex = 0;
			for (uint32_t attachmentIndex = 1; attachmentIndex < spec.Attachments.size(); attachmentIndex++)
			{
				if (spec.Attachments[attachmentIndex] == AttachmentFormat::Depth)
				{
					attachments.push_back(m_Data->DepthImageView);
				}
				else
				{
					if (spec.Multisample)
					{
						attachments.push_back(m_Data->ColorImageViews[nonDepthAttachmentIndex + 1]);
						toAdd.push_back(m_Data->AttachmentImageViews[nonDepthAttachmentIndex][i]);
					}
					else
						attachments.push_back(m_Data->AttachmentImageViews[nonDepthAttachmentIndex][i]);
					nonDepthAttachmentIndex++;
				}
			}

			for (VkImageView view : toAdd)
				attachments.push_back(view);

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = m_Data->RenderPass;
			createInfo.attachmentCount = (uint32_t)attachments.size();
			createInfo.pAttachments = attachments.data();
			createInfo.width = spec.Width;
			createInfo.height = spec.Height;
			createInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(vkd.Device, &createInfo, vkd.Allocator, &m_Data->Framebuffers[i]);
			VK_CHECK(result, "Failed to create Vulkan framebuffer!");
		}

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(vkd.PhysicalDevice, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		VkResult result = vkCreateSampler(vkd.Device, &samplerInfo, vkd.Allocator, &m_Data->Sampler);
		VK_CHECK(result, "Failed to create Vulkan sampler!");

		for (uint32_t i = 0; i < m_Data->ImageCount; i++)
		{
			m_Data->Descriptors[i] = ImGui_ImplVulkan_AddTexture(m_Data->Sampler, m_Data->ImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		m_Data->ClearValues.push_back(VkClearValue{ .color = { 0.0f, 0.0f, 0.0f, 1.0f} }); // main attachment
		for (size_t i = 0; i < m_Data->AttachmentImages.size(); i++)
			m_Data->ClearValues.push_back(VkClearValue{ .color = { 0.0f, 0.0f, 0.0f, 0.0f } }); // extra color attachments
		m_Data->ClearValues.push_back(VkClearValue{ .depthStencil = { 1.0f, 0 } }); // depth attachment
	}

	VulkanFramebuffer::~VulkanFramebuffer()
	{
		m_Renderer->SubmitResourceFree([fbd = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroySampler(vkd.Device, fbd->Sampler, vkd.Allocator);

			for (uint32_t i = 0; i < fbd->AttachmentImages.size(); i++)
			{
				for (uint32_t j = 0; j < fbd->ImageCount; j++)
				{
					vkDestroyImage(vkd.Device, fbd->AttachmentImages[i][j], vkd.Allocator);
					vkFreeMemory(vkd.Device, fbd->AttachmentImageMemorys[i][j], vkd.Allocator);
					vkDestroyImageView(vkd.Device, fbd->AttachmentImageViews[i][j], vkd.Allocator);
				}
			}

			for (uint32_t i = 0; i < fbd->ColorImages.size(); i++)
			{
				vkDestroyImageView(vkd.Device, fbd->ColorImageViews[i], vkd.Allocator);
				vkDestroyImage(vkd.Device, fbd->ColorImages[i], vkd.Allocator);
				vkFreeMemory(vkd.Device, fbd->ColorImageMemorys[i], vkd.Allocator);
			}

			vkDestroyImageView(vkd.Device, fbd->DepthImageView, vkd.Allocator);
			vkDestroyImage(vkd.Device, fbd->DepthImage, vkd.Allocator);
			vkFreeMemory(vkd.Device, fbd->DepthImageMemory, vkd.Allocator);

			for (uint32_t i = 0; i < fbd->ImageCount; i++)
			{
				vkDestroyFramebuffer(vkd.Device, fbd->Framebuffers[i], vkd.Allocator);
				vkDestroyImageView(vkd.Device, fbd->ImageViews[i], vkd.Allocator);
				vkDestroyImage(vkd.Device, fbd->Images[i], vkd.Allocator);
				vkFreeMemory(vkd.Device, fbd->ImageMemorys[i], vkd.Allocator);
			}

			vkDestroyRenderPass(vkd.Device, fbd->RenderPass, vkd.Allocator);

			delete fbd;
		});
	}

	void VulkanFramebuffer::BeginRenderPass(CommandBuffer commandBuffer)
	{
		auto& vkd = m_Renderer->GetVulkanData();
		m_Data->ImageIndex = m_Renderer->GetSwapchain()->GetImageIndex();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_Data->RenderPass;
		renderPassInfo.renderArea.extent = { m_Specification.Width, m_Specification.Height };
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.framebuffer = m_Data->Framebuffers[m_Data->ImageIndex];

		renderPassInfo.clearValueCount = (uint32_t)m_Data->ClearValues.size();
		renderPassInfo.pClearValues = m_Data->ClearValues.data();

		vkCmdBeginRenderPass(commandBuffer.As<VkCommandBuffer>(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void VulkanFramebuffer::EndRenderPass(CommandBuffer commandBuffer)
	{
		vkCmdEndRenderPass(commandBuffer.As<VkCommandBuffer>());
	}

	void VulkanFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		std::vector<VkFramebuffer> tempFramebuffers;
		for (VkFramebuffer framebuffer : m_Data->Framebuffers)
			tempFramebuffers.push_back(framebuffer);
		std::vector<VkImage> tempImages;
		for (VkImage image : m_Data->Images)
			tempImages.push_back(image);
		std::vector<VkImageView> tempImageViews;
		for (VkImageView imageView : m_Data->ImageViews)
			tempImageViews.push_back(imageView);
		std::vector<VkDeviceMemory> tempDeviceMemorys;
		for (VkDeviceMemory memory : m_Data->ImageMemorys)
			tempDeviceMemorys.push_back(memory);
		std::vector<VkDescriptorSet> tempDescriptors;
		for (VkDescriptorSet descriptor : m_Data->Descriptors)
			tempDescriptors.push_back(descriptor);
		std::vector<std::vector<VkImage>> tempAttachments;
		for (std::vector<VkImage> attachment : m_Data->AttachmentImages)
		{
			std::vector<VkImage> tempImageAttachment;
			for (VkImage image : attachment)
				tempImageAttachment.push_back(image);

			tempAttachments.push_back(tempImageAttachment);
		}
		std::vector<std::vector<VkDeviceMemory>> tempAttachmentMemorys;
		for (std::vector<VkDeviceMemory> attachment : m_Data->AttachmentImageMemorys)
		{
			std::vector<VkDeviceMemory> tempImageAttachment;
			for (VkDeviceMemory memory : attachment)
				tempImageAttachment.push_back(memory);

			tempAttachmentMemorys.push_back(tempImageAttachment);
		}
		std::vector<std::vector<VkImageView>> tempAttachmentViews;
		for (std::vector<VkImageView> attachment : m_Data->AttachmentImageViews)
		{
			std::vector<VkImageView> tempImageAttachment;
			for (VkImageView image : attachment)
				tempImageAttachment.push_back(image);

			tempAttachmentViews.push_back(tempImageAttachment);
		}
		std::vector<VkImage> tempColorImages;
		for (VkImage colorImage : m_Data->ColorImages)
			tempColorImages.push_back(colorImage);
		std::vector<VkDeviceMemory> tempColorMemorys;
		for (VkDeviceMemory memory : m_Data->ColorImageMemorys)
			tempColorMemorys.push_back(memory);
		std::vector<VkImageView> tempColorImageViews;
		for (VkImageView view : m_Data->ColorImageViews)
			tempColorImageViews.push_back(view);

		m_Renderer->SubmitResourceFree(
		[imageCount = m_Data->ImageCount, framebuffers = tempFramebuffers,
		images = tempImages, views = tempImageViews, memorys = tempDeviceMemorys, descriptors = tempDescriptors,
		depthImage = m_Data->DepthImage, depthView = m_Data->DepthImageView, depthMemory = m_Data->DepthImageMemory,
		colorImages = tempColorImages, colorViews = tempColorImageViews, colorMemorys = tempColorMemorys,
		attachments = tempAttachments, attachmentMemorys = tempAttachmentMemorys, attachmentViews = tempAttachmentViews](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDeviceWaitIdle(vkd.Device);

			for (uint32_t i = 0; i < attachments.size(); i++)
			{
				for (uint32_t j = 0; j < imageCount; j++)
				{
					vkDestroyImage(vkd.Device, attachments[i][j], vkd.Allocator);
					vkFreeMemory(vkd.Device, attachmentMemorys[i][j], vkd.Allocator);
					vkDestroyImageView(vkd.Device, attachmentViews[i][j], vkd.Allocator);
				}
			}

			for (uint32_t i = 0; i < colorImages.size(); i++)
			{
				vkDestroyImageView(vkd.Device, colorViews[i], vkd.Allocator);
				vkDestroyImage(vkd.Device, colorImages[i], vkd.Allocator);
				vkFreeMemory(vkd.Device, colorMemorys[i], vkd.Allocator);
			}

			vkDestroyImageView(vkd.Device, depthView, vkd.Allocator);
			vkDestroyImage(vkd.Device, depthImage, vkd.Allocator);
			vkFreeMemory(vkd.Device, depthMemory, vkd.Allocator);

			for (uint32_t i = 0; i < imageCount; i++)
			{
				vkDestroyFramebuffer(vkd.Device, framebuffers[i], vkd.Allocator);
				vkDestroyImageView(vkd.Device, views[i], vkd.Allocator);
				vkDestroyImage(vkd.Device, images[i], vkd.Allocator);
				vkFreeMemory(vkd.Device, memorys[i], vkd.Allocator);

				ImGui_ImplVulkan_RemoveTexture(descriptors[i]);
			}
		});

		auto& vkd = m_Renderer->GetVulkanData();

		m_Data->Framebuffers.clear();
		m_Data->ImageViews.clear();
		m_Data->Images.clear();
		m_Data->ImageMemorys.clear();
		m_Data->ColorImages.clear();
		m_Data->ColorImageViews.clear();
		m_Data->ColorImageMemorys.clear();
		m_Data->Descriptors.clear();
		m_Data->DepthImage = nullptr;
		m_Data->DepthImageView = nullptr;
		m_Data->DepthImageMemory = nullptr;

		Swapchain* swapchain = m_Renderer->GetSwapchain();

		m_Specification.Width = width;
		m_Specification.Height = height;

		m_Data->ImageCount = swapchain->GetImageCount();

		if (m_Specification.Multisample)
			m_Data->MSAASampleCount = Utils::GetMaxUsableSampleCount(m_Renderer);
		else
			m_Data->MSAASampleCount = VK_SAMPLE_COUNT_1_BIT;

		m_Data->Images.resize(m_Data->ImageCount);
		m_Data->ImageMemorys.resize(m_Data->ImageCount);
		m_Data->ImageViews.resize(m_Data->ImageCount);
		m_Data->Framebuffers.resize(m_Data->ImageCount);
		m_Data->Descriptors.resize(m_Data->ImageCount);

		uint32_t attachmentCount = 0;
		for (uint32_t i = 0; i < m_Specification.Attachments.size(); i++)
		{
			if (i == 0)
				continue;
			if (m_Specification.Attachments[i] == AttachmentFormat::Depth)
				continue;
			attachmentCount++;
		}

		m_Data->AttachmentImages.resize((size_t)attachmentCount);
		m_Data->AttachmentImageMemorys.resize((size_t)attachmentCount);
		m_Data->AttachmentImageViews.resize((size_t)attachmentCount);
		for (uint32_t i = 0; i < m_Data->AttachmentImages.size(); i++)
		{
			m_Data->AttachmentImages[i].resize(m_Data->ImageCount);
			m_Data->AttachmentImageMemorys[i].resize(m_Data->ImageCount);
			m_Data->AttachmentImageViews[i].resize(m_Data->ImageCount);
		}

		for (uint32_t i = 0; i < m_Data->ImageCount; i++)
		{
			uint32_t j = 0, k = 0;
			for (AttachmentFormat attachmentFormat : m_Specification.Attachments)
			{
				VkFormat format = Utils::AttachmentFormatToVkFormat(attachmentFormat);
				if (format == (VkFormat)0 && attachmentFormat != AttachmentFormat::Depth)
				{
					format = swapchain->GetNativeData<SwapchainData>().ImageFormat;
				}
				else if (attachmentFormat == AttachmentFormat::Depth)
				{
					format = Utils::FindDepthFormat(vkd.PhysicalDevice);

					if (m_Data->DepthImage)
					{
						j++;
						continue;
					}

					Utils::CreateImage(
						vkd.Device,
						vkd.PhysicalDevice,
						vkd.Allocator,
						m_Specification.Width,
						m_Specification.Height,
						format,
						VK_IMAGE_TILING_OPTIMAL,
						m_Data->MSAASampleCount,
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						m_Data->DepthImage,
						m_Data->DepthImageMemory
					);

					m_Data->DepthImageView = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->DepthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT);
					Utils::TransitionImageLayout(m_Renderer, m_Data->DepthImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

					j++;
					continue;
				}

				if (j == 0)
				{
					Utils::CreateImage(
						vkd.Device,
						vkd.PhysicalDevice,
						vkd.Allocator,
						m_Specification.Width, m_Specification.Height,
						format,
						VK_IMAGE_TILING_OPTIMAL,
						VK_SAMPLE_COUNT_1_BIT,
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						m_Data->Images[i],
						m_Data->ImageMemorys[i]
					);

					m_Data->ImageViews[i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->Images[i], swapchain->GetNativeData<SwapchainData>().ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
				}
				else
				{
					Utils::CreateImage(
						vkd.Device,
						vkd.PhysicalDevice,
						vkd.Allocator,
						m_Specification.Width,
						m_Specification.Height,
						format,
						VK_IMAGE_TILING_OPTIMAL,
						VK_SAMPLE_COUNT_1_BIT,
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
						m_Data->AttachmentImages[k][i],
						m_Data->AttachmentImageMemorys[k][i]
					);
					m_Data->AttachmentImageViews[k][i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->AttachmentImages[k][i], format, VK_IMAGE_ASPECT_COLOR_BIT);
					k++;
				}
				j++;
			}
		}

		{
			std::vector<VkAttachmentDescription> attachments;
			std::vector<VkAttachmentReference> colorRefs;
			VkAttachmentReference depthRef{};

			for (uint32_t i = 0; i < m_Specification.Attachments.size(); i++)
			{
				AttachmentFormat attachmentFormat = m_Specification.Attachments[i];

				bool depth = false;

				VkFormat format = Utils::AttachmentFormatToVkFormat(attachmentFormat);
				if (format == (VkFormat)0 && attachmentFormat != AttachmentFormat::Depth)
				{
					format = swapchain->GetNativeData<SwapchainData>().ImageFormat;
				}
				else if (attachmentFormat == AttachmentFormat::Depth)
				{
					format = Utils::FindDepthFormat(vkd.PhysicalDevice);
					depth = true;
				}

				VkAttachmentDescription attachment{};
				attachment.format = format;
				attachment.samples = m_Data->MSAASampleCount;
				attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				if (i == 0)
					attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				else if (depth)
					attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				else
					attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				attachments.push_back(attachment);

				VkAttachmentReference attachmentRef{};
				attachmentRef.attachment = i;
				attachmentRef.layout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				if (!depth)
					colorRefs.push_back(attachmentRef);
				else
					depthRef = attachmentRef;
			}

			std::vector<VkAttachmentReference> resolveRefs;
			if (m_Specification.Multisample)
			{
				for (uint32_t i = 0; i < colorRefs.size(); i++)
				{
					uint32_t attachment = colorRefs[i].attachment;
					VkAttachmentDescription resolve{};
					resolve.format = attachments[attachment].format;
					resolve.samples = VK_SAMPLE_COUNT_1_BIT;
					resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					resolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					attachments.push_back(resolve);

					VkAttachmentReference resolveRef{};
					resolveRef.attachment = (uint32_t)attachments.size() - 1;
					resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					resolveRefs.push_back(resolveRef);
				}
			}

			m_Data->ColorImages.resize(resolveRefs.size());
			m_Data->ColorImageMemorys.resize(resolveRefs.size());
			m_Data->ColorImageViews.resize(resolveRefs.size());
			for (uint32_t i = 0; i < resolveRefs.size(); i++)
			{
				uint32_t attachment = resolveRefs[i].attachment;
				VkFormat format = attachments[attachment].format;

				Utils::CreateImage(
					vkd.Device,
					vkd.PhysicalDevice,
					vkd.Allocator,
					m_Specification.Width,
					m_Specification.Height,
					format,
					VK_IMAGE_TILING_OPTIMAL,
					m_Data->MSAASampleCount,
					VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					m_Data->ColorImages[i],
					m_Data->ColorImageMemorys[i]
				);

				m_Data->ColorImageViews[i] = Utils::CreateImageView(vkd.Device, vkd.Allocator, m_Data->ColorImages[i], format, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}

		for (uint32_t i = 0; i < m_Data->ImageCount; i++)
		{
			std::vector<VkImageView> toAdd;
			std::vector<VkImageView> attachments;

			if (m_Specification.Multisample)
			{
				attachments.push_back(m_Data->ColorImageViews[0]);
				toAdd.push_back(m_Data->ImageViews[i]);
			}
			else
				attachments.push_back(m_Data->ImageViews[i]);

			uint32_t nonDepthAttachmentIndex = 0;
			for (uint32_t attachmentIndex = 1; attachmentIndex < m_Specification.Attachments.size(); attachmentIndex++)
			{
				if (m_Specification.Attachments[attachmentIndex] == AttachmentFormat::Depth)
				{
					attachments.push_back(m_Data->DepthImageView);
				}
				else
				{
					if (m_Specification.Multisample)
					{
						attachments.push_back(m_Data->ColorImageViews[nonDepthAttachmentIndex + 1]);
						toAdd.push_back(m_Data->AttachmentImageViews[nonDepthAttachmentIndex][i]);
					}
					else
						attachments.push_back(m_Data->AttachmentImageViews[nonDepthAttachmentIndex][i]);
					nonDepthAttachmentIndex++;
				}
			}

			for (VkImageView view : toAdd)
				attachments.push_back(view);

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = m_Data->RenderPass;
			createInfo.attachmentCount = (uint32_t)attachments.size();
			createInfo.pAttachments = attachments.data();
			createInfo.width = m_Specification.Width;
			createInfo.height = m_Specification.Height;
			createInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(vkd.Device, &createInfo, vkd.Allocator, &m_Data->Framebuffers[i]);
			VK_CHECK(result, "Failed to create Vulkan framebuffer!");
		}

		for (uint32_t i = 0; i < m_Data->ImageCount; i++)
		{
			m_Data->Descriptors[i] = ImGui_ImplVulkan_AddTexture(m_Data->Sampler, m_Data->ImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	uint32_t VulkanFramebuffer::GetColorAttachmentCount() const
	{
		uint32_t count = 0;

		for (AttachmentFormat format : m_Specification.Attachments)
		{
			if (format != AttachmentFormat::Depth)
				count++;
		}

		return count;
	}

	void VulkanFramebuffer::CopyAttachmentImageToBuffer(CommandBuffer commandBuffer, uint32_t attachmentIndex, Buffer<StagingBuffer>* buffer)
	{
		CV_ASSERT(attachmentIndex > 0 && "Can't copy main color attachment to buffer!");

		attachmentIndex--; // account for first color attachment

		if (!m_Specification.Multisample)
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_Data->AttachmentImages[attachmentIndex][m_Data->ImageIndex];
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer.As<VkCommandBuffer>(),
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
		else
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_Data->AttachmentImages[attachmentIndex][m_Data->ImageIndex];
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer.As<VkCommandBuffer>(),
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}

		VkImageSubresourceLayers subresource{};
		subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource.mipLevel = 0;
		subresource.baseArrayLayer = 0;
		subresource.layerCount = 1;

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource = subresource;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { m_Specification.Width, m_Specification.Height };
		region.imageExtent.depth = 1;

		vkCmdCopyImageToBuffer(
			commandBuffer.As<VkCommandBuffer>(),
			m_Data->AttachmentImages[attachmentIndex][m_Data->ImageIndex],
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffer->GetNativeData<BufferData>().Buffer,
			1, &region
		);
	}

	void VulkanFramebuffer::CopyAttachmentImageToBuffer(CommandBuffer commandBuffer, uint32_t attachmentIndex, Buffer<StagingBuffer>* buffer, const glm::vec2& pixelCoordinate)
	{
		CV_ASSERT(attachmentIndex > 0 && "Can't copy main color attachment to buffer!");

		attachmentIndex--; // account for first color attachment

		if (!m_Specification.Multisample)
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_Data->AttachmentImages[attachmentIndex][m_Data->ImageIndex];
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer.As<VkCommandBuffer>(),
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
		else
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = m_Data->AttachmentImages[attachmentIndex][m_Data->ImageIndex];
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer.As<VkCommandBuffer>(),
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}

		VkImageSubresourceLayers subresource{};
		subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource.mipLevel = 0;
		subresource.baseArrayLayer = 0;
		subresource.layerCount = 1;

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource = subresource;
		region.imageOffset = { (int)pixelCoordinate.x, (int)pixelCoordinate.y, 0};
		region.imageExtent = { 1, 1 };
		region.imageExtent.depth = 1;

		vkCmdCopyImageToBuffer(
			commandBuffer.As<VkCommandBuffer>(),
			m_Data->AttachmentImages[attachmentIndex][m_Data->ImageIndex],
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffer->GetNativeData<BufferData>().Buffer,
			1, &region
		);
	}

	void VulkanFramebuffer::CopyAttachmentImageToBuffer(uint32_t attachmentIndex, Buffer<StagingBuffer>* buffer)
	{
		CommandBuffer commandBuffer = m_Renderer->BeginSingleTimeCommands();	
		CopyAttachmentImageToBuffer(commandBuffer, attachmentIndex, buffer);
		m_Renderer->EndSingleTimeCommands(commandBuffer);
	}

	void* VulkanFramebuffer::GetCurrentDescriptor() const
	{
		return m_Data->Descriptors[m_Data->ImageIndex];
	}

	VkRenderPass VulkanFramebuffer::GetRenderPass() const
	{
		return m_Data->RenderPass;
	}

}
