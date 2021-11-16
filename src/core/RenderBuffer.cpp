#include "RenderBuffer.h"

RenderBuffer::RenderBuffer(int width, int height, RenderBufferFormat format) :
	mWidth(width), mHeight(height), mFormat(format), GLStateObject(GLStateObjectType::RenderBuffer)
{
	glCreateRenderbuffers(1, &mId);
	glNamedRenderbufferStorage(mId, static_cast<GLenum>(format), width, height);
}

RenderBuffer::~RenderBuffer()
{
	glDeleteRenderbuffers(1, &mId);
}

RenderBufferPtr RenderBuffer::create(int width, int height, RenderBufferFormat format)
{
	return std::make_shared<RenderBuffer>(width, height, format);
}
