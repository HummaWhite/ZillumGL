#include "Pipeline.h"
#include "../util/Error.h"

std::map<TexturePtr, Pipeline::TextureBindParam> Pipeline::mImageBindRec;

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

ImagePtr Pipeline::readFramePixels()
{
	int width = (mViewport.z >> 2) << 2;
	int height = mViewport.w;
	size_t size = static_cast<size_t>(width) * height * 3;
	auto data = new uint16_t[size];
	auto image = Image::createEmpty(width, height, ImageDataType::Int8);
	glReadPixels(mViewport.x, mViewport.y, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
	memcpy(image->data(), data, size);
	delete[] data;
	return image;
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

void Pipeline::bindTextureToImage(TexturePtr texture, uint32_t unit, int level, ImageAccess access, TextureFormat format)
{
	if (!texture)
		return;
	if (canRebindTexture(texture, { unit, level, access, format }))
		glBindImageTexture(unit, texture->id(), level, false, 0, static_cast<GLenum>(access), static_cast<GLenum>(format));
}

bool Pipeline::canRebindTexture(TexturePtr texture, const TextureBindParam& param)
{
	auto itr = mImageBindRec.find(texture);
	if (itr == mImageBindRec.end())
	{
		mImageBindRec[texture] = param;
		return true;
	}
	if (itr->second != param)
	{
		itr->second = param;
		return true;
	}
	return false;
}
