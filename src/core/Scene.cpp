#include "Scene.h"

#include "../thirdparty/pugixml/pugixml.hpp"

#include <sstream>

std::tuple<glm::vec3, glm::vec3, glm::vec3> loadTransform(const pugi::xml_node& node)
{
	glm::vec3 pos, scale, rot;

	std::stringstream posStr(node.attribute("translate").as_string());
	posStr >> pos.x >> pos.y >> pos.z;

	std::stringstream scaleStr(node.attribute("scale").as_string());
	scaleStr >> scale.x >> scale.y >> scale.z;

	std::stringstream rotStr(node.attribute("rotate").as_string());
	rotStr >> rot.x >> rot.y >> rot.z;

	return { pos, scale, rot };
}

std::pair<ModelInstancePtr, std::optional<glm::vec3>> loadModelInstance(const pugi::xml_node& modelNode)
{
	auto modelPath = modelNode.attribute("path").as_string();
	auto model = Resource::openModelInstance(modelPath, glm::vec3(0.0f));

	std::string name(modelNode.attribute("name").as_string());
	model->setName(name);
	model->setPath(modelPath);

	auto transNode = modelNode.child("transform");
	auto [pos, scale, rot] = loadTransform(transNode);

	model->setPos(pos);
	model->setScale(scale.x, scale.y, scale.z);
	model->setRotation(rot);

	if (std::string(modelNode.attribute("type").as_string()) == "light")
	{
		glm::vec3 radiance;
		std::stringstream radStr(modelNode.child("radiance").attribute("value").as_string());
		radStr >> radiance.r >> radiance.g >> radiance.b;
		return { model, radiance };
	}

	auto matNode = modelNode.child("material");
	auto material = loadMaterial(matNode);
	if (!material)
		return { model, std::nullopt };

	for (auto& m : model->materials())
		m = material.value();

	return { model, std::nullopt };
}

bool Scene::load(const File::path& path)
{
	Error::bracketLine<0>("Scene " + path.generic_string());
	Resource::clear();
	clear();

	pugi::xml_document doc;
	doc.load_file(path.generic_string().c_str());
	auto scene = doc.child("scene");
	if (!scene)
		return false;
	{
		auto integrator = scene.child("integrator");
		std::string integStr(integrator.attribute("type").as_string());
		
		auto size = integrator.child("size");
		filmWidth = size.attribute("width").as_int();
		filmHeight = size.attribute("height").as_int();

		auto toneMapping = integrator.child("toneMapping");
		// TODO: load toneMapping
		Error::bracketLine<1>("Integrator " + integStr);
	}
	{
		auto samplerNode = scene.child("sampler");
		sampler = (std::string(samplerNode.attribute("type").as_string()) == "sobol") ? 1 : 0;
	}
	{
		auto cameraNode = scene.child("camera");

		glm::vec3 pos, angle;
		std::string posStr(cameraNode.child("position").attribute("value").as_string());
		std::stringstream posSS(posStr);
		posSS >> pos.x >> pos.y >> pos.z;

		std::string angleStr(cameraNode.child("angle").attribute("value").as_string());
		std::stringstream angleSS(angleStr);
		angleSS >> angle.x >> angle.y >> angle.z;
		
		camera.setPos(pos);
		camera.setAngle(angle);
		camera.setFOV(cameraNode.child("fov").attribute("value").as_float());
		camera.setAspect(static_cast<float>(filmWidth) / filmHeight);
		camera.setLensRadius(cameraNode.child("lensRadius").attribute("value").as_float());
		camera.setFocalDist(cameraNode.child("focalDistance").attribute("value").as_float());
		originalCamera = previewCamera = camera;

		Error::bracketLine<1>("Camera " + std::string(cameraNode.attribute("type").as_string()));
		Error::bracketLine<2>("Position " + posStr);
		Error::bracketLine<2>("Angle " + angleStr);
		Error::bracketLine<2>("FOV " + std::to_string(camera.FOV()));
		Error::bracketLine<2>("LensRadius " + std::to_string(camera.lensRadius()));
		Error::bracketLine<2>("FocalDistance " + std::to_string(camera.focalDist()));
	}
	{
		auto modelInstances = scene.child("modelInstances");
		for (auto instance = modelInstances.first_child(); instance; instance = instance.next_sibling())
		{
			auto [model, radiance] = loadModelInstance(instance);
			if (radiance.has_value())
				addLight(model, radiance.value());
			else
				addObject(model);
		}
	}
	{
		envMap = EnvironmentMap::create(scene.child("envMap").attribute("path").as_string());
	}
	return true;
}

void Scene::saveToFile(const File::path& path)
{
}

void Scene::createGLContext(bool resetTextures)
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> matTexIndices;

	lightSumPdf = 0.0f;
	nLightTriangles = 0;
	objPrimCount = 0;

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
			meshInstance->globalMatIndex = (meshInstance->matIndex != -1) ?
				meshInstance->matIndex + offIndMaterial : -1;

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

	BVH bvh(vertices, indices);
	auto bvhBuf = bvh.build();

	Error::bracketLine<0>("Scene generating light sampling table");

	std::vector<glm::vec3> lightPower;
	std::vector<float> pdf;

	auto luminance = [](const glm::vec3& v) -> float
	{
		return glm::dot(v, glm::vec3(0.299f, 0.587f, 0.114f));
	};

	for (const auto light : lights)
	{
		const auto [lt, sumPower] = light;
		auto model = lt->modelMatrix();

		for (const auto& meshInstance : lt->meshInstances())
		{
			const auto& meshData = meshInstance->meshData;
			float sumArea = 0.0f;
			for (size_t i = 0; i < meshData->indices.size() / 3; i++)
			{
				glm::vec3 va(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 0]], 1.0f));
				glm::vec3 vb(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 1]], 1.0f));
				glm::vec3 vc(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 2]], 1.0f));
				float area = glm::length(glm::cross(vc - va, vb - va));
				sumArea += area;
			}

			for (size_t i = 0; i < meshData->indices.size() / 3; i++)
			{
				glm::vec3 va(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 0]], 1.0f));
				glm::vec3 vb(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 1]], 1.0f));
				glm::vec3 vc(model * glm::vec4(meshData->positions[meshData->indices[i * 3 + 2]], 1.0f));

				float area = glm::length(glm::cross(vc - va, vb - va));
				glm::vec3 power = sumPower * area / sumArea;
				lightPower.push_back(power);
				pdf.push_back(luminance(power)); 
				lightSumPdf += luminance(power);
			}
			nLightTriangles += meshData->indices.size() / 3;
		}
	}
	auto [lightAlias, lightProb] = AliasTable::build<int32_t>(pdf);

	glContext.vertex = TextureBuffered::createFromVector(vertices, TextureFormat::Col3x32f);
	glContext.normal = TextureBuffered::createFromVector(normals, TextureFormat::Col3x32f);
	glContext.texCoord = TextureBuffered::createFromVector(texCoords, TextureFormat::Col2x32f);
	glContext.index = TextureBuffered::createFromVector(indices, TextureFormat::Col1x32i);
	glContext.bound = TextureBuffered::createFromVector(bvhBuf.bounds, TextureFormat::Col3x32f);
	glContext.hitTable = TextureBuffered::createFromVector(bvhBuf.hitTable, TextureFormat::Col3x32i);
	glContext.matTexIndex = TextureBuffered::createFromVector(matTexIndices, TextureFormat::Col1x32i);
	glContext.material = TextureBuffered::createFromVector(materials, TextureFormat::Col4x32f);
	glContext.lightPower = TextureBuffered::createFromVector(lightPower, TextureFormat::Col3x32f);
	glContext.lightAlias = TextureBuffered::createFromVector(lightAlias, TextureFormat::Col1x32i);
	glContext.lightProb = TextureBuffered::createFromVector(lightProb, TextureFormat::Col1x32f);
	if (resetTextures)
		glContext.textures = Texture2DArray::createFromImages(Resource::getAllImages(), TextureFormat::Col3x32f);
	glContext.texUVScale = TextureBuffered::createFromVector(glContext.textures->texScales(), TextureFormat::Col2x32f);

	if (resetTextures)
	{
		sobolTex = Sampler::genSobolSeqTexture(SampleNum, SampleDim);
		noiseTex = Sampler::genNoiseTexture(filmWidth, filmHeight);
	}
	vertexCount = vertices.size();
	triangleCount = vertices.size() / 3;
	boxCount = bvhBuf.bounds.size();

	Error::bracketLine<0>("Scene GL context created");
}

void Scene::clear()
{
	objects.clear();
	lights.clear();
	materials.clear();
}

void Scene::addObject(ModelInstancePtr object)
{
	objects.push_back(object);
}

void Scene::addMaterial(const Material& material)
{
	materials.push_back(material);
}

void Scene::addLight(ModelInstancePtr light, const glm::vec3& power)
{
	lights.push_back({ light, power });
}
