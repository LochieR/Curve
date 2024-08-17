#include "cvpch.h"
#include "VulkanShader.h"

#include "VulkanData.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

namespace cv {

	namespace Utils {

		static const char* GetCacheDirectory()
		{
			return "assets/cache/shader";
		}

		static void CreateCacheDirectory()
		{
			std::string cacheDir = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDir))
				std::filesystem::create_directories(cacheDir);
		}

		static VkShaderStageFlags GetVkShaderStageFromCurveStage(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
				case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
			}

			CV_ASSERT(false && "Unknown shader stage!");
			return 0;
		}

	}

	VulkanShader::VulkanShader(VulkanRenderer* renderer, const std::filesystem::path& filepath)
		: m_Renderer(renderer), m_Filepath(filepath)
	{
		m_Data = new ShaderData();
		Reload();
	}

	VulkanShader::~VulkanShader()
	{
		m_Renderer->SubmitResourceFree([vertexModule = m_Data->VertexModule, fragmentModule = m_Data->FragmentModule, data = m_Data](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			if (vertexModule)
				vkDestroyShaderModule(vkd.Device, vertexModule, vkd.Allocator);
			if (fragmentModule)
				vkDestroyShaderModule(vkd.Device, fragmentModule, vkd.Allocator);
			
			delete data;
		});
	}

	void VulkanShader::Reload()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		m_Renderer->SubmitResourceFree([vertexModule = m_Data->VertexModule, fragmentModule = m_Data->FragmentModule](VulkanRenderer* renderer)
		{
			auto& vkd = renderer->GetVulkanData();

			if (vertexModule)
				vkDestroyShaderModule(vkd.Device, vertexModule, vkd.Allocator);
			if (fragmentModule)
				vkDestroyShaderModule(vkd.Device, fragmentModule, vkd.Allocator);
		});

		Utils::CreateCacheDirectory();

		std::string source = ReadFile(m_Filepath);
		std::array<std::string, 2> shaders = PreProcess(source);

		CompileOrGetVulkanBinaries(shaders);
		CreateShaderModules();
	}

	void VulkanShader::CompileOrGetVulkanBinaries(const std::array<std::string, 2>& sources)
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
		const bool debug = false;
		if (debug)
		{
			options.SetGenerateDebugInfo();
			options.SetOptimizationLevel(shaderc_optimization_level_zero);
		}
		else
		{
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
		}

		{ // vertex shader
			std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

			std::filesystem::path shaderFilePath = m_Filepath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + ".cached_vulkan.vert");

			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			if (in.is_open())
			{
				in.seekg(0, std::ios::end);
				auto size = in.tellg();
				in.seekg(0, std::ios::beg);

				m_Data->VertexData.resize(size / sizeof(uint32_t));
				in.read((char*)m_Data->VertexData.data(), size);
			}
			else
			{
				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(sources[0], shaderc_glsl_vertex_shader, shaderFilePath.string().c_str(), options);
				if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					std::cerr << module.GetErrorMessage() << std::endl;
					CV_ASSERT(false);
				}

				m_Data->VertexData = std::vector<uint32_t>(module.cbegin(), module.cend());

				std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
				if (out.is_open())
				{
					out.write((char*)m_Data->VertexData.data(), m_Data->VertexData.size() * sizeof(uint32_t));
					out.flush();
					out.close();
				}
			}
		}
		{ // fragment shader
			std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

			std::filesystem::path shaderFilePath = m_Filepath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + ".cached_vulkan.frag");

			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			if (in.is_open())
			{
				in.seekg(0, std::ios::end);
				auto size = in.tellg();
				in.seekg(0, std::ios::beg);

				m_Data->FragmentData.resize(size / sizeof(uint32_t));
				in.read((char*)m_Data->FragmentData.data(), size);
			}
			else
			{
				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(sources[1], shaderc_glsl_fragment_shader, shaderFilePath.string().c_str(), options);
				if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					std::cerr << module.GetErrorMessage() << std::endl;
					CV_ASSERT(false);
				}

				m_Data->FragmentData = std::vector<uint32_t>(module.cbegin(), module.cend());

				std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
				if (out.is_open())
				{
					out.write((char*)m_Data->FragmentData.data(), m_Data->FragmentData.size() * sizeof(uint32_t));
					out.flush();
					out.close();
				}
			}
		}

		Reflect("Vertex Shader", m_Data->VertexData);
		Reflect("Fragment Shader", m_Data->FragmentData);
	}

	void VulkanShader::CreateShaderModules()
	{
		auto& vkd = m_Renderer->GetVulkanData();

		{
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = 4 * m_Data->VertexData.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(m_Data->VertexData.data());

			VkResult result = vkCreateShaderModule(vkd.Device, &createInfo, vkd.Allocator, &m_Data->VertexModule);
			VK_CHECK(result, "Failed to create shader module!");
		}
		{
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = 4 * m_Data->FragmentData.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(m_Data->FragmentData.data());

			VkResult result = vkCreateShaderModule(vkd.Device, &createInfo, vkd.Allocator, &m_Data->FragmentModule);
			VK_CHECK(result, "Failed to create shader module!");
		}
	}

	void VulkanShader::Reflect(const std::string& name, const std::vector<uint32_t>& data) const
	{
		spirv_cross::Compiler compiler(data);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		CV_INFO("Shader::Reflect - ", name, " ", m_Filepath);
		CV_INFO("\t", resources.uniform_buffers.size(), " uniform buffer", (resources.uniform_buffers.size() == 1 ? "" : "s"));
		CV_INFO("\t", resources.sampled_images.size(), " resource", (resources.sampled_images.size() == 1 ? "" : "s"));

		if (resources.uniform_buffers.size() > 0)
			CV_INFO("Uniform buffers:");
		for (const auto& resource : resources.uniform_buffers)
		{
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(bufferType));
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = static_cast<int>(bufferType.member_types.size());

			CV_INFO("  ", resource.name);
			CV_INFO("\tSize = ", bufferSize);
			CV_INFO("\tBinding = ", binding);
			CV_INFO("\tMembers = ", memberCount);
		}

		if (resources.sampled_images.size() > 0)
			CV_INFO("Resources:");
		for (const auto& resource : resources.sampled_images)
		{
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			CV_INFO("\tID = ", resource.id);
			CV_INFO("\tBinding = ", binding);
		}
	}

	std::string VulkanShader::ReadFile(const std::filesystem::path& filepath)
	{
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);
		if (in)
		{
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			if (size != -1)
			{
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
			}
			else
			{
				CV_ERROR("Could not read from file '", filepath, "'");
			}
		}
		else
		{
			CV_ERROR("Could not open file '", filepath, "'");
		}

		return result;
	}

	std::array<std::string, 2> VulkanShader::PreProcess(const std::string& source)
	{
		std::array<std::string, 2> result;

		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		while (pos != std::string::npos)
		{
			size_t eol = source.find_first_of("\r\n", pos);
			CV_ASSERT(eol != std::string::npos);
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);

			CV_ASSERT(type == "vertex" || type == "fragment" || type == "pixel" && "Invalid shader source type!");

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			CV_ASSERT(nextLinePos != std::string::npos);
			pos = source.find(typeToken, nextLinePos);

			if (type == "vertex")
				result[0] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
			else if (type == "fragment" || type == "pixel")
				result[1] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		}

		return result;
	}

}
