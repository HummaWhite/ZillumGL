#include "Scene.h"

SceneBuffer Scene::genBuffers()
{
	auto vertexBuf = std::make_shared<BufferTexture>();
	auto normalBuf = std::make_shared<BufferTexture>();
	auto texCoordBuf = std::make_shared<BufferTexture>();
	auto indexBuf = std::make_shared<BufferTexture>();
	auto boundBuf = std::make_shared<BufferTexture>();
	auto hitTableBuf = std::make_shared<BufferTexture>();
	auto matTexIndexBuf = std::make_shared<BufferTexture>();
	auto materialBuf = std::make_shared<BufferTexture>();
	auto lightPowerBuf = std::make_shared<BufferTexture>();
	auto lightAliasBuf = std::make_shared<BufferTexture>();
	auto lightProbBuf = std::make_shared<BufferTexture>();
	auto uvScaleBuf = std::make_shared<BufferTexture>();

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> matTexIndices;

	uint32_t offIndVertex = 0;
	uint32_t offIndMaterial = 0;
	for (const auto obj : objects)
	{
		const auto [object, matIndex] = obj;

		auto modelMat = object->materials();
		materials.insert(materials.end(), modelMat.begin(), modelMat.end());

		auto meshes = object->meshes();
		for (const auto& m : meshes)
		{
			std::vector<uint32_t> transIndices(m.indices.size());
			std::transform(m.indices.begin(), m.indices.end(), transIndices.begin(),
				[offIndVertex](uint32_t x) { return (x + offIndVertex); }
			);
			//std::cout << (m.matTexIndex >> 16) << " " << (m.matTexIndex & 0xffff) + offIndMaterial << "\n";
			std::vector<uint32_t> meshMat(m.indices.size() / 3, m.matTexIndex + offIndMaterial);

			vertices.insert(vertices.end(), m.vertices.begin(), m.vertices.end());
			normals.insert(normals.end(), m.normals.begin(), m.normals.end());
			texCoords.insert(texCoords.end(), m.texCoords.begin(), m.texCoords.end());
			indices.insert(indices.end(), transIndices.begin(), transIndices.end());
			matTexIndices.insert(matTexIndices.end(), meshMat.begin(), meshMat.end());

			offIndVertex += m.vertices.size();
			objPrimCount += m.indices.size() / 3;
		}
		offIndMaterial += modelMat.size();
	}

	for (const auto light : lights)
	{
		auto lt = light.first;
		auto meshes = lt->meshes();

		for (const auto& m : meshes)
		{
			std::vector<uint32_t> transIndices(m.indices.size());
			std::transform(m.indices.begin(), m.indices.end(), transIndices.begin(),
				[offIndVertex](uint32_t x) { return (x + offIndVertex); }
			);

			vertices.insert(vertices.end(), m.vertices.begin(), m.vertices.end());
			normals.insert(normals.end(), m.normals.begin(), m.normals.end());
			//texCoords.insert(texCoords.end(), m.texCoords.begin(), m.texCoords.end());
			indices.insert(indices.end(), transIndices.begin(), transIndices.end());

			offIndVertex += m.vertices.size();
		}
	}

	BVH bvh(vertices, indices, bvhBuildMethod);
	auto bvhBuf = bvh.build();

	auto [lightPower, lightAlias, lightProb] = genLightTable();

	vertexBuf->allocate(vertices, GL_RGB32F);
	normalBuf->allocate(normals, GL_RGB32F);
	texCoordBuf->allocate(texCoords, GL_RG32F);
	indexBuf->allocate(indices, GL_R32I);
	boundBuf->allocate(bvhBuf.bounds, GL_RGB32F);
	hitTableBuf->allocate(bvhBuf.hitTable, GL_RGB32I);
	matTexIndexBuf->allocate(matTexIndices, GL_R32I);
	materialBuf->allocate(materials, GL_RGB32F);
	lightPowerBuf->allocate(lightPower, GL_RGB32F);
	lightAliasBuf->allocate(lightAlias, GL_R32I);
	lightProbBuf->allocate(lightProb, GL_R32F);

	auto [textures, uvScale] = Resource::createTextureBatch();
	uvScaleBuf->allocate(uvScale, GL_RG32F);

	return SceneBuffer{ vertexBuf, normalBuf, texCoordBuf, indexBuf, boundBuf,
		hitTableBuf, matTexIndexBuf, materialBuf, lightPowerBuf, lightAliasBuf, lightProbBuf,
		textures, uvScaleBuf };
}

void Scene::addObject(std::shared_ptr<Model> object, uint8_t matIndex)
{
	objects.push_back({ object, matIndex });
}

void Scene::addMaterial(const Material& material)
{
	materials.push_back(material);
}

void Scene::addLight(std::shared_ptr<Model> light, const glm::vec3& radiance)
{
	lights.push_back({ light, radiance });
}

std::tuple<std::vector<glm::vec3>, std::vector<int32_t>, std::vector<float>> Scene::genLightTable()
{
	typedef std::pair<int, float> Element;

	std::vector<glm::vec3> power;
	std::vector<int32_t> alias;
	std::vector<float> pdf;
	lightSum = 0.0f;

	auto brightness = [](const glm::vec3& v) -> float
	{
		return glm::dot(v, glm::vec3(0.299f, 0.587f, 0.114f));
	};

	for (const auto light : lights)
	{
		const auto [lt, radiance] = light;

		auto meshes = lt->meshes();
		for (const auto& m : meshes)
		{
			int fCount = m.indices.size() / 3;
			for (int i = 0; i < fCount; i++)
			{
				glm::vec3 va = m.vertices[m.indices[i * 3 + 0]];
				glm::vec3 vb = m.vertices[m.indices[i * 3 + 1]];
				glm::vec3 vc = m.vertices[m.indices[i * 3 + 2]];

				float area = 0.5f * glm::length(glm::cross(vc - va, vb - va));
				glm::vec3 p = radiance * area * glm::pi<float>() * 2.0f;
				power.push_back(p);
				pdf.push_back(brightness(p));
				lightSum += brightness(p);
			}
		}
	}

	nLights = pdf.size();
	float sumInv = nLights / lightSum;
	alias.resize(nLights);

	Element* greater = new Element[nLights * 2], * lesser = new Element[nLights * 2];
	int gTop = 0, lTop = 0;

	for (int i = 0; i < nLights; i++)
	{
		pdf[i] *= sumInv;
		(pdf[i] < 1.0f ? lesser[lTop++] : greater[gTop++]) = Element(i, pdf[i]);
	}

	while (gTop != 0 && lTop != 0)
	{
		auto [l, pl] = lesser[--lTop];
		auto [g, pg] = greater[--gTop];

		alias[l] = g;
		pdf[l] = pl;

		pg += pl - 1.0f;
		(pg < 1.0f ? lesser[lTop++] : greater[gTop++]) = Element(g, pg);
	}

	while (gTop != 0)
	{
		auto [g, pg] = greater[--gTop];
		alias[g] = g;
		pdf[g] = pg;
	}

	while (lTop != 0)
	{
		auto [l, pl] = lesser[--lTop];
		alias[l] = l;
		pdf[l] = pl;
	}

	delete[] greater;
	delete[] lesser;

	return { power, alias, pdf };
}
