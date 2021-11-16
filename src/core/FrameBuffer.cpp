#include "FrameBuffer.h"
#include "../util/Error.h"

FrameBuffer* FrameBuffer::curBinding = nullptr;

FrameBuffer::FrameBuffer(int width, int height, RenderBufferFormat rbFormat,
	const std::vector<TexturePtr>& texToAttach) :
	mWidth(width), mHeight(height), GLStateObject(GLStateObjectType::FrameBuffer)
{
	glCreateFramebuffers(1, &mId);
	mRenderBuffer = RenderBuffer::create(width, height, rbFormat);
	glNamedFramebufferRenderbuffer(mId, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mRenderBuffer->id());

	std::vector<GLenum> attachments;
	for (int i = 0; i < texToAttach.size(); i++)
	{
		auto tex = texToAttach[i];
		GLenum attachment = GL_COLOR_ATTACHMENT0 + i;
		glNamedFramebufferTexture(mId, attachment, tex->id(), 0);
		//glNamedFramebufferTexture2DEXT(mId, attachment, GL_TEXTURE_2D, tex->id(), 0);
		attachments.push_back(attachment);
	}
	glNamedFramebufferDrawBuffers(mId, attachments.size(), attachments.data());
	checkComplete();
}

FrameBuffer::~FrameBuffer()
{
	glDeleteFramebuffers(1, &mId);
}

void FrameBuffer::bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mId);
	curBinding = this;
}

void FrameBuffer::unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	curBinding = nullptr;
}

FrameBufferPtr FrameBuffer::create(int width, int height, RenderBufferFormat rbFormat,
	const std::vector<TexturePtr>& texToAttach)
{
	return std::make_shared<FrameBuffer>(width, height, rbFormat, texToAttach);
}

bool FrameBuffer::checkComplete()
{
	GLenum status = glCheckNamedFramebufferStatus(mId, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		Error::log("FrameBuffer", "not complete");
		return false;
	}
	return true;
}
