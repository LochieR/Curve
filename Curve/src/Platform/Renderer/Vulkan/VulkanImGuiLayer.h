#pragma once

#include "VulkanRenderer.h"
#include "Curve/ImGui/ImGuiLayer.h"

namespace cv {

	struct ImGuiLayerData;

	class VulkanImGuiLayer : public ImGuiLayer
	{
	public:
		VulkanImGuiLayer(VulkanRenderer* renderer);
		virtual ~VulkanImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void Begin() override;
		virtual void End() override;
		virtual void UpdateViewports() override;

		void ConfigureStyle();
	private:
		void CreateVulkanObjects();
	private:
		VulkanRenderer* m_Renderer = nullptr;
		ImGuiLayerData* m_Data = nullptr;
	};

}
