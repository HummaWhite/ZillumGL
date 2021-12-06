#include "Mesh.h"

void MeshData::createGLContext()
{
	auto nVertices = positions.size();

	std::vector<VertexType> vertexData(nVertices);
	for (size_t i = 0; i < nVertices; i++)
		vertexData[i] = { positions[i], texcoords[i], normals[i] };

	mVertexBuffer = VertexBuffer::createFromVector(vertexData);

	if (!indices.empty())
		mIndexBuffer = VertexBuffer::createFromVector(indices);

	mHasGLContext = true;
}

GLuint MeshData::bufferId() const
{
	Error::check(mHasGLContext, "buffer not generated");
	return mVertexBuffer->id();
}

void MeshData::render(PipelinePtr ctx, ShaderPtr shader)
{
	if (!ctx)
		return;
	if (!mHasGLContext)
	{
		Error::bracketLine<0>("Mesh the mesh has not generated GL context");
		return;
	}
	if (!mIndexBuffer)
		ctx->draw(mVertexBuffer, VertexArray::layoutPos3Tex2Norm3Interw(), shader);
	else
		ctx->drawIndexed(mVertexBuffer, VertexArray::layoutPos3Tex2Norm3Interw(),
			mIndexBuffer, shader);
}

MeshDataPtr MeshData::create()
{
	return std::make_shared<MeshData>();
}

MeshInstancePtr MeshInstance::create()
{
	return std::make_shared<MeshInstance>();
}
