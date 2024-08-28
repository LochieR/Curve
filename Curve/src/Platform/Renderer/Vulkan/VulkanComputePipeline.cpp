#include "cvpch.h"
#include "VulkanComputePipeline.h"

#include "VulkanData.h"

namespace cv {

	namespace Utils {

		VkDescriptorType GetVkDescriptorTypeFromWireResourceType(ShaderResourceType type);
		VkShaderStageFlags GetVkShaderStageFromWireStage(ShaderStage stage);

	}

	VulkanComputePipeline::VulkanComputePipeline(VulkanRenderer* renderer, Shader* shader, const InputLayout& layout)
		: m_Renderer(renderer)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		m_Data = new PipelineData();

		CV_ASSERT(shader->IsCompute());
		auto& sd = shader->GetNativeData<ShaderData>();

		std::vector<VkDescriptorSetLayoutBinding> bindings;

		for (const auto& shaderResource : layout.ShaderResources)
		{
			VkDescriptorType descriptorType = Utils::GetVkDescriptorTypeFromWireResourceType(shaderResource.ResourceType);

			VkDescriptorSetLayoutBinding binding{};
			binding.binding = shaderResource.Binding;
			binding.descriptorType = descriptorType;
			binding.descriptorCount = shaderResource.ResourceCount;
			binding.stageFlags = Utils::GetVkShaderStageFromWireStage(shaderResource.Stage);
			binding.pImmutableSamplers = nullptr;

			bindings.push_back(binding);
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = (uint32_t)bindings.size();
		layoutInfo.pBindings = bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(vkd.Device, &layoutInfo, vkd.Allocator, &m_Data->SetLayout);
		VK_CHECK(result, "Failed to create Vulkan descriptor set layout!");

		std::vector<VkPushConstantRange> pushConstantRanges;
		
		for (const auto& pushConstant : layout.PushConstants)
		{
			VkPushConstantRange range{};
			range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			range.size = pushConstant.Size;
			range.offset = pushConstant.Offset;

			pushConstantRanges.push_back(range);
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_Data->SetLayout;
		
		result = vkCreatePipelineLayout(vkd.Device, &pipelineLayoutInfo, vkd.Allocator, &m_Data->PipelineLayout);
		VK_CHECK(result, "Failed to create Vulkan pipeline layout!");

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vkd.DescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_Data->SetLayout;

		result = vkAllocateDescriptorSets(vkd.Device, &allocInfo, &m_Data->DescriptorSet);
		VK_CHECK(result, "Failed to allocate Vulkan descriptor sets!");

		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStageInfo.module = sd.ComputeModule;
		shaderStageInfo.pName = "main";
		
		VkComputePipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		createInfo.layout = m_Data->PipelineLayout;
		createInfo.stage = shaderStageInfo;
		
		result = vkCreateComputePipelines(vkd.Device, nullptr, 1, &createInfo, vkd.Allocator, &m_Data->Pipeline);
		VK_CHECK(result, "Failed to create Vulkan compute pipeline!");
	}

	VulkanComputePipeline::~VulkanComputePipeline()
	{
		m_Renderer->SubmitResourceFree([pd = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroyPipeline(vkd.Device, pd->Pipeline, vkd.Allocator);
			vkDestroyPipelineLayout(vkd.Device, pd->PipelineLayout, vkd.Allocator);
			vkDestroyDescriptorSetLayout(vkd.Device, pd->SetLayout, vkd.Allocator);
		});
	}

	void VulkanComputePipeline::Bind(CommandBuffer commandBuffer) const
	{
		vkCmdBindPipeline(commandBuffer.As<VkCommandBuffer>(), VK_PIPELINE_BIND_POINT_COMPUTE, m_Data->Pipeline);
	}

	void VulkanComputePipeline::PushConstants(CommandBuffer commandBuffer, size_t size, const void* data, size_t offset)
	{
		vkCmdPushConstants(commandBuffer.As<VkCommandBuffer>(), m_Data->PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, (uint32_t)offset, (uint32_t)size, data);
	}

	void VulkanComputePipeline::UpdateDescriptor(BufferBase* buffer, uint32_t binding, uint32_t index)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = ((BufferData*)buffer->GetNativeData())->Buffer;
		bufferInfo.range = buffer->GetSize();
		bufferInfo.offset = 0;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_Data->DescriptorSet;
		write.dstBinding = binding;
		write.dstArrayElement = index;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(vkd.Device, 1, &write, 0, nullptr);
	}

	void VulkanComputePipeline::BindDescriptor(CommandBuffer commandBuffer) const
	{
		vkCmdBindDescriptorSets(
			commandBuffer.As<VkCommandBuffer>(),
			VK_PIPELINE_BIND_POINT_COMPUTE,
			m_Data->PipelineLayout,
			0,
			1, &m_Data->DescriptorSet,
			0, nullptr
		);
	}

}
