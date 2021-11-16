#pragma once

#include <iostream>
#include <memory>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "GLStateObject.h"

class RenderBuffer;
using RenderBufferPtr = std::shared_ptr<RenderBuffer>;

enum class RenderBufferFormat
{
	Depth24Stencil8 = GL_DEPTH24_STENCIL8,
	Depth32fStencil8 = GL_DEPTH32F_STENCIL8
};

class RenderBuffer :
	public GLStateObject
{
public:
	RenderBuffer(int width, int height, RenderBufferFormat format);
	~RenderBuffer();

	int width() const { return mWidth; }
	int height() const { return mHeight; }
	RenderBufferFormat format() const { return mFormat; }

	static RenderBufferPtr create(int width, int height, RenderBufferFormat format);

private:
	int mWidth, mHeight;
	RenderBufferFormat mFormat;
};