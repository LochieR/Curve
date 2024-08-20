#pragma once

#include "Curve/Core/Layer.h"

#include <imgui.h>

namespace cv {

	class ImGuiLayer : public Layer
	{
	public:
		virtual ~ImGuiLayer() = default;

		virtual void Begin() = 0;
		virtual void End() = 0;
		virtual void UpdateViewports() = 0;
	};

}
