#pragma once

#include "../stb_image/stb_image.h"
#include "../stb_image/stb_image_write.h"
#include "../util/File.h"

class Image;
using ImagePtr = std::shared_ptr<Image>;

enum class ImageDataType
{
	Int8, Float32
};

class Image
{
public:
	Image() = default;
	~Image();

	int width() const { return mWidth; }
	int height() const { return mHeight; }
	int channels() const { return mChannels; }
	ImageDataType dataType() const { return mDataType; }
	uint8_t* data() { return mData; }

	static ImagePtr createFromFile(const File::path& path, ImageDataType type, int channels = 3);
	static ImagePtr createEmpty(int width, int height, ImageDataType type, int channels = 3);

private:
	int mWidth, mHeight;
	int mChannels;
	ImageDataType mDataType;
	uint8_t* mData = nullptr;
};