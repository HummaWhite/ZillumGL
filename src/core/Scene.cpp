#include "Scene.h"

SceneBuffer Scene::genBuffers()
{
	auto vertexBuf = std::make_shared<Buffer>();
	auto normalBuf = std::make_shared<Buffer>();
	auto indexBuf = std::make_shared<Buffer>();
	auto boundBuf = std::make_shared<Buffer>();
	auto sizeIndexBuf = std::make_shared<Buffer>();
	auto materialBuf = std::make_shared<Buffer>();

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> indices;
	std::vector<uint8_t> matIndices;

	uint32_t indexOffset = 0;

	for (auto object : objects)
	{
		auto meshes = object->meshes();
		for (auto m : meshes)
		{
			std::vector<uint32_t> transIndices(m.indices.size());
			std::transform(m.indices.begin(), m.indices.end(), transIndices.begin(),
				[indexOffset](uint32_t x) { return (x + indexOffset); }
			);

			std::vector<uint8_t> meshMat(m.indices.size() / 3, m.matIndex);

			vertices.insert(vertices.end(), m.vertices.begin(), m.vertices.end());
			normals.insert(normals.end(), m.normals.begin(), m.normals.end());
			indices.insert(indices.end(), transIndices.begin(), transIndices.end());
			matIndices.insert(matIndices.end(), meshMat.begin(), meshMat.end());

			indexOffset += m.vertices.size();
		}
	}

	BVH bvh(vertices, indices, matIndices);
	auto bvhBuf = bvh.build();
	std::vector<int> sizeIndices(bvhBuf.sizeIndices.size());

	/*
	std::transform(bvhBuf.sizeIndices.begin(), bvhBuf.sizeIndices.end(), sizeIndices.begin(),
		[](int x)*/

	vertexBuf->allocate(vertices.size() * sizeof(glm::vec3), vertices.data(), 0);
	normalBuf->allocate(normals.size() * sizeof(glm::vec3), normals.data(), 0);
	indexBuf->allocate(indices.size() * sizeof(uint32_t), indices.data(), 0);
	boundBuf->allocate(bvhBuf.bounds.size() * sizeof(AABB), bvhBuf.bounds.data(), 0);
	sizeIndexBuf->allocate(bvhBuf.sizeIndices.size() * sizeof(int), bvhBuf.sizeIndices.data(), 0);
	materialBuf->allocate(materials.size() * sizeof(Material), materials.data(), 0);

	return SceneBuffer{ vertexBuf, normalBuf, indexBuf, boundBuf, sizeIndexBuf, materialBuf };
}

void Scene::addObject(std::shared_ptr<Model> object)
{
	objects.push_back(object);
}

void Scene::addMaterial(const Material& material)
{
	materials.push_back(material);
}
