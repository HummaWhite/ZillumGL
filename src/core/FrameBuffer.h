#pragma once

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Texture.h"
#include "RenderBuffer.h"

class FrameBuffer
{
public:
	FrameBuffer() : m_ID(0), m_Width(0), m_Height(0) {}
	~FrameBuffer();

	GLuint ID() const { return m_ID; }
	int width() const { return m_Width; }
	int height() const { return m_Height; }

	void generate(int width, int height);
	void bind() const;
	void unbind() const;
	void attachRenderBuffer(GLenum attachment, RenderBuffer& renderBuffer);
	void detachRenderBuffer(GLenum attachment);
	void attachTexture(GLenum attachment, Texture& texture, int mipmapLevel = 0);
	void detachTexture(GLenum attachment);
	bool checkComplete();

	void activateAttachmentTargets(const std::vector<GLenum>& attachments);

private:
	GLuint m_ID;
	int m_Width, m_Height;
};