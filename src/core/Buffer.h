#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "GLStateObject.h"
#include "GLDataType.h"

class Buffer;
using BufferPtr = std::shared_ptr<Buffer>;
class VertexBuffer;
using VertexBufferPtr = std::shared_ptr<VertexBuffer>;

enum class BufferUsage
{
	StreamDraw = GL_STREAM_DRAW, StaticDraw = GL_STATIC_DRAW, DynamicDraw = GL_DYNAMIC_DRAW,
	StreamRead = GL_STREAM_READ, StaticRead = GL_STATIC_READ, DynamicRead = GL_DYNAMIC_READ,
	StreamCopy = GL_STREAM_COPY, StaticCopy = GL_STATIC_COPY, DynamicCopy = GL_DYNAMIC_COPY
};

class Buffer :
	public GLStateObject
{
public:
	Buffer(int64_t size, const void* data, BufferUsage usage);
	~Buffer();

	void allocate(int64_t size, const void* data, BufferUsage usage = BufferUsage::StaticDraw);
	void write(int64_t offset, int64_t size, const void* data);

	int64_t size() const { return mSize; }

	static BufferPtr create(int64_t size, const void* data, BufferUsage usage = BufferUsage::StaticDraw);

protected:
	int64_t mSize;
};

class VertexBuffer :
	public Buffer
{
public:
	VertexBuffer(const void* data, size_t size, size_t nVertices, BufferUsage usage) :
		mNumVertices(nVertices), Buffer(size, data, usage) {}

	size_t numVertices() const { return mNumVertices; }

	static VertexBufferPtr create(const void* data, size_t size, size_t nVertices, BufferUsage usage = BufferUsage::StaticDraw);

	template<typename T>
	static VertexBufferPtr createFromVector(const std::vector<T>& data, BufferUsage usage = BufferUsage::StaticDraw)
	{
		return create(data.data(), data.size() * sizeof(T), data.size(), usage);
	}

	template<typename T>
	static VertexBufferPtr createTyped(const void* data, size_t nVertices, BufferUsage usage = BufferUsage::StaticDraw)
	{
		return create(data, nVertices * sizeof(T), nVertices, usage);
	}

private:
	size_t mNumVertices;
};