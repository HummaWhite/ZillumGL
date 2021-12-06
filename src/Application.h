#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <functional>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <random>
#include <ctime>
#include <map>

#include "util/Error.h"

class Application
{
public:
	using InitFunc = std::function<void(int, int, GLFWwindow*)>;
	using MainLoopFunc = std::function<void()>;

public:
	Application(int width, int height, const std::string& title);
	~Application();

	void bindInitFunc(InitFunc func) { mInitFunc = func; }
	void bindMainLoopFunc(MainLoopFunc func) { mMainLoopFunc = func; }

	int run();

private:
	GLFWwindow* mMainWindow = nullptr;

	InitFunc mInitFunc;
	MainLoopFunc mMainLoopFunc;

	static std::vector<GLFWmonitor*> Monitors;
};