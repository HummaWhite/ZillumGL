#include "FrameBuffer.h"

FrameBuffer::~FrameBuffer()
{
	glDeleteFramebuffers(1, &m_ID);
}

void FrameBuffer::generate(int width, int height)
{
	glCreateFramebuffers(1, &m_ID);
	m_Width = width;
	m_Height = height;
}

void FrameBuffer::bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_ID);
}

void FrameBuffer::unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::attachRenderBuffer(GLenum attachment, RenderBuffer& renderBuffer)
{
	if (renderBuffer.width() != m_Width || renderBuffer.height() != m_Height)
	{
		std::cout << "FrameBuffer(" << m_ID << ")::Warning : RenderBuffer size not equal\n";
		return;
	}
	glNamedFramebufferRenderbuffer(m_ID, attachment, GL_RENDERBUFFER, renderBuffer.ID());
	if (!checkComplete()) return;
}

void FrameBuffer::detachRenderBuffer(GLenum attachment)
{
	glNamedFramebufferRenderbuffer(m_ID, attachment, GL_RENDERBUFFER, 0);
}

void FrameBuffer::attachTexture(GLenum attachment, Texture& texture, int mipmapLevel)
{
	glNamedFramebufferTexture2DEXT(m_ID, attachment, GL_TEXTURE_2D, texture.ID(), mipmapLevel);
	if (!checkComplete()) return;
}

void FrameBuffer::detachTexture(GLenum attachment)
{
	glNamedFramebufferTexture(m_ID, attachment, 0, 0);
}

bool FrameBuffer::checkComplete()
{
	GLenum status = glCheckNamedFramebufferStatus(m_ID, GL_FRAMEBUFFER);
	//std::cout << status << "\n";
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "Error: framebuffer not complete" << std::endl;
		return false;
	}
	return true;
}

void FrameBuffer::activateAttachmentTargets(const std::vector<GLenum>& attachments)
{
	glNamedFramebufferDrawBuffers(m_ID, attachments.size(), (GLenum*)&attachments[0]);
}
