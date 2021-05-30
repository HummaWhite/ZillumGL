#include "Texture.h"

Texture::Texture() :
	m_ID(0), m_Width(0), m_Height(0), m_BitsPerPixel(0)
{
}

Texture::~Texture()
{
	glDeleteTextures(1, &m_ID);
}

void Texture::generate2D(int width, int height, GLuint internalFormat)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(internalFormat, width, height, GL_RGB, GL_FLOAT);
}

void Texture::generateDepth2D(int width, int height, GLuint format)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(format, width, height, GL_DEPTH_COMPONENT, GL_FLOAT);
}

void Texture::loadSingle(const std::string& filePath, GLuint internalFormat)
{
	//stbi_set_flip_vertically_on_load(1);
	GLubyte* data = stbi_load(filePath.c_str(), &m_Width, &m_Height, &m_BitsPerPixel, 4);
	if (data == nullptr)
	{
		std::cout << "[Error]\t\tunable to load texture: " << filePath << std::endl;
		return;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(internalFormat, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, data);

	if (data != nullptr) stbi_image_free(data);
}

void Texture::loadHDRPanorama(const std::string& filePath)
{
	//stbi_set_flip_vertically_on_load(1);
	float* data = stbi_loadf(filePath.c_str(), &m_Width, &m_Height, &m_BitsPerPixel, 0);
	if (data == nullptr)
	{
		std::cout << "[Error]\t\tunable to load texture: " << filePath << std::endl;
		return;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(GL_RGB16F, m_Width, m_Height, GL_RGB, GL_FLOAT, data);

	if (data != nullptr) stbi_image_free(data);
}

void Texture::generateTexBuffer(const Buffer& buffer, GLenum format)
{
	glCreateTextures(GL_TEXTURE_BUFFER, 1, &m_ID);
	glTextureBuffer(m_ID, format, buffer.ID());
}

void Texture::setFilter(GLuint filterType)
{
	glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, filterType);
	glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, filterType);
}

void Texture::setWrapping(GLuint wrappingType)
{
	glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, wrappingType);
	glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, wrappingType);
}

void Texture::setFilterAndWrapping(GLuint filterType, GLuint wrappingType)
{
	setFilter(filterType);
	setWrapping(wrappingType);
}

void Texture::setParameter(GLenum pname, GLint value)
{
	glTextureParameteri(m_ID, pname, value);
}

void Texture::generateMipmap()
{
	glGenerateTextureMipmap(m_ID);
}

void Texture::loadData(GLuint internalFormat, int width, int height, GLuint sourceFormat, GLuint dataType, const void* data)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(internalFormat, width, height, sourceFormat, dataType, data);
}

void Texture::allocate2D(GLuint internalFormat, int width, int height, GLuint sourceFormat, GLuint dataType, const void* data)
{
	glTextureImage2DEXT(m_ID, GL_TEXTURE_2D, 0, internalFormat, width, height, 0, sourceFormat, dataType, data);
	setFilterAndWrapping(GL_LINEAR, GL_CLAMP_TO_EDGE);
}

void BufferTexture::allocate(int size, const void* data, GLenum format)
{
	buf.allocate(size, data, 0);
	generateTexBuffer(buf, format);
}

void BufferTexture::write(int offset, int size, const void* data)
{
	buf.write(offset, size, data);
}
