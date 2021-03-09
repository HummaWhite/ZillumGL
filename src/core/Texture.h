#pragma once

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

#include "Buffer.h"

class Texture
{
public:
	Texture();
	~Texture();

	GLuint ID() const { return m_ID; }
	int width() const { return m_Width; }
	int height() const { return m_Height; }

	void generate2D(int width, int height, GLuint internalFormat);
	void generateDepth2D(int width, int height, GLuint format);

	void loadSingle(const std::string& filePath, GLuint internalFormat = GL_RGBA);
	void loadHDRPanorama(const std::string& filePath);

	void generateTexBuffer(const Buffer& buffer, GLenum format);

	void setFilter(GLuint filterType);
	void setWrapping(GLuint wrappingType);
	void setFilterAndWrapping(GLuint filterType, GLuint wrappingType);
	void setParameter(GLenum pname, GLint value);

	void generateMipmap();

	void loadData(GLuint internalFormat, int width, int height, GLuint sourceFormat, GLuint dataType, const void* data);

private:
	void allocate2D(GLuint internalFormat, int width, int height, GLuint sourceFormat, GLuint dataType, const void* data = nullptr);

private:
	GLuint m_ID;
	int m_Width, m_Height, m_BitsPerPixel;
};

class BufferTexture:
	public Texture
{
public:
	void allocate(int size, const void* data, GLenum format);

	int size() const { return buf.size(); }

private:
	Buffer buf;
};