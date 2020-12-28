#include "Texture.h"

Texture::Texture() :
	m_ID(0), m_Width(0), m_Height(0), m_BitsPerPixel(0), m_TextureType(0)
{
}

Texture::~Texture()
{
	glDeleteTextures(1, &m_ID);
}

void Texture::generate2D(int width, int height, GLuint internalFormat)
{
	m_TextureType = GL_TEXTURE_2D;

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(internalFormat, width, height, GL_RGB, GL_FLOAT);
}

void Texture::generateDepth2D(int width, int height, GLuint format)
{
	m_TextureType = GL_TEXTURE_2D;

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(format, width, height, GL_DEPTH_COMPONENT, GL_FLOAT);
}

void Texture::generateCube(int width, GLuint internalFormat)
{
	m_TextureType = GL_TEXTURE_CUBE_MAP;

	glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_ID);
	allocateCube(internalFormat, width, GL_RGB, GL_FLOAT);
}

void Texture::generateDepthCube(int width, GLuint format)
{
	m_TextureType = GL_TEXTURE_CUBE_MAP;

	glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_ID);
	allocateCube(format, width, GL_DEPTH_COMPONENT, GL_FLOAT);
}

void Texture::loadSingle(const std::string& filePath, GLuint internalFormat)
{
	m_TextureType = GL_TEXTURE_2D;

	//stbi_set_flip_vertically_on_load(1);
	GLubyte* data = stbi_load(filePath.c_str(), &m_Width, &m_Height, &m_BitsPerPixel, 4);
	if (data == nullptr)
	{
		std::cout << "Error: unable to load texture: " << filePath << std::endl;
		return;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(internalFormat, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, data);

	if (data != nullptr) stbi_image_free(data);
}

void Texture::loadFloat(const std::string& filePath)
{
	m_TextureType = GL_TEXTURE_2D;

	//stbi_set_flip_vertically_on_load(1);
	float* data = stbi_loadf(filePath.c_str(), &m_Width, &m_Height, &m_BitsPerPixel, 0);
	if (data == nullptr)
	{
		std::cout << "Error: unable to load texture: " << filePath << std::endl;
		return;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(GL_RGB16F, m_Width, m_Height, GL_RGB, GL_FLOAT, data);

	if (data != nullptr) stbi_image_free(data);
}

void Texture::loadCube(const std::vector<std::string>& filePaths, GLuint internalFormat)
{
	m_TextureType = GL_TEXTURE_CUBE_MAP;

	//stbi_set_flip_vertically_on_load(1);

	GLubyte* data[6];
	for (int i = 0; i < filePaths.size(); i++)
	{
		data[i] = stbi_load(filePaths[i].c_str(), &m_Width, &m_Height, &m_BitsPerPixel, 4);
		if (data == nullptr)
		{
			std::cout << "Error: unable to load texture: " << filePaths[i] << std::endl;
			return;
		}
	}

	for (int i = 0; i < filePaths.size(); i++)
	{
		glTextureImage2DEXT
		(
			m_ID, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0, internalFormat, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data
		);
		allocateCube(internalFormat, m_Width, GL_RGBA, GL_UNSIGNED_BYTE, data);
		stbi_image_free(data);
	}
}

void Texture::loadFloat(const float* data, int width, int height)
{
	m_TextureType = GL_TEXTURE_2D;

	if (data == nullptr)
	{
		std::cout << "Error: unable to load texture. " << std::endl;
		return;
	}

	m_Width = width, m_Height = height;

	glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
	allocate2D(GL_RGB16F, m_Width, m_Height, GL_RGB, GL_FLOAT, data);
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

	if (m_TextureType != GL_TEXTURE_CUBE_MAP) return;

	glTextureParameteri(m_ID, GL_TEXTURE_WRAP_R, wrappingType);
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

void Texture::bind(int slot) const
{
	//glBindTextureUnit(GL_TEXTURE0 + slot, m_ID);
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(m_TextureType, m_ID);
}

void Texture::unbind() const
{
	glBindTexture(m_TextureType, 0);
}

void Texture::allocate2D(GLuint internalFormat, int width, int height, GLuint sourceFormat, GLuint dataType, const void* data)
{
	glTextureImage2DEXT(m_ID, GL_TEXTURE_2D, 0, internalFormat, width, height, 0, sourceFormat, dataType, data);
}

void Texture::allocateCube(GLuint internalFormat, int width, GLuint sourceFormat, GLuint dataType, const void* data)
{
	for (int i = 0; i < 6; i++)
	{
		glTextureImage2DEXT
		(
			m_ID, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
			width, width, 0, sourceFormat, dataType, data
		);
	}
}
