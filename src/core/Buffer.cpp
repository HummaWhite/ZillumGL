#include "Buffer.h"

void Buffer::allocate(int size, const void* data, GLuint elementsCount, bool batched, GLenum type)
{
	m_ElementsCount = elementsCount;
	m_Size = size;
	m_Batched = batched;

	glCreateBuffers(1, &m_ID);
	glNamedBufferData(m_ID, size, data, type);
}

void Buffer::write(int offset, int size, const void* data)
{
	if (offset + size > m_Size || !m_ID) return;
	glNamedBufferSubData(m_ID, offset, size, data);
}

Buffer::~Buffer()
{
	glDeleteBuffers(1, &m_ID);
}
