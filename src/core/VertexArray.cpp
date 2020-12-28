#include "VertexArray.h"

VertexArray::VertexArray()
	:m_VerticesCount(0)
{
	glCreateVertexArrays(1, &m_ID);
}

VertexArray::~VertexArray()
{
	glDeleteVertexArrays(1, &m_ID);
}

void VertexArray::addBuffer(const Buffer& vb, BufferLayout layout)
{
	addBuffer(&vb, layout);
}

void VertexArray::addBuffer(const Buffer* vb, BufferLayout layout)
{
	if (vb == nullptr) return;
	const auto& elements = layout.elements();
	GLuint offset = 0;
	GLuint bindingIndex = 0;

	for (int i = 0; i < elements.size(); i++)
	{
		glEnableVertexArrayAttrib(m_ID, i);
		int count = elements[i].count;
		GLuint type = elements[i].type;
		bool normalized = elements[i].normalized;
		glVertexArrayAttribFormat(m_ID, i, count, type, normalized, offset);
		glVertexArrayAttribBinding(m_ID, i, bindingIndex);
		GLuint add = sizeofGLType(type) * count;
		if (vb->batched()) add *= vb->elementsCount();
		offset += add;
	}

	GLuint stride = vb->batched() ? 0 : layout.stride();
	glVertexArrayVertexBuffer(m_ID, bindingIndex, vb->ID(), 0, stride);
	m_VerticesCount = vb->elementsCount();
}

void VertexArray::attachElementBuffer(const Buffer& eb)
{
	glVertexArrayElementBuffer(m_ID, eb.ID());
}

void VertexArray::detachElementBuffer()
{
	glVertexArrayElementBuffer(m_ID, 0);
}

void VertexArray::bind() const
{
	glBindVertexArray(m_ID);
}

void VertexArray::unbind() const
{
	glBindVertexArray(0);
}