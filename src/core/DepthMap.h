#pragma once

#include "FrameBuffer.h"
#include "Texture.h"
#include "RenderBuffer.h"

class DepthMap
{
public:
	void init(int type, int width, int height, GLuint format);

	void bind();
	void unbind();

	int width() const { return m_Width; }
	int height() const { return m_Height; }
	Texture& texture() { return m_Tex; }

	enum DepthMapType
	{
		CUBE = 0,
		DIRECTIONAL,
		SCREEN_SPACE
	};

private:
	FrameBuffer m_FB;
	Texture m_Tex;
	int m_Width = 0;
	int m_Height = 0;
};