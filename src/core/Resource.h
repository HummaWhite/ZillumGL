#pragma once

#include <optional>
#include <stack>
#include <map>

#include "../util/NamespaceDecl.h"
#include "../util/File.h"
#include "Texture.h"
#include "Model.h"

class Resource
{
public:
	static ImagePtr getImageByIndex(int index);
	static ImagePtr getImageByPath(const File::path& path);
	static int addImage(const File::path& path);
	static std::vector<ImagePtr>& getAllImages() { return imagePool; }

private:
	static std::vector<ImagePtr> imagePool;
	static std::map<File::path, int> mapPathToImageIndex;
};