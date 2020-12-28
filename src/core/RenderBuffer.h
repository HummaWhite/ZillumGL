#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

class RenderBuffer
{
public:
	RenderBuffer() : m_ID(0), m_Width(0), m_Height(0) {}
	~RenderBuffer();

	GLuint ID() const { return m_ID; }
	int width() const { return m_Width; }
	int height() const { return m_Height; }

	void allocate(GLenum format, int width, int height);

private:
	GLuint m_ID;
	int m_Width, m_Height;
};