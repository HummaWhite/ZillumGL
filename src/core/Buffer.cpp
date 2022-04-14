#include "Buffer.h"
#include "../util/Error.h"

Buffer::Buffer(int64_t size, const void* data, BufferUsage usage) :
	mSize(size), GLStateObject(GLStateObjectType::Buffer)
{
	glCreateBuffers(1, &mId);
	glNamedBufferData(mId, size, data, static_cast<GLenum>(usage));
}

Buffer::~Buffer()
{
	glDeleteBuffers(1, &mId);
}

void Buffer::allocate(int64_t size, const void* data, BufferUsage usage)
{
	mSize = size;
	glNamedBufferData(mId, size, data, static_cast<GLenum>(usage));
}

void Buffer::write(int64_t offset, int64_t size, const void* data)
{
	Error::check(offset + size <= mSize, "[Buffer]\twrite size > allocated");
	glNamedBufferSubData(mId, offset, size, data);
}

void Buffer::read(int64_t offset, int64_t size, void* data)
{
	Error::check(offset + size <= mSize, "[Buffer]\tread size > allocated");
	glGetNamedBufferSubData(mId, offset, size, data);
}

BufferPtr Buffer::create(int64_t size, const void* data, BufferUsage usage)
{
	return std::make_shared<Buffer>(size, data, usage);
}

VertexBufferPtr VertexBuffer::create(const void* data, size_t size, size_t nVertices, BufferUsage usage)
{
	return std::make_shared<VertexBuffer>(data, size, nVertices, usage);
}
