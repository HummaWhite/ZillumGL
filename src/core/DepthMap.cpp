#include "DepthMap.h"

void DepthMap::init(int type, int width, int height, GLuint format)
{
	if (type != CUBE)
	{
		m_FB.generate(width, height);
		m_Tex.generateDepth2D(width, height, format);
	}
	else
	{
		m_FB.generate(width, width);
		m_Tex.generateDepthCube(width, format);
	}

	m_Tex.setFilterAndWrapping(GL_LINEAR, GL_CLAMP_TO_EDGE);

	m_FB.attachTexture(GL_DEPTH_ATTACHMENT, m_Tex);
	m_FB.activateAttachmentTargets({ GL_DEPTH_ATTACHMENT });
	m_Width = width, m_Height = height;
}

void DepthMap::bind()
{
	m_FB.bind();
}

void DepthMap::unbind()
{
	m_FB.unbind();
}
