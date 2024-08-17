#pragma once

#include "VulkanRenderer.h"
#include "Curve/Renderer/Shader.h"

namespace cv {

	struct ShaderData;

	class VulkanShader : public Shader
	{
	public:
		VulkanShader(VulkanRenderer* renderer, const std::filesystem::path& filepath);
		virtual ~VulkanShader();
		
		virtual void Reload() override;

		virtual const std::filesystem::path& GetFilepath() const override { return m_Filepath; }

		virtual void* GetNativeData() override { return m_Data; }
		virtual const void* GetNativeData() const override { return m_Data; }
	private:
		void CompileOrGetVulkanBinaries(const std::array<std::string, 2>& sources);
		void CreateShaderModules();

		void Reflect(const std::string& name, const std::vector<uint32_t>& data) const;

		static std::string ReadFile(const std::filesystem::path& filepath);
		static std::array<std::string, 2> PreProcess(const std::string& source);
	private:
		VulkanRenderer* m_Renderer = nullptr;
		std::filesystem::path m_Filepath;
		ShaderData* m_Data = nullptr;
	};

}
