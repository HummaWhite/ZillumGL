#include "RenderBuffer.h"

RenderBuffer::~RenderBuffer()
{
	glDeleteRenderbuffers(1, &m_ID);
}

void RenderBuffer::allocate(GLenum format, int width, int height)
{
	glCreateRenderbuffers(1, &m_ID);
	glNamedRenderbufferStorage(m_ID, format, width, height);
	m_Width = width;
	m_Height = height;
}
