#include "Pipeline.h"
#include "../util/Error.h"

void Pipeline::setViewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
	mViewport = { x, y, width, height };
}

void Pipeline::clear(const glm::vec4& color)
{
	glClearColor(color.x, color.y, color.z, color.w);
	glClear(static_cast<GLbitfield>(mClearBuffer));
}

void Pipeline::draw(VertexBufferPtr vertices, VertexArrayPtr interpretor, ShaderPtr shader)
{
	if (vertices == nullptr || interpretor == nullptr)
	{
		Error::bracketLine<0>("Renderer null pointer to vertex data or data interpretor");
		return;
	}
	shader->enable();
	interpretor->bind();

	glPolygonMode(static_cast<GLenum>(mFaceToDraw), static_cast<GLenum>(mPolyMode));
	glVertexArrayVertexBuffer(interpretor->id(), 0, vertices->id(), 0, interpretor->stride());
	glDrawArrays(static_cast<GLenum>(mPrimType), 0, vertices->numVertices());

	interpretor->unbind();
}

void Pipeline::drawIndexed(VertexBufferPtr vertices, VertexArrayPtr interpretor,
	VertexBufferPtr indices, ShaderPtr shader)
{
	if (vertices == nullptr || interpretor == nullptr || indices == nullptr)
	{
		Error::bracketLine<0>("Renderer null pointer to vertex data or data interpretor");
		return;
	}
	shader->enable();
	interpretor->bind();

	glPolygonMode(static_cast<GLenum>(mFaceToDraw), static_cast<GLenum>(mPolyMode));
	glVertexArrayVertexBuffer(interpretor->id(), 0, vertices->id(), 0, interpretor->stride());
	glVertexArrayElementBuffer(interpretor->id(), indices->id());
	glDrawElements(static_cast<GLenum>(mPrimType), indices->numVertices(), GL_UNSIGNED_INT, 0);

	interpretor->unbind();
}

std::vector<uint8_t> Pipeline::readFramePixels()
{
	std::vector<uint8_t> ret(static_cast<size_t>(mViewport.z) * mViewport.w * 3);
	glReadPixels(mViewport.x, mViewport.y, mViewport.z, mViewport.w, GL_RGB, GL_UNSIGNED_BYTE, ret.data());
	return ret;
}

PipelinePtr Pipeline::create(const PipelineCreateInfo& createInfo)
{
	return std::make_shared<Pipeline>(createInfo);
}

void Pipeline::dispatchCompute(int xNum, int yNum, int zNum, ShaderPtr shader)
{
	if (!shader)
	{
		Error::bracketLine<0>("Compute no shader used");
		return;
	}
	shader->enable();
	glDispatchCompute(xNum, yNum, zNum);
}

void Pipeline::memoryBarrier(MemoryBarrierBit bits)
{
	glMemoryBarrier(static_cast<GLbitfield>(bits));
}

void Pipeline::bindTextureToImage(TexturePtr tex, uint32_t unit, int level, ImageAccess access, TextureFormat format)
{
	if (!tex)
		return;
	glBindImageTexture(unit, tex->id(), level, false, 0, static_cast<GLenum>(access), static_cast<GLenum>(format));
}
