#include "Application.h"

const char* DefaultScenePath = "res/scene.xml";

int main(int argc, char *argv[])
{
	const char* sceneFilePath = (argc == 1) ? DefaultScenePath : argv[1];
    Application::init("Zillum", sceneFilePath);
    return Application::run();
}