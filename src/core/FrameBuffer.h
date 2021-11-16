#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <set>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Texture.h"
#include "RenderBuffer.h"
#include "GLStateObject.h"

class FrameBuffer;
using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

class FrameBuffer :
	public GLStateObject
{
public:
	FrameBuffer(int width, int height, RenderBufferFormat rbFormat, const std::vector<TexturePtr>& texToAttach);
	~FrameBuffer();

	int width() const { return mWidth; }
	int height() const { return mHeight; }

	void bind();
	void unbind();

	static FrameBufferPtr create(int width, int height, RenderBufferFormat rbFormat,
		const std::vector<TexturePtr>& texToAttach);

	static FrameBuffer* currentBinding() { return curBinding; }

private:
	bool checkComplete();

private:
	int mWidth, mHeight;
	RenderBufferPtr mRenderBuffer;

	static FrameBuffer* curBinding;
};