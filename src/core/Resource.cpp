#include "Resource.h"
#include "../util/Error.h"

std::vector<ImagePtr> Resource::imagePool;
std::map<File::path, int> Resource::mapPathToImageIndex;

ImagePtr Resource::getImageByIndex(int index)
{
	Error::check(index < imagePool.size(), "Image index out of bound");
	return imagePool[index];
}

ImagePtr Resource::getImageByPath(const File::path& path)
{
	auto res = mapPathToImageIndex.find(path);
	if (res == mapPathToImageIndex.end())
		return nullptr;
	return getImageByIndex(res->second);
}

int Resource::addImage(const File::path& path)
{
	auto res = mapPathToImageIndex.find(path);
	if (res != mapPathToImageIndex.end())
		return res->second;
	mapPathToImageIndex[path] = imagePool.size();
	imagePool.push_back(Image::createFromFile(path, ImageDataType::Int8));
	return imagePool.size() - 1;
}
