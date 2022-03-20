#include "Application.h"
#include "thirdparty/pugixml/pugixml.hpp"

const std::string DefaultScenePath = "res/scene.xml";

std::pair<int, int> loadConfigFilmSize()
{
	pugi::xml_document doc;
	doc.load_file(DefaultScenePath.c_str());
	auto size = doc.child("scene").child("integrator").child("size");
	int width = size.attribute("width").as_int();
	int height = size.attribute("height").as_int();
	return { width, height };
}

int main()
{
	auto [width, height] = loadConfigFilmSize();
    Application::init(width, height, "Zillum");
    return Application::run();
}
