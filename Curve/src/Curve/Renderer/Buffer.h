#pragma once

#include "CommandBuffer.h"
#include "Curve/Core/Base.h"

#include <cstdint>

namespace cv {

	enum class ShaderDataType
	{
		None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
	};

	static uint32_t ShaderDataTypeSize(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:    return 4;
			case ShaderDataType::Float2:   return 4 * 2;
			case ShaderDataType::Float3:   return 4 * 3;
			case ShaderDataType::Float4:   return 4 * 4;
			case ShaderDataType::Mat3:     return 4 * 3 * 3;
			case ShaderDataType::Mat4:     return 4 * 4 * 4;
			case ShaderDataType::Int:      return 4;
			case ShaderDataType::Int2:     return 4 * 2;
			case ShaderDataType::Int3:     return 4 * 3;
			case ShaderDataType::Int4:     return 4 * 4;
			case ShaderDataType::Bool:     return 1;
		}

		CV_ASSERT(false && "Unknown ShaderDataType!");
		return 0;
	}

	struct BufferElement
	{
		std::string Name;
		ShaderDataType Type;
		uint32_t Size;
		size_t Offset;
		bool Normalized;

		BufferElement() = default;

		BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
			: Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized)
		{
		}

		uint32_t GetComponentCount() const
		{
			switch (Type)
			{
				case ShaderDataType::Float:   return 1;
				case ShaderDataType::Float2:  return 2;
				case ShaderDataType::Float3:  return 3;
				case ShaderDataType::Float4:  return 4;
				case ShaderDataType::Mat3:    return 3;
				case ShaderDataType::Mat4:    return 4;
				case ShaderDataType::Int:     return 1;
				case ShaderDataType::Int2:    return 2;
				case ShaderDataType::Int3:    return 3;
				case ShaderDataType::Int4:    return 4;
				case ShaderDataType::Bool:    return 1;
			}

			CV_ASSERT(false && "Unknown ShaderDataType!");
			return 0;
		}
	};

	enum class ShaderStage : uint8_t
	{
		Vertex = 0,
		Fragment,
		Compute
	};

	struct PushConstantInfo
	{
		uint32_t Size;
		uint32_t Offset;
		ShaderStage Stage;
	};

	enum class ShaderResourceType
	{
		UniformBuffer,
		CombinedImageSampler,
		StorageBuffer
	};

	struct ShaderResourceInfo
	{
		ShaderResourceType ResourceType;
		uint32_t Binding;
		ShaderStage Stage;
		uint32_t ResourceCount = 1;
	};

	class BufferLayout
	{
	public:
		BufferLayout() {}

		BufferLayout(std::initializer_list<BufferElement> elements)
			: m_Elements(elements)
		{
			CalculateOffsetsAndStride();
		}

		uint32_t GetStride() const { return m_Stride; }
		const std::vector<BufferElement>& GetElements() const { return m_Elements; }

		std::vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
		std::vector<BufferElement>::iterator end() { return m_Elements.end(); }
		std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
		std::vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }
	private:
		void CalculateOffsetsAndStride()
		{
			size_t offset = 0;
			m_Stride = 0;
			for (auto& element : m_Elements)
			{
				element.Offset = offset;
				offset += element.Size;
				m_Stride += element.Size;
			}
		}
	private:
		std::vector<BufferElement> m_Elements;
		uint32_t m_Stride = 0;

		friend struct InputLayout;
	};

	struct InputLayout
	{
		BufferLayout VertexLayout;
		std::vector<PushConstantInfo> PushConstants;
		std::vector<ShaderResourceInfo> ShaderResources;

		void CalculateOffsets()
		{
			VertexLayout.CalculateOffsetsAndStride();

			size_t offset = 0;
			for (auto& pushConstant : PushConstants)
			{
				pushConstant.Offset = (uint32_t)offset;
				offset += pushConstant.Size;
			}
		}
	};

	enum BufferType
	{
		VertexBuffer = 1 << 0,
		IndexBuffer = 1 << 1,
		StorageBuffer = 1 << 2,
		StagingBuffer = 1 << 3
	};

	constexpr inline BufferType operator|(BufferType lhs, BufferType rhs)
	{
		return static_cast<BufferType>(
			static_cast<std::underlying_type<BufferType>::type>(lhs) |
			static_cast<std::underlying_type<BufferType>::type>(rhs)
		);
	}

	constexpr inline BufferType operator&(BufferType lhs, BufferType rhs)
	{
		return static_cast<BufferType>(
			static_cast<std::underlying_type<BufferType>::type>(lhs) &
			static_cast<std::underlying_type<BufferType>::type>(rhs)
		);
	}

	constexpr inline BufferType operator^(BufferType lhs, BufferType rhs)
	{
		return static_cast<BufferType>(
			static_cast<std::underlying_type<BufferType>::type>(lhs) ^
			static_cast<std::underlying_type<BufferType>::type>(rhs)
		);
	}

	constexpr inline BufferType operator~(BufferType type)
	{
		return static_cast<BufferType>(~static_cast<std::underlying_type<BufferType>::type>(type));
	}

	constexpr inline BufferType& operator|=(BufferType& lhs, BufferType rhs)
	{
		lhs = lhs | rhs;
		return lhs;
	}

	constexpr inline BufferType& operator&=(BufferType& lhs, BufferType rhs)
	{
		lhs = lhs & rhs;
		return lhs;
	}

	constexpr inline BufferType& operator^=(BufferType& lhs, BufferType rhs)
	{
		lhs = lhs ^ rhs;
		return lhs;
	}

	class BufferBase
	{
	public:
		virtual ~BufferBase() = default;

		virtual void Bind(CommandBuffer commandBuffer) const = 0;

		virtual void SetData(const void* data, size_t size) = 0;
		virtual void SetData(int data, size_t size) = 0;

		virtual void* Map(size_t size) = 0;
		virtual void Unmap() = 0;

		virtual size_t GetSize() const = 0;

		virtual void* GetNativeData() = 0;
		virtual const void* GetNativeData() const = 0;
	};

	template<BufferType Type>
	class Buffer
	{
	public:
		~Buffer() { delete m_Base; }

		void Bind(CommandBuffer commandBuffer) const { m_Base->Bind(commandBuffer); }

		void SetData(const void* data, size_t size) { m_Base->SetData(data, size); }
		void SetData(int data, size_t size) { m_Base->SetData(data, size); }

		void* Map(size_t size) { return m_Base->Map(size); }
		void Unmap() { m_Base->Unmap(); }

		size_t GetSize() const { return m_Base->GetSize(); }

		template<typename T>
		T& GetNativeData() { return *reinterpret_cast<T*>(GetNativeData()); }
		template<typename T>
		const T& GetNativeData() const { return *reinterpret_cast<const T*>(GetNativeData()); }

		void* GetNativeData() { return m_Base->GetNativeData(); }
		const void* GetNativeData() const { return m_Base->GetNativeData(); }

		BufferBase* GetBase() const { return m_Base; }
	private:
		Buffer(BufferBase* base)
			: m_Base(base)
		{
		}
	private:
		BufferBase* m_Base = nullptr;

		friend class Renderer;
	};

}
