#pragma once

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "GLSizeofType.h"

struct BufferElement
{
	GLuint type;
	int count;
	bool normalized;
};

class BufferLayout
{
private:
	std::vector<BufferElement> m_Elements;
	GLuint m_Stride;

public:
	BufferLayout() : m_Stride(0) {};
	BufferLayout(std::vector<BufferElement> elements, GLuint stride) : m_Elements(elements), m_Stride(stride) {}
	~BufferLayout() {};

	std::vector<BufferElement> elements() const { return m_Elements; }
	GLuint stride() const { return m_Stride; }

	template<typename T>
	void add(int count, bool normalized = GL_FALSE)
	{
		static_assert(false);
	}

	template<>
	void add<float>(int count, bool normalized)
	{
		m_Elements.push_back({ GL_FLOAT, count, normalized });
		m_Stride += count * sizeof(float);
	}

	template<>
	void add<GLuint>(int count, bool normalized)
	{
		m_Elements.push_back({ GL_UNSIGNED_INT, count, normalized });
		m_Stride += count * sizeof(GLuint);
	}

	template<>
	void add<int>(int count, bool normalized)
	{
		m_Elements.push_back({ GL_INT, count, normalized });
		m_Stride += count * sizeof(int);
	}

	template<>
	void add<GLbyte>(int count, bool normalized)
	{
		m_Elements.push_back({ GL_BYTE, count, normalized });
		m_Stride += count * sizeof(GLbyte);
	}

	template<GLuint T>
	void add(int count, bool normalized = 0)
	{
		m_Elements.push_back({ T, count, normalized });
		m_Stride += count * sizeofGLType(T);
	}
};

const BufferLayout LAYOUT_POS2 =
{
	{
		{ GL_FLOAT, 2, false }
	},
	8
};

const BufferLayout LAYOUT_POS3_UV2_NORM3 =
{
	{
		{ GL_FLOAT, 3, false },
		{ GL_FLOAT, 2, false },
		{ GL_FLOAT, 3, false }
	},
	32
};

const BufferLayout LAYOUT_POS3_UV2_NORM3_TAN3 =
{
	{
		{ GL_FLOAT, 3, false },
		{ GL_FLOAT, 2, false },
		{ GL_FLOAT, 3, false },
		{ GL_FLOAT, 3, false }
	},
	44
};

const BufferLayout LAYOUT_POS3_UV2_NORM3_TAN3_BTN3 =
{
	{
		{ GL_FLOAT, 3, false },
		{ GL_FLOAT, 2, false },
		{ GL_FLOAT, 3, false },
		{ GL_FLOAT, 3, false },
		{ GL_FLOAT, 3, false }
	},
	56
};