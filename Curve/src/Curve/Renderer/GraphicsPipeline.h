#pragma once

#include "Shader.h"
#include "CommandBuffer.h"
#include "NativeRendererObject.h"

namespace cv {

	enum class PrimitiveTopology
	{
		TriangleList = 0,
		LineList,
		LineStrip
	};

	class GraphicsPipeline : public NativeRendererObject
	{
	public:
		virtual ~GraphicsPipeline() = default;

		virtual void Bind(CommandBuffer commandBuffer) const = 0;
		virtual void PushConstants(CommandBuffer commandBuffer, ShaderStage shaderStage, size_t size, const void* data, size_t offset = 0) = 0;
		virtual void SetLineWidth(CommandBuffer commandBuffer, float lineWidth) = 0;

		template<typename T>
		void PushConstants(CommandBuffer commandBuffer, ShaderStage shaderStage, const T& data, size_t offset = 0)
		{
			PushConstants(commandBuffer, shaderStage, sizeof(T), &data, offset);
		}
	};

}
