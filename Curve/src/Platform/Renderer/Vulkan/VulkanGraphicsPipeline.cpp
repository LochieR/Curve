#include "cvpch.h"
#include "VulkanGraphicsPipeline.h"

#include "VulkanData.h"
#include "VulkanBuffer.h"

namespace cv {

	namespace Utils {

		VkVertexInputBindingDescription GetBindingDescription(const BufferLayout& layout);
		std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(const BufferLayout& layout);

		static VkShaderStageFlags GetVkShaderStageFromWireStage(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
				case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
			}

			CV_ASSERT(false);
			return 0;
		}

		static VkDescriptorType GetVkDescriptorTypeFromWireResourceType(ShaderResourceType type)
		{
			switch (type)
			{
				case ShaderResourceType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				case ShaderResourceType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				case ShaderResourceType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}

			CV_ASSERT(false);
			return (VkDescriptorType)-1;
		}

		static VkPrimitiveTopology GetVkPrimitiveTopologyFromWirePrimitiveTopology(PrimitiveTopology topology)
		{
			switch (topology)
			{
				case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				case PrimitiveTopology::LineList:	  return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				case PrimitiveTopology::LineStrip:    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			}

			CV_ASSERT(false);
			return (VkPrimitiveTopology)-1;
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

	VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanRenderer* renderer, Shader* shader, PrimitiveTopology topology, const InputLayout& layout)
		: m_Renderer(renderer)
	{
		m_Data = new GraphicsPipelineData();

		auto& sd = shader->GetNativeData<ShaderData>();
		auto& vkd = renderer->GetVulkanData();

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = sd.VertexModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = sd.FragmentModule;
		fragShaderStageInfo.pName = "main";

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		if (topology == PrimitiveTopology::LineList || topology == PrimitiveTopology::LineStrip)
		{
			dynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
		}

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkVertexInputBindingDescription bindingDescription = Utils::GetBindingDescription(layout.VertexLayout);
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions = Utils::GetAttributeDescriptions(layout.VertexLayout);

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size();
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = Utils::GetVkPrimitiveTopologyFromWirePrimitiveTopology(topology);
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkExtent2D swapchainExtent = vkd.Swapchain->GetNativeData<SwapchainData>().Extent;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapchainExtent.width;
		viewport.height = (float)swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = vkd.MultisampleCount;
		multisampling.sampleShadingEnable = VK_TRUE;
		multisampling.minSampleShading = 0.2f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {};
		depthStencil.back = {};

		const std::vector<PushConstantInfo>& pushConstantInfos = layout.PushConstants;
		std::vector<VkPushConstantRange> pushConstantRanges;

		for (const auto& pushConstant : pushConstantInfos)
		{
			VkPushConstantRange& range = pushConstantRanges.emplace_back();
			range.size = pushConstant.Size;
			range.offset = pushConstant.Offset;
			range.stageFlags = Utils::GetVkShaderStageFromWireStage(pushConstant.Stage);
		}

		const std::vector<ShaderResourceInfo>& shaderResourceInfos = layout.ShaderResources;
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		uint32_t i = 0;
		for (const auto& shaderResource : shaderResourceInfos)
		{
			VkDescriptorType descriptorType = Utils::GetVkDescriptorTypeFromWireResourceType(shaderResource.ResourceType);

			VkDescriptorSetLayoutBinding binding{};
			binding.binding = shaderResource.Binding;
			binding.descriptorType = descriptorType;
			binding.descriptorCount = shaderResource.ResourceCount;
			binding.stageFlags = Utils::GetVkShaderStageFromWireStage(shaderResource.Stage);
			binding.pImmutableSamplers = nullptr;

			bindings.push_back(binding);

			i++;
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)bindings.size();
		layoutInfo.pBindings = bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(vkd.Device, &layoutInfo, vkd.Allocator, &m_Data->SetLayout);
		VK_CHECK(result, "Failed to create Vulkan descriptor set layout!");

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vkd.DescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_Data->SetLayout;

		result = vkAllocateDescriptorSets(vkd.Device, &allocInfo, &m_Data->DescriptorSet);
		VK_CHECK(result, "Failed to allocate Vulkan descriptor sets!");

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_Data->SetLayout;
		pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

		result = vkCreatePipelineLayout(vkd.Device, &pipelineLayoutInfo, vkd.Allocator, &m_Data->PipelineLayout);
		VK_CHECK(result, "Failed to create Vulkan pipeline layout!");

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = (uint32_t)shaderStages.size();
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_Data->PipelineLayout;
		pipelineInfo.renderPass = vkd.Swapchain->GetNativeData<SwapchainData>().RenderPass;
		pipelineInfo.subpass = 0;

		result = vkCreateGraphicsPipelines(vkd.Device, nullptr, 1, &pipelineInfo, vkd.Allocator, &m_Data->Pipeline);
		VK_CHECK(result, "Failed to create Vulkan graphics pipeline!");
	}

	VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
	{
		m_Renderer->SubmitResourceFree([gpd = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroyPipeline(vkd.Device, gpd->Pipeline, vkd.Allocator);
			vkDestroyPipelineLayout(vkd.Device, gpd->PipelineLayout, vkd.Allocator);
			vkDestroyDescriptorSetLayout(vkd.Device, gpd->SetLayout, vkd.Allocator);
		});
	}

	void VulkanGraphicsPipeline::Bind(CommandBuffer commandBuffer) const
	{
		auto& scd = m_Renderer->GetVulkanData().Swapchain->GetNativeData<SwapchainData>();

		VkCommandBuffer cmd = commandBuffer.As<VkCommandBuffer>();

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Data->Pipeline);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)scd.Extent.width;
		viewport.height = (float)scd.Extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = scd.Extent;
		vkCmdSetScissor(cmd, 0, 1, &scissor);
	}

	void VulkanGraphicsPipeline::PushConstants(CommandBuffer commandBuffer, ShaderStage shaderStage, size_t size, const void* data, size_t offset)
	{
		vkCmdPushConstants(commandBuffer.As<VkCommandBuffer>(), m_Data->PipelineLayout, Utils::GetVkShaderStageFromWireStage(shaderStage), (uint32_t)offset, (uint32_t)size, data);
	}

	void VulkanGraphicsPipeline::SetLineWidth(CommandBuffer commandBuffer, float lineWidth)
	{
		vkCmdSetLineWidth(commandBuffer.As<VkCommandBuffer>(), lineWidth);
	}

}
