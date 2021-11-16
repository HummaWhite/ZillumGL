#include "Texture.h"
#include "../util/Error.h"

std::pair<TextureSourceFormat, DataType> getImageFormat(ImagePtr image)
{
	bool integ = (image->dataType() == ImageDataType::Int8);
	auto srcType = integ ? DataType::U8 : DataType::F32;

	switch (image->channels())
	{
	case 1:
		return { integ ? TextureSourceFormat::Col1i : TextureSourceFormat::Col1f, srcType };
	case 2:
		return { integ ? TextureSourceFormat::Col2i : TextureSourceFormat::Col2f, srcType };
	case 3:
		return { integ ? TextureSourceFormat::Col3i : TextureSourceFormat::Col3f, srcType };
	case 4:
		return { integ ? TextureSourceFormat::Col4i : TextureSourceFormat::Col4f, srcType };
	default:
		break;
	}
	Error::impossiblePath();
	return { integ ? TextureSourceFormat::Col1i : TextureSourceFormat::Col1f, srcType };
}

Texture::Texture(TextureFormat format, TextureType type) :
	mFormat(format), mType(type), GLStateObject(GLStateObjectType::Texture)
{
	glCreateTextures(static_cast<GLenum>(type), 1, &mId);
}

Texture::~Texture()
{
	glDeleteTextures(1, &mId);
}

void Texture::setFilter(TextureFilter filter)
{
	glTextureParameteri(mId, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(filter));
	glTextureParameteri(mId, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(filter));
}

void Texture::setWrapping(TextureWrapping wrapping)
{
	glTextureParameteri(mId, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapping));
	glTextureParameteri(mId, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapping));
	//glTextureParameteri(mId, GL_TEXTURE_WRAP_R, static_cast<GLint>(wrapping));
}

void Texture::setFilterWrapping(TextureFilter filter, TextureWrapping wrapping)
{
	setFilter(filter);
	setWrapping(wrapping);
}

Texture2D::Texture2D(int width, int height, TextureFormat format) :
	mWidth(width), mHeight(height), Texture(format, TextureType::Dim2)
{
	allocate(format, width, height, TextureSourceFormat::Col3f, DataType::U8, nullptr);
}

Texture2D::Texture2D(ImagePtr image, TextureFormat format) :
	mWidth(image->width()), mHeight(image->height()), Texture(format, TextureType::Dim2)
{
	auto [srcFormat, srcType] = getImageFormat(image);
	allocate(format, mWidth, mHeight, srcFormat, srcType, image->data());
}

Texture2D::Texture2D(const File::path& path, TextureFormat format) :
	Texture(format, TextureType::Dim2)
{
	auto image = Image::createFromFile(path, ImageDataType::Float32);
	mWidth = image->width();
	mHeight = image->height();
	auto [srcFormat, srcType] = getImageFormat(image);
	allocate(format, mWidth, mHeight, srcFormat, srcType, image->data());
}

Texture2D::Texture2D(TextureFormat format, int width, int height,
	TextureSourceFormat srcFormat, DataType srcType, const void* data) :
	mWidth(width), mHeight(height), Texture(format, TextureType::Dim2)
{
	allocate(format, width, height, srcFormat, srcType, data);
}

Texture2DPtr Texture2D::createEmpty(int width, int height, TextureFormat format)
{
	return std::make_shared<Texture2D>(width, height, format);
}

Texture2DPtr Texture2D::createFromImage(ImagePtr image, TextureFormat format)
{
	return std::make_shared<Texture2D>(image, format);
}

Texture2DPtr Texture2D::createFromFile(const File::path& path, TextureFormat format)
{
	return std::make_shared<Texture2D>(path, format);
}

Texture2DPtr Texture2D::createFromMemory(TextureFormat format, int width, int height,
	TextureSourceFormat srcFormat, DataType srcType, const void* data)
{
	return std::make_shared<Texture2D>(format, width, height,
		srcFormat, srcType, data);
}

void Texture2D::allocate(TextureFormat format, int width, int height,
	TextureSourceFormat srcFormat, DataType srcType, const void* data)
{
	glTextureImage2DEXT(mId, GL_TEXTURE_2D, 0, static_cast<GLint>(format),
		width, height, 0, static_cast<GLenum>(srcFormat), static_cast<GLenum>(srcType), data);
	setFilterWrapping(TextureFilter::Linear, TextureWrapping::Repeat);
}

Texture2DArray::Texture2DArray(const std::vector<ImagePtr>& images, TextureFormat format) :
	Texture(format, TextureType::Dim2Array)
{
	mMaxWidth = 0, mMaxHeight = 0;
	for (const auto& img : images)
	{
		mMaxWidth = std::max(mMaxWidth, img->width());
		mMaxHeight = std::max(mMaxHeight, img->height());
	}
	
	glTextureImage3DEXT(mId, GL_TEXTURE_2D_ARRAY, 0, GL_SRGB,
		mMaxWidth, mMaxHeight, images.size(), 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	
	for (int i = 0; i < images.size(); i++)
	{
		const auto& img = images[i];
		glTextureSubImage3D(mId, 0, 0, 0, i,
		img->width(), img->height(), 1, GL_RGB, GL_UNSIGNED_BYTE, img->data());
	}
	glTextureParameteri(mId, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(mId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTextureParameteri(mId, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(mId, GL_TEXTURE_WRAP_T, GL_REPEAT);

	mTexScales.resize(images.size());
	for (int i = 0; i < images.size(); i++)
	{
		const auto& img = images[i];
		mTexScales[i] = glm::vec2(img->width(), img->height()) / glm::vec2(mMaxWidth, mMaxHeight);
	}
}

glm::vec2& Texture2DArray::getTexScale(int index)
{
	Error::check(index >= 0 && index < mTexScales.size(), "[Texture2DArray] index out of bound");
	return mTexScales[index];
}

Texture2DArrayPtr Texture2DArray::createFromImages(const std::vector<ImagePtr>& images, TextureFormat format)
{
	return std::make_shared<Texture2DArray>(images, format);
}

TextureBuffered::TextureBuffered(BufferPtr buffer, TextureFormat format) :
	mBuffer(buffer), Texture(format, TextureType::Buffered)
{
	Error::check(buffer != nullptr, "[TextureBuffered]\tnull buffer ptr");
	glCreateTextures(GL_TEXTURE_BUFFER, 1, &mId);
	glTextureBuffer(mId, static_cast<GLenum>(format), buffer->id());
}

void TextureBuffered::write(int64_t offset, int64_t size, const void* data)
{
	Error::check(offset + size <= mBuffer->size(), "[TextureBuffered]\twrite size > allocated");
	glNamedBufferSubData(mBuffer->id(), offset, size, data);
}

TextureBufferedPtr TextureBuffered::createFromBuffer(BufferPtr buffer, TextureFormat format)
{
	return std::make_shared<TextureBuffered>(buffer, format);
}
