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

		static VkBufferUsageFlags GetBufferUsage(BufferType type)
		{
			VkBufferUsageFlags flags = 0;

			if (type & VertexBuffer)
				flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			if (type & IndexBuffer)
				flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			if (type & StorageBuffer)
				flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			if (type & StagingBuffer)
				flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			return flags;
		}

		static bool NeedsStagingBuffer(BufferType type)
		{
			return (type & IndexBuffer) || (type & StorageBuffer);
		}

		static VkMemoryPropertyFlags GetBufferMemoryProperties(BufferType type)
		{
			VkMemoryPropertyFlags flags = 0;

			if (NeedsStagingBuffer(type))
				flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			else
				flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			return flags;
		}

	}

	VulkanBuffer::VulkanBuffer(VulkanRenderer* renderer, BufferType type, size_t size, const void* data)
		: m_Renderer(renderer), m_Type(type)
	{
		auto& vkd = renderer->GetVulkanData();

		m_Data = new BufferData();
		m_Data->Size = size;

		Utils::CreateBuffer(
			vkd.Device,
			vkd.PhysicalDevice,
			vkd.Allocator,
			size,
			Utils::GetBufferUsage(type),
			Utils::GetBufferMemoryProperties(type),
			m_Data->Buffer,
			m_Data->Memory
		);

		if (Utils::NeedsStagingBuffer(type))
		{
			m_StagingData = new BufferData();

			Utils::CreateBuffer(
				vkd.Device,
				vkd.PhysicalDevice,
				vkd.Allocator,
				size,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				m_StagingData->Buffer,
				m_StagingData->Memory
			);
		}

		if (data)
		{
			void* memory = Map(size);
			memcpy(memory, data, size);
			Unmap();

			if (Utils::NeedsStagingBuffer(type))
				Utils::CopyBuffer(m_Renderer, m_StagingData->Buffer, m_Data->Buffer, size);
		}
	}

	VulkanBuffer::~VulkanBuffer()
	{
		m_Renderer->SubmitResourceFree([bd = m_Data, sd = m_StagingData](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			vkDestroyBuffer(vkd.Device, bd->Buffer, vkd.Allocator);
			vkFreeMemory(vkd.Device, bd->Memory, vkd.Allocator);

			if (sd)
			{
				vkDestroyBuffer(vkd.Device, sd->Buffer, vkd.Allocator);
				vkFreeMemory(vkd.Device, sd->Memory, vkd.Allocator);
			}

			delete bd;
		});
	}

	void VulkanBuffer::Bind(CommandBuffer commandBuffer) const
	{
		if (m_Type & VertexBuffer)
		{
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(commandBuffer.As<VkCommandBuffer>(), 0, 1, &m_Data->Buffer, &offset);
		}
		if (m_Type & IndexBuffer)
		{
			vkCmdBindIndexBuffer(commandBuffer.As<VkCommandBuffer>(), m_Data->Buffer, 0, VK_INDEX_TYPE_UINT32);
		}
	}

	void VulkanBuffer::SetData(const void* data, size_t size)
	{
		void* memory = Map(size);
		memcpy(memory, data, size);
		Unmap();
	}

	void VulkanBuffer::SetData(int data, size_t size)
	{
		void* memory = Map(size);
		memset(memory, data, size);
		Unmap();
	}

	void* VulkanBuffer::Map(size_t size)
	{
		auto& vkd = m_Renderer->GetVulkanData();

		if (Utils::NeedsStagingBuffer(m_Type))
		{
			void* memory;
			vkMapMemory(vkd.Device, m_StagingData->Memory, 0, size, 0, &memory);
			return memory;
		}
		else
		{
			void* memory;
			vkMapMemory(vkd.Device, m_Data->Memory, 0, size, 0, &memory);
			return memory;
		}
	}

	void VulkanBuffer::Unmap()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		if (Utils::NeedsStagingBuffer(m_Type))
		{
			vkUnmapMemory(vkd.Device, m_StagingData->Memory);
			Utils::CopyBuffer(m_Renderer, m_StagingData->Buffer, m_Data->Buffer, m_Data->Size);
		}
		else
			vkUnmapMemory(vkd.Device, m_Data->Memory);
	}

	size_t VulkanBuffer::GetSize() const
	{
		return m_Data->Size;
	}

}
