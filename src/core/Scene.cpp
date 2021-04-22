#include "Scene.h"

SceneBuffer Scene::genBuffers()
{
	auto vertexBuf = std::make_shared<BufferTexture>();
	auto normalBuf = std::make_shared<BufferTexture>();
	auto texCoordBuf = std::make_shared<BufferTexture>();
	auto indexBuf = std::make_shared<BufferTexture>();
	auto boundBuf = std::make_shared<BufferTexture>();
	auto hitTableBuf = std::make_shared<BufferTexture>();
	auto matIndexBuf = std::make_shared<BufferTexture>();

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;
	std::vector<uint8_t> matIndices;

	uint32_t indexOffset = 0;

	for (int i = 0; i < objects.size(); i++)
	{
		const auto [object, matIndex] = objects[i];
		auto meshes = object->meshes();
		for (auto m : meshes)
		{
			std::vector<uint32_t> transIndices(m.indices.size());
			std::transform(m.indices.begin(), m.indices.end(), transIndices.begin(),
				[indexOffset](uint32_t x) { return (x + indexOffset); }
			);

			std::vector<uint8_t> meshMat(m.indices.size() / 3, matIndex);

			vertices.insert(vertices.end(), m.vertices.begin(), m.vertices.end());
			normals.insert(normals.end(), m.normals.begin(), m.normals.end());
			texCoords.insert(texCoords.end(), m.texCoords.begin(), m.texCoords.end());
			indices.insert(indices.end(), transIndices.begin(), transIndices.end());
			matIndices.insert(matIndices.end(), meshMat.begin(), meshMat.end());

			indexOffset += m.vertices.size();
		}
	}

	BVH bvh(vertices, indices, bvhBuildMethod);
	auto bvhBuf = bvh.build();

	vertexBuf->allocate(vertices.size() * sizeof(glm::vec3), vertices.data(), GL_RGB32F);
	normalBuf->allocate(normals.size() * sizeof(glm::vec3), normals.data(), GL_RGB32F);
	texCoordBuf->allocate(texCoords.size() * sizeof(glm::vec2), texCoords.data(), GL_RG32F);
	indexBuf->allocate(indices.size() * sizeof(uint32_t), indices.data(), GL_R32I);
	boundBuf->allocate(bvhBuf.bounds.size() * sizeof(AABB), bvhBuf.bounds.data(), GL_RGB32F);
	hitTableBuf->allocate(bvhBuf.hitTable.size() * sizeof(int), bvhBuf.hitTable.data(), GL_RGB32I);
	matIndexBuf->allocate(matIndices.size() * sizeof(uint8_t), matIndices.data(), GL_R8I);

	return SceneBuffer{ vertexBuf, normalBuf, texCoordBuf, indexBuf, boundBuf, hitTableBuf, matIndexBuf };
}

void Scene::addObject(std::shared_ptr<Model> object, uint8_t matIndex)
{
	objects.push_back({ object, matIndex });
}

void Scene::addMaterial(const Material& material)
{
	materials.push_back(material);
}

void Scene::addLight(const Light& light)
{
	lights.push_back(light);
}

void Scene::addLight(const glm::vec3& pa, const glm::vec3& pb, const glm::vec3& pc, const glm::vec3& power)
{
	addLight(Light{ pa, pb, pc, power });
}

void Scene::setBVHBuildMethod(BVHSplitMethod method)
{
	bvhBuildMethod = method;
}
