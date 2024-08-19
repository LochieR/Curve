#include "View/ViewLayer.h"

#include <Curve/Core/EntryPoint.h>

cv::Application* cv::CreateApplication(int argc, char** argv)
{
	ApplicationSpecification spec{};
	spec.CommandLineArgs = { argc, argv };
	spec.WindowWidth = 1280;
	spec.WindowHeight = 720;
	spec.WindowTitle = "Curve View";
	spec.UseImGui = true;
	spec.UseDefaultTitlebar = true;

	Application* app = new Application(spec);
	app->PushLayer(new ViewLayer());

	return app;
}
