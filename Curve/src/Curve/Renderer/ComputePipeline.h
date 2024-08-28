#pragma once

#include "Shader.h"
#include "Buffer.h"
#include "CommandBuffer.h"

#include <type_traits>

namespace cv {

	class ComputePipeline
	{
	public:
		virtual ~ComputePipeline() = default;

		virtual void Bind(CommandBuffer commandBuffer) const = 0;
		virtual void PushConstants(CommandBuffer commandBuffer, size_t size, const void* data, size_t offset = 0) = 0;

		virtual void BindDescriptor(CommandBuffer commandBuffer) const = 0;

		template<typename T>
		void PushConstants(CommandBuffer commandBuffer, const T& data, size_t offset = 0)
		{
			PushConstants(commandBuffer, sizeof(T), &data, offset);
		}

		template<BufferType Type>
		std::enable_if_t<Type & StorageBuffer> UpdateDescriptor(Buffer<Type>* buffer, uint32_t binding, uint32_t index = 0)
		{
			UpdateDescriptor(buffer->GetBase(), binding, index);
		}
	private:
		virtual void UpdateDescriptor(BufferBase* buffer, uint32_t binding, uint32_t index) = 0;
	};

}
