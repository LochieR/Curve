#include "cvpch.h"
#include "VulkanBuffer.h"

#include "VulkanData.h"

namespace cv {

	namespace Utils {

		static VkFormat WireFormatToVkFormat(ShaderDataType type)
		{
			switch (type)
			{
				case ShaderDataType::Float: return VK_FORMAT_R32_SFLOAT;
				case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
				case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
				case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
				case ShaderDataType::Int: return VK_FORMAT_R32_SINT;
				case ShaderDataType::Int2: return VK_FORMAT_R32G32_SINT;
				case ShaderDataType::Int3: return VK_FORMAT_R32G32B32_SINT;
				case ShaderDataType::Int4: return VK_FORMAT_R32G32B32A32_SINT;
				case ShaderDataType::Mat3: return VK_FORMAT_R32G32B32_SFLOAT;
				case ShaderDataType::Mat4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			}

			CV_ASSERT(false);
			return (VkFormat)0;
		}

		VkVertexInputBindingDescription GetBindingDescription(const BufferLayout& layout)
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = layout.GetStride();
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(const BufferLayout& layout)
		{
			const std::vector<BufferElement>& elements = layout.GetElements();

			std::vector<VkVertexInputAttributeDescription> attributeDescriptions(elements.size());

			for (uint32_t i = 0; i < elements.size(); i++)
			{
				attributeDescriptions[i].binding = 0;
				attributeDescriptions[i].location = i;
				attributeDescriptions[i].format = WireFormatToVkFormat(elements[i].Type);
				attributeDescriptions[i].offset = (uint32_t)elements[i].Offset;
			}

			return attributeDescriptions;
		}

	}

	VulkanVertexBuffer::VulkanVertexBuffer(VulkanRenderer* renderer, size_t size)
		: m_Renderer(renderer)
	{
		auto& vkd = renderer->GetVulkanData();

		m_Data = new BufferData();

		Utils::CreateBuffer(
			vkd.Device,
			vkd.PhysicalDevice,
			vkd.Allocator,
			size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_Data->Buffer,
			m_Data->Memory
		);
		m_Data->Size = size;
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		m_Renderer->SubmitResourceFree([bd = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroyBuffer(vkd.Device, bd->Buffer, vkd.Allocator);
			vkFreeMemory(vkd.Device, bd->Memory, vkd.Allocator);

			delete bd;
		});
	}

	void VulkanVertexBuffer::Bind(CommandBuffer commandBuffer) const
	{
		size_t offset = 0;
		vkCmdBindVertexBuffers(commandBuffer.As<VkCommandBuffer>(), 0, 1, &m_Data->Buffer, &offset);
	}

	void VulkanVertexBuffer::SetData(const void* vertexData, uint32_t size)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		void* data;
		vkMapMemory(vkd.Device, m_Data->Memory, 0, size, 0, &data);
		memcpy(data, vertexData, (size_t)size);
		vkUnmapMemory(vkd.Device, m_Data->Memory);
	}

	VulkanIndexBuffer::VulkanIndexBuffer(VulkanRenderer* renderer, uint32_t* indices, uint32_t indexCount)
		: m_Renderer(renderer)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		m_Data = new BufferData();
		m_Data->Size = sizeof(uint32_t) * indexCount;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Utils::CreateBuffer(
			vkd.Device,
			vkd.PhysicalDevice,
			vkd.Allocator,
			m_Data->Size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory
		);

		void* data;
		vkMapMemory(vkd.Device, stagingBufferMemory, 0, m_Data->Size, 0, &data);
		memcpy(data, indices, m_Data->Size);
		vkUnmapMemory(vkd.Device, stagingBufferMemory);

		Utils::CreateBuffer(
			vkd.Device,
			vkd.PhysicalDevice,
			vkd.Allocator,
			m_Data->Size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Data->Buffer,
			m_Data->Memory
		);
		Utils::CopyBuffer(
			m_Renderer,
			stagingBuffer,
			m_Data->Buffer,
			m_Data->Size
		);

		vkDestroyBuffer(vkd.Device, stagingBuffer, vkd.Allocator);
		vkFreeMemory(vkd.Device, stagingBufferMemory, vkd.Allocator);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		m_Renderer->SubmitResourceFree([bd = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroyBuffer(vkd.Device, bd->Buffer, vkd.Allocator);
			vkFreeMemory(vkd.Device, bd->Memory, vkd.Allocator);

			delete bd;
		});
	}

	void VulkanIndexBuffer::Bind(CommandBuffer commandBuffer) const
	{
		vkCmdBindIndexBuffer(commandBuffer.As<VkCommandBuffer>(), m_Data->Buffer, 0, VK_INDEX_TYPE_UINT32);
	}

}
