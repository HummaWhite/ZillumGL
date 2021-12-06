#include "Image.h"
#include "../util/Error.h"

Image::~Image()
{
	if (mData != nullptr)
		delete[] mData;
}

ImagePtr Image::createFromFile(const File::path& path, ImageDataType type, int channels)
{
	auto image = std::make_shared<Image>();
	auto pathStr = path.generic_string();

	uint8_t* data = (type == ImageDataType::Int8) ?
		stbi_load(pathStr.c_str(), &image->mWidth, &image->mHeight, &image->mChannels, channels) :
		reinterpret_cast<uint8_t*>(
			stbi_loadf(pathStr.c_str(), &image->mWidth, &image->mHeight, &image->mChannels, channels)
		);

	if (data == nullptr)
	{
		Error::bracketLine<0>("Image Failed to load from file: " + path.generic_string());
		return nullptr;
	}
	image->mDataType = type;
	int bytesPerChannel = (type == ImageDataType::Int8) ? 1 : sizeof(float);
	size_t size = static_cast<size_t>(image->mWidth) * image->mHeight * channels * bytesPerChannel;

	image->mData = new uint8_t[size];
	memcpy(image->mData, data, size);
	stbi_image_free(data);
	return image;
}

ImagePtr Image::createEmpty(int width, int height, ImageDataType type, int channels)
{
	Error::check(channels <= 4 && channels >= 1, "Invalid image channel parameter");

	auto image = std::make_shared<Image>();
	int bytesPerChannel = (type == ImageDataType::Int8) ? 1 : sizeof(float);
	size_t size = static_cast<size_t>(width) * height * channels * bytesPerChannel;
	image->mWidth = width;
	image->mHeight = height;
	image->mChannels = channels;
	image->mDataType = type;
	image->mData = new uint8_t[size];
	return image;
}