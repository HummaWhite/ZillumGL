#include "Scene.h"

SceneBuffer Scene::genBuffers()
{
	SceneBuffer ret;

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> matTexIndices;

	uint32_t offIndVertex = 0;
	uint32_t offIndMaterial = 0;
	for (const auto& object : objects)
	{
		auto modelMat = object->materials();
		materials.insert(materials.end(), modelMat.begin(), modelMat.end());

		auto model = object->modelMatrix();
		glm::mat3 modelInv = glm::transpose(glm::inverse(model));

		for (const auto& meshInstance : object->meshInstances())
		{
			const auto& meshData = meshInstance->meshData;
			for (const auto& v : meshData->positions)
				vertices.push_back(model * glm::vec4(v, 1.0f));
			for (const auto& n : meshData->normals)
				normals.push_back(glm::normalize(modelInv * n));
			for (const auto& t : meshData->texcoords)
				texCoords.push_back(t);
			for (const auto& i : meshData->indices)
				indices.push_back(i + offIndVertex);
			for (size_t i = 0; i < meshData->indices.size() / 3; i++)
				matTexIndices.push_back(offIndMaterial + (meshInstance->texIndex << 16 | meshInstance->matIndex));

			offIndVertex += meshData->positions.size();
			objPrimCount += meshData->indices.size() / 3;
		}
		offIndMaterial += modelMat.size();
	}

	for (const auto light : lights)
	{
		auto lt = light.first;
		auto model = lt->modelMatrix();
		glm::mat3 modelInv = glm::transpose(glm::inverse(model));

		for (const auto& meshInstance : lt->meshInstances())
		{
			const auto& meshData = meshInstance->meshData;
			for (const auto& v : meshData->positions)
				vertices.push_back(model * glm::vec4(v, 1.0f));
			for (const auto& n : meshData->normals)
				normals.push_back(glm::normalize(modelInv * n));
			for (const auto& i : meshData->indices)
				indices.push_back(offIndVertex + i);
			offIndVertex += meshData->positions.size();
		}
	}

	BVH bvh(vertices, indices, bvhBuildMethod);
	auto bvhBuf = bvh.build();

	auto [lightPower, lightAlias, lightProb] = genLightTable();

	ret.vertex = TextureBuffered::createFromVector(vertices, TextureFormat::Col3x32f);
	ret.normal = TextureBuffered::createFromVector(normals, TextureFormat::Col3x32f);
	ret.texCoord = TextureBuffered::createFromVector(texCoords, TextureFormat::Col2x32f);
	ret.index = TextureBuffered::createFromVector(indices, TextureFormat::Col1x32i);
	ret.bound = TextureBuffered::createFromVector(bvhBuf.bounds, TextureFormat::Col3x32f);
	ret.hitTable = TextureBuffered::createFromVector(bvhBuf.hitTable, TextureFormat::Col3x32i);
	ret.matTexIndex = TextureBuffered::createFromVector(matTexIndices, TextureFormat::Col1x32i);
	ret.material = TextureBuffered::createFromVector(materials, TextureFormat::Col3x32f);
	ret.lightPower = TextureBuffered::createFromVector(lightPower, TextureFormat::Col3x32f);
	ret.lightAlias = TextureBuffered::createFromVector(lightAlias, TextureFormat::Col1x32i);
	ret.lightProb = TextureBuffered::createFromVector(lightProb, TextureFormat::Col1x32f);
	ret.textures = Texture2DArray::createFromImages(Resource::getAllImages(), TextureFormat::Col3x32f);
	ret.texUVScale = TextureBuffered::createFromVector(ret.textures->texScales(), TextureFormat::Col2x32f);

	return ret;
}

void Scene::addObject(ModelInstancePtr object)
{
	objects.push_back(object);
}

void Scene::addMaterial(const Material& material)
{
	materials.push_back(material);
}

void Scene::addLight(ModelInstancePtr light, const glm::vec3& radiance)
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
		auto model = lt->modelMatrix();

		for (const auto& meshInstance : lt->meshInstances())
		{
			const auto& meshData = meshInstance->meshData;
			for (size_t i = 0; i < meshData->indices.size() / 3; i++)
			{
				glm::vec3 va(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 0]], 1.0f));
				glm::vec3 vb(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 1]], 1.0f));
				glm::vec3 vc(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 2]], 1.0f));

				float area = 0.5f * glm::length(glm::cross(vc - va, vb - va));
				glm::vec3 p = radiance * area * glm::pi<float>() * 2.0f;
				power.push_back(p);
				pdf.push_back(brightness(p));
				lightSum += brightness(p);
			}
		}
		/*auto meshes = lt->meshes();
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
		}*/
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
