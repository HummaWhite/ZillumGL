#include "Application.h"

std::vector<GLFWmonitor*> Application::Monitors;

std::pair<int, int> getWindowSize(GLFWwindow* window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return { width, height };
}

Application::Application(int width, int height, const std::string& title)
{
	if (glfwInit() != GLFW_TRUE)
		Error::exit("failed to init GLFW");

	if (Monitors.empty())
	{
		int nMonitors;
		auto monitors = glfwGetMonitors(&nMonitors);
		Monitors.resize(nMonitors);
		std::copy(monitors, monitors + nMonitors, Monitors.begin());
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DECORATED, true);
	glfwWindowHint(GLFW_RESIZABLE, true);
	glfwWindowHint(GLFW_VISIBLE, false);

	mMainWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(mMainWindow);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		Error::exit("failed to init GLAD");
}

Application::~Application()
{
	if (mMainWindow)
		glfwDestroyWindow(mMainWindow);
}

int Application::run()
{
	auto [width, height] = getWindowSize(mMainWindow);
	mInitFunc(width, height, mMainWindow);

	glfwShowWindow(mMainWindow);
	while (true)
	{
		if (glfwWindowShouldClose(mMainWindow))
			return 0;

		mMainLoopFunc();

		glfwSwapBuffers(mMainWindow);
		glfwPollEvents();
	}
}
