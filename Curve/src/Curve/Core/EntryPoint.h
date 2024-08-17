#pragma once

#include "Application.h"

namespace cv {

	int Main(int argc, char** argv)
	{
		Log::Init();

		Application* app = CreateApplication(argc, argv);
		app->Run();
		delete app;

		return 0;
	}

}

#ifdef CV_DIST

#include <windows.h>

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	return cv::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return cv::Main(argc, argv);
}

#endif
