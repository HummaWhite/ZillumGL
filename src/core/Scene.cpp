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

	Error::log("Scene", "generating light sampling table");

	std::vector<glm::vec3> lightPower;
	std::vector<float> pdf;

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
				lightPower.push_back(p);
				pdf.push_back(brightness(p));
				lightSumPdf += brightness(p);
			}
			nLightTriangles += meshData->indices.size() / 3;
		}
	}
	auto [lightAlias, lightProb] = AliasTable::build<int32_t>(pdf);

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

	Error::log("Scene", "generating buffers for GL");
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
