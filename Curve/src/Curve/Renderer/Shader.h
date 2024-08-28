#pragma once

#include "NativeRendererObject.h"

#include <filesystem>

namespace cv {

	class Shader : public NativeRendererObject
	{
	public:
		virtual ~Shader() = default;

		virtual void Reload() = 0;

		virtual bool IsCompute() const = 0;
		virtual const std::filesystem::path& GetFilepath() const = 0;

		template<typename T>
		T& GetNativeData() { return *reinterpret_cast<T*>(((NativeRendererObject*)this)->GetNativeData()); }
		template<typename T>
		const T& GetNativeData() const { return *reinterpret_cast<T*>(((NativeRendererObject*)this)->GetNativeData()); }
	};

}
