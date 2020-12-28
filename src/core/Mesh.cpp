#include "Mesh.h"

void Mesh::loadMesh(const void* data, int count, std::vector<GLuint> indices, BufferLayout layout, bool batchedData)
{
	m_VB.allocate(count * layout.stride(), data, count, batchedData);
	m_VA.addBuffer(m_VB, layout);

	m_EB.allocate(indices.size() * sizeof(GLuint), &indices[0], indices.size());
}

void Mesh::loadShape(Shape& shape)
{
	m_VA = shape.VA();
	m_VB = shape.VB();
}

void Mesh::addTexture(TextureMesh* tex)
{
	m_Textures.push_back(tex);
}

void Mesh::draw(Shader& shader)
{
	if (!m_EB.ID()) renderer.draw(m_VA, shader);
	else renderer.draw(m_VA, m_EB, shader);
}
