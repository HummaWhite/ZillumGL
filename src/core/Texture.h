#pragma once

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "stb_image/stb_image.h"
#include "stb_image/stb_image_write.h"

class Texture
{
public:
	Texture();
	~Texture();

	GLuint ID() const { return m_ID; }
	GLuint type() const { return m_TextureType; }
	int width() const { return m_Width; }
	int height() const { return m_Height; }

	void generate2D(int width, int height, GLuint internalFormat);
	void generateDepth2D(int width, int height, GLuint format);
	void generateCube(int width, GLuint internalFormat);
	void generateDepthCube(int width, GLuint format);

	void loadSingle(const std::string& filePath, GLuint internalFormat = GL_RGBA);
	void loadFloat(const std::string& filePath);
	void loadCube(const std::vector<std::string>& filePaths, GLuint internalFormat = GL_RGBA);

	void loadFloat(const float* data, int width, int height);

	void setFilter(GLuint filterType);
	void setWrapping(GLuint wrappingType);
	void setFilterAndWrapping(GLuint filterType, GLuint wrappingType);
	void setParameter(GLenum pname, GLint value);

	void generateMipmap();

	void bind(int slot) const;
	void unbind() const;

private:
	void allocate2D(GLuint internalFormat, int width, int height, GLuint sourceFormat, GLuint dataType, const void* data = nullptr);
	void allocateCube(GLuint internalFormat, int width, GLuint sourceFormat, GLuint dataType, const void* data = nullptr);

private:
	GLuint m_ID, m_TextureType;
	int m_Width, m_Height, m_BitsPerPixel;
};