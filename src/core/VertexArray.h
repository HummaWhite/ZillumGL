#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Buffer.h"
#include "BufferLayout.h"

class VertexArray
{
public:
	VertexArray();
	~VertexArray();

	void addBuffer(const Buffer& vb, BufferLayout layout);
	void addBuffer(const Buffer* vb, BufferLayout layout);

	void attachElementBuffer(const Buffer& eb);
	void detachElementBuffer();

	GLuint ID() const { return m_ID; }
	void bind() const;
	void unbind() const;
	GLuint verticesCount() const { return m_VerticesCount; }

private:
	GLuint m_ID;
	GLuint m_VerticesCount;
};