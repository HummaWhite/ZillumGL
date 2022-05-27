#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GLStateObject.h"
#include "Image.h"
#include "Buffer.h"

class Texture;
class Texture1D;
class Texture1DArray;
class Texture2D;
class Texture2DArray;
class Texture3D;
class TextureBuffered;

using TexturePtr = std::shared_ptr<Texture>;
using Texture2DPtr = std::shared_ptr<Texture2D>;
using Texture2DArrayPtr = std::shared_ptr<Texture2DArray>;
using TextureBufferedPtr = std::shared_ptr<TextureBuffered>;

enum class TextureType
{
	Dim1 = GL_TEXTURE_1D, Dim1Array = GL_TEXTURE_1D_ARRAY,
	Dim2 = GL_TEXTURE_2D, Dim2Array = GL_TEXTURE_2D_ARRAY,
	Dim3 = GL_TEXTURE_3D,
	Buffered = GL_TEXTURE_BUFFER
};

enum class TextureFilter
{
	Linear = GL_LINEAR, Nearest = GL_NEAREST
};

enum class TextureWrapping
{
	ClampToEdge = GL_CLAMP_TO_EDGE, Repeat = GL_REPEAT
};

enum class TextureFormat
{
	Col1Base = GL_RED,
	Col1x8 = GL_R8, Col1x8i = GL_R8I, Col1x8u = GL_R8UI,
	Col1x16 = GL_R16, Col1x16i = GL_R16I, Col1x16u = GL_R16UI, Col1x16f = GL_R16F,
	Col1x32i = GL_R32I, Col1x32u = GL_R32UI, Col1x32f = GL_R32F,

	Col2Base = GL_RG,
	Col2x8 = GL_RG8, Col2x8i = GL_RG8I, Col2x8u = GL_RG8UI,
	Col2x16 = GL_RG16, Col2x16i = GL_RG16I, Col2x16u = GL_RG16UI, Col2x16f = GL_RG16F,
	Col2x32i = GL_RG32I, Col2x32u = GL_RG32UI, Col2x32f = GL_RG32F,

	Col3Base = GL_RGB,
	Col3x8 = GL_RGB8, Col3x8i = GL_RGB8I, Col3x8u = GL_RGB8UI,
	Col3x12 = GL_RGB12,
	Col3x16 = GL_RGB16, Col3x16i = GL_RGB16I, Col3x16u = GL_RGB16UI, Col3x16f = GL_RGB16F,
	Col3x32i = GL_RGB32I, Col3x32u = GL_RGB32UI, Col3x32f = GL_RGB32F,

	Col4Base = GL_RGBA,
	Col4x8 = GL_RGBA8, Col4x8i = GL_RGBA8I, Col4x8u = GL_RGBA8UI,
	Col4x12 = GL_RGBA12,
	Col4x16 = GL_RGBA16, Col4x16i = GL_RGBA16I, Col4x16u = GL_RGBA16UI, Col4x16f = GL_RGBA16F,
	Col4x32i = GL_RGBA32I, Col4x32u = GL_RGBA32UI, Col4x32f = GL_RGBA32F
};

enum class TextureSourceFormat
{
	Col1f = GL_RED, Col1i = GL_RED_INTEGER,
	Col2f = GL_RG, Col2i = GL_RG_INTEGER,
	Col3f = GL_RGB, Col3i = GL_RGB_INTEGER,
	Col4f = GL_RGBA, Col4i = GL_RGBA_INTEGER,
	Depth = GL_DEPTH_COMPONENT,
	DepthStencil = GL_DEPTH_STENCIL
};

enum class ImageAccess
{
	ReadOnly = GL_READ_ONLY,
	WriteOnly = GL_WRITE_ONLY,
	ReadWrite = GL_READ_WRITE
};

class Texture :
	public GLStateObject
{
public:
	Texture(TextureFormat format, TextureType type);
	~Texture();

	void setFilter(TextureFilter filter);
	void setWrapping(TextureWrapping wrapping);
	void setFilterWrapping(TextureFilter filter, TextureWrapping wrapping);
	void clear(int level, TextureFormat format, DataType type, const void* data);
	void setZero(int level);

	TextureFormat format() const { return mFormat; }
	TextureType type() const { return mType; }

protected:
	TextureFormat mFormat;
	TextureType mType;
};

class Texture2D :
	public Texture
{
public:
	Texture2D(int width, int height, TextureFormat format);
	Texture2D(ImagePtr image, TextureFormat format);
	Texture2D(const File::path& path, TextureFormat format);
	Texture2D(TextureFormat format, int width, int height,
		TextureSourceFormat srcFormat, DataType srcType, const void* data);

	int width() const { return mWidth; }
	int height() const { return mHeight; }
	glm::ivec2 size() const { return { mWidth, mHeight }; }

	ImagePtr readFromDevice();

	static Texture2DPtr createEmpty(int width, int height, TextureFormat format);
	static Texture2DPtr createFromImage(ImagePtr image, TextureFormat format);
	static Texture2DPtr createFromFile(const File::path& path, TextureFormat format);
	static Texture2DPtr createFromMemory(TextureFormat format, int width, int height,
		TextureSourceFormat srcFormat, DataType srcType, const void* data);

private:
	void allocate(TextureFormat format, int width, int height,
		TextureSourceFormat srcFormat, DataType srcType, const void* data);

private:
	int mWidth, mHeight;
};

class Texture2DArray :
	public Texture
{
public:
	Texture2DArray(const std::vector<ImagePtr>& images, TextureFormat format);

	int maxWidth() const { return mMaxWidth; }
	int maxHeight() const { return mMaxHeight; }
	int numTextures() const { return mTexScales.size(); }
	std::vector<glm::vec2>& texScales() { return mTexScales; }
	glm::vec2& getTexScale(int index);

	static Texture2DArrayPtr createFromImages(const std::vector<ImagePtr>& images, TextureFormat format);

private:
	int mMaxWidth, mMaxHeight;
	std::vector<glm::vec2> mTexScales;
};

class TextureBuffered :
	public Texture
{
public:
	TextureBuffered(BufferPtr buffer, TextureFormat format);

	int64_t size() const { return mBuffer->size(); }
	BufferPtr buffer() const { return mBuffer; }

	void write(int64_t offset, int64_t size, const void* data);
	void read(int64_t offset, int64_t size, void* data);

	static TextureBufferedPtr createFromBuffer(BufferPtr buffer, TextureFormat format);

	template<typename T>
	static TextureBufferedPtr createFromVector(const std::vector<T>& data, TextureFormat format, BufferUsage usage = BufferUsage::StaticDraw)
	{
		auto buffer = Buffer::create(data.size() * sizeof(T), data.data(), usage);
		return createFromBuffer(buffer, format);
	}

	template<typename T>
	static TextureBufferedPtr createTyped(const void* data, size_t count, TextureFormat format, BufferUsage usage = BufferUsage::StaticDraw)
	{
		auto buffer = Buffer::create(count * sizeof(T), data, usage);
		return createFromBuffer(buffer, format);
	}

private:
	BufferPtr mBuffer;
};