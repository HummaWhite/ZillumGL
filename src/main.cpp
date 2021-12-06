#include "Application.h"
#include "PathTracer.h"
#include "thirdparty/pugixml/pugixml.hpp"

std::pair<int, int> loadConfigFilmSize()
{
	pugi::xml_document doc;
	doc.load_file(SceneConfigPath.c_str());
	auto size = doc.child("scene").child("integrator").child("size");
	int width = size.attribute("width").as_int();
	int height = size.attribute("height").as_int();
	return { width, height };
}

int main()
{
	auto [width, height] = loadConfigFilmSize();

    Application app(width, height, "Zillum");
	app.bindInitFunc(PathTracer::init);
	app.bindMainLoopFunc(PathTracer::mainLoop);

    return app.run();
}
