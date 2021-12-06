#include "Scene.h"

#include "../thirdparty/pugixml/pugixml.hpp"

#include <sstream>

std::pair<ModelInstancePtr, std::optional<glm::vec3>> loadModelInstance(const pugi::xml_node& modelNode)
{
	auto model = Resource::openModelInstance(modelNode.attribute("path").as_string(), glm::vec3(0.0f));

	auto trans = modelNode.child("transform");
	glm::vec3 pos, scale, rot;

	std::stringstream posStr(trans.attribute("translate").as_string());
	posStr >> pos.x >> pos.y >> pos.z;

	std::stringstream scaleStr(trans.attribute("scale").as_string());
	scaleStr >> scale.x >> scale.y >> scale.z;

	std::stringstream rotStr(trans.attribute("rotate").as_string());
	rotStr >> rot.x >> rot.y >> rot.z;

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
	std::string typeStr(matNode.attribute("type").as_string());
	Error::bracketLine<1>("Material " + typeStr);

	if (typeStr == "default")
		return { model, std::nullopt };

	Material mat;
	
	auto color = matNode.first_child();
	auto metIor = color.next_sibling();
	auto rough = metIor.next_sibling();

	std::stringstream colorStr(color.attribute("value").as_string());
	colorStr >> mat.albedo.r >> mat.albedo.g >> mat.albedo.b;

	std::stringstream metIorStr(metIor.attribute("value").as_string());
	metIorStr >> mat.metIor;

	std::stringstream roughStr(rough.attribute("value").as_string());
	roughStr >> mat.roughness;

	mat.type = (typeStr == "metalWorkflow") ? 0 :
		(typeStr == "dielectric") ? 1 : 2;

	for (auto& m : model->materials())
		m = mat;

	return { model, std::nullopt };
}

void Scene::load()
{
	Error::bracketLine<0>("Scene " + SceneConfigPath);

	pugi::xml_document doc;
	doc.load_file(SceneConfigPath.c_str());
	auto scene = doc.child("scene");
	{
		auto integrator = scene.child("integrator");
		std::string integStr(integrator.attribute("type").as_string());
		aoMode = (integStr != "path");
		maxBounce = integrator.child("maxBounce").attribute("value").as_int();
		
		auto size = integrator.child("size");
		filmWidth = size.attribute("width").as_int();
		filmHeight = size.attribute("height").as_int();

		auto toneMapping = integrator.child("toneMapping");
		// TODO: load toneMapping
		Error::bracketLine<1>("Integrator " + integStr);
		Error::bracketLine<2>("MaxBounce " + std::to_string(maxBounce));
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
		lensRadius = cameraNode.child("lensRadius").attribute("value").as_float();
		focalDist = cameraNode.child("focalDistance").attribute("value").as_float();

		Error::bracketLine<1>("Camera " + std::string(cameraNode.attribute("type").as_string()));
		Error::bracketLine<2>("Position " + posStr);
		Error::bracketLine<2>("Angle " + angleStr);
		Error::bracketLine<2>("FOV " + std::string(cameraNode.child("fov").attribute("value").as_string()));
		Error::bracketLine<2>("LensRadius " + std::to_string(lensRadius));
		Error::bracketLine<2>("FocalDistance " + std::to_string(focalDist));
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
}

void Scene::saveToFile(const File::path& path)
{
}

void Scene::createGLContext()
{
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

	BVH bvh(vertices, indices, BVHSplitMethod::SAH);
	auto bvhBuf = bvh.build();

	Error::bracketLine<0>("Scene generating light sampling table");

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

	glContext.vertex = TextureBuffered::createFromVector(vertices, TextureFormat::Col3x32f);
	glContext.normal = TextureBuffered::createFromVector(normals, TextureFormat::Col3x32f);
	glContext.texCoord = TextureBuffered::createFromVector(texCoords, TextureFormat::Col2x32f);
	glContext.index = TextureBuffered::createFromVector(indices, TextureFormat::Col1x32i);
	glContext.bound = TextureBuffered::createFromVector(bvhBuf.bounds, TextureFormat::Col3x32f);
	glContext.hitTable = TextureBuffered::createFromVector(bvhBuf.hitTable, TextureFormat::Col3x32i);
	glContext.matTexIndex = TextureBuffered::createFromVector(matTexIndices, TextureFormat::Col1x32i);
	glContext.material = TextureBuffered::createFromVector(materials, TextureFormat::Col3x32f);
	glContext.lightPower = TextureBuffered::createFromVector(lightPower, TextureFormat::Col3x32f);
	glContext.lightAlias = TextureBuffered::createFromVector(lightAlias, TextureFormat::Col1x32i);
	glContext.lightProb = TextureBuffered::createFromVector(lightProb, TextureFormat::Col1x32f);
	glContext.textures = Texture2DArray::createFromImages(Resource::getAllImages(), TextureFormat::Col3x32f);
	glContext.texUVScale = TextureBuffered::createFromVector(glContext.textures->texScales(), TextureFormat::Col2x32f);

	sobolTex = Sampler::genSobolSeqTexture(SampleNum, SampleDim);
	noiseTex = Sampler::genNoiseTexture(filmWidth, filmHeight);

	Error::bracketLine<0>("Scene GL context created");
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
