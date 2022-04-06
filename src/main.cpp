#include "Application.h"
#include "thirdparty/pugixml/pugixml.hpp"

const char* DefaultScenePath = "res/scene.xml";

std::pair<int, int> loadConfigFilmSize(const char* filePath)
{
	pugi::xml_document doc;
	doc.load_file(filePath);
	auto size = doc.child("scene").child("integrator").child("size");
	int width = size.attribute("width").as_int();
	int height = size.attribute("height").as_int();
	return { width, height };
}

int main(int argc, char *argv[])
{
	std::pair<int, int> size;
	const char* sceneFilePath = (argc == 1) ? DefaultScenePath : argv[1];
	auto [width, height] = loadConfigFilmSize(sceneFilePath);
    Application::init(width, height, "Zillum", sceneFilePath);
    return Application::run();
}
